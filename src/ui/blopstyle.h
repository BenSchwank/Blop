#pragma once

// v3.16.1: Unified Blop overlay/sheet design language.
//
// All in-window overlays (A4Layout, ColorPicker, PagesBar, TileContext,
// MorphTray, OverlayHost) and all top-level QDialog overlays (Settings,
// ProfileEditor, NewNote, GraphAxis, GraphQuickAction, GraphTangentX) share
// these helpers so the user gets one visual language across the whole app
// instead of every overlay inventing its own radii, borders, backdrop alpha
// and slide-in curve.
//
// Visual contract (v3.28.2 Notion/Obsidian + K/J mix):
//   - App shell / library: Obsidian-like dark nav + Notion-quiet light content
//   - Editor: NoteChrome (charcoal/light + blue #5B9DFF), no Blop purple
//   - Touch + mouse: min control height 40dp; hover + press states both present
//   - surface bg:      BlopTheme::surfaceBase() at ~94% alpha
//   - surface border:  quiet hairline (no accent glow)
//   - surface radius:  12 dp
//   - surface shadow:  soft lift
//   - backdrop desk:   ~38% black veil
//   - slide-in:        BlopMotion::kStandard OutCubic
//
// Use `BlopStyle::applySurface(card)` on any QFrame/QWidget that should look
// like a Blop sheet, `BlopStyle::segmentQss()` for chip/segmented controls
// (Neue Notiz, Settings, Seite & Notiz), and `BlopStyle::touchTargetMinDp()`
// for any primary tap target.

#include <QColor>
#include <QString>
#include <QWidget>

namespace BlopStyle {

// Canonical color literals (also exposed as named QStringLiterals for use in
// stylesheets that need to concatenate further selectors).
QColor surfaceBg();
QColor surfaceBorder();
QColor surfaceShadow();
QColor backdrop(bool forAndroid);
QColor accent();
QColor textPrimary();
QColor textSecondary();

// --- Notion paper / Obsidian shell (Desktop K, literal hex — never themed()) ---
// Paper stays light even when the app is in Dark mode. Obsidian chrome stays
// charcoal regardless of Light/Dark. Do not pass these through BlopTheme::themed().
QColor paperBg();          // #F7F7F5 dialogs/settings
QColor paperBgLibrary();   // #F5F5F5 library page
QColor paperInk();         // #1C1E24
QColor paperInkMuted();    // #6B6F76
QColor paperChipBg();      // #F0EFED
QColor paperRowBg();       // #FFFFFF row groups on paper
QColor obsidianBg();       // #1A1D24 chrome / elevated nav
QColor obsidianDesk();     // #16181E workspace (= sidebar)
QColor obsidianNav();      // #16181E library sidebar (dark anchor)
QColor obsidianText();     // #F2F2F2
QColor obsidianSheet();    // #22262F overflow / elevated sheet

int surfaceRadiusDp();

/// Minimum interactive height for primary controls (chips, toolbar slots,
/// search fields). Satisfies finger + stylus without bloating mouse UI.
int touchTargetMinDp();

/// Notion/Obsidian segmented chip control (Format, Light/Dark, Layout, …).
/// Soft accent fill + accent border when checked; works in Light and Dark.
/// Uses BlopTheme (library / Settings / Neue Notiz).
QString segmentQss();

/// Same chip language, but NoteChrome tokens (Seite & Notiz while editing).
/// Avoids Dark-BlopTheme text on a Light editor sheet (and vice versa).
QString noteSegmentQss();

/// Fixed paper chips (dark ink on Notion paper) — use on Settings / Neue Notiz /
/// Seite & Notiz paper panes regardless of app Dark mode.
QString paperSegmentQss();

/// Primary / secondary / input styles for Notion paper surfaces.
QString paperPrimaryButtonQss();
QString paperSecondaryButtonQss();
QString paperDestructiveButtonQss();
QString paperInputQss();

/// Force a widget (and its QPalette) to paint Notion paper so Dark-mode
/// Window=#000 does not show through transparent parents.
void paintPaperSurface(QWidget *w, const QString &objectName = QString());

/// Quiet icon/tool button: transparent idle, soft hover, press. For title-bar
/// and sidebar icon strips (mouse hover + touch press).
QString quietIconButtonQss(int radiusPx = 8);

/// In-window menu row (Overflow ⋯): touch-tall, hover + press, NoteChrome blue.
QString menuItemQss(bool destructive = false);

/// Hairline separator color for fixed Obsidian overflow sheet (not theme border).
QString obsidianSeparatorQss();

/// Quiet thin scrollbar for Notion paper lists (Neue Notiz tags, Settings pages).
QString paperScrollbarQss();

// Stylesheet snippet that styles a QFrame/QWidget with objectName == cardName
// as a Blop sheet surface. The caller is responsible for setting
// `card->setObjectName(cardName)` first.
QString surfaceStyle(const QString &cardName);

// v3.18.1: shared accent hover/press QSS for the transparent icon
// QToolButtons in the Android toolbar (menu, export, page manager). The
// block existed 3x identically in MainWindow::applyTheme().
QString toolButtonAccentQss(const QColor &accent, int radiusPx = 16);

// One-call helpers. Each applies a stylesheet + drop-shadow effect.
void applySurface(QWidget *card, const QString &objectName = QStringLiteral("BlopSurface"));
void applyBackdrop(QWidget *host, bool forAndroid = false);

// Installs a one-shot slide-in animation that runs on the next showEvent of
// `card`. The animation translates the card from (y + 12dp) to (y) over
// 220ms with OutCubic, plus a 140ms opacity fade via the windowOpacity
// property when `card` is a top-level (Window/Dialog) or via a
// QGraphicsOpacityEffect-free alpha path otherwise.
void installPresentAnim(QWidget *card);

// Same idea but explicitly anchored: instead of "translate +12dp -> 0",
// the card slides up from below by `slideDistanceDp` dp.
void installPresentAnimFromBottom(QWidget *card, int slideDistanceDp = 12);

} // namespace BlopStyle
