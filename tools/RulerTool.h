#pragma once
#include "AbstractTool.h"
#include "RulerItem.h"
#include <QGraphicsScene>

class RulerTool : public AbstractTool {
    Q_OBJECT
public:
    using AbstractTool::AbstractTool;

    ToolMode mode() const override { return ToolMode::Ruler; }
    QString name() const override { return "Lineal"; }
    QString iconName() const override { return "ruler"; }

    void onActivated() override {}

    void setConfig(const ToolConfig& config) override {
        AbstractTool::setConfig(config);
        emit requestUpdate();
    }

    static RulerItem* ensureRulerExists(QGraphicsScene* scene, const ToolConfig& config) {
        if (!scene) return nullptr;
        RulerItem* ruler = nullptr;
        for (QGraphicsItem* item : scene->items()) {
            if (item->type() == RulerItem::Type) {
                ruler = static_cast<RulerItem*>(item);
                break;
            }
        }
        if (!ruler) {
            ruler = new RulerItem();
            ruler->setPos(scene->sceneRect().center());
            ruler->setZValue(5000);
            scene->addItem(ruler);
        }
        ruler->setVisible(true);
        ruler->setConfig(config);
        return ruler;
    }

signals:
    void requestUpdate();
};
