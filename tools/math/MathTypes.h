#pragma once

#include <QString>
#include <functional>

struct ParsedExpression {
    bool ok{false};
    QString error;
    QString normalizedInput;
    std::function<double(double)> fn;
};

struct GraphEvalPoint {
    double x{0.0};
    double y{0.0};
};
