#pragma once
#include <QObject>
#include <QPoint>
#include <QColor>
#include <QDebug>
#include "ToolManager.h"

class ToolUIBridge : public QObject {
    Q_OBJECT

    // Basis
    Q_PROPERTY(int activeToolMode READ activeToolMode NOTIFY toolChanged)
    Q_PROPERTY(bool settingsVisible READ settingsVisible WRITE setSettingsVisible NOTIFY settingsVisibleChanged)
    Q_PROPERTY(QPoint overlayPosition READ overlayPosition WRITE setOverlayPosition NOTIFY overlayPositionChanged)

    // Pen Properties
    Q_PROPERTY(int penWidth READ penWidth WRITE setPenWidth NOTIFY configChanged)
    Q_PROPERTY(QColor penColor READ penColor WRITE setPenColor NOTIFY configChanged)
    Q_PROPERTY(double opacity READ opacity WRITE setOpacity NOTIFY configChanged)
    Q_PROPERTY(int smoothing READ smoothing WRITE setSmoothing NOTIFY configChanged)
    Q_PROPERTY(bool pressureSensitivity READ pressureSensitivity WRITE setPressureSensitivity NOTIFY configChanged)

    // (Die anderen Properties lassen wir der Übersicht halber erst mal weg,
    // da wir Schritt für Schritt vorgehen, aber der Code muss kompilieren,
    // daher füge ich Dummy-Getter für die Overlay-Switch-Cases hinzu, falls nötig)

public:
    static ToolUIBridge& instance() {
        static ToolUIBridge _instance;
        return _instance;
    }

    int activeToolMode() const { return ToolManager::instance().activeTool() ? (int)ToolManager::instance().activeTool()->mode() : 0; }

    bool settingsVisible() const { return m_settingsVisible; }
    void setSettingsVisible(bool v) {
        if(m_settingsVisible != v) {
            m_settingsVisible = v;
            emit settingsVisibleChanged();
        }
    }

    QPoint overlayPosition() const { return m_overlayPosition; }
    void setOverlayPosition(const QPoint& p) { m_overlayPosition = p; emit overlayPositionChanged(); }

    // --- GETTER & SETTER ---

    // Width
    int penWidth() const { return ToolManager::instance().config().penWidth; }
    void setPenWidth(int v) {
        auto c = ToolManager::instance().config();
        if(c.penWidth != v) { c.penWidth = v; ToolManager::instance().updateConfig(c); emit configChanged(); }
    }

    // Color
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

    // Pressure
    bool pressureSensitivity() const { return ToolManager::instance().config().pressureSensitivity; }
    void setPressureSensitivity(bool v) {
        auto c = ToolManager::instance().config();
        if(c.pressureSensitivity != v) { c.pressureSensitivity = v; ToolManager::instance().updateConfig(c); emit configChanged(); }
    }

signals:
    void toolChanged();
    void configChanged();
    void settingsVisibleChanged();
    void overlayPositionChanged();

private slots:
    void onActiveToolChanged(AbstractTool* tool) {
        if (m_connectedTool) disconnect(m_connectedTool, &AbstractTool::requestSettingsMenu, this, nullptr);
        m_connectedTool = tool;
        if (m_connectedTool) {
            connect(m_connectedTool, &AbstractTool::requestSettingsMenu, this, [this](){
                setSettingsVisible(!m_settingsVisible);
            });
        }
        emit toolChanged();
    }

private:
    AbstractTool* m_connectedTool{nullptr};

    ToolUIBridge() {
        connect(&ToolManager::instance(), &ToolManager::toolChanged, this, &ToolUIBridge::onActiveToolChanged);
    }

    QPoint m_overlayPosition{100, 100};
    bool m_settingsVisible{false};
};
