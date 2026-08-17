#pragma once

#include "MathTypes.h"
#include <QString>
#include <QVector>

class NumericAnalysis {
public:
    static double derivativeCentral(const ParsedExpression& expr, double x, double h = 1e-3);
    static QVector<double> findRootsBisection(const ParsedExpression& expr, double xmin, double xmax, int samples = 256);
    static QVector<double> findExtrema(const ParsedExpression& expr, double xmin, double xmax, int samples = 256);

    /// Drag one real root to a new x. Polynomials are rebuilt as a·Π(x−ri);
    /// everything else is shifted in x so that root still lands on the new spot.
    static QString moveRootInExpression(const QString &expression, double oldRoot,
                                        double newRoot, double xmin, double xmax);
};
