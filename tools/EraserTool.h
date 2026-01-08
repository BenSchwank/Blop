#pragma once
#include "AbstractStrokeTool.h"
#include "StrokeItem.h"
#include "../UIStyles.h"
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QPainterPath>
#include <QPainterPathStroker>

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

protected:
    QPen createPen() const override { return QPen(); }

private:
    void eraseAt(QPointF pos, QGraphicsScene* scene) {
        if (!scene) return;

        // Radius
        double r = (m_config.eraserMode == EraserMode::Pixel) ? (m_config.penWidth / 2.0) : 5.0;

        // Bereich um die Maus
        QRectF rect(pos.x() - r, pos.y() - r, 2*r, 2*r);

        // Suche Items, die den Radierer berühren
        QList<QGraphicsItem*> items = scene->items(rect, Qt::IntersectsItemShape);

        // Radierer-Form (Kreis)
        QPainterPath eraserShape;
        eraserShape.addEllipse(pos, r, r);

        for (QGraphicsItem* item : items) {
            // Wir bearbeiten nur GraphicsPathItems (unsere Striche)
            QGraphicsPathItem* pathItem = dynamic_cast<QGraphicsPathItem*>(item);

            if (pathItem) {
                // FEATURE: "Nur Textmarker löschen"
                // Marker liegen auf Z <= 5 (DrawBehind=-10, Normal=5). Tinte >= 10.
                if (m_config.eraserKeepInk && pathItem->zValue() >= 10) {
                    continue;
                }

                // --- MODUS 1: OBJEKT RADIERER (Ganzes Item weg) ---
                if (m_config.eraserMode == EraserMode::Object) {
                    scene->removeItem(item);
                    delete item;
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

                    // Wenn nichts mehr übrig ist -> Item ganz löschen
                    if (pathItem->path().isEmpty()) {
                        scene->removeItem(item);
                        delete item;
                    }
                }
            }
        }
    }
};
