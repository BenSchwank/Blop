#pragma once
#include "AbstractTool.h"
#include "RulerItem.h"
#include <QGraphicsScene>
#include <QGraphicsView>
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

        const bool havePreferred = !qIsNaN(preferredScenePos.x()) &&
                                   !qIsNaN(preferredScenePos.y());
        auto spawnFallback = [&]() -> QPointF {
            if (havePreferred)
                return preferredScenePos;
            const auto views = scene->views();
            if (!views.isEmpty() && views.first() && views.first()->viewport()) {
                auto *v = views.first();
                return v->mapToScene(v->viewport()->rect().center());
            }
            return scene->sceneRect().center();
        };

        auto visibleInAnyView = [&](RulerItem *r) -> bool {
            if (!r)
                return false;
            const QRectF bb = r->sceneBoundingRect();
            for (QGraphicsView *v : scene->views()) {
                if (!v || !v->viewport())
                    continue;
                const QRectF vis =
                    v->mapToScene(v->viewport()->rect()).boundingRect();
                if (vis.intersects(bb) || vis.contains(r->pos()))
                    return true;
            }
            return false;
        };

        RulerItem* ruler = findActiveRuler(scene);

        // Zielpunkt ist die Lineal-MITTE (nicht die linke obere Ecke), sonst
        // liegt das Lineal zur Haelfte neben dem Blatt.
        bool recenter = false;
        QPointF centerTarget;
        if (!ruler) {
            ruler = new RulerItem();
            ruler->setZValue(1000);
            scene->addItem(ruler);
            rememberActiveRuler(scene, ruler);
            recenter = true;
            centerTarget = spawnFallback();
        } else if (havePreferred) {
            // Always re-center into the current viewport on tool activate.
            recenter = true;
            centerTarget = preferredScenePos;
        } else if (!visibleInAnyView(ruler)) {
            recenter = true;
            centerTarget = spawnFallback();
        }

        ruler->setVisible(true);
        ruler->setZValue(1000);
        ruler->setConfig(config);
        if (recenter) {
            const QSizeF bs = ruler->bodySize();
            ruler->setPos(centerTarget - QPointF(bs.width() / 2.0, bs.height() / 2.0));
        }
        for (QGraphicsView *v : scene->views()) {
            if (v && v->viewport())
                v->viewport()->update();
        }

        return ruler;
    }

signals:
    void requestUpdate();
};
