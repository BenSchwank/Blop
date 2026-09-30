#pragma once

#include <QIcon>
#include <QPainter>
#include <QPixmap>

/// Bubble only. Tray, window icon, rails and the open notch.
inline QPixmap assistantLogoPixmap(int side) {
    const QPixmap src(QStringLiteral(":/assistant/logo-mark.png"));
    QPixmap out(side, side);
    out.fill(Qt::transparent);
    if (src.isNull() || side <= 0)
        return out;
    const int pad = qMax(0, side / 16);
    const QPixmap scaled =
        src.scaled(side - 2 * pad, side - 2 * pad, Qt::KeepAspectRatio,
                   Qt::SmoothTransformation);
    QPainter painter(&out);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawPixmap((side - scaled.width()) / 2,
                       (side - scaled.height()) / 2, scaled);
    return out;
}

/// Full mark with the word, for the boot screen.
inline QPixmap assistantFullLogoPixmap(int width) {
    const QPixmap src(QStringLiteral(":/assistant/logo-full.jpg"));
    if (src.isNull() || width <= 0)
        return {};
    return src.scaledToWidth(width, Qt::SmoothTransformation);
}

inline QIcon assistantLogoIcon() {
    QIcon icon;
    for (int side : {16, 20, 24, 32, 48, 64, 128})
        icon.addPixmap(assistantLogoPixmap(side));
    return icon;
}
