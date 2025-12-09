#include "KineticPredictor.h"
#include <algorithm>

KineticPredictor::KineticPredictor()
    : m_lastTimestamp(0)
    , m_hasPriorPoint(false)
    , m_responsiveness(0.7) // Hohe Direktheit
{
}

void KineticPredictor::reset()
{
    m_smoothedPos = QPointF(0, 0);
    m_velocity = QPointF(0, 0);
    m_lastTimestamp = 0;
    m_hasPriorPoint = false;
}

void KineticPredictor::addPoint(const QPointF& rawPos, qint64 timestamp)
{
    if (!m_hasPriorPoint) {
        m_smoothedPos = rawPos;
        m_velocity = QPointF(0, 0);
        m_lastTimestamp = timestamp;
        m_hasPriorPoint = true;
        return;
    }

    double dt = static_cast<double>(timestamp - m_lastTimestamp);

    // 1. HARD FILTER: Zeit-Anomalien
    if (dt <= 0.001) return; // Zu schnell (Doppel-Event)
    if (dt > 200.0) {        // Zu langsam (Lag) -> Reset
        m_smoothedPos = rawPos;
        m_velocity = QPointF(0,0);
        m_lastTimestamp = timestamp;
        return;
    }

    // 2. HARD FILTER: Teleportation
    // Distanz berechnen
    double dx = rawPos.x() - m_smoothedPos.x();
    double dy = rawPos.y() - m_smoothedPos.y();
    double dist = std::hypot(dx, dy);

    // Wenn der Sprung physikalisch unmöglich ist (z.B. > 100px in 5ms), ignorieren oder resetten
    if (dist > 150.0) {
        m_smoothedPos = rawPos;
        m_velocity = QPointF(0,0);
        m_lastTimestamp = timestamp;
        return;
    }

    // 3. Velocity Calculation mit Clamp
    QPointF newVel = QPointF(dx, dy) / dt;
    double speed = std::hypot(newVel.x(), newVel.y());

    if (speed > MAX_VELOCITY) {
        // Begrenzen auf Maximum (verhindert das "Wegschießen")
        newVel = newVel / speed * MAX_VELOCITY;
    }

    // 4. Glättung (Adaptive Exponential Moving Average)
    // Wir glätten die Geschwindigkeit, nicht die Position, für "Momentum"
    // Bei hohem Speed glätten wir weniger (direkter), bei langsamem mehr (sauberer)
    double alpha = 0.6;
    if (speed > 1.5) alpha = 0.9; // Fast direkt bei schnellen Strichen

    m_velocity = (m_velocity * (1.0 - alpha)) + (newVel * alpha);

    // Position nachziehen
    // Wir vertrauen dem neuen Punkt mehr als der alten Position
    m_smoothedPos = (m_smoothedPos * 0.3) + (rawPos * 0.7);

    m_lastTimestamp = timestamp;
}

QPointF KineticPredictor::getPredictedPoint() const
{
    if (!m_hasPriorPoint) return QPointF();

    // Einfache Lineare Prädiktion ist am stabilsten
    // Position + Geschwindigkeit * Zeit
    return m_smoothedPos + (m_velocity * PREDICTION_MS);
}
