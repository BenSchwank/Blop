#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QThread>
#include <QMutex>
#include <QVector2D>
#include <qevent.h>
#include <qmatrix4x4.h>
#include <vector>
#include <deque>

// Datenstruktur für einen einzelnen Vertex auf der GPU
struct Vertex {
    GLfloat x, y;       // Position
    GLfloat r, g, b, a; // Farbe
};

// Verwaltet rohe Input-Daten (vom Stylus)
struct InputPoint {
    QVector2D pos;
    float pressure;
    qint64 timestamp;
};

// Worker, der im Hintergrund rohe Punkte in Dreiecke (Vertices) umrechnet
class GeometryWorker : public QObject {
    Q_OBJECT
public:
    void processInput(const std::vector<InputPoint>& points);

signals:
    void geometryReady(const std::vector<Vertex>& newVertices);
};

class HighPerformanceCanvas : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit HighPerformanceCanvas(QWidget *parent = nullptr);
    ~HighPerformanceCanvas();

protected:
    void initializeGL() ;
    void paintGL() ;
    void resizeGL(int w, int h) ;

    // Input Handling (High Frequency)
    bool event(QEvent *event) ;
    void handleTouchEvent(QTouchEvent *event);
    void handleMouseEvent(QMouseEvent *event);

private:
    // OpenGL Ressourcen
    QOpenGLShaderProgram *m_program;
    QOpenGLVertexArrayObject m_vao;

    // VBOs für verschiedene Ebenen
    QOpenGLBuffer m_vboStatic;  // Die fertige Notiz (Großer Speicher)
    QOpenGLBuffer m_vboActive;  // Der aktuelle Strich (Wächst)
    QOpenGLBuffer m_vboPhantom; // Die Vorhersage (Wird jeden Frame gelöscht)

    // Shader Uniforms
    int m_matrixLoc;
    QMatrix4x4 m_projection;

    // Threading & Synchronisation
    QThread* m_workerThread;
    GeometryWorker* m_geoWorker;
    QMutex m_mutex;

    // Daten-Buffer (CPU Seite)
    std::vector<Vertex> m_staticGeometry; // Fertige Dreiecke
    std::vector<Vertex> m_activeGeometry; // Laufender Strich
    std::vector<Vertex> m_phantomGeometry;// Prädiktion

    // Input Prädiktion
    std::deque<InputPoint> m_inputHistory;

    // Methoden
    void uploadGeometry();
    void generatePhantomInk(); // Berechnet die Vorhersage (Negative Latenz)
    std::vector<Vertex> tessellateStroke(const InputPoint& p1, const InputPoint& p2);
};
