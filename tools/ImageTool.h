#pragma once
#include "AbstractTool.h"
#include "blop_dialogs.h"
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

#ifdef Q_OS_ANDROID
        // QFileDialog is a top-level window and crashes Qt 6.10 Android.
        // Native content picker is a follow-up; swallow the press so the tool
        // doesn't fall through to drawing.
        Q_UNUSED(event);
        BlopDialogs::notify(
            QApplication::activeWindow(), QStringLiteral("Bild"),
            QStringLiteral("Bild-Import auf Android folgt in einem Update."));
        return true;
#else
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
#endif
    }
};
