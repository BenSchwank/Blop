#pragma once

#include <QColor>
#include <QString>

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

/// Farbwahl als Overlay; bei Abbruch false, sonst *color gesetzt.
bool showColorPickerOverlay(QWidget *parent, QColor *color,
                            const QString &title = QString());
