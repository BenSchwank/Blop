#pragma once

#include <QObject>
#include <QInputEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QPainter>
#include "../ToolMode.h"
#include "../ToolSettings.h"

class AbstractTool : public QObject {
    Q_OBJECT

public:
    explicit AbstractTool(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~AbstractTool() = default;

    // Identifikation
    virtual ToolMode mode() const = 0;
    virtual QString name() const = 0;
    virtual QString iconName() const = 0;

    // Aktivierung/Deaktivierung (Setup/Cleanup)
    virtual void onActivated() {}
    virtual void onDeactivated() {}

    // Input Handling (Jetzt mit scene Pointer!)
    virtual bool handleMousePress(QGraphicsSceneMouseEvent* event, QGraphicsScene* scene) { return false; }
    virtual bool handleMouseMove(QGraphicsSceneMouseEvent* event, QGraphicsScene* scene) { return false; }
    virtual bool handleMouseRelease(QGraphicsSceneMouseEvent* event, QGraphicsScene* scene) { return false; }

    // Für Tablet/Stift-Events (Druckstärke, Neigung)
    virtual bool handleTabletEvent(QTabletEvent* event, const QPointF& scenePos) { return false; }

    // Overlay Rendering
    virtual void drawOverlay(QPainter* painter, const QRectF& rect) {}

    // Konfiguration
    virtual void setConfig(const ToolConfig& config) { m_config = config; }
    ToolConfig config() const { return m_config; }

signals:
    void overlayChanged();
    void contentModified();
    void requestSettingsMenu();

protected:
    ToolConfig m_config;
};
