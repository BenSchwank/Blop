#include "InputPredictor.h"

KineticPredictor::KineticPredictor()
    : m_currentPos(0, 0), m_velocity(0, 0), m_lastTimestamp(0),
    m_smoothingFactor(0.5), m_predictionTime(0.0)
{
}

void KineticPredictor::reset() {
    m_currentPos = QPointF(0, 0);
    m_velocity = QPointF(0, 0);
    m_lastTimestamp = 0;
    // Konservative Startwerte
    m_smoothingFactor = 0.8;
    m_predictionTime = 0.0;
}

void KineticPredictor::addPoint(const QPointF& rawPos, qint64 timestamp) {
    // 1. Initialisierung beim ersten Punkt
    if (m_lastTimestamp == 0) {
        m_currentPos = rawPos;
        m_lastTimestamp = timestamp;
        return;
    }

    // 2. Zeitdifferenz berechnen (dt)
    double dt = static_cast<double>(timestamp - m_lastTimestamp);
    if (dt <= 0.0) return; // Schutz gegen Division durch Null

    // 3. Rohe Geschwindigkeit berechnen (Instant Velocity)
    QPointF rawVelocity = (rawPos - m_currentPos) / dt;

    // Geschwindigkeit in Pixel pro Frame (ungefähr) für Schwellenwerte
    double speed = std::sqrt(rawVelocity.x() * rawVelocity.x() + rawVelocity.y() * rawVelocity.y());

    // --- 4. THE SMART PATH (Adaptive Logic) ---
    // Hier entscheidet die "Mini-AI", wie sich der Stift verhält.

    if (speed > MIN_SPEED_THRESHOLD) {
        // SCHNELL: Wir vertrauen dem Input mehr (weniger Glättung) und schauen weiter voraus.
        // Ziel: "Snappy" Gefühl, scharfe Kurven.
        m_smoothingFactor = 0.6 + (speed * 0.1); // Dynamisch erhöhen bis max ca 0.9
        if (m_smoothingFactor > 0.95) m_smoothingFactor = 0.95;

        m_predictionTime = 12.0 + (speed * 5.0); // Vorhersage 12ms bis 20ms
        if (m_predictionTime > MAX_PREDICTION_MS) m_predictionTime = MAX_PREDICTION_MS;
    } else {
        // LANGSAM: Wir glätten stark (Stabilisierung) und sagen wenig voraus.
        // Ziel: Kein Zittern bei kleinen Details (Punkte, Kommas).
        m_smoothingFactor = 0.2;
        m_predictionTime = 2.0; // Fast keine Vorhersage bei Stillstand
    }

    // --- 5. FAST PATH (Physics Update) ---
    // Exponential Moving Average (EMA) für Position
    // NewPos = (Raw * Alpha) + (Old * (1 - Alpha))
    m_currentPos = (rawPos * m_smoothingFactor) + (m_currentPos * (1.0 - m_smoothingFactor));

    // EMA für Geschwindigkeit (dämpft Spitzen ab)
    double velAlpha = 0.4; // Konstanter Glättungsfaktor für Velocity
    m_velocity = (rawVelocity * velAlpha) + (m_velocity * (1.0 - velAlpha));

    m_lastTimestamp = timestamp;
}

QPointF KineticPredictor::predict() const {
    // Einfache kinematische Projektion: P_neu = P_akt + v * t
    // Wir nutzen die geglättete Geschwindigkeit und die adaptive Zeit.
    return m_currentPos + (m_velocity * m_predictionTime);
}
