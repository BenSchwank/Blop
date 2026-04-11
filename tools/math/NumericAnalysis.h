#pragma once

#include "MathTypes.h"
#include <QVector>

class NumericAnalysis {
public:
    static double derivativeCentral(const ParsedExpression& expr, double x, double h = 1e-3);
    static QVector<double> findRootsBisection(const ParsedExpression& expr, double xmin, double xmax, int samples = 256);
    static QVector<double> findExtrema(const ParsedExpression& expr, double xmin, double xmax, int samples = 256);
};
