#pragma once
#include <QColor>

namespace UIStyles {
// ─── Blop "Productive Dark" Palette ──────────────────────────────────────────
// Base layers (warm navy, not flat black)
static const QColor Background     = QColor("#0C0D17"); // App-Hintergrund
static const QColor Sidebar        = QColor("#13141F"); // Panels, Sidebar
static const QColor PageBackground = QColor("#1A1B2E"); // Elevated surfaces
static const QColor SceneBackground= QColor("#0C0D17"); // Canvas-Hintergrund
// Toolbar
static const QColor ToolbarBg     = QColor(28, 29, 50, 245); // #1C1D32 warm navy
static const QColor ToolbarBorder = QColor(120, 92, 252, 55); // subtle purple glow
// Accent (warmer / richer purple)
static const QColor Accent        = QColor("#7C5CFC");  // primary interactive
static const QColor AccentSubtle  = QColor(124, 92, 252, 40); // bg highlight
// Text
static const QColor Text          = QColor(230, 228, 255, 235); // #E6E4FF warm white
static const QColor TextSecondary = QColor(255, 255, 255, 128); // 50% white
} // namespace UIStyles
