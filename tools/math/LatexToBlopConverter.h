#pragma once

#include <QString>

/// Converts LaTeX math notation (from OCR models) to the Blop graph-entry
/// syntax that MathExpressionParser expects.
///
/// Supported conversions:
///   \frac{a}{b}   →  (a)/(b)
///   \sin, \cos …  →  sin, cos …
///   x^{2}         →  x^2
///   \sqrt{x}      →  sqrt(x)
///   \pi           →  pi
///   \cdot          →  *
///   2x (implicit) →  2*x
///   \left(, \right) → (, )
class LatexToBlopConverter {
public:
    /// Main conversion entry point.
    /// Returns an expression in Blop-compatible syntax, or an empty string
    /// if the input was empty or un-processable.
    static QString convert(const QString &latex);

private:
    /// Strip leading/trailing LaTeX wrappers like $ … $, \( … \), etc.
    static QString stripMathDelimiters(const QString &s);

    /// Replace LaTeX commands with Blop equivalents.
    static QString replaceCommands(const QString &s);

    /// Insert explicit multiplication signs where implicit multiplication
    /// is used (e.g. 2x → 2*x, )(  → )*( ).
    static QString insertExplicitMultiplication(const QString &s);
};
