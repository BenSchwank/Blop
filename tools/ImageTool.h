#pragma once
#include "AbstractTool.h"
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
                if (pixmap.width() > 800)
                    pixmap = pixmap.scaledToWidth(800, Qt::SmoothTransformation);
                auto *item = new QGraphicsPixmapItem(pixmap);
                item->setPos(pos);
                item->setFlags(QGraphicsItem::ItemIsSelectable |
                               QGraphicsItem::ItemIsMovable);
                item->setZValue(5);
                if (self) {
                  const qreal op = self->config().imageOpacity > 0.01
                                       ? self->config().imageOpacity
                                       : self->config().opacity;
                  item->setOpacity(qBound(0.1, op, 1.0));
                }
                safeScene->addItem(item);
                if (self)
                    emit self->contentModified();
            });
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
                const qreal op = m_config.imageOpacity > 0.01
                                     ? m_config.imageOpacity
                                     : m_config.opacity;
                item->setOpacity(qBound(0.1, op, 1.0));
                scene->addItem(item);

                emit contentModified();
            }
        }
        return true;
#endif
    }
};
