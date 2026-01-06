#pragma once
#include <QObject>
#include <QPoint>
#include "ToolManager.h"

class ToolUIBridge : public QObject {
    Q_OBJECT
    // Properties für QML
    Q_PROPERTY(int activeToolMode READ activeToolMode NOTIFY toolChanged)
    Q_PROPERTY(QPoint overlayPosition READ overlayPosition WRITE setOverlayPosition NOTIFY overlayPositionChanged)
    Q_PROPERTY(bool settingsVisible READ settingsVisible WRITE setSettingsVisible NOTIFY settingsVisibleChanged)

    // Konfigurations-Werte für das Binding im Overlay
    Q_PROPERTY(int penWidth READ penWidth WRITE setPenWidth NOTIFY configChanged)
    Q_PROPERTY(QColor penColor READ penColor WRITE setPenColor NOTIFY configChanged)
    Q_PROPERTY(double highlighterOpacity READ highlighterOpacity WRITE setHighlighterOpacity NOTIFY configChanged)
    // ... weitere Properties hier ergänzen

public:
    static ToolUIBridge& instance() {
        static ToolUIBridge _instance;
        return _instance;
    }

    int activeToolMode() const {
        return ToolManager::instance().activeTool() ? (int)ToolManager::instance().activeTool()->mode() : 0;
    }

    QPoint overlayPosition() const { return m_overlayPosition; }
    void setOverlayPosition(const QPoint& pos) {
        if (m_overlayPosition != pos) {
            m_overlayPosition = pos;
            emit overlayPositionChanged();
        }
    }

    bool settingsVisible() const { return m_settingsVisible; }
    void setSettingsVisible(bool visible) {
        if (m_settingsVisible != visible) {
            m_settingsVisible = visible;
            emit settingsVisibleChanged();
        }
    }

    // --- Getter/Setter für Config (Proxy zum ToolManager) ---
    int penWidth() const { return ToolManager::instance().config().penWidth; }
    void setPenWidth(int w) {
        auto conf = ToolManager::instance().config();
        if (conf.penWidth != w) {
            conf.penWidth = w;
            ToolManager::instance().updateConfig(conf);
            emit configChanged();
        }
    }

    QColor penColor() const { return ToolManager::instance().config().penColor; }
    void setPenColor(const QColor& c) {
        auto conf = ToolManager::instance().config();
        if (conf.penColor != c) {
            conf.penColor = c;
            ToolManager::instance().updateConfig(conf);
            emit configChanged();
        }
    }

    double highlighterOpacity() const { return ToolManager::instance().config().penColor.alphaF(); }
    void setHighlighterOpacity(double a) {
        auto conf = ToolManager::instance().config();
        // Nur Alpha ändern, Farbe beibehalten
        QColor c = conf.penColor;
        c.setAlphaF(a);
        conf.penColor = c;
        ToolManager::instance().updateConfig(conf);
        emit configChanged();
    }

signals:
    void toolChanged();
    void overlayPositionChanged();
    void settingsVisibleChanged();
    void configChanged();

private:
    ToolUIBridge() {
        // Verbinde ToolManager Signale mit dieser Bridge
        connect(&ToolManager::instance(), &ToolManager::toolChanged, this, &ToolUIBridge::toolChanged);

        // Wenn ein Tool "requestSettingsMenu" sendet (Second Tap)
        connect(&ToolManager::instance(), &ToolManager::toolChanged, [this](AbstractTool* tool){
            if(tool) {
                connect(tool, &AbstractTool::requestSettingsMenu, this, [this](){
                    setSettingsVisible(!m_settingsVisible);
                });
            }
        });
    }

    QPoint m_overlayPosition{100, 100};
    bool m_settingsVisible{false};
};
