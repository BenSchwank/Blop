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
    static RulerItem* ensureRulerExists(QGraphicsScene* scene, const ToolConfig& config) {
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
            // Startposition: Mitte der Szene (oder Viewport-Mitte, wenn möglich)
            QRectF r = scene->sceneRect();
            ruler->setPos(r.center());
            ruler->setZValue(5000); // Über allem anderen liegen
            scene->addItem(ruler);
        }

        // 3. Konfigurieren und Anzeigen
        ruler->setVisible(true);
        ruler->setConfig(config);

        return ruler;
    }

signals:
    void requestUpdate();
};
