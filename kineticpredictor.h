#ifndef KINETICPREDICTOR_H
#define KINETICPREDICTOR_H

#include <QPointF>
#include <cmath>

class KineticPredictor
{
public:
    KineticPredictor();

    void reset();
    void addPoint(const QPointF& p, qint64 timestamp);

    QPointF getPredictedPoint() const;
    QPointF getSmoothedPoint() const { return m_smoothedPos; }

private:
    QPointF m_smoothedPos;
    QPointF m_velocity;
    qint64 m_lastTimestamp;
    bool m_hasPriorPoint;

    // Einstellungen
    double m_responsiveness;

    // Sicherheitsschwellwerte
    static constexpr double MAX_VELOCITY = 5.0; // Max Pixel pro Millisekunde (sehr schnell!)
    static constexpr double PREDICTION_MS = 12.0; // Fixer Horizont für Stabilität
};

#endif // KINETICPREDICTOR_H
