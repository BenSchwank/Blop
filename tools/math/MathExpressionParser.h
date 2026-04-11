#pragma once

#include "MathTypes.h"

class MathExpressionParser {
public:
    static ParsedExpression parseFunctionExpression(const QString& input);
    /// Symbolic d/dx for display (chip labels). Empty if unsupported or parse error.
    static QString symbolicDerivativeString(const QString& input);
};
