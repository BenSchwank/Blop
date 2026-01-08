#pragma once
#include <QObject>
#include <QInputEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsScene>
#include <QPainter>
#include "../ToolMode.h"
#include "../ToolSettings.h"

class AbstractTool : public QObject {
    Q_OBJECT

public:
    explicit AbstractTool(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~AbstractTool() = default;

    virtual ToolMode mode() const = 0;
    virtual QString name() const = 0;
    virtual QString iconName() const = 0;

    virtual void onActivated() {}
    virtual void onDeactivated() {}

    virtual bool handleMousePress(QGraphicsSceneMouseEvent* event, QGraphicsScene* scene) { return false; }
    virtual bool handleMouseMove(QGraphicsSceneMouseEvent* event, QGraphicsScene* scene) { return false; }
    virtual bool handleMouseRelease(QGraphicsSceneMouseEvent* event, QGraphicsScene* scene) { return false; }

    virtual bool handleTabletEvent(QTabletEvent* event, const QPointF& scenePos) { return false; }
    virtual void drawOverlay(QPainter* painter, const QRectF& rect) {}

    virtual void setConfig(const ToolConfig& config) { m_config = config; }
    ToolConfig config() const { return m_config; }

    // WICHTIG: Diese Methode muss public sein!
    void triggerSettingsMenu() {
        emit requestSettingsMenu();
    }

signals:
    void overlayChanged();
    void contentModified();
    void requestSettingsMenu();

protected:
    ToolConfig m_config;
};
