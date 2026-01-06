#pragma once
#include "AbstractTool.h"
#include <QGraphicsPixmapItem>
#include <QFileDialog>
#include <QApplication>

class ImageTool : public AbstractTool {
    Q_OBJECT
public:
    using AbstractTool::AbstractTool;

    ToolMode mode() const override { return ToolMode::Image; }
    QString name() const override { return "Bild"; }
    QString iconName() const override { return "image"; }

    bool handleMousePress(QGraphicsSceneMouseEvent* event, QGraphicsScene* scene) override {
        if (!scene) return false;

        QString fileName = QFileDialog::getOpenFileName(nullptr, "Bild öffnen", "", "Bilder (*.png *.jpg *.jpeg)");

        if (!fileName.isEmpty()) {
            QPixmap pixmap(fileName);
            if (!pixmap.isNull()) {
                if (pixmap.width() > 800) pixmap = pixmap.scaledToWidth(800, Qt::SmoothTransformation);

                auto* item = new QGraphicsPixmapItem(pixmap);
                item->setPos(event->scenePos());
                item->setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable);
                item->setZValue(5);
                scene->addItem(item);

                emit contentModified();
            }
        }
        return true;
    }
};
