#include "LatexToBlopConverter.h"

#include <QRegularExpression>

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------

QString LatexToBlopConverter::convert(const QString &latex)
{
    if (latex.trimmed().isEmpty())
        return {};

    QString s = stripMathDelimiters(latex);
    s = replaceCommands(s);
    s = insertExplicitMultiplication(s);

    // Final cleanup
    s = s.simplified();                       // collapse whitespace
    s.replace(QLatin1String(" "), QString());  // remove all spaces

    return s;
}

// ---------------------------------------------------------------------------
// stripMathDelimiters
// ---------------------------------------------------------------------------

QString LatexToBlopConverter::stripMathDelimiters(const QString &s)
{
    QString t = s.trimmed();

    // $...$  or $$...$$
    while (t.startsWith(QLatin1Char('$')))
        t = t.mid(1);
    while (t.endsWith(QLatin1Char('$')))
        t.chop(1);

    // \( ... \)   or   \[ ... \]
    for (const auto &pair : {
             std::pair{QStringLiteral("\\("), QStringLiteral("\\)")},
             std::pair{QStringLiteral("\\["), QStringLiteral("\\]")},
         }) {
        if (t.startsWith(pair.first))
            t = t.mid(pair.first.size());
        if (t.endsWith(pair.second))
            t.chop(pair.second.size());
    }

    return t.trimmed();
}

// ---------------------------------------------------------------------------
// replaceCommands
// ---------------------------------------------------------------------------

QString LatexToBlopConverter::replaceCommands(const QString &s)
{
    QString t = s;

    // --- \frac{a}{b}  →  (a)/(b) ---
    // We handle nested braces iteratively.
    {
        static const QRegularExpression reFrac(
            QStringLiteral(R"(\\frac\s*)"));
        int search = 0;
        while (true) {
            const auto m = reFrac.match(t, search);
            if (!m.hasMatch())
                break;
            const int fracStart = m.capturedStart();
            int pos = m.capturedEnd(); // should point to first '{'

            auto extractBraceGroup = [&](int start) -> QPair<QString, int> {
                if (start >= t.size() || t.at(start) != QLatin1Char('{'))
                    return {QString(), start};
                int depth = 0;
                int i = start;
                while (i < t.size()) {
                    if (t.at(i) == QLatin1Char('{'))
                        ++depth;
                    else if (t.at(i) == QLatin1Char('}')) {
                        --depth;
                        if (depth == 0)
                            return {t.mid(start + 1, i - start - 1), i + 1};
                    }
                    ++i;
                }
                return {t.mid(start + 1), t.size()}; // unmatched
            };

            const auto [num, afterNum] = extractBraceGroup(pos);
            const auto [den, afterDen] = extractBraceGroup(afterNum);
            QString replacement = QStringLiteral("(%1)/(%2)").arg(num, den);
            t.replace(fracStart, afterDen - fracStart, replacement);
            search = fracStart + replacement.size();
        }
    }

    // --- \sqrt{...} → sqrt(...) ---
    // Also handle \sqrt[n]{...} → (...)^(1/(n))   (nth root)
    {
        static const QRegularExpression reSqrt(
            QStringLiteral(R"(\\sqrt\s*)"));
        int search = 0;
        while (true) {
            const auto m = reSqrt.match(t, search);
            if (!m.hasMatch())
                break;
            const int start = m.capturedStart();
            int pos = m.capturedEnd();

            QString nthRoot;
            // optional [n]
            if (pos < t.size() && t.at(pos) == QLatin1Char('[')) {
                int close = t.indexOf(QLatin1Char(']'), pos);
                if (close > pos) {
                    nthRoot = t.mid(pos + 1, close - pos - 1);
                    pos = close + 1;
                }
            }
            // {body}
            if (pos < t.size() && t.at(pos) == QLatin1Char('{')) {
                int depth = 0, i = pos;
                while (i < t.size()) {
                    if (t.at(i) == QLatin1Char('{'))  ++depth;
                    else if (t.at(i) == QLatin1Char('}')) { --depth; if (depth == 0) break; }
                    ++i;
                }
                const QString body = t.mid(pos + 1, i - pos - 1);
                QString replacement;
                if (nthRoot.isEmpty())
                    replacement = QStringLiteral("sqrt(%1)").arg(body);
                else
                    replacement = QStringLiteral("(%1)^(1/(%2))").arg(body, nthRoot);
                t.replace(start, i + 1 - start, replacement);
                search = start + replacement.size();
            } else {
                search = pos;
            }
        }
    }

    // --- Simple command replacements (order matters!) ---
    static const std::pair<QLatin1String, QLatin1String> simpleReplacements[] = {
        // Braces / sizing
        {QLatin1String("\\left("),  QLatin1String("(")},
        {QLatin1String("\\right)"), QLatin1String(")")},
        {QLatin1String("\\left["),  QLatin1String("(")},
        {QLatin1String("\\right]"), QLatin1String(")")},
        {QLatin1String("\\left|"),  QLatin1String("abs(")},
        {QLatin1String("\\right|"), QLatin1String(")")},
        {QLatin1String("\\{"),      QLatin1String("(")},
        {QLatin1String("\\}"),      QLatin1String(")")},

        // Operators
        {QLatin1String("\\cdot"),   QLatin1String("*")},
        {QLatin1String("\\times"), QLatin1String("*")},
        {QLatin1String("\\div"),   QLatin1String("/")},
        {QLatin1String("\\pm"),    QLatin1String("+")},  // simplify ±

        // Trig & hyperbolic
        {QLatin1String("\\sin"),   QLatin1String("sin")},
        {QLatin1String("\\cos"),   QLatin1String("cos")},
        {QLatin1String("\\tan"),   QLatin1String("tan")},
        {QLatin1String("\\cot"),   QLatin1String("1/tan")},  // approx
        {QLatin1String("\\sec"),   QLatin1String("1/cos")},
        {QLatin1String("\\csc"),   QLatin1String("1/sin")},
        {QLatin1String("\\arcsin"), QLatin1String("asin")},
        {QLatin1String("\\arccos"), QLatin1String("acos")},
        {QLatin1String("\\arctan"), QLatin1String("atan")},
        {QLatin1String("\\sinh"),  QLatin1String("sinh")},
        {QLatin1String("\\cosh"),  QLatin1String("cosh")},
        {QLatin1String("\\tanh"),  QLatin1String("tanh")},

        // Log / exp
        {QLatin1String("\\ln"),    QLatin1String("log")},  // Blop uses log = natural log
        {QLatin1String("\\log"),   QLatin1String("log")},
        {QLatin1String("\\exp"),   QLatin1String("exp")},

        // Constants
        {QLatin1String("\\pi"),    QLatin1String("pi")},
        {QLatin1String("\\infty"), QLatin1String("inf")},

        // Miscellaneous
        {QLatin1String("\\abs"),   QLatin1String("abs")},

        // Spacing commands (remove)
        {QLatin1String("\\,"),     QLatin1String("")},
        {QLatin1String("\\;"),     QLatin1String("")},
        {QLatin1String("\\:"),     QLatin1String("")},
        {QLatin1String("\\!"),     QLatin1String("")},
        {QLatin1String("\\quad"),  QLatin1String("")},
        {QLatin1String("\\qquad"), QLatin1String("")},
        {QLatin1String("\\ "),     QLatin1String("")},

        // Display mode (remove)
        {QLatin1String("\\displaystyle"), QLatin1String("")},
        {QLatin1String("\\textstyle"),    QLatin1String("")},
    };

    for (const auto &[from, to] : simpleReplacements)
        t.replace(from, to);

    // --- x^{expr} → x^(expr)   and   x_{expr} → just remove subscript ---
    {
        // Powers: ^{...} → ^(...)
        static const QRegularExpression rePow(
            QStringLiteral(R"(\^\{([^}]*)\})"));
        t.replace(rePow, QStringLiteral("^(\\1)"));

        // Single-char power without braces is fine: x^2 stays x^2

        // Subscripts: _{...} → remove (we don't use subscripts in f(x))
        static const QRegularExpression reSub(
            QStringLiteral(R"(_\{([^}]*)\})"));
        t.replace(reSub, QString());
        // Single char subscript _n → remove
        static const QRegularExpression reSubSingle(
            QStringLiteral(R"(_[a-zA-Z0-9])"));
        t.replace(reSubSingle, QString());
    }

    // --- Remove any remaining backslash commands we don't handle ---
    {
        static const QRegularExpression reUnknownCmd(
            QStringLiteral(R"(\\[a-zA-Z]+)"));
        t.replace(reUnknownCmd, QString());
    }

    // --- Remove leftover braces (LaTeX grouping) ---
    t.replace(QLatin1Char('{'), QLatin1Char('('));
    t.replace(QLatin1Char('}'), QLatin1Char(')'));

    // Remove redundant double-parens like ((...)) → (...)
    while (t.contains(QStringLiteral("(("))) {
        static const QRegularExpression reDoubleParen(
            QStringLiteral(R"(\(\(([^()]*)\)\))"));
        const QString before = t;
        t.replace(reDoubleParen, QStringLiteral("(\\1)"));
        if (t == before)
            break;
    }

    return t;
}

// ---------------------------------------------------------------------------
// insertExplicitMultiplication
// ---------------------------------------------------------------------------

QString LatexToBlopConverter::insertExplicitMultiplication(const QString &s)
{
    if (s.size() < 2)
        return s;

    QString out;
    out.reserve(s.size() + 16);
    out += s.at(0);

    for (int i = 1; i < s.size(); ++i) {
        const QChar prev = s.at(i - 1);
        const QChar cur  = s.at(i);

        const bool needStar = [&]() {
            // digit followed by letter/opening paren:  2x → 2*x, 3( → 3*(
            if (prev.isDigit() && (cur.isLetter() || cur == QLatin1Char('(')))
                return true;
            // closing paren followed by letter/digit/opening paren:  )x → )*x
            if (prev == QLatin1Char(')') &&
                (cur.isLetter() || cur.isDigit() || cur == QLatin1Char('(')))
                return true;
            // letter/digit followed by opening paren, but NOT if prev is part
            // of a function name — we check if prev is a letter and next is '('.
            // Function names are handled elsewhere, so just skip letter+( cases
            // like sin( or cos(. We detect function context crudely:
            // if the sequence is  [a-z]( we assume it is a function call.
            // But digit( definitely needs *.
            // We skip letter-( since functions like sin( should not get *.
            return false;
        }();

        if (needStar)
            out += QLatin1Char('*');
        out += cur;
    }

    return out;
}
