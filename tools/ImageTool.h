#pragma once
#include "AbstractTool.h"
#include "ImagePlacement.h"
#include "blop_dialogs.h"
#include <QGraphicsPixmapItem>
#include <QFileDialog>
#include <QApplication>
#include <QPointer>

#ifdef Q_OS_ANDROID
#include "androidcontentpicker.h"
#endif

class ImageTool : public AbstractTool {
    Q_OBJECT
public:
    using AbstractTool::AbstractTool;

    ToolMode mode() const override { return ToolMode::Image; }
    QString name() const override { return "Bild"; }
    QString iconName() const override { return "image"; }

    /// Place an already-loaded pixmap (tests / non-dialog callers).
    QGraphicsPixmapItem *placePixmap(QGraphicsScene *scene, const QPixmap &pixmap,
                                     const QPointF &pos) {
        if (!scene)
            return nullptr;
        auto *item = blopCreateImageItem(pixmap, pos, m_config);
        if (!item)
            return nullptr;
        scene->addItem(item);
        m_lastCompletedItem = item;
        emit contentModified();
        return item;
    }

    bool handleMousePress(QGraphicsSceneMouseEvent* event, QGraphicsScene* scene) override {
        if (!scene) return false;

#ifdef Q_OS_ANDROID
        const QPointF pos = event->scenePos();
        QPointer<QGraphicsScene> safeScene(scene);
        QPointer<ImageTool> self(this);
        AndroidContentPicker::instance().pickOpen(
            {QStringLiteral("image/*")},
            [safeScene, pos, self](const QString &path) {
                if (path.isEmpty() || !safeScene)
                    return;
                QPixmap pixmap(path);
                if (pixmap.isNull()) {
                    BlopDialogs::notify(
                        QApplication::activeWindow(), QStringLiteral("Bild"),
                        QStringLiteral("Bild konnte nicht geladen werden."));
                    return;
                }
                if (self)
                    self->placePixmap(safeScene, pixmap, pos);
            });
        return true;
#else
        QString fileName = QFileDialog::getOpenFileName(nullptr, "Bild öffnen", "", "Bilder (*.png *.jpg *.jpeg)");

        if (!fileName.isEmpty()) {
            QPixmap pixmap(fileName);
            if (!pixmap.isNull())
                placePixmap(scene, pixmap, event->scenePos());
        }
        return true;
#endif
    }
};
