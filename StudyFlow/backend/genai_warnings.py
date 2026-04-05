"""Suppress noisy FutureWarnings from Google SDKs (Render log alerts, not app errors)."""
import warnings


def suppress_known_google_warnings() -> None:
    warnings.filterwarnings(
        "ignore",
        category=FutureWarning,
        module=r"google\.api_core\._python_version_support",
    )
    # google.generativeai emits a multiline FutureWarning; module field is often empty — use (?s).
    warnings.filterwarnings(
        "ignore",
        category=FutureWarning,
        message=r"(?s).*generativeai.*package has ended.*",
    )
