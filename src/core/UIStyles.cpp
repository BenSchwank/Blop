#include "UIStyles.h"

#include "blop_theme.h"

namespace UIStyles {

// Default values mirror the historical Dark palette so that any code that
// reads these BEFORE refresh() runs at startup still sees sensible values.
QColor Background     = QColor("#0C0D17");
QColor Sidebar        = QColor("#13141F");
QColor PageBackground = QColor("#1A1B2E");
QColor SceneBackground = QColor("#0C0D17");
QColor ToolbarBg     = QColor(28, 29, 50, 245);
QColor ToolbarBorder = QColor(120, 92, 252, 55);
QColor Accent        = QColor("#7C5CFC");
QColor AccentSubtle  = QColor(124, 92, 252, 40);
QColor Text          = QColor(230, 228, 255, 235);
QColor TextSecondary = QColor(255, 255, 255, 128);

void refresh() {
  Background      = BlopTheme::surfaceBackground();
  Sidebar         = BlopTheme::surfaceBase();
  PageBackground  = BlopTheme::surfaceElevated();
  SceneBackground = BlopTheme::surfaceBackground();
  // Toolbar surface keeps a slight alpha (frosted look) regardless of theme.
  {
    QColor base = BlopTheme::surfaceBase();
    base.setAlpha(245);
    ToolbarBg = base;
  }
  // Toolbar border follows the accent (subtle glow) -- works in both modes.
  {
    QColor a = BlopTheme::accentPrimary();
    a.setAlpha(55);
    ToolbarBorder = a;
  }
  Accent          = BlopTheme::accentPrimary();
  AccentSubtle    = BlopTheme::accentSubtle();
  Text            = BlopTheme::textPrimary();
  TextSecondary   = BlopTheme::textSecondary();
}

} // namespace UIStyles
