#pragma once
#include "AbstractTool.h"
#include "RulerItem.h"
#include <QGraphicsScene>
#include <QHash>
#include <limits>

// Per-scene cache for the (at most one) RulerItem. Replaces the
// O(N) scene->items() walk that used to fire on every wheel /
// touch / move event in the views and stroke tools. The cache
// self-validates via QGraphicsItem::scene() so a removed ruler
// gracefully falls back to a single re-scan + repopulate.
namespace RulerCache {
    inline QHash<QGraphicsScene*, RulerItem*>& store() {
        static QHash<QGraphicsScene*, RulerItem*> s;
        return s;
    }
}

inline RulerItem* findActiveRuler(QGraphicsScene* scene) {
    if (!scene) return nullptr;
    auto& cache = RulerCache::store();
    auto it = cache.find(scene);
    if (it != cache.end()) {
        RulerItem* r = it.value();
        if (r && r->scene() == scene) return r;
        cache.erase(it);
    }
    for (QGraphicsItem* item : scene->items()) {
        if (item->type() == RulerItem::Type) {
            auto* r = static_cast<RulerItem*>(item);
            cache.insert(scene, r);
            return r;
        }
    }
    return nullptr;
}

inline void rememberActiveRuler(QGraphicsScene* scene, RulerItem* ruler) {
    if (!scene) return;
    if (ruler) RulerCache::store().insert(scene, ruler);
    else       RulerCache::store().remove(scene);
}

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

        RulerItem* ruler = findActiveRuler(scene);

        // Erstellen, falls nicht vorhanden
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
            rememberActiveRuler(scene, ruler);
        }

        // 3. Konfigurieren und Anzeigen
        // Important UX rule: once the ruler exists, keep its scene position.
        // Do not reposition it to viewport center when the tool is re-activated.
        ruler->setVisible(true);
        ruler->setConfig(config);

        return ruler;
    }

signals:
    void requestUpdate();
};
