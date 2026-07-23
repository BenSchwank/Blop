#pragma once
#include "AbstractStrokeTool.h"
#include "StrokeItem.h"
#include "UIStyles.h"
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QPointer>
#include <QSet>

class EraserTool : public AbstractStrokeTool {
    Q_OBJECT
public:
    using AbstractStrokeTool::AbstractStrokeTool;
    ToolMode mode() const override { return ToolMode::Eraser; }
    QString name() const override { return "Radierer"; }
    QString iconName() const override { return "eraser"; }

    // --- MAUS-PRESS ---
    bool handleMousePress(QGraphicsSceneMouseEvent* event, QGraphicsScene* scene) override {
        if (!scene) return false;
        eraseAt(event->scenePos(), scene);
        return true;
    }

    // --- MAUS-MOVE ---
    bool handleMouseMove(QGraphicsSceneMouseEvent* event, QGraphicsScene* scene) override {
        eraseAt(event->scenePos(), scene);
        return true;
    }

    // --- MAUS-RELEASE ---
    bool handleMouseRelease(QGraphicsSceneMouseEvent*, QGraphicsScene*) override {
        return true;
    }

    // --- TABLET ---
    bool handleTabletEvent(QTabletEvent* event, const QPointF& scenePos) override {
        if (m_sceneRef) {
            eraseAt(scenePos, m_sceneRef);
        }
        return AbstractStrokeTool::handleTabletEvent(event, scenePos);
    }

protected:
    QPen createPen() const override { return QPen(Qt::white, m_config.penWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin); }
    StrokeItem::StrokeStyle strokeStyle() const override { return StrokeItem::Eraser; }

private:
    void eraseAt(QPointF pos, QGraphicsScene* scene) {
        if (!scene) return;

        // Radius
        double r = (m_config.eraserMode == EraserMode::Pixel) ? (m_config.penWidth / 2.0) : 5.0;

        // Bereich um die Maus
        QRectF rect(pos.x() - r, pos.y() - r, 2*r, 2*r);

        // v119 perf: bbox prefilter first (cheap O(log N) tree query),
        // then the precise IntersectsItemShape test only on the
        // candidates. On dense scenes the difference adds up - the
        // shape test allocates a QPainterPath per item.
        QList<QGraphicsItem*> items = scene->items(rect, Qt::IntersectsItemBoundingRect);

        // Radierer-Form (Kreis)
        QPainterPath eraserShape;
        eraserShape.addEllipse(pos, r, r);

        // Items die komplett gelöscht werden sollen – NACH der Schleife löschen,
        // nie während der Iteration (verhindert use-after-free / Absturz).
        // QSet for O(1) "already queued" lookup; the original QList::contains
        // was O(N) and dominated for large erase sweeps.
        QList<QGraphicsItem*> toDelete;
        QSet<QGraphicsItem*> toDeleteSet;

        for (QGraphicsItem* item : items) {
            // Bereits zum Löschen vorgemerkt – überspringen
            if (toDeleteSet.contains(item)) continue;

            // Precise hit test (replaces the IntersectsItemShape on
            // scene->items): only continue if the eraser ellipse
            // actually intersects the item's shape.
            if (!item->shape().intersects(item->mapFromScene(eraserShape))) continue;

            // Object eraser also removes tagged text / images / stickies.
            if (m_config.eraserMode == EraserMode::Object) {
                const QString tag = item->data(0).toString();
                if (tag == QLatin1String("text") ||
                    tag == QLatin1String("image") ||
                    tag == QLatin1String("sticky_note") ||
                    tag == QLatin1String("shape")) {
                    toDelete.append(item);
                    toDeleteSet.insert(item);
                    continue;
                }
            }

            // Wir bearbeiten nur GraphicsPathItems (unsere Striche)
            QGraphicsPathItem* pathItem = dynamic_cast<QGraphicsPathItem*>(item);
            if (!pathItem) continue;

            // Verhindere, dass der Radierer seinen eigenen visuellen Pfad löscht
            if (m_currentItem && pathItem == m_currentItem) continue;

            // FEATURE: "Nur Textmarker löschen"
            // Marker liegen auf Z <= 5 (DrawBehind=-10, Normal=5). Tinte >= 10.
            if (m_config.eraserKeepInk && pathItem->zValue() >= 10) continue;

            // --- MODUS 1: OBJEKT RADIERER (Ganzes Item weg) ---
            if (m_config.eraserMode == EraserMode::Object) {
                toDelete.append(item);
                toDeleteSet.insert(item);
            }

            // --- MODUS 2: PIXEL RADIERER (Schneiden) ---
            else {
                QPainterPath currentPath = pathItem->path();

                // FIX: "Linie wird zur Form"-Problem beheben
                // Wenn das Item noch ein Strich ist (hat einen Pen), müssen wir es
                // zuerst in eine Fläche (Outline) umwandeln.
                if (pathItem->pen().style() != Qt::NoPen) {
                    QPainterPathStroker stroker;
                    stroker.setWidth(pathItem->pen().widthF());
                    stroker.setCapStyle(pathItem->pen().capStyle());
                    stroker.setJoinStyle(pathItem->pen().joinStyle());
                    stroker.setMiterLimit(pathItem->pen().miterLimit());

                    // Erstelle die Umriss-Form des Striches
                    QPainterPath outline = stroker.createStroke(currentPath);

                    // Jetzt subtrahieren wir den Radierer von der Fläche
                    QPainterPath newPath = outline.subtracted(eraserShape);

                    // Das Item ist jetzt eine Fläche, kein Strich mehr!
                    pathItem->setPath(newPath);

                    // WICHTIG: Stift entfernen, dafür Pinsel setzen (mit der Farbe des alten Stifts)
                    pathItem->setBrush(pathItem->pen().brush());
                    pathItem->setPen(Qt::NoPen);
                }
                else {
                    // Das Item ist bereits eine Fläche (wurde schonmal radiert)
                    // Einfach weiter subtrahieren
                    QPainterPath newPath = currentPath.subtracted(eraserShape);
                    pathItem->setPath(newPath);
                }

                // Wenn nichts mehr übrig ist -> Item zum Löschen vormerken
                if (pathItem->path().isEmpty()) {
                    // Pressure-Punkte erst leeren wenn wir es wirklich löschen
                    StrokeItem* strokeItem = dynamic_cast<StrokeItem*>(pathItem);
                    if (strokeItem) strokeItem->setPoints({});
                    toDelete.append(item);
                    toDeleteSet.insert(item);
                }
            }
        }

        // Erst NACH der Schleife löschen – verhindert Absturz durch Zugriff auf
        // bereits gelöschte Pointer während der Iteration.
        for (QGraphicsItem* item : toDelete) {
            scene->removeItem(item);
            delete item;
        }
        if (!toDelete.isEmpty())
            emit contentModified();
    }
};
