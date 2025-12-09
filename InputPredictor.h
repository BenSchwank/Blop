#ifndef INPUTPREDICTOR_H
#define INPUTPREDICTOR_H

#include <QPointF>
#include <QElapsedTimer>
#include <cmath>

/**
 * @brief High-Performance Kinetic Predictor für Stift-Eingaben.
 * Nutzt einen adaptiven Glättungsalgorithmus (Smart EMA), um Latenz zu verstecken.
 */
class KineticPredictor {
public:
    KineticPredictor();

    // Setzt den Status zurück (Wichtig bei PenDown Event)
    void reset();

    // Fügt einen echten Messpunkt hinzu und updated die Physik-Engine
    void addPoint(const QPointF& pos, qint64 timestamp);

    // Berechnet die "Phantom Ink" Position (Zukunft)
    QPointF predict() const;

private:
    // --- State Variables ---
    QPointF m_currentPos;       // Geglättete Position
    QPointF m_velocity;         // Aktueller Geschwindigkeitsvektor (px/ms)
    qint64 m_lastTimestamp;     // Zeitstempel des letzten echten Punktes

    // --- "Smart" Parameters (werden dynamisch angepasst) ---
    double m_smoothingFactor;   // Alpha: 0.0 (träge) bis 1.0 (direkt)
    double m_predictionTime;    // Wie viele ms schauen wir in die Zukunft?

    // --- Constants ---
    // Mindestgeschwindigkeit, ab der wir die Prediction hochfahren
    static constexpr double MIN_SPEED_THRESHOLD = 0.05;
    // Maximale Zeit für die Vorhersage (cap gegen Overshooting)
    static constexpr double MAX_PREDICTION_MS = 20.0;
};

#endif // INPUTPREDICTOR_H
