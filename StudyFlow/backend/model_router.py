"""Adaptive OpenRouter model routing (no imports from ai_service)."""
from __future__ import annotations

import os
from contextvars import ContextVar
from typing import Any, Iterable, Optional

_ROUTING_TIER: ContextVar[str] = ContextVar("studyflow_routing_tier", default="free")

_DEPRECATED_MODEL_ALIASES = {
    "gemini-2.0-flash": "gemini-2.5-flash",
    "gemini-2.0-flash-lite": "gemini-2.5-flash-lite",
}

_TASK_LOAD = {
    "chat": "light",
    "task_help": "light",
    "selection_edit": "light",
    "audio": "light",
    "handwriting": "light",
    "assistant": "light",
    "summary": "medium",
    "quiz": "medium",
    "flashcards": "medium",
    "podcast": "medium",
    "marketing": "medium",
    "image": "medium",
    "plan": "heavy",
    "smart_learning": "heavy",
    "elaboration": "heavy",
    "repetition": "heavy",
    "learning_video": "heavy",
    "document_patch": "heavy",
    "artifact_refine": "medium",
    "general": "medium",
}


def set_routing_tier(tier: Optional[str]) -> None:
    _ROUTING_TIER.set((tier or "free").strip().lower() or "free")


def get_routing_tier() -> str:
    return _ROUTING_TIER.get()


def resolve_model_preference(model_preference: str) -> str:
    raw = (model_preference or "").strip()
    if not raw:
        return raw
    # OpenRouter slugs (provider/model) pass through unchanged
    if "/" in raw:
        return raw
    return _DEPRECATED_MODEL_ALIASES.get(raw, raw)


def estimate_content_size(content: Any) -> int:
    """Rough character / byte estimate for routing heuristics."""
    if content is None:
        return 0
    if isinstance(content, str):
        return len(content)
    if isinstance(content, (int, float, bool)):
        return len(str(content))
    if isinstance(content, bytes):
        return len(content)
    if isinstance(content, (list, tuple)):
        return sum(estimate_content_size(item) for item in content)
    if isinstance(content, dict):
        total = 0
        for key in ("text", "content", "data", "message", "body"):
            val = content.get(key)
            if isinstance(val, str):
                total += len(val)
        if total:
            return total
        try:
            import json

            return len(json.dumps(content, ensure_ascii=False))
        except Exception:
            return len(str(content))
    path = getattr(content, "path", None)
    if path:
        try:
            return os.path.getsize(path)
        except OSError:
            return 40_000
    return len(str(content))


def _tier_flags(tier: Optional[str]) -> tuple[bool, bool]:
    t = (tier or get_routing_tier() or "free").strip().lower()
    paid = t in ("pro", "premium", "custom")
    premium = t in ("premium", "custom")
    return paid, premium


def route_model(
    task: str,
    model_preference: Optional[str] = None,
    tier: Optional[str] = None,
    content_size: int = 0,
) -> str:
    """
    Explicit user/settings model wins (including OpenRouter slugs).
    Auto mode balances task complexity, material size, and subscription tier.
    """
    pref = (model_preference or "").strip()
    if pref:
        return resolve_model_preference(pref)

    env_override = (os.environ.get(f"TASK_MODEL_{(task or 'general').upper()}") or "").strip()
    if env_override:
        return resolve_model_preference(env_override)

    paid, premium = _tier_flags(tier)
    size = max(0, int(content_size or 0))
    load = _TASK_LOAD.get(task or "general", "medium")

    if load == "light":
        if size < 2500 and not paid:
            return "gemini-2.5-flash-lite"
        return "gemini-3.7-flash" if paid else "gemini-2.5-flash"

    if load == "heavy":
        if size > 90_000 and premium:
            return "claude-sonnet-5.5"
        if size > 50_000 and paid:
            return "gemini-3.7-flash"
        if size > 20_000:
            return "gemini-3.7-flash" if paid else "gemini-2.5-flash"
        return "gemini-3.7-flash" if paid else "gemini-2.5-flash"

    # medium
    if size > 70_000 and paid:
        return "gemini-3.7-flash"
    if size < 2000 and not paid:
        return "gemini-2.5-flash-lite"
    return "gemini-3.7-flash" if paid else "gemini-2.5-flash"