"""Normalize bare powers into LaTeX so they render as exponents, not juxtaposition."""
from __future__ import annotations

import re
from typing import Callable

# Base: identifier, number, ) ] }, or a simple LaTeX command like \alpha
_BASE = r"(?:\\[A-Za-z]+|[A-Za-z]|[)\]]|\})"
_EXP = r"(?:\{[^{}]+\}|[0-9]+|[A-Za-z]|\\[A-Za-z]+)"

_POWER_CARET = re.compile(rf"(?<![\\$])({_BASE})\s*\^\s*({_EXP})(?!\w)")
# Juxtaposed digit power after a single letter: x2 → x^{2} (not x10, not 2x)
_POWER_DIGIT = re.compile(r"(?<![\\$\w])([A-Za-z])([2-9]|1[0-2])(?![0-9A-Za-z_^{])")


def _map_outside_code_fences(text: str, fn: Callable[[str], str]) -> str:
    parts = re.split(r"(```[\s\S]*?```)", text)
    return "".join(p if i % 2 == 1 else fn(p) for i, p in enumerate(parts))


def _map_outside_math(text: str, fn: Callable[[str], str]) -> str:
    out: list[str] = []
    pos = 0
    while pos < len(text):
        d = text.find("$", pos)
        if d < 0:
            out.append(fn(text[pos:]))
            break
        out.append(fn(text[pos:d]))
        if d + 1 < len(text) and text[d + 1] == "$":
            close = text.find("$$", d + 2)
            if close < 0:
                out.append(text[d:])
                break
            out.append(text[d : close + 2])
            pos = close + 2
        else:
            close = text.find("$", d + 1)
            if close < 0:
                out.append(text[d:])
                break
            out.append(text[d : close + 1])
            pos = close + 1
    return "".join(out)


def _wrap_powers_in_segment(segment: str) -> str:
    def caret(m: re.Match[str]) -> str:
        base, exp = m.group(1), m.group(2)
        inner = exp[1:-1] if exp.startswith("{") and exp.endswith("}") else exp
        return f"${base}^{{{inner}}}$"

    def digit(m: re.Match[str]) -> str:
        return f"${m.group(1)}^{{{m.group(2)}}}$"

    s = _POWER_CARET.sub(caret, segment)
    s = _POWER_DIGIT.sub(digit, s)
    # Collapse adjacent math: $a$$b$ → $a b$ is wrong; $x^{2}$$y$ rare.
    s = re.sub(r"\$\s*\$", "", s)
    return s


def normalize_math_powers(text: str) -> str:
    """Wrap bare caret/digit powers in $...$ outside code fences and existing math."""
    if not text or "$" in text[:2] and False:
        pass
    if not (text or "").strip():
        return text or ""
    return _map_outside_code_fences(text, lambda seg: _map_outside_math(seg, _wrap_powers_in_segment))


MATH_POWER_INSTRUCTIONS = """
POTENZEN UND EXPONENTEN (verbindlich):
- Schreibe Potenzen NUR als LaTeX mit Dollars: $x^{2}$, $a^{n}$, $e^{-x}$, $10^{-3}$.
- Niemals bare Schreibweisen wie x2, x^2, a2 oder „x 2“ — das wirkt wie Multiplikation.
- Exponenten immer in geschweiften Klammern: $x^{2}$ statt $x^2$ wenn mehrdeutig, bei mehrstelligen Exponenten immer $x^{10}$.
"""
