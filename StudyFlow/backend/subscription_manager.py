import json
import os
from datetime import datetime, timedelta, timezone
from typing import Any, Dict, List, Optional, Set

from fastapi import HTTPException

from auth_manager import AuthManager


class SubscriptionManager:
    """Manages subscription tiers, feature matrix and per-user subscriptions in Supabase."""

    # Feature keys must match the keys used in main.py (FEATURE_MULTIPLIER / deduct_tokens_by_usage).
    KNOWN_FEATURE_KEYS: Set[str] = {
        "plan",
        "quiz",
        "flashcards",
        "summary",
        "audio_transcribe",
        "image_to_text",
        "podcast",
        "learning_video",
        "elaboration",
        "elaboration_refine",
        "repetition",
        "task_help",
        "chat",
    }

    DEFAULT_TIERS: List[Dict[str, Any]] = [
        {
            "name": "free",
            "display_name": "Free",
            "price_monthly_eur": 0.0,
            "price_yearly_eur": 0.0,
            "tokens_monthly": 50,
            "is_admin_only": False,
            "is_default": True,
        },
        {
            "name": "pro",
            "display_name": "Pro",
            "price_monthly_eur": 3.0,
            "price_yearly_eur": 24.0,
            "tokens_monthly": 300,
            "is_admin_only": False,
            "is_default": False,
        },
        {
            "name": "premium",
            "display_name": "Premium",
            "price_monthly_eur": 5.0,
            "price_yearly_eur": 48.0,
            "tokens_monthly": 1000,
            "is_admin_only": False,
            "is_default": False,
        },
        {
            "name": "custom",
            "display_name": "Custom",
            "price_monthly_eur": 0.0,
            "price_yearly_eur": 0.0,
            "tokens_monthly": 0,
            "is_admin_only": True,
            "is_default": False,
        },
    ]

    DEFAULT_FEATURE_MATRIX: Dict[str, Dict[str, bool]] = {
        "free": {
            "plan": True,
            "quiz": True,
            "flashcards": True,
            "summary": True,
            "audio_transcribe": False,
            "image_to_text": False,
            "podcast": False,
            "learning_video": False,
            "elaboration": False,
            "elaboration_refine": False,
            "repetition": False,
            "task_help": False,
            "chat": False,
        },
        "pro": {
            "plan": True,
            "quiz": True,
            "flashcards": True,
            "summary": True,
            "audio_transcribe": True,
            "image_to_text": True,
            "podcast": True,
            "learning_video": False,
            "elaboration": True,
            "elaboration_refine": True,
            "repetition": True,
            "task_help": True,
            "chat": True,
        },
        "premium": {
            "plan": True,
            "quiz": True,
            "flashcards": True,
            "summary": True,
            "audio_transcribe": True,
            "image_to_text": True,
            "podcast": True,
            "learning_video": True,
            "elaboration": True,
            "elaboration_refine": True,
            "repetition": True,
            "task_help": True,
            "chat": True,
        },
    }

    @staticmethod
    def _get_db():
        from data_manager import DataManager
        return DataManager._init_supabase()

    @staticmethod
    def _now() -> datetime:
        return datetime.now(timezone.utc)

    @staticmethod
    def _default_period_end(months: int = 1) -> datetime:
        return SubscriptionManager._now() + timedelta(days=30 * months)

    @staticmethod
    def _get_default_tier() -> str:
        """Returns the name of the default tier. Falls back to 'free'."""
        db = SubscriptionManager._get_db()
        if not db:
            return "free"
        try:
            res = db.table("subscription_tiers").select("name").eq("is_default", True).execute()
            if res.data:
                return res.data[0]["name"]
        except Exception as exc:
            print(f"SubscriptionManager: get default tier failed: {exc}")
        return "free"

    # --- Tier / Feature CRUD ---

    @staticmethod
    def get_tiers() -> List[Dict[str, Any]]:
        """Returns all tiers sorted by price (free first, custom last)."""
        db = SubscriptionManager._get_db()
        if not db:
            return list(SubscriptionManager.DEFAULT_TIERS)
        try:
            res = db.table("subscription_tiers").select("*").execute()
            if not res.data:
                return []
            tiers = sorted(
                res.data,
                key=lambda t: (not t.get("is_default", False), t.get("price_monthly_eur", 0), t.get("name")),
            )
            # Custom tier always at the end
            tiers = [t for t in tiers if t.get("name") != "custom"] + [t for t in tiers if t.get("name") == "custom"]
            return tiers
        except Exception as exc:
            print(f"SubscriptionManager: get_tiers failed: {exc}")
            return list(SubscriptionManager.DEFAULT_TIERS)

    @staticmethod
    def get_tier(name: str) -> Optional[Dict[str, Any]]:
        db = SubscriptionManager._get_db()
        if not db:
            return None
        try:
            res = db.table("subscription_tiers").select("*").eq("name", name).execute()
            return res.data[0] if res.data else None
        except Exception as exc:
            print(f"SubscriptionManager: get_tier failed: {exc}")
            return None

    @staticmethod
    def get_features(tier_name: str) -> Dict[str, bool]:
        """Returns feature map for a tier. Defaults to all False if no row exists."""
        db = SubscriptionManager._get_db()
        if not db:
            return SubscriptionManager.DEFAULT_FEATURE_MATRIX.get(tier_name, {})
        try:
            res = (
                db.table("subscription_features")
                .select("feature_key, allowed")
                .eq("tier_name", tier_name)
                .execute()
            )
            return {row["feature_key"]: row["allowed"] for row in res.data} if res.data else {}
        except Exception as exc:
            print(f"SubscriptionManager: get_features failed: {exc}")
            return SubscriptionManager.DEFAULT_FEATURE_MATRIX.get(tier_name, {})

    @staticmethod
    def get_all_features() -> Dict[str, Dict[str, bool]]:
        """Returns {tier_name: {feature_key: allowed}} for all tiers."""
        db = SubscriptionManager._get_db()
        if not db:
            return dict(SubscriptionManager.DEFAULT_FEATURE_MATRIX)
        try:
            res = db.table("subscription_features").select("tier_name, feature_key, allowed").execute()
            out: Dict[str, Dict[str, bool]] = {}
            for row in res.data:
                out.setdefault(row["tier_name"], {})[row["feature_key"]] = row["allowed"]
            return out
        except Exception as exc:
            print(f"SubscriptionManager: get_all_features failed: {exc}")
            return dict(SubscriptionManager.DEFAULT_FEATURE_MATRIX)

    @staticmethod
    def upsert_tier(
        name: str,
        display_name: str,
        price_monthly_eur: float,
        price_yearly_eur: float,
        tokens_monthly: int,
        is_admin_only: bool = False,
        is_default: bool = False,
    ) -> Dict[str, Any]:
        """Admin: create or update a tier."""
        db = SubscriptionManager._get_db()
        if not db:
            raise HTTPException(status_code=500, detail="Datenbank nicht erreichbar")
        try:
            # If setting this tier as default, unset any existing default first.
            if is_default:
                db.table("subscription_tiers").update({"is_default": False}).neq("name", name).execute()
            data = {
                "name": name,
                "display_name": display_name,
                "price_monthly_eur": price_monthly_eur,
                "price_yearly_eur": price_yearly_eur,
                "tokens_monthly": tokens_monthly,
                "is_admin_only": is_admin_only,
                "is_default": is_default,
                "updated_at": SubscriptionManager._now().isoformat(),
            }
            db.table("subscription_tiers").upsert(data).execute()
            return {"status": "success", "tier": name}
        except Exception as exc:
            print(f"SubscriptionManager: upsert_tier failed: {exc}")
            raise HTTPException(status_code=500, detail=f"Tier konnte nicht gespeichert werden: {exc}")

    @staticmethod
    def delete_tier(name: str) -> Dict[str, Any]:
        """Admin: delete a tier. Cannot delete built-in tiers (free, pro, premium, custom)."""
        if name in ("free", "pro", "premium", "custom"):
            raise HTTPException(status_code=400, detail="Standard-Tiers können nicht gelöscht werden.")
        db = SubscriptionManager._get_db()
        if not db:
            raise HTTPException(status_code=500, detail="Datenbank nicht erreichbar")
        try:
            # Move users on this tier to default tier first.
            default = SubscriptionManager._get_default_tier()
            db.table("subscriptions").update({"tier": default}).eq("tier", name).execute()
            db.table("users").update({"subscription_tier": default}).eq("subscription_tier", name).execute()
            db.table("subscription_tiers").delete().eq("name", name).execute()
            return {"status": "success", "deleted": name}
        except Exception as exc:
            print(f"SubscriptionManager: delete_tier failed: {exc}")
            raise HTTPException(status_code=500, detail=f"Tier konnte nicht gelöscht werden: {exc}")

    @staticmethod
    def set_feature(tier_name: str, feature_key: str, allowed: bool) -> Dict[str, Any]:
        """Admin: allow or disallow a feature for a tier."""
        if feature_key not in SubscriptionManager.KNOWN_FEATURE_KEYS:
            raise HTTPException(status_code=400, detail=f"Unbekannter Feature-Key: {feature_key}")
        db = SubscriptionManager._get_db()
        if not db:
            raise HTTPException(status_code=500, detail="Datenbank nicht erreichbar")
        try:
            db.table("subscription_features").upsert({
                "tier_name": tier_name,
                "feature_key": feature_key,
                "allowed": allowed,
            }).execute()
            return {"status": "success", "tier": tier_name, "feature": feature_key, "allowed": allowed}
        except Exception as exc:
            print(f"SubscriptionManager: set_feature failed: {exc}")
            raise HTTPException(status_code=500, detail=f"Feature konnte nicht gesetzt werden: {exc}")

    # --- User subscription ---

    @staticmethod
    def get_user_subscription(username: str) -> Dict[str, Any]:
        """Returns subscription row + tier + features for a user."""
        db = SubscriptionManager._get_db()
        if not db:
            return {"tier": "free", "status": "active", "features": SubscriptionManager.DEFAULT_FEATURE_MATRIX.get("free", {})}
        try:
            sub_res = db.table("subscriptions").select("*").eq("username", username).execute()
            if not sub_res.data:
                # No subscription row yet: create default one.
                default = SubscriptionManager._get_default_tier()
                SubscriptionManager._ensure_subscription_row(username, default)
                sub_res = db.table("subscriptions").select("*").eq("username", username).execute()
            sub = sub_res.data[0]
            tier_name = sub.get("tier", "free")
            features = SubscriptionManager.get_features(tier_name)
            return {**sub, "features": features}
        except Exception as exc:
            print(f"SubscriptionManager: get_user_subscription failed: {exc}")
            return {"tier": "free", "status": "active", "features": SubscriptionManager.get_features("free")}

    @staticmethod
    def _ensure_subscription_row(username: str, tier: str):
        db = SubscriptionManager._get_db()
        if not db:
            return
        try:
            db.table("subscriptions").upsert({
                "username": username,
                "tier": tier,
                "status": "active",
                "provider": "none",
                "current_period_start": SubscriptionManager._now().isoformat(),
                "current_period_end": SubscriptionManager._default_period_end().isoformat(),
                "cancel_at_period_end": False,
                "updated_at": SubscriptionManager._now().isoformat(),
            }).execute()
        except Exception as exc:
            print(f"SubscriptionManager: ensure_subscription_row failed: {exc}")

    @staticmethod
    def set_user_subscription(
        username: str,
        tier: str,
        status: str = "active",
        provider: str = "none",
        provider_customer_id: Optional[str] = None,
        provider_subscription_id: Optional[str] = None,
        current_period_start: Optional[datetime] = None,
        current_period_end: Optional[datetime] = None,
        cancel_at_period_end: bool = False,
        reset_tokens: bool = True,
    ) -> Dict[str, Any]:
        """Admin or payment-webhook: set a user's subscription tier and period."""
        db = SubscriptionManager._get_db()
        if not db:
            raise HTTPException(status_code=500, detail="Datenbank nicht erreichbar")
        try:
            tier_info = SubscriptionManager.get_tier(tier)
            if not tier_info:
                raise HTTPException(status_code=400, detail=f"Tier '{tier}' existiert nicht.")
            if tier_info.get("is_admin_only") and provider != "admin" and not provider.startswith("manual"):
                raise HTTPException(status_code=403, detail=f"Tier '{tier}' kann nur vom Admin vergeben werden.")

            now = SubscriptionManager._now()
            period_start = current_period_start or now
            period_end = current_period_end or SubscriptionManager._default_period_end()

            db.table("subscriptions").upsert({
                "username": username,
                "tier": tier,
                "status": status,
                "provider": provider,
                "provider_customer_id": provider_customer_id,
                "provider_subscription_id": provider_subscription_id,
                "current_period_start": period_start.isoformat(),
                "current_period_end": period_end.isoformat(),
                "cancel_at_period_end": cancel_at_period_end,
                "updated_at": now.isoformat(),
            }).execute()

            # Keep users.subscription_tier in sync for legacy reads.
            db.table("users").update({"subscription_tier": tier}).eq("username", username).execute()

            # Optionally grant tokens based on the new tier.
            if reset_tokens and tier_info.get("tokens_monthly", 0) > 0:
                tokens = int(tier_info["tokens_monthly"])
                db.table("users").update({"tokens": tokens}).eq("username", username).execute()

            return {"status": "success", "username": username, "tier": tier}
        except HTTPException:
            raise
        except Exception as exc:
            print(f"SubscriptionManager: set_user_subscription failed: {exc}")
            raise HTTPException(status_code=500, detail=f"Abo konnte nicht gesetzt werden: {exc}")

    # --- Feature checks ---

    @staticmethod
    def feature_allowed(username: str, feature_key: str) -> bool:
        """Returns True if the user is allowed to use a feature."""
        if feature_key not in SubscriptionManager.KNOWN_FEATURE_KEYS:
            # Unknown features default to allowed (safer than breaking existing routes).
            return True
        sub = SubscriptionManager.get_user_subscription(username)
        if sub.get("status") != "active":
            return False
        features = sub.get("features", {})
        return bool(features.get(feature_key, False))

    @staticmethod
    def ensure_feature(username: str, feature_key: str):
        """Raises HTTPException 403 if feature is not allowed."""
        if not SubscriptionManager.feature_allowed(username, feature_key):
            tier = SubscriptionManager.get_user_subscription(username).get("tier", "free")
            raise HTTPException(
                status_code=403,
                detail=f"Diese Funktion ist im {tier.capitalize()}-Abo nicht enthalten. Bitte upgraden.",
            )

    @staticmethod
    def get_effective_tier(username: str) -> str:
        return SubscriptionManager.get_user_subscription(username).get("tier", "free")

    @staticmethod
    def get_monthly_tokens(username: str) -> int:
        sub = SubscriptionManager.get_user_subscription(username)
        tier_name = sub.get("tier", "free")
        tier_info = SubscriptionManager.get_tier(tier_name)
        return int(tier_info.get("tokens_monthly", 0)) if tier_info else 0

    # --- Seed ---

    @staticmethod
    def seed_defaults():
        """Idempotently seed default tiers and feature matrix."""
        db = SubscriptionManager._get_db()
        if not db:
            print("SubscriptionManager: seed_defaults skipped, no DB")
            return
        try:
            for tier in SubscriptionManager.DEFAULT_TIERS:
                db.table("subscription_tiers").upsert(tier).execute()
            for tier_name, features in SubscriptionManager.DEFAULT_FEATURE_MATRIX.items():
                for feature_key, allowed in features.items():
                    db.table("subscription_features").upsert({
                        "tier_name": tier_name,
                        "feature_key": feature_key,
                        "allowed": allowed,
                    }).execute()
            # Ensure every user has a subscription row.
            users_res = db.table("users").select("username").execute()
            default = SubscriptionManager._get_default_tier()
            for row in users_res.data:
                SubscriptionManager._ensure_subscription_row(row["username"], default)
            print("SubscriptionManager: defaults seeded")
        except Exception as exc:
            print(f"SubscriptionManager: seed_defaults failed: {exc}")
