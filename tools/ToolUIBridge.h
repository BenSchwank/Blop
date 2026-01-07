#pragma once
#include <QObject>
#include <QPoint>
#include <QColor>
#include "ToolManager.h"

class ToolUIBridge : public QObject {
    Q_OBJECT

    // --- BASIS ---
    Q_PROPERTY(int activeToolMode READ activeToolMode NOTIFY toolChanged)
    Q_PROPERTY(bool settingsVisible READ settingsVisible WRITE setSettingsVisible NOTIFY settingsVisibleChanged)
    Q_PROPERTY(QPoint overlayPosition READ overlayPosition WRITE setOverlayPosition NOTIFY overlayPositionChanged)

    Q_PROPERTY(int penWidth READ penWidth WRITE setPenWidth NOTIFY configChanged)
    Q_PROPERTY(QColor penColor READ penColor WRITE setPenColor NOTIFY configChanged)
    Q_PROPERTY(double opacity READ opacity WRITE setOpacity NOTIFY configChanged)

    // --- TOOLS SPEZIFISCH ---
    Q_PROPERTY(int smoothing READ smoothing WRITE setSmoothing NOTIFY configChanged)
    Q_PROPERTY(bool pressureSensitivity READ pressureSensitivity WRITE setPressureSensitivity NOTIFY configChanged)

    Q_PROPERTY(int hardness READ hardness WRITE setHardness NOTIFY configChanged)
    Q_PROPERTY(bool tiltShading READ tiltShading WRITE setTiltShading NOTIFY configChanged)
    Q_PROPERTY(QString texture READ texture WRITE setTexture NOTIFY configChanged)

    Q_PROPERTY(bool smartLine READ smartLine WRITE setSmartLine NOTIFY configChanged)
    Q_PROPERTY(bool drawBehind READ drawBehind WRITE setDrawBehind NOTIFY configChanged)
    Q_PROPERTY(int tipType READ tipType WRITE setTipType NOTIFY configChanged)

    Q_PROPERTY(int eraserMode READ eraserMode WRITE setEraserMode NOTIFY configChanged)
    Q_PROPERTY(bool eraserKeepInk READ eraserKeepInk WRITE setEraserKeepInk NOTIFY configChanged)

    Q_PROPERTY(int lassoMode READ lassoMode WRITE setLassoMode NOTIFY configChanged)
    Q_PROPERTY(bool aspectLock READ aspectLock WRITE setAspectLock NOTIFY configChanged)

    Q_PROPERTY(double imageOpacity READ imageOpacity WRITE setImageOpacity NOTIFY configChanged)
    Q_PROPERTY(int imageFilter READ imageFilter WRITE setImageFilter NOTIFY configChanged)
    Q_PROPERTY(int borderWidth READ borderWidth WRITE setBorderWidth NOTIFY configChanged)
    Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor NOTIFY configChanged) // NEU

    Q_PROPERTY(int rulerUnit READ rulerUnit WRITE setRulerUnit NOTIFY configChanged)
    Q_PROPERTY(bool rulerSnap READ rulerSnap WRITE setRulerSnap NOTIFY configChanged)
    Q_PROPERTY(bool compassMode READ compassMode WRITE setCompassMode NOTIFY configChanged)

    Q_PROPERTY(int shapeType READ shapeType WRITE setShapeType NOTIFY configChanged)
    Q_PROPERTY(int shapeFill READ shapeFill WRITE setShapeFill NOTIFY configChanged)
    Q_PROPERTY(bool showGrid READ showGrid WRITE setShowGrid NOTIFY configChanged)
    Q_PROPERTY(bool showXAxis READ showXAxis WRITE setShowXAxis NOTIFY configChanged) // NEU
    Q_PROPERTY(bool showYAxis READ showYAxis WRITE setShowYAxis NOTIFY configChanged) // NEU

    Q_PROPERTY(QColor paperColor READ paperColor WRITE setPaperColor NOTIFY configChanged)
    Q_PROPERTY(bool collapsed READ collapsed WRITE setCollapsed NOTIFY configChanged)

    Q_PROPERTY(QString fontFamily READ fontFamily WRITE setFontFamily NOTIFY configChanged)
    Q_PROPERTY(bool bold READ bold WRITE setBold NOTIFY configChanged)
    Q_PROPERTY(bool italic READ italic WRITE setItalic NOTIFY configChanged)
    Q_PROPERTY(bool underline READ underline WRITE setUnderline NOTIFY configChanged)
    Q_PROPERTY(int fontSize READ fontSize WRITE setFontSize NOTIFY configChanged)
    Q_PROPERTY(int textAlignment READ textAlignment WRITE setTextAlignment NOTIFY configChanged)
    Q_PROPERTY(bool fillTextBackground READ fillTextBackground WRITE setFillTextBackground NOTIFY configChanged)
    Q_PROPERTY(QColor textBackgroundColor READ textBackgroundColor WRITE setTextBackgroundColor NOTIFY configChanged) // NEU

public:
    static ToolUIBridge& instance() {
        static ToolUIBridge _instance;
        return _instance;
    }

    // --- Core Getter/Setter ---
    int activeToolMode() const { return ToolManager::instance().activeTool() ? (int)ToolManager::instance().activeTool()->mode() : 0; }

    bool settingsVisible() const { return m_settingsVisible; }
    void setSettingsVisible(bool v) {
        if(m_settingsVisible != v) { m_settingsVisible = v; emit settingsVisibleChanged(); }
    }

    QPoint overlayPosition() const { return m_overlayPosition; }
    void setOverlayPosition(const QPoint& p) { m_overlayPosition = p; emit overlayPositionChanged(); }

    // --- Config Properties ---

    // Pen Width
    int penWidth() const { return ToolManager::instance().config().penWidth; }
    void setPenWidth(int v) {
        auto c = ToolManager::instance().config();
        if(c.penWidth != v) { c.penWidth = v; ToolManager::instance().updateConfig(c); emit configChanged(); }
    }

    // Pen Color
    QColor penColor() const { return ToolManager::instance().config().penColor; }
    void setPenColor(const QColor& v) {
        auto c = ToolManager::instance().config();
        if(c.penColor != v) { c.penColor = v; ToolManager::instance().updateConfig(c); emit configChanged(); }
    }

    // Opacity
    double opacity() const { return ToolManager::instance().config().opacity; }
    void setOpacity(double v) {
        auto c = ToolManager::instance().config();
        if(std::abs(c.opacity - v) > 0.001) { c.opacity = v; ToolManager::instance().updateConfig(c); emit configChanged(); }
    }

    // Smoothing
    int smoothing() const { return ToolManager::instance().config().smoothing; }
    void setSmoothing(int v) {
        auto c = ToolManager::instance().config();
        if(c.smoothing != v) { c.smoothing = v; ToolManager::instance().updateConfig(c); emit configChanged(); }
    }

    // Pressure Sensitivity
    bool pressureSensitivity() const { return ToolManager::instance().config().pressureSensitivity; }
    void setPressureSensitivity(bool v) {
        auto c = ToolManager::instance().config();
        if(c.pressureSensitivity != v) { c.pressureSensitivity = v; ToolManager::instance().updateConfig(c); emit configChanged(); }
    }

    // Hardness
    int hardness() const { return ToolManager::instance().config().hardness; }
    void setHardness(int v) {
        auto c = ToolManager::instance().config();
        if(c.hardness != v) { c.hardness = v; ToolManager::instance().updateConfig(c); emit configChanged(); }
    }

    // Tilt Shading
    bool tiltShading() const { return ToolManager::instance().config().tiltShading; }
    void setTiltShading(bool v) {
        auto c = ToolManager::instance().config();
        if(c.tiltShading != v) { c.tiltShading = v; ToolManager::instance().updateConfig(c); emit configChanged(); }
    }

    // Texture
    QString texture() const { return ToolManager::instance().config().texture; }
    void setTexture(const QString& v) {
        auto c = ToolManager::instance().config();
        if(c.texture != v) { c.texture = v; ToolManager::instance().updateConfig(c); emit configChanged(); }
    }

    // Smart Line
    bool smartLine() const { return ToolManager::instance().config().smartLine; }
    void setSmartLine(bool v) {
        auto c = ToolManager::instance().config();
        if(c.smartLine != v) { c.smartLine = v; ToolManager::instance().updateConfig(c); emit configChanged(); }
    }

    // Draw Behind
    bool drawBehind() const { return ToolManager::instance().config().drawBehind; }
    void setDrawBehind(bool v) {
        auto c = ToolManager::instance().config();
        if(c.drawBehind != v) { c.drawBehind = v; ToolManager::instance().updateConfig(c); emit configChanged(); }
    }

    // Tip Type
    int tipType() const { return (int)ToolManager::instance().config().tipType; }
    void setTipType(int v) {
        auto c = ToolManager::instance().config();
        if((int)c.tipType != v) { c.tipType = (HighlighterTip)v; ToolManager::instance().updateConfig(c); emit configChanged(); }
    }

    // Eraser Mode
    int eraserMode() const { return (int)ToolManager::instance().config().eraserMode; }
    void setEraserMode(int v) {
        auto c = ToolManager::instance().config();
        if((int)c.eraserMode != v) { c.eraserMode = (EraserMode)v; ToolManager::instance().updateConfig(c); emit configChanged(); }
    }

    // Eraser Keep Ink
    bool eraserKeepInk() const { return ToolManager::instance().config().eraserKeepInk; }
    void setEraserKeepInk(bool v) {
        auto c = ToolManager::instance().config();
        if(c.eraserKeepInk != v) { c.eraserKeepInk = v; ToolManager::instance().updateConfig(c); emit configChanged(); }
    }

    // Lasso Mode
    int lassoMode() const { return (int)ToolManager::instance().config().lassoMode; }
    void setLassoMode(int v) {
        auto c = ToolManager::instance().config();
        if((int)c.lassoMode != v) { c.lassoMode = (LassoMode)v; ToolManager::instance().updateConfig(c); emit configChanged(); }
    }

    // Aspect Lock
    bool aspectLock() const { return ToolManager::instance().config().aspectLock; }
    void setAspectLock(bool v) {
        auto c = ToolManager::instance().config();
        if(c.aspectLock != v) { c.aspectLock = v; ToolManager::instance().updateConfig(c); emit configChanged(); }
    }

    // Image Opacity
    double imageOpacity() const { return ToolManager::instance().config().imageOpacity; }
    void setImageOpacity(double v) {
        auto c = ToolManager::instance().config();
        if(std::abs(c.imageOpacity - v) > 0.001) { c.imageOpacity = v; ToolManager::instance().updateConfig(c); emit configChanged(); }
    }

    // Image Filter
    int imageFilter() const { return ToolManager::instance().config().imageFilter; }
    void setImageFilter(int v) {
        auto c = ToolManager::instance().config();
        if(c.imageFilter != v) { c.imageFilter = v; ToolManager::instance().updateConfig(c); emit configChanged(); }
    }

    // Border Width
    int borderWidth() const { return ToolManager::instance().config().borderWidth; }
    void setBorderWidth(int v) {
        auto c = ToolManager::instance().config();
        if(c.borderWidth != v) { c.borderWidth = v; ToolManager::instance().updateConfig(c); emit configChanged(); }
    }

    // Border Color (NEU)
    QColor borderColor() const { return ToolManager::instance().config().borderColor; }
    void setBorderColor(const QColor& v) {
        auto c = ToolManager::instance().config();
        if(c.borderColor != v) { c.borderColor = v; ToolManager::instance().updateConfig(c); emit configChanged(); }
    }

    // Ruler Unit
    int rulerUnit() const { return (int)ToolManager::instance().config().rulerUnit; }
    void setRulerUnit(int v) {
        auto c = ToolManager::instance().config();
        if((int)c.rulerUnit != v) { c.rulerUnit = (RulerUnit)v; ToolManager::instance().updateConfig(c); emit configChanged(); }
    }

    // Ruler Snap
    bool rulerSnap() const { return ToolManager::instance().config().rulerSnap; }
    void setRulerSnap(bool v) {
        auto c = ToolManager::instance().config();
        if(c.rulerSnap != v) { c.rulerSnap = v; ToolManager::instance().updateConfig(c); emit configChanged(); }
    }

    // Compass Mode
    bool compassMode() const { return ToolManager::instance().config().compassMode; }
    void setCompassMode(bool v) {
        auto c = ToolManager::instance().config();
        if(c.compassMode != v) { c.compassMode = v; ToolManager::instance().updateConfig(c); emit configChanged(); }
    }

    // Shape Type
    int shapeType() const { return (int)ToolManager::instance().config().shapeType; }
    void setShapeType(int v) {
        auto c = ToolManager::instance().config();
        if((int)c.shapeType != v) { c.shapeType = (ShapeType)v; ToolManager::instance().updateConfig(c); emit configChanged(); }
    }

    // Shape Fill
    int shapeFill() const { return ToolManager::instance().config().shapeFill; }
    void setShapeFill(int v) {
        auto c = ToolManager::instance().config();
        if(c.shapeFill != v) { c.shapeFill = v; ToolManager::instance().updateConfig(c); emit configChanged(); }
    }

    // Show Grid
    bool showGrid() const { return ToolManager::instance().config().showGrid; }
    void setShowGrid(bool v) {
        auto c = ToolManager::instance().config();
        if(c.showGrid != v) { c.showGrid = v; ToolManager::instance().updateConfig(c); emit configChanged(); }
    }

    // Show X Axis (NEU)
    bool showXAxis() const { return ToolManager::instance().config().showXAxis; }
    void setShowXAxis(bool v) {
        auto c = ToolManager::instance().config();
        if(c.showXAxis != v) { c.showXAxis = v; ToolManager::instance().updateConfig(c); emit configChanged(); }
    }

    // Show Y Axis (NEU)
    bool showYAxis() const { return ToolManager::instance().config().showYAxis; }
    void setShowYAxis(bool v) {
        auto c = ToolManager::instance().config();
        if(c.showYAxis != v) { c.showYAxis = v; ToolManager::instance().updateConfig(c); emit configChanged(); }
    }

    // Paper Color
    QColor paperColor() const { return ToolManager::instance().config().paperColor; }
    void setPaperColor(const QColor& v) {
        auto c = ToolManager::instance().config();
        if(c.paperColor != v) { c.paperColor = v; ToolManager::instance().updateConfig(c); emit configChanged(); }
    }

    // Collapsed
    bool collapsed() const { return ToolManager::instance().config().collapsed; }
    void setCollapsed(bool v) {
        auto c = ToolManager::instance().config();
        if(c.collapsed != v) { c.collapsed = v; ToolManager::instance().updateConfig(c); emit configChanged(); }
    }

    // Font Family
    QString fontFamily() const { return ToolManager::instance().config().fontFamily; }
    void setFontFamily(const QString& v) {
        auto c = ToolManager::instance().config();
        if(c.fontFamily != v) { c.fontFamily = v; ToolManager::instance().updateConfig(c); emit configChanged(); }
    }

    // Bold
    bool bold() const { return ToolManager::instance().config().bold; }
    void setBold(bool v) {
        auto c = ToolManager::instance().config();
        if(c.bold != v) { c.bold = v; ToolManager::instance().updateConfig(c); emit configChanged(); }
    }

    // Italic
    bool italic() const { return ToolManager::instance().config().italic; }
    void setItalic(bool v) {
        auto c = ToolManager::instance().config();
        if(c.italic != v) { c.italic = v; ToolManager::instance().updateConfig(c); emit configChanged(); }
    }

    // Underline
    bool underline() const { return ToolManager::instance().config().underline; }
    void setUnderline(bool v) {
        auto c = ToolManager::instance().config();
        if(c.underline != v) { c.underline = v; ToolManager::instance().updateConfig(c); emit configChanged(); }
    }

    // Font Size
    int fontSize() const { return ToolManager::instance().config().fontSize; }
    void setFontSize(int v) {
        auto c = ToolManager::instance().config();
        if(c.fontSize != v) { c.fontSize = v; ToolManager::instance().updateConfig(c); emit configChanged(); }
    }

    // Text Alignment
    int textAlignment() const { return ToolManager::instance().config().alignment; }
    void setTextAlignment(int v) {
        auto c = ToolManager::instance().config();
        if(c.alignment != v) { c.alignment = v; ToolManager::instance().updateConfig(c); emit configChanged(); }
    }

    // Fill Text Background
    bool fillTextBackground() const { return ToolManager::instance().config().fillTextBackground; }
    void setFillTextBackground(bool v) {
        auto c = ToolManager::instance().config();
        if(c.fillTextBackground != v) { c.fillTextBackground = v; ToolManager::instance().updateConfig(c); emit configChanged(); }
    }

    // Text Background Color (NEU)
    QColor textBackgroundColor() const { return ToolManager::instance().config().textBackgroundColor; }
    void setTextBackgroundColor(const QColor& v) {
        auto c = ToolManager::instance().config();
        if(c.textBackgroundColor != v) { c.textBackgroundColor = v; ToolManager::instance().updateConfig(c); emit configChanged(); }
    }

signals:
    void toolChanged();
    void configChanged();
    void settingsVisibleChanged();
    void overlayPositionChanged();

private:
    ToolUIBridge() {
        connect(&ToolManager::instance(), &ToolManager::toolChanged, this, &ToolUIBridge::toolChanged);
        connect(&ToolManager::instance(), &ToolManager::toolChanged, [this](AbstractTool* t){
            if(t) connect(t, &AbstractTool::requestSettingsMenu, this, [this](){ setSettingsVisible(!m_settingsVisible); });
        });
    }
    QPoint m_overlayPosition{100, 100};
    bool m_settingsVisible{false};
};
