#pragma once

#include <QColor>
#include <QString>
#include <functional>

class QWidget;

struct A4LayoutDialogResult {
  bool accepted{false};
  int backgroundType{2};
  QColor paperColor{Qt::white};
};

/// Modales Overlay im Eltern-Widget (kein separates OS-Fenster).
bool showA4LayoutOverlay(QWidget *parent, const QString &windowTitle,
                         const QString &subtitle, int initialType,
                         const QColor &initialPaper, A4LayoutDialogResult *out);

/// Non-blocking Android-safe variant (no local event loop / exec).
void showA4LayoutOverlayAsync(
    QWidget *parent, const QString &windowTitle, const QString &subtitle,
    int initialType, const QColor &initialPaper,
    std::function<void(const A4LayoutDialogResult &)> onDone);

/// Farbwahl als Overlay; bei Abbruch false, sonst *color gesetzt.
bool showColorPickerOverlay(QWidget *parent, QColor *color,
                            const QString &title = QString());
