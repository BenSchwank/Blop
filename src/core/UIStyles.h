#pragma once
#include <QColor>

// v3.17.1: UIStyles is now a thin theme-aware proxy over BlopTheme.
// The original namespace-level `static const QColor` constants are kept
// as variables that get refreshed whenever the theme changes, so the
// ~42 existing `UIStyles::Sidebar.name()` call sites pick up Dark/Light
// without any source edit. Refresh is wired in BlopTheme::install() and
// re-run on the themeChanged signal.
//
// Brand identity for the Accent stays the same in both Dark and Light
// modes (the user explicitly wanted that). Background/Sidebar/Text move.
namespace UIStyles {

extern QColor Background;        // App-Hintergrund
extern QColor Sidebar;           // Panels, Sidebar
extern QColor PageBackground;    // Elevated surfaces (cards)
extern QColor SceneBackground;   // Canvas-Hintergrund (same as Background)
extern QColor ToolbarBg;         // Toolbar surface
extern QColor ToolbarBorder;     // Toolbar 1-px border / glow
extern QColor Accent;            // Primary interactive color
extern QColor AccentSubtle;      // Background highlight tint
extern QColor Text;              // Primary text on dark/light surface
extern QColor TextSecondary;     // Muted secondary text

// Re-read all values from the currently selected BlopTheme. Cheap, called
// once at startup and on each Dark/Light/Accent change.
void refresh();

} // namespace UIStyles
