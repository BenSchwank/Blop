#pragma once

#include "MathTypes.h"

class MathEvaluator {
public:
    static double evalAt(const ParsedExpression& expr, double x);
};
