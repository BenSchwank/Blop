"""Cached OpenRouter model catalog and study-token pricing helpers."""
from __future__ import annotations

import os
import time
from typing import Any, Dict, List, Optional

import requests

import openrouter_genai as genai

MODELS_URL = "https://openrouter.ai/api/v1/models"
_CACHE_TTL_SEC = 3600
_cache: Dict[str, Any] = {"at": 0.0, "models": []}

# Calibrated so google/gemini-2.5-flash ≈ in 1.4 / out 4.0 per 1k study tokens
_STUDY_BASELINE = {"in": 1.4, "out": 4.0}
_CALIBRATION_SLUG = "google/gemini-2.5-flash"


def _api_key() -> str:
    return (os.environ.get("OPENROUTER_API_KEY") or "").strip()


def _fetch_models_raw() -> List[Dict[str, Any]]:
    key = _api_key()
    if not key:
        return []
    headers = {
        "Authorization": f"Bearer {key}",
        "HTTP-Referer": os.environ.get("OPENROUTER_HTTP_REFERER", "https://www.blop-study.com"),
        "X-Title": os.environ.get("OPENROUTER_APP_TITLE", "Blop Study"),
    }
    try:
        resp = requests.get(MODELS_URL, headers=headers, timeout=30)
        if not resp.ok:
            print(f"OpenRouter models list failed: {resp.status_code}")
            return []
        payload = resp.json()
        data = payload.get("data") if isinstance(payload, dict) else None
        return data if isinstance(data, list) else []
    except Exception as exc:
        print(f"OpenRouter models list error: {exc!r}")
        return []


def _get_models_cached() -> List[Dict[str, Any]]:
    now = time.time()
    if _cache["models"] and now - float(_cache["at"]) < _CACHE_TTL_SEC:
        return _cache["models"]
    models = _fetch_models_raw()
    if models:
        _cache["models"] = models
        _cache["at"] = now
    return _cache["models"]


def _usd_per_token(value: Any) -> float:
    try:
        return float(value or 0)
    except (TypeError, ValueError):
        return 0.0


def _scale_study_rates(prompt_usd_per_token: float, completion_usd_per_token: float) -> Dict[str, float]:
    """Convert OpenRouter $/token into study units per 1k tokens (flash-calibrated)."""
    if prompt_usd_per_token <= 0 and completion_usd_per_token <= 0:
        return dict(_STUDY_BASELINE)

    cal_in = 0.0
    cal_out = 0.0
    for entry in _get_models_cached():
        if str(entry.get("id") or "") != _CALIBRATION_SLUG:
            continue
        pricing = entry.get("pricing") if isinstance(entry.get("pricing"), dict) else {}
        cal_in = _usd_per_token(pricing.get("prompt"))
        cal_out = _usd_per_token(pricing.get("completion"))
        break

    def per_1k(usd_per_token: float, cal_usd: float, baseline: float) -> float:
        if usd_per_token <= 0:
            return baseline
        if cal_usd <= 0:
            return round(usd_per_token * 1e6 * 1000, 2)
        return round(baseline * (usd_per_token / cal_usd), 2)

    return {
        "in": max(0.1, per_1k(prompt_usd_per_token, cal_in, _STUDY_BASELINE["in"])),
        "out": max(0.1, per_1k(completion_usd_per_token, cal_out, _STUDY_BASELINE["out"])),
    }


def _entry_by_slug(slug: str) -> Optional[Dict[str, Any]]:
    slug = (slug or "").strip()
    if not slug:
        return None
    for entry in _get_models_cached():
        if str(entry.get("id") or "") == slug:
            return entry
    return None


def _internal_to_slug(model_name: str) -> str:
    return genai.map_model((model_name or "").strip())


def get_catalog_for_ui() -> List[Dict[str, Any]]:
    """Models for settings UI: id, display name, study-token rates per 1k."""
    out: List[Dict[str, Any]] = []
    for entry in _get_models_cached():
        mid = str(entry.get("id") or "").strip()
        if not mid:
            continue
        pricing = entry.get("pricing") if isinstance(entry.get("pricing"), dict) else {}
        rates = _scale_study_rates(_usd_per_token(pricing.get("prompt")), _usd_per_token(pricing.get("completion")))
        name = str(entry.get("name") or mid)
        out.append(
            {
                "id": mid,
                "name": name,
                "prompt_per_1k": rates["in"],
                "completion_per_1k": rates["out"],
            }
        )
    out.sort(key=lambda row: (row["prompt_per_1k"], row["name"]))
    return out


def rates_for_internal_model(
    model_name: str,
    known_rates: Optional[Dict[str, Dict[str, float]]] = None,
) -> Dict[str, float]:
    """Resolve study-token rates: curated map first, then OpenRouter catalog."""
    model_name = (model_name or "").strip()
    catalog = known_rates if known_rates is not None else {}

    if model_name in catalog:
        return dict(catalog[model_name])
    if "/" in model_name:
        entry = _entry_by_slug(model_name)
        if entry:
            pricing = entry.get("pricing") if isinstance(entry.get("pricing"), dict) else {}
            return _scale_study_rates(
                _usd_per_token(pricing.get("prompt")),
                _usd_per_token(pricing.get("completion")),
            )
    for key, rates in catalog.items():
        if model_name.startswith(key):
            return dict(rates)

    slug = _internal_to_slug(model_name)
    entry = _entry_by_slug(slug)
    if entry:
        pricing = entry.get("pricing") if isinstance(entry.get("pricing"), dict) else {}
        return _scale_study_rates(
            _usd_per_token(pricing.get("prompt")),
            _usd_per_token(pricing.get("completion")),
        )

    return {"in": 1.0, "out": 3.0}