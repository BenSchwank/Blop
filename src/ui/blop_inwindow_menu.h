#pragma once

#include <QIcon>
#include <QList>
#include <QPoint>
#include <QString>
#include <functional>

class QWidget;

/// In-window menu replacement for QMenu. Used on Android where QMenu's
/// implicit top-level QWindow triggers
///   "Failed to acquire deadlock protector for QAndroidPlatformOpenGLWindow"
/// when a background accessibility service holds the EGL surface (Qt 6.10
/// + Android 16). Backend: a QFrame child of `anchor->window()` with no
/// platform-level window — therefore no EGL surface allocation.
namespace BlopInWindowMenu {

struct Item {
  QString label;
  QIcon icon;
  std::function<void()> handler;
  /// Red label for destructive actions (delete, etc.).
  bool destructive = false;
  /// If true, label/icon/handler are ignored; a horizontal divider is drawn.
  bool separator = false;
};

/// Show a menu containing `items` anchored at `anchorGlobal`. The menu is
/// clamped to fit within `anchor->window()`'s rect. A semi-transparent
/// backdrop intercepts touches outside the menu rect and closes it.
void show(QWidget *anchor, const QPoint &anchorGlobal,
          const QList<Item> &items);

} // namespace BlopInWindowMenu
