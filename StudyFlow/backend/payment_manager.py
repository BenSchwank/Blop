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
    return bool(STRIPE_SECRET_KEY and STRIPE_PUBLISHABLE_KEY)


def _stripe_dict(value: Any) -> Dict[str, Any]:
    if isinstance(value, dict):
        return value
    if hasattr(value, "to_dict_recursive"):
        return value.to_dict_recursive()
    try:
        return dict(value)
    except (TypeError, ValueError):
        return {}


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
        # Create a customer and link it to the username via metadata.
        customer = stripe.Customer.create(
            email=email,
            metadata={"username": username},
        )

        session = stripe.checkout.Session.create(
            customer=customer.id,
            client_reference_id=username,
            mode="subscription",
            line_items=[{"price": price_id, "quantity": 1}],
            success_url=f"{success}?subscription=success&checkout_session_id={{CHECKOUT_SESSION_ID}}",
            cancel_url=cancel,
            subscription_data={
                "metadata": {"username": username, "tier": tier, "interval": interval},
            },
            metadata={"username": username, "tier": tier, "interval": interval},
            allow_promotion_codes=True,
            billing_address_collection="auto",
        )
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
        session = _stripe_dict(stripe.checkout.Session.retrieve(checkout_session_id))
        metadata = _stripe_dict(session.get("metadata", {}))
        subscription_id = session.get("subscription")
        if not subscription_id:
            raise HTTPException(status_code=409, detail="Stripe-Abo wurde noch nicht erstellt.")
        subscription = _stripe_dict(stripe.Subscription.retrieve(subscription_id))
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
        customer = _stripe_dict(stripe.Customer.retrieve(session.get("customer"))) if session.get("customer") else {}
        customer_metadata = _stripe_dict(customer.get("metadata", {}))
        owner_candidates = {
            str(value).strip()
            for value in (
                metadata.get("username"),
                subscription_metadata.get("username"),
                customer_metadata.get("username"),
                session.get("client_reference_id"),
            )
            if value
        }
        if username not in owner_candidates:
            print(f"Stripe checkout owner mismatch: session={checkout_session_id}, authenticated={username}, candidates={sorted(owner_candidates)}")
            raise HTTPException(status_code=403, detail="Checkout gehört nicht zum angemeldeten Benutzer.")
        _handle_subscription_created_or_updated(subscription)
        tier = metadata.get("tier") or subscription_metadata.get("tier") or "free"
        return {"status": "success", "tier": tier}
    except HTTPException:
        raise
    except Exception as exc:
        print(f"Stripe reconcile checkout failed ({type(exc).__name__}): {exc}")
        message = getattr(exc, "user_message", None) or str(exc)
        raise HTTPException(status_code=502, detail=f"Stripe-Zahlung konnte nicht bestätigt werden: {message}")


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

def _credit_tokens(username: str, tier: str, interval: str):
    """Grant tokens based on the subscription interval."""
    db = SubscriptionManager._get_db()
    if not db:
        return
    try:
        credits = _subscription_credits_for_interval(tier, interval)
        if credits <= 0:
            return
        # Set tokens to the credit amount for the period (resets each billing cycle).
        db.table("users").update({"tokens": credits}).eq("username", username).execute()
        print(f"PaymentManager: credited {credits} tokens to {username} for {tier}/{interval}")
    except Exception as exc:
        print(f"PaymentManager: credit_tokens failed: {exc}")


def _handle_subscription_created_or_updated(subscription: Dict[str, Any]):
    subscription = _stripe_dict(subscription)
    metadata = _stripe_dict(subscription.get("metadata", {}))
    username = metadata.get("username")
    tier = metadata.get("tier")
    interval = metadata.get("interval", "month")
    if not username or not tier:
        print(f"PaymentManager: stripe subscription missing metadata: {subscription.get('id')}")
        return

    # Determine period end from the subscription object.
    current_period_end_ts = subscription.get("current_period_end")
    current_period_start_ts = subscription.get("current_period_start")
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
        provider_customer_id=subscription.get("customer"),
        provider_subscription_id=subscription.get("id"),
        current_period_start=period_start,
        current_period_end=period_end,
        cancel_at_period_end=subscription.get("cancel_at_period_end", False),
        reset_tokens=False,
    )
    _credit_tokens(username, tier, interval)
    if is_new_subscription:
        _notify_subscription_started(username, tier, interval, "stripe")


def _handle_subscription_deleted(subscription: Dict[str, Any]):
    subscription = _stripe_dict(subscription)
    metadata = _stripe_dict(subscription.get("metadata", {}))
    username = metadata.get("username")
    if not username:
        return
    # Downgrade to default tier at period end. For simplicity we downgrade immediately.
    default_tier = SubscriptionManager._get_default_tier()
    SubscriptionManager.set_user_subscription(
        username=username,
        tier=default_tier,
        status="canceled",
        provider="stripe",
        provider_customer_id=subscription.get("customer"),
        provider_subscription_id=subscription.get("id"),
        reset_tokens=True,
    )


def handle_stripe_webhook(payload: bytes, sig_header: str) -> Dict[str, str]:
    if not _stripe_enabled() or not STRIPE_WEBHOOK_SECRET:
        raise HTTPException(status_code=503, detail="Stripe Webhook ist nicht konfiguriert.")
    try:
        event = _stripe_dict(stripe.Webhook.construct_event(payload, sig_header, STRIPE_WEBHOOK_SECRET))
    except Exception as exc:
        print(f"Stripe webhook verification failed ({type(exc).__name__}): {exc}")
        raise HTTPException(status_code=400, detail=f"Ungültiger Stripe-Webhook: {exc}")

    event_type = event.get("type")
    event_data = _stripe_dict(event.get("data", {}))
    data = _stripe_dict(event_data.get("object", {}))
    print(f"Stripe webhook received: {event_type}")

    if event_type == "checkout.session.completed":
        # Expand the subscription to get full metadata and period dates.
        subscription_id = data.get("subscription")
        if subscription_id:
            sub = _stripe_dict(stripe.Subscription.retrieve(subscription_id))
            _handle_subscription_created_or_updated(sub)
    elif event_type == "invoice.paid":
        # Recurring payment succeeded: credit tokens again.
        subscription_id = data.get("subscription")
        if subscription_id:
            sub = _stripe_dict(stripe.Subscription.retrieve(subscription_id))
            _handle_subscription_created_or_updated(sub)
    elif event_type == "customer.subscription.deleted":
        _handle_subscription_deleted(data)
    elif event_type == "customer.subscription.updated":
        _handle_subscription_created_or_updated(data)

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
