#include "NumericAnalysis.h"
#include "MathEvaluator.h"
#include "MathExpressionParser.h"

#include <QRegularExpression>
#include <QSet>
#include <QtMath>

namespace {
double bisectRoot(const ParsedExpression& expr, double a, double b, int iters = 28) {
    double fa = MathEvaluator::evalAt(expr, a);
    double fb = MathEvaluator::evalAt(expr, b);
    if (!qIsFinite(fa) || !qIsFinite(fb) || fa * fb > 0.0)
        return qQNaN();
    for (int i = 0; i < iters; ++i) {
        const double m = 0.5 * (a + b);
        const double fm = MathEvaluator::evalAt(expr, m);
        if (!qIsFinite(fm))
            return qQNaN();
        if (fa * fm <= 0.0) {
            b = m;
            fb = fm;
        } else {
            a = m;
            fa = fm;
        }
    }
    return 0.5 * (a + b);
}
}

double NumericAnalysis::derivativeCentral(const ParsedExpression& expr, double x, double h) {
    const double f1 = MathEvaluator::evalAt(expr, x + h);
    const double f0 = MathEvaluator::evalAt(expr, x - h);
    if (!qIsFinite(f1) || !qIsFinite(f0))
        return qQNaN();
    return (f1 - f0) / (2.0 * h);
}

double NumericAnalysis::secondDerivativeCentral(const ParsedExpression& expr, double x, double h) {
    const double f1 = MathEvaluator::evalAt(expr, x + h);
    const double f0 = MathEvaluator::evalAt(expr, x);
    const double fm = MathEvaluator::evalAt(expr, x - h);
    if (!qIsFinite(f1) || !qIsFinite(f0) || !qIsFinite(fm))
        return qQNaN();
    return (f1 - 2.0 * f0 + fm) / (h * h);
}

QVector<double> NumericAnalysis::findRootsBisection(const ParsedExpression& expr, double xmin, double xmax, int samples) {
    QVector<double> roots;
    if (samples < 4 || xmax <= xmin)
        return roots;
    const double dx = (xmax - xmin) / static_cast<double>(samples);
    double x0 = xmin;
    double y0 = MathEvaluator::evalAt(expr, x0);
    for (int i = 1; i <= samples; ++i) {
        const double x1 = xmin + dx * static_cast<double>(i);
        const double y1 = MathEvaluator::evalAt(expr, x1);
        if (qIsFinite(y0) && qIsFinite(y1) && y0 * y1 <= 0.0) {
            const double r = bisectRoot(expr, x0, x1);
            if (qIsFinite(r)) {
                if (roots.isEmpty() || qAbs(roots.back() - r) > 1e-3)
                    roots.push_back(r);
            }
        }
        x0 = x1;
        y0 = y1;
    }
    return roots;
}

QVector<double> NumericAnalysis::findExtrema(const ParsedExpression& expr, double xmin, double xmax, int samples) {
    QVector<double> ex;
    if (samples < 8 || xmax <= xmin)
        return ex;
    ParsedExpression dExpr;
    dExpr.ok = true;
    dExpr.fn = [&](double x) { return derivativeCentral(expr, x, 1e-3); };
    ex = findRootsBisection(dExpr, xmin, xmax, samples);
    return ex;
}

QVector<double> NumericAnalysis::findInflectionPoints(const ParsedExpression& expr, double xmin, double xmax, int samples) {
    QVector<double> pts;
    if (samples < 8 || xmax <= xmin)
        return pts;
    ParsedExpression d2;
    d2.ok = true;
    d2.fn = [&](double x) { return secondDerivativeCentral(expr, x, 1e-3); };
    return findRootsBisection(d2, xmin, xmax, samples);
}

QVector<double> NumericAnalysis::findIntersections(const ParsedExpression& f, const ParsedExpression& g,
                                                   double xmin, double xmax, int samples) {
    ParsedExpression diff;
    diff.ok = true;
    diff.fn = [&](double x) {
        const double a = MathEvaluator::evalAt(f, x);
        const double b = MathEvaluator::evalAt(g, x);
        if (!qIsFinite(a) || !qIsFinite(b))
            return qQNaN();
        return a - b;
    };
    return findRootsBisection(diff, xmin, xmax, samples);
}

double NumericAnalysis::definiteIntegralSimpson(const ParsedExpression& expr, double a, double b, int n) {
    if (!qIsFinite(a) || !qIsFinite(b) || b <= a)
        return qQNaN();
    n = qMax(2, n);
    if (n % 2)
        ++n;
    const double h = (b - a) / static_cast<double>(n);
    double sum = MathEvaluator::evalAt(expr, a) + MathEvaluator::evalAt(expr, b);
    if (!qIsFinite(sum))
        return qQNaN();
    for (int i = 1; i < n; ++i) {
        const double y = MathEvaluator::evalAt(expr, a + h * i);
        if (!qIsFinite(y))
            return qQNaN();
        sum += (i % 2 == 0) ? 2.0 * y : 4.0 * y;
    }
    return sum * h / 3.0;
}

QString NumericAnalysis::detectFreeParam(const QString &expression) {
    static const QSet<QString> kReserved = {
        QStringLiteral("x"), QStringLiteral("e"), QStringLiteral("pi"), QStringLiteral("π"),
        QStringLiteral("sin"), QStringLiteral("cos"), QStringLiteral("tan"),
        QStringLiteral("asin"), QStringLiteral("acos"), QStringLiteral("atan"),
        QStringLiteral("sinh"), QStringLiteral("cosh"), QStringLiteral("tanh"),
        QStringLiteral("log"), QStringLiteral("ln"), QStringLiteral("lg"),
        QStringLiteral("exp"), QStringLiteral("sqrt"), QStringLiteral("abs"),
        QStringLiteral("floor"), QStringLiteral("ceil"), QStringLiteral("round"),
        QStringLiteral("min"), QStringLiteral("max"), QStringLiteral("pow"),
        QStringLiteral("y"), QStringLiteral("f"), QStringLiteral("g"),
    };
    QString rhs = expression.trimmed();
    const int eq = rhs.indexOf(QLatin1Char('='));
    if (eq >= 0)
        rhs = rhs.mid(eq + 1);
    static const QRegularExpression idRe(QStringLiteral(R"([A-Za-zπ][A-Za-z0-9_]*)"));
    auto it = idRe.globalMatch(rhs);
    while (it.hasNext()) {
        const QString id = it.next().captured();
        if (!kReserved.contains(id.toLower()) && id != QStringLiteral("π"))
            return id;
    }
    return {};
}

QString NumericAnalysis::substituteParam(const QString &expression, const QString &paramName,
                                         double value) {
    if (paramName.isEmpty())
        return expression;
    QString out = expression;
    const QString lit = QStringLiteral("(%1)").arg(QString::number(value, 'g', 8));
    const QRegularExpression re(
        QStringLiteral(R"((?<![A-Za-z0-9_])%1(?![A-Za-z0-9_]))").arg(QRegularExpression::escape(paramName)));
    out.replace(re, lit);
    return out;
}

namespace {
QString stripFnPrefix(QString expr) {
    expr = expr.trimmed();
    if (expr.startsWith(QLatin1String("y"), Qt::CaseInsensitive) ||
        expr.startsWith(QLatin1String("f("), Qt::CaseInsensitive)) {
        const int eq = expr.indexOf(QLatin1Char('='));
        if (eq >= 0)
            expr = expr.mid(eq + 1).trimmed();
    }
    return expr;
}

QString formatRootTerm(double r) {
    if (qAbs(r) < 1e-8)
        return QStringLiteral("x");
    const QString mag = QString::number(qAbs(r), 'g', 4);
    return r > 0.0 ? QStringLiteral("(x-%1)").arg(mag)
                   : QStringLiteral("(x+%1)").arg(mag);
}

QString formatFactoredPolynomial(double leading, const QVector<double> &roots) {
    QString s;
    if (qAbs(leading + 1.0) < 1e-6)
        s += QLatin1Char('-');
    else if (qAbs(leading - 1.0) >= 1e-6)
        s += QString::number(leading, 'g', 4);
    for (double r : roots)
        s += formatRootTerm(r);
    if (s.isEmpty() || s == QLatin1String("-"))
        s += QLatin1Char('0');
    return s;
}

bool looksLikeRealPolynomial(const ParsedExpression &expr, const QVector<double> &roots,
                             double xmin, double xmax, double *outLeading) {
    if (roots.isEmpty() || !expr.ok)
        return false;
    double xProbe = xmax + 1.25;
    for (int tries = 0; tries < 10; ++tries) {
        bool far = true;
        for (double r : roots) {
            if (qAbs(xProbe - r) < 0.4)
                far = false;
        }
        if (far)
            break;
        xProbe += 1.1;
    }
    const double fx = MathEvaluator::evalAt(expr, xProbe);
    double prod = 1.0;
    for (double r : roots)
        prod *= (xProbe - r);
    if (!qIsFinite(fx) || !qIsFinite(prod) || qAbs(prod) < 1e-12)
        return false;
    const double a = fx / prod;
    if (!qIsFinite(a))
        return false;

    int checked = 0;
    int matched = 0;
    for (int i = 0; i <= 14; ++i) {
        const double x = xmin + (xmax - xmin) * (static_cast<double>(i) / 14.0);
        bool nearRoot = false;
        for (double r : roots) {
            if (qAbs(x - r) < 0.06)
                nearRoot = true;
        }
        if (nearRoot)
            continue;
        const double y = MathEvaluator::evalAt(expr, x);
        if (!qIsFinite(y))
            continue;
        double yp = a;
        for (double r : roots)
            yp *= (x - r);
        ++checked;
        if (qAbs(y - yp) <= 0.08 * (1.0 + qAbs(y)))
            ++matched;
    }
    if (checked < 4 || matched < checked - 1)
        return false;
    if (outLeading)
        *outLeading = a;
    return true;
}

QString shiftExpressionInX(const QString &expression, double delta) {
    if (qAbs(delta) < 1e-10)
        return expression;
    QString rhs = stripFnPrefix(expression);
    const QString inner =
        QStringLiteral("(x-(%1))").arg(QString::number(delta, 'g', 6));
    static const QRegularExpression xRe(
        QStringLiteral(R"((?<![A-Za-z0-9_])x(?![A-Za-z0-9_]))"));
    rhs.replace(xRe, inner);
    return rhs;
}
} // namespace

QString NumericAnalysis::moveRootInExpression(const QString &expression, double oldRoot,
                                              double newRoot, double xmin, double xmax) {
    if (!qIsFinite(oldRoot) || !qIsFinite(newRoot))
        return expression;
    const ParsedExpression parsed =
        MathExpressionParser::parseFunctionExpression(expression);
    if (!parsed.ok)
        return expression;

    QVector<double> roots = findRootsBisection(parsed, xmin, xmax, 360);
    int idx = -1;
    double best = 1e9;
    for (int i = 0; i < roots.size(); ++i) {
        const double d = qAbs(roots[i] - oldRoot);
        if (d < best) {
            best = d;
            idx = i;
        }
    }
    if (idx < 0 || best > 0.35)
        return shiftExpressionInX(expression, newRoot - oldRoot);

    double leading = 1.0;
    if (looksLikeRealPolynomial(parsed, roots, xmin, xmax, &leading)) {
        roots[idx] = newRoot;
        return formatFactoredPolynomial(leading, roots);
    }
    return shiftExpressionInX(expression, newRoot - oldRoot);
}
