#pragma once
#include <QObject>
#include <QStringList>
#include "ToolManager.h"
#include "../ToolSettings.h"
#include "AbstractTool.h"

class ToolUIBridge : public QObject {
    Q_OBJECT

    Q_PROPERTY(int activeToolMode READ activeToolMode NOTIFY activeToolChanged)

    // ... Pen Properties ...
    Q_PROPERTY(int penWidth READ penWidth WRITE setPenWidth NOTIFY configChanged)
    Q_PROPERTY(QColor penColor READ penColor WRITE setPenColor NOTIFY configChanged)
    Q_PROPERTY(double opacity READ opacity WRITE setOpacity NOTIFY configChanged)
    Q_PROPERTY(int smoothing READ smoothing WRITE setSmoothing NOTIFY configChanged)
    Q_PROPERTY(bool pressure READ pressure WRITE setPressure NOTIFY configChanged)

    // ... Pencil ...
    Q_PROPERTY(int hardness READ hardness WRITE setHardness NOTIFY configChanged)
    Q_PROPERTY(bool tiltShading READ tiltShading WRITE setTiltShading NOTIFY configChanged)

    // ... Highlighter ...
    Q_PROPERTY(bool smartLine READ smartLine WRITE setSmartLine NOTIFY configChanged)
    Q_PROPERTY(bool drawBehind READ drawBehind WRITE setDrawBehind NOTIFY configChanged)
    Q_PROPERTY(int highlighterTip READ highlighterTip WRITE setHighlighterTip NOTIFY configChanged)

    // ... Eraser ...
    Q_PROPERTY(int eraserMode READ eraserMode WRITE setEraserMode NOTIFY configChanged)
    Q_PROPERTY(bool eraserKeepInk READ eraserKeepInk WRITE setEraserKeepInk NOTIFY configChanged)

    // ... Lasso ...
    Q_PROPERTY(int lassoMode READ lassoMode WRITE setLassoMode NOTIFY configChanged)
    Q_PROPERTY(bool aspectLock READ aspectLock WRITE setAspectLock NOTIFY configChanged)

    // ... Ruler ...
    Q_PROPERTY(bool rulerSnap READ rulerSnap WRITE setRulerSnap NOTIFY rulerConfigChanged)
    Q_PROPERTY(bool compassMode READ compassMode WRITE setCompassMode NOTIFY rulerConfigChanged)
    Q_PROPERTY(bool infiniteRuler READ infiniteRuler WRITE setInfiniteRuler NOTIFY rulerConfigChanged)
    Q_PROPERTY(int rulerUnit READ rulerUnit WRITE setRulerUnit NOTIFY rulerConfigChanged)

    Q_PROPERTY(int holdShapeSensitivity READ holdShapeSensitivity WRITE setHoldShapeSensitivity NOTIFY configChanged)
    Q_PROPERTY(bool holdEnableCircle READ holdEnableCircle WRITE setHoldEnableCircle NOTIFY configChanged)
    Q_PROPERTY(bool holdEnableTriangle READ holdEnableTriangle WRITE setHoldEnableTriangle NOTIFY configChanged)
    Q_PROPERTY(int holdStillDelayMs READ holdStillDelayMs WRITE setHoldStillDelayMs NOTIFY configChanged)
    Q_PROPERTY(int shapeToolKind READ shapeToolKind WRITE setShapeToolKind NOTIFY configChanged)
    Q_PROPERTY(double shapeMathA READ shapeMathA WRITE setShapeMathA NOTIFY configChanged)
    Q_PROPERTY(double shapeMathB READ shapeMathB WRITE setShapeMathB NOTIFY configChanged)
    Q_PROPERTY(double shapeMathC READ shapeMathC WRITE setShapeMathC NOTIFY configChanged)
    Q_PROPERTY(double shapeMathD READ shapeMathD WRITE setShapeMathD NOTIFY configChanged)
    Q_PROPERTY(bool shapeSineFixedParams READ shapeSineFixedParams WRITE setShapeSineFixedParams NOTIFY configChanged)
    Q_PROPERTY(int shapeAxisTicks READ shapeAxisTicks WRITE setShapeAxisTicks NOTIFY configChanged)
    Q_PROPERTY(QStringList graphFunctionExpressions READ graphFunctionExpressions NOTIFY configChanged)
    Q_PROPERTY(int graphSelectedFunction READ graphSelectedFunction WRITE setGraphSelectedFunction NOTIFY configChanged)
    Q_PROPERTY(double graphXMin READ graphXMin WRITE setGraphXMin NOTIFY configChanged)
    Q_PROPERTY(double graphXMax READ graphXMax WRITE setGraphXMax NOTIFY configChanged)
    Q_PROPERTY(double graphYMin READ graphYMin WRITE setGraphYMin NOTIFY configChanged)
    Q_PROPERTY(double graphYMax READ graphYMax WRITE setGraphYMax NOTIFY configChanged)
    Q_PROPERTY(int settingsRevision READ settingsRevision NOTIFY settingsRevisionChanged)

    // Basis
    Q_PROPERTY(bool settingsVisible READ settingsVisible WRITE setSettingsVisible NOTIFY settingsVisibleChanged)
    Q_PROPERTY(QPoint overlayPosition READ overlayPosition WRITE setOverlayPosition NOTIFY overlayPositionChanged)

public:
    static ToolUIBridge& instance() {
        static ToolUIBridge _instance;
        return _instance;
    }

    explicit ToolUIBridge(QObject* parent = nullptr) : QObject(parent) {
        connect(&ToolManager::instance(), &ToolManager::toolChanged, this, &ToolUIBridge::onToolChanged);
        connect(&ToolManager::instance(), &ToolManager::configChanged, this, &ToolUIBridge::configChanged);
    }

    int activeToolMode() const { return (int)ToolManager::instance().activeToolMode(); }

    bool settingsVisible() const { return m_settingsVisible; }
    void setSettingsVisible(bool v) {
        if (m_settingsVisible != v) {
            m_settingsVisible = v;
            emit settingsVisibleChanged();
        }
        if (v) {
            ++m_settingsRevision;
            emit settingsRevisionChanged();
            emit configChanged();
        }
    }

    QPoint overlayPosition() const { return m_overlayPosition; }
    void setOverlayPosition(const QPoint& p) { m_overlayPosition=p; emit overlayPositionChanged(); }

    // GETTERS
    int penWidth() const { return ToolManager::instance().config().penWidth; }
    QColor penColor() const { return ToolManager::instance().config().penColor; }
    double opacity() const { return ToolManager::instance().config().opacity; }
    int smoothing() const { return ToolManager::instance().config().smoothing; }
    bool pressure() const { return ToolManager::instance().config().pressureSensitivity; }
    int hardness() const { return ToolManager::instance().config().hardness; }
    bool tiltShading() const { return ToolManager::instance().config().tiltShading; }
    bool smartLine() const { return ToolManager::instance().config().smartLine; }
    bool drawBehind() const { return ToolManager::instance().config().drawBehind; }
    int highlighterTip() const { return (int)ToolManager::instance().config().tipType; }
    int eraserMode() const { return (int)ToolManager::instance().config().eraserMode; }
    bool eraserKeepInk() const { return ToolManager::instance().config().eraserKeepInk; }
    int lassoMode() const { return (int)ToolManager::instance().config().lassoMode; }
    bool aspectLock() const { return ToolManager::instance().config().aspectLock; }

    bool rulerSnap() const { return ToolManager::instance().config().rulerSnap; }
    bool compassMode() const { return ToolManager::instance().config().compassMode; }
    bool infiniteRuler() const { return ToolManager::instance().config().infiniteRuler; }
    int rulerUnit() const { return (int)ToolManager::instance().config().rulerUnit; }

    int holdShapeSensitivity() const { return ToolManager::instance().config().holdShapeSensitivity; }
    bool holdEnableCircle() const { return ToolManager::instance().config().holdEnableCircle; }
    bool holdEnableTriangle() const { return ToolManager::instance().config().holdEnableTriangle; }
    int holdStillDelayMs() const { return ToolManager::instance().config().holdStillDelayMs; }
    int shapeToolKind() const { return static_cast<int>(ToolManager::instance().config().shapeToolKind); }
    double shapeMathA() const { return ToolManager::instance().config().shapeMathA; }
    double shapeMathB() const { return ToolManager::instance().config().shapeMathB; }
    double shapeMathC() const { return ToolManager::instance().config().shapeMathC; }
    double shapeMathD() const { return ToolManager::instance().config().shapeMathD; }
    bool shapeSineFixedParams() const { return ToolManager::instance().config().shapeSineFixedParams; }
    int shapeAxisTicks() const { return ToolManager::instance().config().shapeAxisTicks; }
    QStringList graphFunctionExpressions() const {
        QStringList out;
        const auto& f = ToolManager::instance().config().graphFunctions;
        for (const auto& e : f) out << e.expression;
        return out;
    }
    int graphSelectedFunction() const { return ToolManager::instance().config().graphSelectedFunction; }
    double graphXMin() const { return ToolManager::instance().config().graphXMin; }
    double graphXMax() const { return ToolManager::instance().config().graphXMax; }
    double graphYMin() const { return ToolManager::instance().config().graphYMin; }
    double graphYMax() const { return ToolManager::instance().config().graphYMax; }
    int settingsRevision() const { return m_settingsRevision; }

    // SETTERS
    void setPenWidth(int w) { updateConfig([w](ToolConfig& c){ c.penWidth = w; }); }
    void setPenColor(QColor col) { updateConfig([col](ToolConfig& c){ c.penColor = col; }); }
    void setOpacity(double v) { updateConfig([v](ToolConfig& c){ c.opacity = v; }); }
    void setSmoothing(int v) { updateConfig([v](ToolConfig& c){ c.smoothing = v; }); }
    void setPressure(bool v) { updateConfig([v](ToolConfig& c){ c.pressureSensitivity = v; }); }
    void setHardness(int v) { updateConfig([v](ToolConfig& c){ c.hardness = v; }); }
    void setTiltShading(bool v) { updateConfig([v](ToolConfig& c){ c.tiltShading = v; }); }
    void setSmartLine(bool v) { updateConfig([v](ToolConfig& c){ c.smartLine = v; }); }
    void setDrawBehind(bool v) { updateConfig([v](ToolConfig& c){ c.drawBehind = v; }); }
    void setHighlighterTip(int v) { updateConfig([v](ToolConfig& c){ c.tipType = (HighlighterTip)v; }); }
    void setEraserMode(int v) { updateConfig([v](ToolConfig& c){ c.eraserMode = (EraserMode)v; }); }
    void setEraserKeepInk(bool v) { updateConfig([v](ToolConfig& c){ c.eraserKeepInk = v; }); }
    void setLassoMode(int v) { updateConfig([v](ToolConfig& c){ c.lassoMode = (LassoMode)v; }); }
    void setAspectLock(bool v) { updateConfig([v](ToolConfig& c){ c.aspectLock = v; }); }

    void setRulerSnap(bool v) { updateConfig([v](ToolConfig& c){ c.rulerSnap = v; }); emit rulerConfigChanged(); }
    void setCompassMode(bool v) { updateConfig([v](ToolConfig& c){ c.compassMode = v; }); emit rulerConfigChanged(); }
    void setInfiniteRuler(bool v) { updateConfig([v](ToolConfig& c){ c.infiniteRuler = v; }); emit rulerConfigChanged(); }
    void setRulerUnit(int v) { updateConfig([v](ToolConfig& c){ c.rulerUnit = (RulerUnit)v; }); emit rulerConfigChanged(); }

    void setHoldShapeSensitivity(int v) {
        updateConfig([v](ToolConfig& c) { c.holdShapeSensitivity = qBound(0, v, 100); });
    }
    void setHoldEnableCircle(bool v) { updateConfig([v](ToolConfig& c) { c.holdEnableCircle = v; }); }
    void setHoldEnableTriangle(bool v) { updateConfig([v](ToolConfig& c) { c.holdEnableTriangle = v; }); }
    void setHoldStillDelayMs(int v) {
        updateConfig([v](ToolConfig& c) { c.holdStillDelayMs = qBound(200, v, 900); });
    }
    void setShapeToolKind(int v) {
        updateConfig([v](ToolConfig& c) {
            c.shapeToolKind = static_cast<ShapeToolKind>(qBound(0, v, 4));
        });
    }
    void setShapeMathA(double v) {
        updateConfig([v](ToolConfig& c) { c.shapeMathA = qBound(0.01, v, 2.0); });
    }
    void setShapeMathB(double v) {
        updateConfig([v](ToolConfig& c) { c.shapeMathB = qBound(0.05, v, 12.0); });
    }
    void setShapeMathC(double v) {
        updateConfig([v](ToolConfig& c) { c.shapeMathC = qBound(-12.57, v, 12.57); });
    }
    void setShapeMathD(double v) {
        updateConfig([v](ToolConfig& c) { c.shapeMathD = qBound(-1.5, v, 1.5); });
    }
    void setShapeSineFixedParams(bool v) {
        updateConfig([v](ToolConfig& c) { c.shapeSineFixedParams = v; });
    }
    void setShapeAxisTicks(int v) {
        updateConfig([v](ToolConfig& c) { c.shapeAxisTicks = qBound(2, v, 12); });
    }
    void setGraphSelectedFunction(int v) {
        updateConfig([v](ToolConfig& c) {
            c.graphSelectedFunction = qBound(0, v, qMax(0, c.graphFunctions.size() - 1));
        });
    }
    void setGraphXMin(double v) { updateConfig([v](ToolConfig& c) { c.graphXMin = v; }); }
    void setGraphXMax(double v) { updateConfig([v](ToolConfig& c) { c.graphXMax = v; }); }
    void setGraphYMin(double v) { updateConfig([v](ToolConfig& c) { c.graphYMin = v; }); }
    void setGraphYMax(double v) { updateConfig([v](ToolConfig& c) { c.graphYMax = v; }); }

    Q_INVOKABLE void addGraphFunction(const QString& expr) {
        updateConfig([expr](ToolConfig& c) {
            GraphFunctionSpec g;
            g.expression = expr.trimmed().isEmpty() ? QStringLiteral("sin(x)") : expr.trimmed();
            c.graphFunctions.push_back(g);
            c.graphSelectedFunction = c.graphFunctions.size() - 1;
        });
    }
    Q_INVOKABLE void removeGraphFunction(int index) {
        updateConfig([index](ToolConfig& c) {
            if (index < 0 || index >= c.graphFunctions.size()) return;
            c.graphFunctions.remove(index);
            if (c.graphFunctions.isEmpty()) c.graphFunctions.push_back(GraphFunctionSpec{});
            c.graphSelectedFunction = qBound(0, c.graphSelectedFunction, c.graphFunctions.size() - 1);
        });
    }
    Q_INVOKABLE void setGraphFunctionExpression(int index, const QString& expr) {
        updateConfig([index, expr](ToolConfig& c) {
            if (index < 0 || index >= c.graphFunctions.size()) return;
            c.graphFunctions[index].expression = expr.trimmed();
        });
    }
    Q_INVOKABLE void setGraphFunctionFlag(int index, const QString& flag, bool on) {
        updateConfig([index, flag, on](ToolConfig& c) {
            if (index < 0 || index >= c.graphFunctions.size()) return;
            auto& f = c.graphFunctions[index];
            if (flag == QLatin1String("derivative")) f.showDerivative = on;
            if (flag == QLatin1String("roots")) f.showRoots = on;
            if (flag == QLatin1String("extrema")) f.showExtrema = on;
            if (flag == QLatin1String("visible")) f.visible = on;
        });
    }
    Q_INVOKABLE bool graphFunctionFlag(int index, const QString& flag) const {
        const auto& c = ToolManager::instance().config();
        if (index < 0 || index >= c.graphFunctions.size()) return false;
        const auto& f = c.graphFunctions[index];
        if (flag == QLatin1String("derivative")) return f.showDerivative;
        if (flag == QLatin1String("roots")) return f.showRoots;
        if (flag == QLatin1String("extrema")) return f.showExtrema;
        if (flag == QLatin1String("visible")) return f.visible;
        return false;
    }

signals:
    void activeToolChanged();
    void configChanged();
    void rulerConfigChanged();
    void settingsVisibleChanged();
    void overlayPositionChanged();
    void settingsRevisionChanged();

private:
    template<typename Func>
    void updateConfig(Func f) {
        auto c = ToolManager::instance().config();
        f(c);
        ToolManager::instance().setConfig(c);
    }

    bool m_settingsVisible{false};
    int m_settingsRevision{0};
    QPoint m_overlayPosition{100, 100};
    AbstractTool* m_connectedTool{nullptr};

private slots:
    void onToolChanged(AbstractTool* tool) {
        if (m_connectedTool) disconnect(m_connectedTool, &AbstractTool::requestSettingsMenu, this, nullptr);
        m_connectedTool = tool;
        if (m_connectedTool) {
            connect(m_connectedTool, &AbstractTool::requestSettingsMenu, this, [this](){
                setSettingsVisible(!m_settingsVisible);
            });
        }
        emit activeToolChanged();
        emit configChanged();
        emit rulerConfigChanged();
    }
};
