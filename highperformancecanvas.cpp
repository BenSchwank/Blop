#include "HighPerformanceCanvas.h"
#include <QTouchEvent>
#include <QMouseEvent>
#include <QMatrix4x4>
#include <cmath>
#include <QDateTime> // <-- HIER: Das hat gefehlt!

// Vertex Shader: Simpler Pass-Through mit Projektionsmatrix
static const char *vertexShaderSource = R"(
    attribute vec2 aPos;
    attribute vec4 aColor;
    uniform mat4 uMatrix;
    varying vec4 vColor;
    void main() {
        gl_Position = uMatrix * vec4(aPos, 0.0, 1.0);
        vColor = aColor;
    }
)";

// Fragment Shader: Gibt einfach die Farbe aus
static const char *fragmentShaderSource = R"(
    varying vec4 vColor;
    void main() {
        gl_FragColor = vColor;
    }
)";

HighPerformanceCanvas::HighPerformanceCanvas(QWidget *parent)
    : QOpenGLWidget(parent)
{
    // Wichtig: Touch-Events akzeptieren
    setAttribute(Qt::WA_AcceptTouchEvents);

    // Worker Thread initialisieren (Entkopplung Input -> Geometrie)
    m_workerThread = new QThread(this);
    m_geoWorker = new GeometryWorker();
    m_geoWorker->moveToThread(m_workerThread);

    // Wenn der Worker fertig gerechnet hat, Daten abholen
    connect(m_geoWorker, &GeometryWorker::geometryReady, this, [this](const std::vector<Vertex>& verts){
        QMutexLocker locker(&m_mutex);
        // Neue Teile an die aktive Geometrie anhängen
        m_activeGeometry.insert(m_activeGeometry.end(), verts.begin(), verts.end());
        update(); // Trigger paintGL
    });

    m_workerThread->start();
}

HighPerformanceCanvas::~HighPerformanceCanvas() {
    makeCurrent();
    m_vboStatic.destroy();
    m_vboActive.destroy();
    m_vboPhantom.destroy();
    delete m_program;
    doneCurrent();

    m_workerThread->quit();
    m_workerThread->wait();
}

void HighPerformanceCanvas::initializeGL() {
    initializeOpenGLFunctions();

    // Hintergrundfarbe (Dunkelgrau für Modern Look)
    glClearColor(0.1f, 0.1f, 0.12f, 1.0f);

    // Blending aktivieren für weiche Kanten (Anti-Aliasing Fake)
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Shader kompilieren
    m_program = new QOpenGLShaderProgram;
    m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource);
    m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource);
    m_program->link();
    m_program->bind();

    // Uniform Locations
    m_matrixLoc = m_program->uniformLocation("uMatrix");

    // VAO Setup
    m_vao.create();
    m_vao.bind();

    // VBOs erstellen (Static = UsagePattern::StaticDraw, Active = DynamicDraw)
    m_vboStatic.create();
    m_vboActive.create();
    m_vboPhantom.create();
}

void HighPerformanceCanvas::resizeGL(int w, int h) {
    // Orthographische Projektion (2D Ansicht)
    m_projection.setToIdentity();
    m_projection.ortho(0, w, h, 0, -1, 1); // 0,0 oben links
}

void HighPerformanceCanvas::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT);
    m_program->bind();
    m_program->setUniformValue(m_matrixLoc, m_projection);
    m_vao.bind();

    QMutexLocker locker(&m_mutex);

    // --- 1. LAYER: Statische Tinte (Alte Striche) ---
    // In einer echten App würde man das nicht jeden Frame hochladen,
    // sondern nur wenn ein Strich beendet wurde.
    if(!m_staticGeometry.empty()) {
        m_vboStatic.bind();
        // Hier sollte man map() oder allocate() nur bei Änderungen nutzen
        // Für Demo-Zwecke vereinfacht:
        m_vboStatic.allocate(m_staticGeometry.data(), m_staticGeometry.size() * sizeof(Vertex));

        m_program->enableAttributeArray("aPos");
        m_program->setAttributeBuffer("aPos", GL_FLOAT, 0, 2, sizeof(Vertex));
        m_program->enableAttributeArray("aColor");
        m_program->setAttributeBuffer("aColor", GL_FLOAT, 2 * sizeof(GLfloat), 4, sizeof(Vertex));

        glDrawArrays(GL_TRIANGLE_STRIP, 0, m_staticGeometry.size());
        m_vboStatic.release();
    }

    // --- 2. LAYER: Aktiver Strich (Live Input) ---
    if(!m_activeGeometry.empty()) {
        m_vboActive.bind();
        m_vboActive.allocate(m_activeGeometry.data(), m_activeGeometry.size() * sizeof(Vertex));

        m_program->enableAttributeArray("aPos");
        m_program->setAttributeBuffer("aPos", GL_FLOAT, 0, 2, sizeof(Vertex));
        m_program->enableAttributeArray("aColor");
        m_program->setAttributeBuffer("aColor", GL_FLOAT, 2 * sizeof(GLfloat), 4, sizeof(Vertex));

        glDrawArrays(GL_TRIANGLE_STRIP, 0, m_activeGeometry.size());
        m_vboActive.release();
    }

    // --- 3. LAYER: Phantom Ink (Prediction Overlay) ---
    // Dieses VBO ist "Ephemeral", es wird jeden Frame neu befüllt
    generatePhantomInk();
    if(!m_phantomGeometry.empty()) {
        m_vboPhantom.bind();
        m_vboPhantom.allocate(m_phantomGeometry.data(), m_phantomGeometry.size() * sizeof(Vertex));

        m_program->enableAttributeArray("aPos");
        m_program->setAttributeBuffer("aPos", GL_FLOAT, 0, 2, sizeof(Vertex));
        m_program->enableAttributeArray("aColor");
        m_program->setAttributeBuffer("aColor", GL_FLOAT, 2 * sizeof(GLfloat), 4, sizeof(Vertex));

        // Zeichne Phantom Ink leicht transparent
        glDrawArrays(GL_TRIANGLE_STRIP, 0, m_phantomGeometry.size());
        m_vboPhantom.release();
    }
}

// Input Coalescing: Wir fangen Events ab, bevor sie spezialisiert werden
bool HighPerformanceCanvas::event(QEvent *event) {
    if (event->type() == QEvent::TouchBegin ||
        event->type() == QEvent::TouchUpdate ||
        event->type() == QEvent::TouchEnd) {
        handleTouchEvent(static_cast<QTouchEvent *>(event));
        return true;
    }
    // Fallback für Maus (Debugging am Desktop)
    if (event->type() == QEvent::MouseMove) {
        handleMouseEvent(static_cast<QMouseEvent *>(event));
        return true; // Wichtig: Event konsumieren
    }
    return QOpenGLWidget::event(event);
}

void HighPerformanceCanvas::handleTouchEvent(QTouchEvent *event) {
    const QList<QTouchEvent::TouchPoint> &points = event->touchPoints();
    if (points.isEmpty()) return;

    // Nur den primären Finger/Stift betrachten (Multi-Touch wäre hier komplexer)
    const QTouchEvent::TouchPoint &tp = points.first();

    InputPoint ip;
    ip.pos = QVector2D(tp.pos());
    ip.pressure = tp.pressure();
    ip.timestamp = QDateTime::currentMSecsSinceEpoch();

    // Zu Historie hinzufügen (für Prediction)
    m_inputHistory.push_back(ip);
    if(m_inputHistory.size() > 10) m_inputHistory.pop_front();

    // Daten an Worker Thread senden zur Geometrie-Berechnung
    // Hier würde man normalerweise "Input Coalescing" machen:
    // Sammle alle Punkte dieses Events (QTouchEvent kann mehrere History-Punkte haben)
    std::vector<InputPoint> batch;
    batch.push_back(ip);

    // Asynchron an den Worker senden (Thread-Safe via Signal/Slot Queue)
    // In einer echten High-End Lösung würde man hier Lock-Free Queues nutzen.
    QMetaObject::invokeMethod(m_geoWorker, [this, batch](){
        m_geoWorker->processInput(batch);
    });

    // Prediction sofort triggern für diesen Frame
    update();
}

void HighPerformanceCanvas::handleMouseEvent(QMouseEvent *event) {
    // Gleiche Logik wie Touch, nur ohne Drucksensitivität
    InputPoint ip;
    ip.pos = QVector2D(event->pos());
    ip.pressure = 1.0f;
    ip.timestamp = QDateTime::currentMSecsSinceEpoch();

    m_inputHistory.push_back(ip);
    if(m_inputHistory.size() > 10) m_inputHistory.pop_front();

    std::vector<InputPoint> batch; batch.push_back(ip);
    QMetaObject::invokeMethod(m_geoWorker, [this, batch](){
        m_geoWorker->processInput(batch);
    });
    update();
}

// --- LOGIK: Phantom Ink (Prediction) ---
void HighPerformanceCanvas::generatePhantomInk() {
    m_phantomGeometry.clear();
    if (m_inputHistory.size() < 2) return;

    // Linearer Extrapolator (Simpelster Prädiktor)
    // In Produktion: Kalman-Filter nutzen!
    const InputPoint& pLast = m_inputHistory.back();
    const InputPoint& pPrev = m_inputHistory[m_inputHistory.size()-2];

    QVector2D velocity = pLast.pos - pPrev.pos;

    // Vorhersage für 2 Frames in die Zukunft (ca 32ms bei 60Hz)
    QVector2D predictedPos = pLast.pos + (velocity * 1.5f);

    // Geometrie für die Vorhersage bauen (Dünner werdend)
    // Wir bauen ein einfaches Rechteck/Dreieck als Strich
    Vertex v1 = {pLast.pos.x(), pLast.pos.y() + 2, 0.5f, 0.5f, 1.0f, 0.5f}; // Halbtransparent Blau
    Vertex v2 = {pLast.pos.x(), pLast.pos.y() - 2, 0.5f, 0.5f, 1.0f, 0.5f};
    Vertex v3 = {predictedPos.x(), predictedPos.y(), 0.5f, 0.5f, 1.0f, 0.0f}; // Spitze transparent

    m_phantomGeometry.push_back(v1);
    m_phantomGeometry.push_back(v2);
    m_phantomGeometry.push_back(v3);
}

// --- WORKER: Geometrie Erzeugung (Tessellation) ---
void GeometryWorker::processInput(const std::vector<InputPoint>& points) {
    std::vector<Vertex> newVerts;

    // Simpler Tessellator: Erzeugt einen "Band" (Triangle Strip) um die Linie
    float thickness = 3.0f;

    for (const auto& p : points) {
        // Hier würde normalerweise komplexe Bezier-Mathematik stehen.
        // Für das Beispiel erzeugen wir ein Quad pro Punkt (Nicht perfekt, aber schnell)

        // Farbe: Weiß, volle Deckkraft
        float r=1, g=1, b=1, a=1;

        // Wir brauchen eigentlich den Vektor zum vorherigen Punkt für die Normale.
        // Vereinfacht: Feste Breite
        Vertex v1 = {p.pos.x(), p.pos.y() + thickness, r, g, b, a};
        Vertex v2 = {p.pos.x(), p.pos.y() - thickness, r, g, b, a};

        newVerts.push_back(v1);
        newVerts.push_back(v2);
    }

    emit geometryReady(newVerts);
}
