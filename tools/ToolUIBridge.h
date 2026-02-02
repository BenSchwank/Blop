#pragma once
#include <QObject>
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
    Q_PROPERTY(int rulerUnit READ rulerUnit WRITE setRulerUnit NOTIFY rulerConfigChanged)

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
    void setSettingsVisible(bool v) { if(m_settingsVisible!=v){ m_settingsVisible=v; emit settingsVisibleChanged(); } }

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
    int rulerUnit() const { return (int)ToolManager::instance().config().rulerUnit; }

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
    void setRulerUnit(int v) { updateConfig([v](ToolConfig& c){ c.rulerUnit = (RulerUnit)v; }); emit rulerConfigChanged(); }

signals:
    void activeToolChanged();
    void configChanged();
    void rulerConfigChanged();
    void settingsVisibleChanged();
    void overlayPositionChanged();

private:
    template<typename Func>
    void updateConfig(Func f) {
        auto c = ToolManager::instance().config();
        f(c);
        ToolManager::instance().setConfig(c);
    }

    bool m_settingsVisible{false};
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
