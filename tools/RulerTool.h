#pragma once
#include "AbstractTool.h"
#include "RulerItem.h"
#include <QGraphicsScene>
#include <limits>

class RulerTool : public AbstractTool {
    Q_OBJECT
public:
    using AbstractTool::AbstractTool;

    ToolMode mode() const override { return ToolMode::Ruler; }
    QString name() const override { return "Lineal"; }
    QString iconName() const override { return "ruler"; }

    // Wird aufgerufen, wenn das Tool aktiviert wird.
    // Da wir hier keine Szene haben, verlassen wir uns auf ensureRulerExists,
    // welches vom View aufgerufen werden muss.
    void onActivated() override {
        emit requestUpdate();
    }

    void setConfig(const ToolConfig& config) override {
        AbstractTool::setConfig(config);
        emit requestUpdate();
    }

    // STATISCHE HELPER-FUNKTION:
    // Diese muss von CanvasView::setTool oder MultiPageNoteView::setToolMode aufgerufen werden!
    static RulerItem* ensureRulerExists(QGraphicsScene* scene, const ToolConfig& config,
                                        const QPointF& preferredScenePos = QPointF(std::numeric_limits<qreal>::quiet_NaN(),
                                                                                   std::numeric_limits<qreal>::quiet_NaN())) {
        if (!scene) return nullptr;

        RulerItem* ruler = nullptr;

        // 1. Suche nach existierendem Lineal
        for (QGraphicsItem* item : scene->items()) {
            if (item->type() == RulerItem::Type) {
                ruler = static_cast<RulerItem*>(item);
                break;
            }
        }

        // 2. Erstellen, falls nicht vorhanden
        if (!ruler) {
            ruler = new RulerItem();
            // Startposition: bevorzugt sichtbarer Viewport-Mittelpunkt, sonst Szenenmitte.
            QRectF r = scene->sceneRect();
            QPointF spawn = r.center();
            if (!qIsNaN(preferredScenePos.x()) && !qIsNaN(preferredScenePos.y()))
                spawn = preferredScenePos;
            ruler->setPos(spawn);
            ruler->setZValue(5000); // Über allem anderen liegen
            scene->addItem(ruler);
        }

        // 3. Konfigurieren und Anzeigen
        if (!ruler->isVisible() && !qIsNaN(preferredScenePos.x()) && !qIsNaN(preferredScenePos.y())) {
            ruler->setPos(preferredScenePos);
        }
        ruler->setVisible(true);
        ruler->setConfig(config);

        return ruler;
    }

signals:
    void requestUpdate();
};
