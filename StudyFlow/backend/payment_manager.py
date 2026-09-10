import base64
import os
import threading
from datetime import datetime, timedelta, timezone
from typing import Any, Dict, Optional

import requests as paypal_requests
import stripe
from fastapi import HTTPException, Request

from email_notify import try_notify_subscription_started
from subscription_manager import SubscriptionManager


# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

def _env(key: str, default: str = "") -> str:
    return (os.environ.get(key) or default).strip().strip('"').strip("'")


STRIPE_SECRET_KEY = _env("STRIPE_SECRET_KEY")
STRIPE_PUBLISHABLE_KEY = _env("STRIPE_PUBLISHABLE_KEY")
STRIPE_WEBHOOK_SECRET = _env("STRIPE_WEBHOOK_SECRET")

# Price IDs configured in Stripe Dashboard. Env names are explicit for each tier/interval.
STRIPE_PRICE_IDS = {
    ("pro", "month"): _env("STRIPE_PRICE_PRO_MONTHLY"),
    ("pro", "year"): _env("STRIPE_PRICE_PRO_YEARLY"),
    ("premium", "month"): _env("STRIPE_PRICE_PREMIUM_MONTHLY"),
    ("premium", "year"): _env("STRIPE_PRICE_PREMIUM_YEARLY"),
}

if STRIPE_SECRET_KEY:
    stripe.api_key = STRIPE_SECRET_KEY


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _stripe_enabled() -> bool:
    return bool(STRIPE_SECRET_KEY)


def _stripe_dict(value: Any) -> Dict[str, Any]:
    if isinstance(value, dict):
        return value
    if hasattr(value, "to_dict_recursive"):
        return value.to_dict_recursive()
    try:
        return dict(value)
    except (TypeError, ValueError):
        return {}


def _stripe_id(value: Any) -> Optional[str]:
    if isinstance(value, str):
        return value
    return _stripe_dict(value).get("id")


def _price_id(tier: str, interval: str) -> str:
    price_id = STRIPE_PRICE_IDS.get((tier, interval))
    if not price_id:
        raise HTTPException(
            status_code=400,
            detail=f"Kein Stripe-Preis für {tier}/{interval} konfiguriert.",
        )
    return price_id


def _public_app_url() -> str:
    return _env("BLOP_APP_PUBLIC_URL", "https://www.blop-study.com").rstrip("/")


def _subscription_credits_for_interval(tier: str, interval: str) -> int:
    """How many months of tokens to credit on a successful checkout/renewal."""
    tier_info = SubscriptionManager.get_tier(tier)
    if not tier_info:
        return 0
    tokens_monthly = int(tier_info.get("tokens_monthly", 0))
    if interval == "year":
        return tokens_monthly * 12
    return tokens_monthly


def _notify_subscription_started(username: str, tier: str, interval: str, provider: str) -> None:
    threading.Thread(
        target=try_notify_subscription_started,
        args=(username, tier, interval, provider),
        daemon=True,
    ).start()


def _subscription_db():
    db = SubscriptionManager._get_db()
    if not db:
        raise HTTPException(status_code=503, detail="Abo-Datenbank nicht erreichbar.")
    return db


def _save_checkout(checkout_session_id: str, username: str, tier: str, interval: str, customer_id: str) -> None:
    _subscription_db().table("subscription_checkouts").upsert({
        "checkout_session_id": checkout_session_id,
        "username": username,
        "tier": tier,
        "billing_interval": interval,
        "provider_customer_id": customer_id,
        "status": "created",
        "updated_at": datetime.now(timezone.utc).isoformat(),
    }).execute()


def _get_checkout(checkout_session_id: str) -> Optional[Dict[str, Any]]:
    result = (
        _subscription_db().table("subscription_checkouts")
        .select("*").eq("checkout_session_id", checkout_session_id).execute()
    )
    return result.data[0] if result.data else None


def _complete_checkout(checkout_session_id: str, subscription_id: str, customer_id: str) -> None:
    _subscription_db().table("subscription_checkouts").update({
        "provider_subscription_id": subscription_id,
        "provider_customer_id": customer_id,
        "status": "completed",
        "completed_at": datetime.now(timezone.utc).isoformat(),
        "updated_at": datetime.now(timezone.utc).isoformat(),
    }).eq("checkout_session_id", checkout_session_id).execute()


def _invoice_subscription_id(invoice: Dict[str, Any]) -> Optional[str]:
    legacy = invoice.get("subscription")
    if isinstance(legacy, str):
        return legacy
    parent = _stripe_dict(invoice.get("parent", {}))
    if parent.get("type") != "subscription_details":
        return None
    details = _stripe_dict(parent.get("subscription_details", {}))
    subscription = details.get("subscription")
    return subscription if isinstance(subscription, str) else _stripe_dict(subscription).get("id")


# ---------------------------------------------------------------------------
# Stripe Checkout
# ---------------------------------------------------------------------------

def create_checkout_session(
    username: str,
    email: str,
    tier: str,
    interval: str,
    success_url: Optional[str] = None,
    cancel_url: Optional[str] = None,
) -> Dict[str, Any]:
    if not _stripe_enabled():
        raise HTTPException(status_code=503, detail="Stripe ist nicht konfiguriert.")
    if tier not in ("pro", "premium"):
        raise HTTPException(status_code=400, detail="Nur Pro und Premium können über Stripe gebucht werden.")
    if interval not in ("month", "year"):
        raise HTTPException(status_code=400, detail="Intervall muss 'month' oder 'year' sein.")

    price_id = _price_id(tier, interval)
    base = _public_app_url()
    success = (success_url or f"{base}/settings").rstrip("/")
    cancel = (cancel_url or f"{base}/pricing?subscription=canceled").rstrip("/")

    try:
        pending = (
            _subscription_db().table("subscription_checkouts")
            .select("checkout_session_id, created_at")
            .eq("username", username).eq("tier", tier).eq("billing_interval", interval)
            .eq("status", "created").order("created_at", desc=True).limit(1).execute()
        )
        if pending.data:
            pending_session = _stripe_dict(stripe.checkout.Session.retrieve(pending.data[0]["checkout_session_id"]))
            if pending_session.get("status") == "open" and pending_session.get("url"):
                return {"url": pending_session["url"], "session_id": pending_session["id"]}
        existing = SubscriptionManager.get_user_subscription(username)
        customer_id = existing.get("provider_customer_id") if existing.get("provider") == "stripe" else None
        customers_by_id: Dict[str, Dict[str, Any]] = {}
        if email:
            for row in _stripe_dict(stripe.Customer.list(email=email, limit=100)).get("data", []) or []:
                customer = _stripe_dict(row)
                if customer.get("id"):
                    customers_by_id[customer["id"]] = customer
        for row in _stripe_dict(stripe.Customer.list(limit=100)).get("data", []) or []:
            customer = _stripe_dict(row)
            if _stripe_dict(customer.get("metadata", {})).get("username") == username and customer.get("id"):
                customers_by_id[customer["id"]] = customer
        customer_rows = list(customers_by_id.values())
        if customer_id and customer_id not in customers_by_id:
            customer_rows.insert(0, {"id": customer_id})

        active_subscriptions = []
        for customer_row in customer_rows:
            candidate_customer_id = customer_row.get("id")
            if not candidate_customer_id:
                continue
            result = _stripe_dict(stripe.Subscription.list(customer=candidate_customer_id, status="all", limit=100))
            for item in result.get("data") or []:
                subscription = _stripe_dict(item)
                if subscription.get("status") in ("active", "trialing", "past_due"):
                    active_subscriptions.append(subscription)

        if active_subscriptions:
            active_subscriptions.sort(key=lambda item: int(item.get("created") or 0), reverse=True)
            active = active_subscriptions[0]
            active_metadata = _stripe_dict(active.get("metadata", {}))
            active["metadata"] = {
                **active_metadata,
                "blop_username": username,
                "username": username,
                "tier": active_metadata.get("tier") or _tier_from_stripe_subscription(active),
                "interval": active_metadata.get("interval") or _interval_from_stripe_subscription(active),
            }
            _handle_subscription_created_or_updated(
                active,
                grant_period_tokens=True,
                operation_key=f"repair:{active.get('id')}",
            )
            raise HTTPException(
                status_code=409,
                detail=(
                    "Für diesen Account existiert bereits ein aktives Stripe-Abo. "
                    "Es wurde mit Blop Study synchronisiert; weitere Käufe wurden blockiert."
                ),
            )

        if not customer_id and customer_rows:
            customer_id = customer_rows[0].get("id")
        if not customer_id:
            customer_metadata = {"blop_username": username, "username": username}
            if email:
                customer_metadata["email"] = email
            customer_id = stripe.Customer.create(
                email=email or None,
                metadata=customer_metadata,
            ).id

        metadata = {"blop_username": username, "username": username, "tier": tier, "interval": interval}
        session = stripe.checkout.Session.create(
            customer=customer_id,
            client_reference_id=username,
            mode="subscription",
            line_items=[{"price": price_id, "quantity": 1}],
            success_url=f"{success}?subscription=success&checkout_session_id={{CHECKOUT_SESSION_ID}}",
            cancel_url=cancel,
            subscription_data={"metadata": metadata},
            metadata=metadata,
            allow_promotion_codes=True,
            billing_address_collection="auto",
        )
        _save_checkout(session.id, username, tier, interval, customer_id)
        return {"url": session.url, "session_id": session.id}
    except HTTPException:
        raise
    except Exception as exc:
        print(f"Stripe create_checkout_session failed ({type(exc).__name__}): {exc}")
        message = getattr(exc, "user_message", None) or str(exc)
        raise HTTPException(status_code=502, detail=f"Stripe Checkout konnte nicht erstellt werden: {message}")


def reconcile_checkout_session(username: str, checkout_session_id: str) -> Dict[str, Any]:
    """Verify a completed Checkout Session and synchronize its subscription."""
    if not _stripe_enabled():
        raise HTTPException(status_code=503, detail="Stripe ist nicht konfiguriert.")
    try:
        checkout = _get_checkout(checkout_session_id)
        if checkout and checkout.get("username") != username:
            raise HTTPException(status_code=403, detail="Checkout gehört nicht zum angemeldeten Benutzer.")
        session = _stripe_dict(stripe.checkout.Session.retrieve(checkout_session_id))
        metadata = _stripe_dict(session.get("metadata", {}))
        customer_id = _stripe_id(session.get("customer"))
        customer = _stripe_dict(stripe.Customer.retrieve(customer_id)) if customer_id else {}
        subscription_id = _stripe_id(session.get("subscription"))
        subscription = _stripe_dict(stripe.Subscription.retrieve(subscription_id)) if subscription_id else {}
        if not subscription:
            safe_username = username.replace("\\", "\\\\").replace("'", "\\'")
            matching_customers = _stripe_dict(
                stripe.Customer.search(query=f"metadata['username']:'{safe_username}'", limit=100)
            ).get("data") or []
            candidates = []
            for matching_customer in matching_customers:
                matching_customer = _stripe_dict(matching_customer)
                matching_customer_id = matching_customer.get("id")
                if not matching_customer_id:
                    continue
                subscriptions = _stripe_dict(
                    stripe.Subscription.list(customer=matching_customer_id, status="all", limit=100)
                )
                for item in subscriptions.get("data", []) or []:
                    item = _stripe_dict(item)
                    if item.get("status") in ("active", "trialing", "past_due"):
                        candidates.append((item, matching_customer))
            if candidates:
                candidates.sort(key=lambda pair: int(pair[0].get("created") or 0), reverse=True)
                subscription, customer = candidates[0]
                subscription_id = subscription.get("id")
                customer_id = customer.get("id")
        if not subscription_id or not subscription:
            raise HTTPException(
                status_code=409,
                detail="Stripe hat für diesen Blop-Account kein aktives Abo gefunden. Prüfe in Stripe die Kunden-Metadaten 'username'.",
            )
        subscription_metadata = _stripe_dict(subscription.get("metadata", {}))
        payment_status = session.get("payment_status")
        checkout_status = session.get("status")
        subscription_status = subscription.get("status")
        payment_confirmed = payment_status in ("paid", "no_payment_required")
        active_subscription = checkout_status == "complete" and subscription_status in ("active", "trialing")
        if not payment_confirmed and not active_subscription:
            raise HTTPException(
                status_code=409,
                detail=(
                    "Die Zahlung ist bei Stripe noch nicht bestätigt "
                    f"(Checkout: {checkout_status or 'unbekannt'}, Zahlung: {payment_status or 'unbekannt'}, "
                    f"Abo: {subscription_status or 'unbekannt'})."
                ),
            )
        customer_metadata = _stripe_dict(customer.get("metadata", {}))
        owner_candidates = {
            str(value).strip()
            for value in (
                (checkout or {}).get("username"),
                metadata.get("blop_username"),
                metadata.get("username"),
                subscription_metadata.get("blop_username"),
                subscription_metadata.get("username"),
                customer_metadata.get("username"),
                session.get("client_reference_id"),
            )
            if value
        }
        if username not in owner_candidates:
            print(f"Stripe checkout owner mismatch: session={checkout_session_id}, authenticated={username}, candidates={sorted(owner_candidates)}")
            raise HTTPException(status_code=403, detail="Checkout gehört nicht zum angemeldeten Benutzer.")
        tier = (checkout or {}).get("tier") or metadata.get("tier") or subscription_metadata.get("tier") or _tier_from_stripe_subscription(subscription)
        interval = (checkout or {}).get("billing_interval") or metadata.get("interval") or subscription_metadata.get("interval") or "month"
        if tier not in ("pro", "premium"):
            raise HTTPException(status_code=409, detail="Stripe-Preis konnte keinem Blop-Abo zugeordnet werden.")
        resolved_metadata = {**subscription_metadata, "blop_username": username, "username": username, "tier": tier, "interval": interval}
        if resolved_metadata != subscription_metadata:
            stripe.Subscription.modify(subscription_id, metadata=resolved_metadata)
        subscription["metadata"] = resolved_metadata
        _handle_subscription_created_or_updated(subscription, operation_key=f"checkout:{checkout_session_id}")
        _complete_checkout(checkout_session_id, subscription_id, customer_id)
        return {"status": "active", "tier": tier}
    except HTTPException:
        raise
    except Exception as exc:
        print(f"Stripe reconcile checkout failed ({type(exc).__name__}): {exc}")
        message = getattr(exc, "user_message", None) or str(exc)
        raise HTTPException(status_code=502, detail=f"Stripe-Zahlung konnte nicht bestätigt werden: {message}")


def _tier_from_stripe_subscription(subscription: Dict[str, Any]) -> str:
    metadata = _stripe_dict(subscription.get("metadata", {}))
    tier = str(metadata.get("tier") or "").strip().lower()
    if tier in ("pro", "premium"):
        return tier
    items = _stripe_dict(subscription.get("items", {})).get("data", []) or []
    for item_value in items:
        item = _stripe_dict(item_value)
        legacy_price = _stripe_dict(item.get("price", {}))
        pricing = _stripe_dict(item.get("pricing", {}))
        price_details = _stripe_dict(pricing.get("price_details", {}))
        price_value = price_details.get("price")
        price_id = legacy_price.get("id") or (
            price_value if isinstance(price_value, str) else _stripe_dict(price_value).get("id")
        )
        for (configured_tier, _), configured_price_id in STRIPE_PRICE_IDS.items():
            if configured_price_id and price_id == configured_price_id:
                return configured_tier
        product_value = legacy_price.get("product") or price_details.get("product")
        product_id = product_value if isinstance(product_value, str) else _stripe_dict(product_value).get("id")
        if not product_id and price_id:
            price = _stripe_dict(stripe.Price.retrieve(price_id))
            product_value = price.get("product")
            product_id = product_value if isinstance(product_value, str) else _stripe_dict(product_value).get("id")
        if product_id:
            product = _stripe_dict(stripe.Product.retrieve(product_id))
            product_name = str(product.get("name") or "").strip().lower()
            if "premium" in product_name:
                return "premium"
            if "pro" in product_name:
                return "pro"
    return ""


def _interval_from_stripe_subscription(subscription: Dict[str, Any]) -> str:
    metadata = _stripe_dict(subscription.get("metadata", {}))
    if metadata.get("interval") in ("month", "year"):
        return metadata["interval"]
    items = _stripe_dict(subscription.get("items", {})).get("data", []) or []
    for item_value in items:
        item = _stripe_dict(item_value)
        recurring = _stripe_dict(_stripe_dict(item.get("price", {})).get("recurring", {}))
        if not recurring:
            recurring = _stripe_dict(_stripe_dict(_stripe_dict(item.get("pricing", {})).get("price_details", {})).get("recurring", {}))
        if recurring.get("interval") in ("month", "year"):
            return recurring["interval"]
    return "month"


def sync_stripe_subscription(username: str, email: str) -> Dict[str, Any]:
    if not _stripe_enabled():
        raise HTTPException(status_code=503, detail="Stripe ist nicht konfiguriert.")
    email = (email or "").strip().lower()
    try:
        customers_by_id: Dict[str, Dict[str, Any]] = {}
        if email:
            for customer_value in _stripe_dict(stripe.Customer.list(email=email, limit=100)).get("data", []) or []:
                customer = _stripe_dict(customer_value)
                if customer.get("id"):
                    customers_by_id[customer["id"]] = customer
        safe_username = username.replace("\\", "\\\\").replace("'", "\\'")
        try:
            username_customers = _stripe_dict(
                stripe.Customer.search(query=f"metadata['username']:'{safe_username}'", limit=100)
            ).get("data", []) or []
        except Exception as exc:
            print(f"Stripe customer metadata search unavailable ({type(exc).__name__}): {exc}")
            username_customers = []
        for customer_value in username_customers:
            customer = _stripe_dict(customer_value)
            if customer.get("id"):
                customers_by_id[customer["id"]] = customer
        for customer_value in _stripe_dict(stripe.Customer.list(limit=100)).get("data", []) or []:
            customer = _stripe_dict(customer_value)
            metadata = _stripe_dict(customer.get("metadata", {}))
            if metadata.get("username") == username and customer.get("id"):
                customers_by_id[customer["id"]] = customer

        candidates = []
        for customer in customers_by_id.values():
            customer_id = customer.get("id")
            customer_email = str(customer.get("email") or "").strip().lower()
            customer_username = _stripe_dict(customer.get("metadata", {})).get("username")
            if not customer_id or (customer_username != username and (not email or customer_email != email)):
                continue
            subscriptions = _stripe_dict(
                stripe.Subscription.list(customer=customer_id, status="all", limit=100)
            ).get("data", []) or []
            for subscription_value in subscriptions:
                subscription = _stripe_dict(subscription_value)
                if subscription.get("status") in ("active", "trialing", "past_due"):
                    tier = _tier_from_stripe_subscription(subscription)
                    if tier:
                        candidates.append(subscription)
        if candidates:
            candidates.sort(key=lambda item: int(item.get("created") or 0), reverse=True)
            subscription = candidates[0]
            metadata = _stripe_dict(subscription.get("metadata", {}))
            resolved = {
                **metadata,
                "blop_username": username,
                "username": username,
                "tier": _tier_from_stripe_subscription(subscription),
                "interval": metadata.get("interval") or _interval_from_stripe_subscription(subscription),
            }
            if resolved != metadata:
                stripe.Subscription.modify(subscription["id"], metadata=resolved)
            subscription["metadata"] = resolved
            _handle_subscription_created_or_updated(
                subscription,
                grant_period_tokens=True,
                operation_key=f"repair:{subscription.get('id')}",
            )
            return {
                "status": "success",
                "found": True,
                "tier": _tier_from_stripe_subscription(subscription),
                "cancel_at_period_end": bool(subscription.get("cancel_at_period_end")),
            }

        current = SubscriptionManager.get_user_subscription(username)
        return {
            "status": "not_found",
            "found": False,
            "tier": current.get("tier", "free"),
        }
    except HTTPException:
        raise
    except Exception as exc:
        print(f"Stripe subscription sync failed ({type(exc).__name__}): {exc}")
        message = getattr(exc, "user_message", None) or str(exc)
        raise HTTPException(status_code=502, detail=f"Stripe-Abo konnte nicht synchronisiert werden: {message}")


def admin_reconcile_stripe_subscriptions(apply: bool = False) -> Dict[str, Any]:
    db = _subscription_db()
    users = db.table("users").select("username, email").execute().data
    users_by_name = {row["username"]: row for row in users}
    users_by_email: Dict[str, list] = {}
    for row in users:
        email = str(row.get("email") or "").strip().lower()
        if email:
            users_by_email.setdefault(email, []).append(row["username"])

    subscription_page = stripe.Subscription.list(status="all", limit=100)
    subscriptions = subscription_page.auto_paging_iter() if hasattr(subscription_page, "auto_paging_iter") else _stripe_dict(subscription_page).get("data", []) or []
    results = []
    for value in subscriptions:
        subscription = _stripe_dict(value)
        if subscription.get("status") not in ("active", "trialing", "past_due"):
            continue
        metadata = _stripe_dict(subscription.get("metadata", {}))
        username = metadata.get("blop_username") or metadata.get("username")
        reason = "metadata" if username in users_by_name else ""
        customer_id = _stripe_id(subscription.get("customer"))
        customer = _stripe_dict(stripe.Customer.retrieve(customer_id))
        email = str(customer.get("email") or "").strip().lower()
        if not reason and email and len(users_by_email.get(email, [])) == 1:
            username = users_by_email[email][0]
            reason = "email"
        tier = metadata.get("tier") or _tier_from_stripe_subscription(subscription)
        outcome = "matched" if reason and tier in ("pro", "premium") else "unmatched"
        if apply and outcome == "matched":
            resolved = {
                **metadata,
                "blop_username": username,
                "username": username,
                "tier": tier,
                "interval": metadata.get("interval") or _interval_from_stripe_subscription(subscription),
            }
            if resolved != metadata:
                stripe.Subscription.modify(subscription["id"], metadata=resolved)
            subscription["metadata"] = resolved
            _handle_subscription_created_or_updated(
                subscription,
                grant_period_tokens=True,
                operation_key=f"repair:{subscription.get('id')}",
            )
            outcome = "applied"
        results.append({
            "subscription_id": subscription.get("id"),
            "customer_id": _stripe_id(subscription.get("customer")),
            "username": username if reason else None,
            "tier": tier or None,
            "status": subscription.get("status"),
            "match_reason": reason or None,
            "outcome": outcome,
        })
    return {"status": "success", "apply": apply, "results": results}


def admin_repair_stripe_subscription(username: str, subscription_id: str) -> Dict[str, Any]:
    if not subscription_id.startswith("sub_"):
        raise HTTPException(status_code=400, detail="Ungültige Stripe Subscription-ID.")
    subscription = _stripe_dict(stripe.Subscription.retrieve(subscription_id))
    if subscription.get("status") not in ("active", "trialing", "past_due"):
        raise HTTPException(status_code=409, detail="Stripe-Abo ist nicht aktiv.")
    tier = _tier_from_stripe_subscription(subscription)
    if tier not in ("pro", "premium"):
        raise HTTPException(status_code=409, detail="Stripe-Preis ist keinem Blop-Tier zugeordnet.")
    metadata = _stripe_dict(subscription.get("metadata", {}))
    resolved = {
        **metadata,
        "blop_username": username,
        "username": username,
        "tier": tier,
        "interval": metadata.get("interval") or _interval_from_stripe_subscription(subscription),
    }
    stripe.Subscription.modify(subscription_id, metadata=resolved)
    subscription["metadata"] = resolved
    _handle_subscription_created_or_updated(
        subscription,
        grant_period_tokens=True,
        operation_key=f"repair:{subscription_id}",
    )
    return {"status": "success", "username": username, "tier": tier, "subscription_id": subscription_id}


def stripe_subscription_health() -> Dict[str, Any]:
    db = _subscription_db()
    tables = {}
    for table in ("subscriptions", "subscription_checkouts", "stripe_webhook_events", "subscription_token_grants"):
        try:
            db.table(table).select("*", count="exact").limit(1).execute()
            tables[table] = True
        except Exception:
            tables[table] = False
    prices = {}
    for (tier, interval), price_id in STRIPE_PRICE_IDS.items():
        key = f"{tier}_{interval}"
        if not price_id:
            prices[key] = False
            continue
        try:
            stripe.Price.retrieve(price_id)
            prices[key] = True
        except Exception:
            prices[key] = False
    events = db.table("stripe_webhook_events").select("event_id, event_type, processing_status, processed_at").order("received_at", desc=True).limit(1).execute()
    return {
        "stripe_secret_configured": bool(STRIPE_SECRET_KEY),
        "webhook_secret_configured": bool(STRIPE_WEBHOOK_SECRET),
        "tables": tables,
        "prices": prices,
        "last_webhook": events.data[0] if events.data else None,
    }


def cancel_stripe_subscription(username: str) -> Dict[str, Any]:
    current = SubscriptionManager.get_user_subscription(username)
    subscription_id = current.get("provider_subscription_id")
    if current.get("provider") != "stripe" or not subscription_id:
        raise HTTPException(status_code=400, detail="Kein aktives Stripe-Abo für diesen Account gefunden.")
    try:
        subscription = _stripe_dict(
            stripe.Subscription.modify(subscription_id, cancel_at_period_end=True)
        )
        _handle_subscription_created_or_updated(subscription, grant_period_tokens=False)
        return {
            "status": "success",
            "cancel_at_period_end": True,
            "current_period_end": subscription.get("current_period_end"),
        }
    except Exception as exc:
        print(f"Stripe subscription cancellation failed ({type(exc).__name__}): {exc}")
        message = getattr(exc, "user_message", None) or str(exc)
        raise HTTPException(status_code=502, detail=f"Abo konnte nicht gekündigt werden: {message}")


# ---------------------------------------------------------------------------
# Customer portal
# ---------------------------------------------------------------------------

def create_portal_session(username: str, customer_id: str) -> Dict[str, Any]:
    if not _stripe_enabled():
        raise HTTPException(status_code=503, detail="Stripe ist nicht konfiguriert.")
    try:
        base = _public_app_url()
        session = stripe.billing_portal.Session.create(
            customer=customer_id,
            return_url=f"{base}/settings",
        )
        return {"url": session.url}
    except HTTPException:
        raise
    except Exception as exc:
        print(f"Stripe create_portal_session failed ({type(exc).__name__}): {exc}")
        message = getattr(exc, "user_message", None) or str(exc)
        raise HTTPException(status_code=502, detail=f"Stripe Portal konnte nicht erstellt werden: {message}")


# ---------------------------------------------------------------------------
# Webhooks
# ---------------------------------------------------------------------------

def _credit_tokens(username: str, tier: str, interval: str, grant_key: str) -> bool:
    db = _subscription_db()
    credits = _subscription_credits_for_interval(tier, interval)
    if credits <= 0:
        return False
    existing = (
        db.table("subscription_token_grants").select("provider_invoice_id")
        .eq("provider", "stripe").eq("provider_invoice_id", grant_key).execute()
    )
    if existing.data:
        return False
    db.table("users").update({"tokens": credits}).eq("username", username).execute()
    try:
        db.table("subscription_token_grants").insert({
            "provider": "stripe",
            "provider_invoice_id": grant_key,
            "username": username,
            "tier": tier,
            "tokens": credits,
        }).execute()
    except Exception:
        existing = (
            db.table("subscription_token_grants").select("provider_invoice_id")
            .eq("provider", "stripe").eq("provider_invoice_id", grant_key).execute()
        )
        if not existing.data:
            raise
    print(f"PaymentManager: reset {username} to {credits} tokens for {tier}/{interval}")
    return True


def _handle_subscription_created_or_updated(
    subscription: Dict[str, Any],
    grant_period_tokens: bool = False,
    operation_key: Optional[str] = None,
    event_id: Optional[str] = None,
):
    subscription = _stripe_dict(subscription)
    metadata = _stripe_dict(subscription.get("metadata", {}))
    username = metadata.get("blop_username") or metadata.get("username")
    tier = metadata.get("tier") or _tier_from_stripe_subscription(subscription)
    interval = metadata.get("interval") or _interval_from_stripe_subscription(subscription)
    if not username or tier not in ("pro", "premium"):
        raise RuntimeError(f"Stripe subscription mapping incomplete: {subscription.get('id')}")

    # Determine period end from the subscription object.
    current_period_end_ts = subscription.get("current_period_end")
    current_period_start_ts = subscription.get("current_period_start")
    items = _stripe_dict(subscription.get("items", {})).get("data", []) or []
    if items and (not current_period_start_ts or not current_period_end_ts):
        first_item = _stripe_dict(items[0])
        current_period_start_ts = current_period_start_ts or first_item.get("current_period_start")
        current_period_end_ts = current_period_end_ts or first_item.get("current_period_end")
    period_end = None
    period_start = None
    if current_period_end_ts:
        period_end = datetime.fromtimestamp(current_period_end_ts, tz=timezone.utc)
    if current_period_start_ts:
        period_start = datetime.fromtimestamp(current_period_start_ts, tz=timezone.utc)

    previous = SubscriptionManager.get_user_subscription(username)
    is_new_subscription = previous.get("provider_subscription_id") != subscription.get("id")

    SubscriptionManager.set_user_subscription(
        username=username,
        tier=tier,
        status=subscription.get("status", "active"),
        provider="stripe",
        provider_customer_id=_stripe_id(subscription.get("customer")),
        provider_subscription_id=subscription.get("id"),
        current_period_start=period_start,
        current_period_end=period_end,
        cancel_at_period_end=subscription.get("cancel_at_period_end", False),
        reset_tokens=False,
        last_provider_event_id=event_id,
    )
    if grant_period_tokens:
        key = operation_key or event_id or f"subscription:{subscription.get('id')}"
        _credit_tokens(username, tier, interval, key)
    if is_new_subscription:
        _notify_subscription_started(username, tier, interval, "stripe")


def _handle_subscription_deleted(subscription: Dict[str, Any]):
    subscription = _stripe_dict(subscription)
    metadata = _stripe_dict(subscription.get("metadata", {}))
    username = metadata.get("blop_username") or metadata.get("username")
    if not username:
        return
    # Downgrade to default tier at period end. For simplicity we downgrade immediately.
    default_tier = SubscriptionManager._get_default_tier()
    SubscriptionManager.set_user_subscription(
        username=username,
        tier=default_tier,
        status="active",
        provider="none",
        provider_customer_id=_stripe_id(subscription.get("customer")),
        provider_subscription_id=subscription.get("id"),
        reset_tokens=True,
    )


def _claim_webhook_event(event_id: str, event_type: str, object_id: Optional[str]) -> bool:
    db = _subscription_db()
    existing = db.table("stripe_webhook_events").select("processing_status, attempt_count").eq("event_id", event_id).execute()
    now = datetime.now(timezone.utc).isoformat()
    if existing.data:
        row = existing.data[0]
        if row.get("processing_status") == "processed":
            return False
        db.table("stripe_webhook_events").update({
            "processing_status": "processing",
            "attempt_count": int(row.get("attempt_count") or 0) + 1,
            "last_error": None,
            "updated_at": now,
        }).eq("event_id", event_id).execute()
        return True
    db.table("stripe_webhook_events").insert({
        "event_id": event_id,
        "event_type": event_type,
        "object_id": object_id,
        "processing_status": "processing",
        "updated_at": now,
    }).execute()
    return True


def _finish_webhook_event(event_id: str, error: Optional[str] = None) -> None:
    values = {
        "processing_status": "failed" if error else "processed",
        "last_error": error,
        "updated_at": datetime.now(timezone.utc).isoformat(),
        "processed_at": None if error else datetime.now(timezone.utc).isoformat(),
    }
    _subscription_db().table("stripe_webhook_events").update(values).eq("event_id", event_id).execute()


def handle_stripe_webhook(payload: bytes, sig_header: str) -> Dict[str, str]:
    if not _stripe_enabled() or not STRIPE_WEBHOOK_SECRET:
        raise HTTPException(status_code=503, detail="Stripe Webhook ist nicht konfiguriert.")
    try:
        event = _stripe_dict(stripe.Webhook.construct_event(payload, sig_header, STRIPE_WEBHOOK_SECRET))
    except Exception as exc:
        print(f"Stripe webhook verification failed ({type(exc).__name__}): {exc}")
        raise HTTPException(status_code=400, detail=f"Ungültiger Stripe-Webhook: {exc}")

    event_id = event.get("id")
    event_type = event.get("type")
    event_data = _stripe_dict(event.get("data", {}))
    data = _stripe_dict(event_data.get("object", {}))
    object_id = data.get("id")
    if not event_id or not event_type:
        raise HTTPException(status_code=400, detail="Stripe-Webhook ohne Event-ID oder Typ.")
    if not _claim_webhook_event(event_id, event_type, object_id):
        return {"status": "duplicate"}

    try:
        if event_type == "checkout.session.completed":
            checkout = _get_checkout(data.get("id"))
            metadata = _stripe_dict(data.get("metadata", {}))
            username = (checkout or {}).get("username") or metadata.get("blop_username") or metadata.get("username")
            tier = (checkout or {}).get("tier") or metadata.get("tier")
            interval = (checkout or {}).get("billing_interval") or metadata.get("interval") or "month"
            subscription_id = _stripe_id(data.get("subscription"))
            if not subscription_id and data.get("customer"):
                listed = _stripe_dict(
                    stripe.Subscription.list(customer=_stripe_id(data.get("customer")), status="all", limit=100)
                ).get("data", []) or []
                matching = [
                    _stripe_dict(value) for value in listed
                    if _stripe_dict(value).get("status") in ("active", "trialing", "past_due")
                    and (not tier or _tier_from_stripe_subscription(_stripe_dict(value)) == tier)
                ]
                matching.sort(key=lambda value: int(value.get("created") or 0), reverse=True)
                subscription_id = matching[0].get("id") if matching else None
            if not subscription_id or not username:
                raise RuntimeError("Checkout webhook mapping incomplete")
            subscription = _stripe_dict(stripe.Subscription.retrieve(subscription_id))
            subscription_metadata = _stripe_dict(subscription.get("metadata", {}))
            subscription["metadata"] = {
                **subscription_metadata,
                "blop_username": username,
                "username": username,
                "tier": tier or _tier_from_stripe_subscription(subscription),
                "interval": interval,
            }
            _handle_subscription_created_or_updated(
                subscription,
                operation_key=f"checkout:{data.get('id')}",
                event_id=event_id,
            )
            _complete_checkout(data.get("id"), subscription_id, data.get("customer"))
        elif event_type in ("customer.subscription.created", "customer.subscription.updated"):
            _handle_subscription_created_or_updated(
                data,
                grant_period_tokens=False,
                event_id=event_id,
            )
        elif event_type == "invoice.paid":
            subscription_id = _invoice_subscription_id(data)
            if not subscription_id:
                raise RuntimeError("Paid invoice has no subscription reference")
            subscription = _stripe_dict(stripe.Subscription.retrieve(subscription_id))
            _handle_subscription_created_or_updated(
                subscription,
                operation_key=f"invoice:{data.get('id')}",
                event_id=event_id,
            )
        elif event_type == "invoice.payment_failed":
            subscription_id = _invoice_subscription_id(data)
            if subscription_id:
                subscription = _stripe_dict(stripe.Subscription.retrieve(subscription_id))
                subscription["status"] = "past_due"
                _handle_subscription_created_or_updated(
                    subscription,
                    grant_period_tokens=False,
                    event_id=event_id,
                )
        elif event_type == "customer.subscription.deleted":
            _handle_subscription_deleted(data)
        _finish_webhook_event(event_id)
    except Exception as exc:
        try:
            _finish_webhook_event(event_id, str(exc)[:1000])
        except Exception as persistence_exc:
            print(f"Stripe webhook failure persistence failed: {persistence_exc}")
        print(f"Stripe webhook processing failed ({event_type}, {event_id}): {exc}")
        raise HTTPException(status_code=500, detail="Stripe-Webhook konnte nicht verarbeitet werden.")

    return {"status": "ok"}


# ---------------------------------------------------------------------------
# PayPal one-time checkout (year or month)
# ---------------------------------------------------------------------------

PAYPAL_CLIENT_ID = _env("PAYPAL_CLIENT_ID")
PAYPAL_CLIENT_SECRET = _env("PAYPAL_CLIENT_SECRET")
PAYPAL_ENV = _env("PAYPAL_ENV", "live")


def _paypal_base_url() -> str:
    return "https://api-m.sandbox.paypal.com" if PAYPAL_ENV == "sandbox" else "https://api-m.paypal.com"


def _paypal_token() -> str:
    if not PAYPAL_CLIENT_ID or not PAYPAL_CLIENT_SECRET:
        raise HTTPException(status_code=503, detail="PayPal ist nicht konfiguriert.")
    url = f"{_paypal_base_url()}/v1/oauth2/token"
    credentials = base64.b64encode(f"{PAYPAL_CLIENT_ID}:{PAYPAL_CLIENT_SECRET}".encode()).decode()
    headers = {
        "Authorization": f"Basic {credentials}",
        "Content-Type": "application/x-www-form-urlencoded",
    }
    data = {"grant_type": "client_credentials"}
    resp = paypal_requests.post(url, headers=headers, data=data, timeout=20)
    if resp.status_code != 200:
        print(f"PayPal token error: {resp.status_code} {resp.text[:200]}")
        raise HTTPException(status_code=500, detail="PayPal Authentifizierung fehlgeschlagen.")
    return resp.json()["access_token"]


def _paypal_enabled() -> bool:
    return bool(PAYPAL_CLIENT_ID and PAYPAL_CLIENT_SECRET)


def _paypal_price_for_tier(tier: str, interval: str) -> float:
    tier_info = SubscriptionManager.get_tier(tier)
    if not tier_info:
        raise HTTPException(status_code=400, detail=f"Tier '{tier}' nicht gefunden.")
    if interval == "year":
        return float(tier_info.get("price_yearly_eur", 0))
    return float(tier_info.get("price_monthly_eur", 0))


def create_paypal_order(username: str, tier: str, interval: str) -> Dict[str, Any]:
    """Create a one-time PayPal order for a subscription period."""
    if not _paypal_enabled():
        raise HTTPException(status_code=503, detail="PayPal ist nicht konfiguriert.")
    if tier not in ("pro", "premium"):
        raise HTTPException(status_code=400, detail="Nur Pro und Premium können über PayPal gebucht werden.")
    if interval not in ("month", "year"):
        raise HTTPException(status_code=400, detail="Intervall muss 'month' oder 'year' sein.")

    amount = _paypal_price_for_tier(tier, interval)
    if amount <= 0:
        raise HTTPException(status_code=400, detail="Ungültiger PayPal-Preis.")

    token = _paypal_token()
    base = _public_app_url()
    return_url = f"{base}/settings?paypal=success&tier={tier}&interval={interval}"
    cancel_url = f"{base}/pricing?paypal=canceled"

    payload = {
        "intent": "CAPTURE",
        "purchase_units": [
            {
                "reference_id": f"{username}:{tier}:{interval}",
                "description": f"Blop Study {tier.capitalize()} ({interval})",
                "amount": {
                    "currency_code": "EUR",
                    "value": f"{amount:.2f}",
                },
                "custom_id": username,
            }
        ],
        "application_context": {
            "brand_name": "Blop Study",
            "landing_page": "BILLING",
            "user_action": "PAY_NOW",
            "return_url": return_url,
            "cancel_url": cancel_url,
        },
    }

    url = f"{_paypal_base_url()}/v2/checkout/orders"
    headers = {"Authorization": f"Bearer {token}", "Content-Type": "application/json"}
    resp = paypal_requests.post(url, headers=headers, json=payload, timeout=20)
    if resp.status_code not in (200, 201):
        print(f"PayPal create order error: {resp.status_code} {resp.text[:400]}")
        raise HTTPException(status_code=500, detail="PayPal Order konnte nicht erstellt werden.")
    data = resp.json()
    approval = next((link["href"] for link in data.get("links", []) if link.get("rel") == "approve"), "")
    if not approval:
        raise HTTPException(status_code=500, detail="PayPal Approval-URL nicht gefunden.")
    return {"order_id": data["id"], "approval_url": approval}


def capture_paypal_order(order_id: str, expected_username: str) -> Dict[str, Any]:
    """Capture a PayPal order and activate the subscription."""
    if not _paypal_enabled():
        raise HTTPException(status_code=503, detail="PayPal ist nicht konfiguriert.")
    token = _paypal_token()

    # Verify the order belongs to the logged-in user before capturing.
    order_resp = paypal_requests.get(
        f"{_paypal_base_url()}/v2/checkout/orders/{order_id}",
        headers={"Authorization": f"Bearer {token}", "Content-Type": "application/json"},
        timeout=20,
    )
    if order_resp.status_code != 200:
        raise HTTPException(status_code=400, detail="PayPal Order konnte nicht verifiziert werden.")
    order_data = order_resp.json()
    purchase_units = order_data.get("purchase_units", [])
    if not purchase_units:
        raise HTTPException(status_code=400, detail="PayPal Order ohne Purchase-Unit.")
    reference_id = purchase_units[0].get("reference_id", "")
    parts = reference_id.split(":")
    if len(parts) != 3:
        raise HTTPException(status_code=400, detail="PayPal Order-Metadaten ungültig.")
    username, tier, interval = parts
    if username != expected_username:
        raise HTTPException(status_code=403, detail="PayPal Order gehört nicht zum angemeldeten Benutzer.")

    url = f"{_paypal_base_url()}/v2/checkout/orders/{order_id}/capture"
    headers = {"Authorization": f"Bearer {token}", "Content-Type": "application/json"}
    resp = paypal_requests.post(url, headers=headers, timeout=20)
    if resp.status_code not in (200, 201):
        print(f"PayPal capture error: {resp.status_code} {resp.text[:400]}")
        raise HTTPException(status_code=500, detail="PayPal Zahlung konnte nicht abgeschlossen werden.")
    data = resp.json()

    if data.get("status") != "COMPLETED":
        raise HTTPException(status_code=400, detail=f"PayPal Zahlung nicht abgeschlossen: {data.get('status')}")

    # Set subscription period.
    now = datetime.now(timezone.utc)
    months = 12 if interval == "year" else 1
    period_end = now + timedelta(days=30 * months)

    previous = SubscriptionManager.get_user_subscription(username)
    is_new_subscription = previous.get("provider_subscription_id") != order_id

    SubscriptionManager.set_user_subscription(
        username=username,
        tier=tier,
        status="active",
        provider="paypal",
        provider_subscription_id=order_id,
        current_period_start=now,
        current_period_end=period_end,
        reset_tokens=False,
    )
    # Credit tokens for the full paid period.
    credits = _subscription_credits_for_interval(tier, interval)
    if credits > 0:
        db = SubscriptionManager._get_db()
        if db:
            db.table("users").update({"tokens": credits}).eq("username", username).execute()
    if is_new_subscription:
        _notify_subscription_started(username, tier, interval, "paypal")

    return {"status": "success", "tier": tier, "interval": interval, "username": username}


# ---------------------------------------------------------------------------
# Public publishable key (for frontend)
# ---------------------------------------------------------------------------

def stripe_publishable_key() -> Optional[str]:
    return STRIPE_PUBLISHABLE_KEY or None


def paypal_client_id() -> Optional[str]:
    return PAYPAL_CLIENT_ID or None
