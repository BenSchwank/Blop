#include "NumericAnalysis.h"
#include "MathEvaluator.h"

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
