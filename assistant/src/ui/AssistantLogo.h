#pragma once

#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

inline void paintAssistantLogo(QPainter *painter, const QRectF &rect) {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(Qt::NoPen);

    const qreal radius = rect.width() * 0.28;
    QPainterPath tile;
    tile.addRoundedRect(rect.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
    painter->setBrush(QColor(0x3E, 0x7B, 0xFF));
    painter->drawPath(tile);

    painter->setBrush(Qt::white);
    const qreal barW = rect.width() * 0.52;
    const qreal barH = rect.height() * 0.13;
    const qreal barX = rect.center().x() - barW / 2.0;
    const qreal barY = rect.top() + rect.height() * 0.34;
    painter->drawRoundedRect(QRectF(barX, barY, barW, barH), barH / 2.0, barH / 2.0);

    const qreal stemW = rect.width() * 0.16;
    const qreal stemH = rect.height() * 0.24;
    const qreal stemX = rect.center().x() - stemW / 2.0;
    const qreal stemY = barY + barH * 0.42;
    painter->drawRoundedRect(QRectF(stemX, stemY, stemW, stemH), stemW / 2.0, stemW / 2.0);
    painter->restore();
}

inline QPixmap assistantLogoPixmap(int side) {
    QPixmap pixmap(side, side);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    const qreal pad = side * 0.06;
    paintAssistantLogo(&painter, QRectF(pad, pad, side - pad * 2.0, side - pad * 2.0));
    return pixmap;
}

inline QIcon assistantLogoIcon() {
    QIcon icon;
    for (int side : {16, 20, 24, 32, 48, 64})
        icon.addPixmap(assistantLogoPixmap(side));
    return icon;
}
