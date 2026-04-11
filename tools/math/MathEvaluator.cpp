#include "MathEvaluator.h"

#include <QtMath>

double MathEvaluator::evalAt(const ParsedExpression& expr, double x) {
    if (!expr.ok || !expr.fn)
        return qQNaN();
    return expr.fn(x);
}
