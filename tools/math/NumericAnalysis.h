#pragma once

#include "MathTypes.h"
#include <QString>
#include <QVector>

class NumericAnalysis {
public:
    static double derivativeCentral(const ParsedExpression& expr, double x, double h = 1e-3);
    static double secondDerivativeCentral(const ParsedExpression& expr, double x, double h = 1e-3);
    static QVector<double> findRootsBisection(const ParsedExpression& expr, double xmin, double xmax, int samples = 256);
    static QVector<double> findExtrema(const ParsedExpression& expr, double xmin, double xmax, int samples = 256);
    /// Inflection points: roots of f'' with sign change.
    static QVector<double> findInflectionPoints(const ParsedExpression& expr, double xmin, double xmax, int samples = 256);
    /// Roots of f − g = 0 (intersections of two curves).
    static QVector<double> findIntersections(const ParsedExpression& f, const ParsedExpression& g,
                                             double xmin, double xmax, int samples = 256);
    /// Composite Simpson rule on [a,b].
    static double definiteIntegralSimpson(const ParsedExpression& expr, double a, double b, int n = 200);

    /// First free identifier in expr that is not x / built-in (e.g. "a" in a*x^2).
    static QString detectFreeParam(const QString &expression);
    /// Replace whole-word paramName with numeric literal.
    static QString substituteParam(const QString &expression, const QString &paramName, double value);

    /// Drag one real root to a new x. Polynomials are rebuilt as a·Π(x−ri);
    /// everything else is shifted in x so that root still lands on the new spot.
    static QString moveRootInExpression(const QString &expression, double oldRoot,
                                        double newRoot, double xmin, double xmax);
};
