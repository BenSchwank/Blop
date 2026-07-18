#pragma once

// Shared Drawboard-inspired note-chrome palette (neutral charcoal, cool accent).
// Kept separate from the global Blop purple library theme so the editor can
// feel like Drawboard PDF while the rest of the app stays on-brand.

#include <QColor>

namespace NoteChrome {

inline QColor canvasBg() { return QColor(42, 42, 42); }       // #2A2A2A
inline QColor panelBg() { return QColor(30, 30, 30); }        // #1E1E1E
inline QColor panelElevated() { return QColor(38, 38, 38); }  // #262626
inline QColor border() { return QColor(64, 64, 64); }         // #404040
inline QColor borderSoft() { return QColor(55, 55, 55); }
inline QColor textPrimary() { return QColor(232, 232, 232); }
inline QColor textSecondary() { return QColor(180, 180, 180); }
inline QColor accent() { return QColor(91, 157, 255); }       // Drawboard-ish blue
inline QColor accentSoft() { return QColor(91, 157, 255, 40); }
inline QColor toolbarFill() { return QColor(36, 36, 36); }
inline QColor toolbarFillEnd() { return QColor(28, 28, 28); }

} // namespace NoteChrome
