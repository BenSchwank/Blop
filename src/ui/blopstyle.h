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

// --- Light paper / Obsidian shell (literal hex — never themed()) ---
// Light Mode design tokens (CSS → Qt mapping):
//   --bg-app #F8FAFC          → paperBg()
//   --bg-surface #FFFFFF      → paperRowBg() / paperSurface()
//   --text-main #1E293B       → paperInk()
//   --text-muted #64748B      → paperInkMuted()
//   --border-color #E2E8F0    → paperBorder()
//   --color-primary-light     → paperPrimaryLight()  (#EFF6FF)
//   --color-primary           → BlopTheme::accentPrimary() / NoteChrome::accent()
//                               (#5B9DFF product accent; not raw #3B82F6 — keeps
//                               NoteChrome consistency across editor chrome)
//   --radius-lg / --radius-md → radiusLgDp() / radiusMdDp()
//   --backdrop rgba(15,23,42,0.4) → lightModalBackdrop()
// Paper stays light even when the app is in Dark mode. Obsidian chrome stays
// charcoal regardless of Light/Dark. Do not pass these through BlopTheme::themed().
QColor paperBg();            // #F8FAFC app/dialog desk
QColor paperBgLibrary();     // #F4F5F7 library page
QColor paperInk();           // #222222
QColor paperInkMuted();      // #6B6B6B
QColor paperChipBg();        // #EBEBEB quiet chip / hover cousin
QColor paperRowBg();         // #FFFFFF elevated row groups
QColor paperSurface();       // #FFFFFF modal/card surface alias
QColor paperBorder();        // #E3E3E3
QColor paperPrimaryLight();  // #EFF6FF active segment / nav fill
QColor paperHover();         // #EBEBEB menu/row hover
QColor lightModalBackdrop(); // rgba(15,23,42,0.4)
QColor obsidianBg();         // #23252A chrome / content
QColor obsidianDesk();       // #1A1916 workspace rail (= sidebar)
QColor obsidianNav();        // #1A1916 library sidebar (dark anchor)
QColor obsidianText();       // #F4F5F7
QColor obsidianSheet();      // #353840 overflow / elevated note cards
QColor obsidianContent();    // #23252A Dark library desk (cards float above)

/// Library shell: title bar, icon rail, sidebars, and the note list.
QColor libraryTitleBar();  // Chrome band: Dark #12141A / Light #1A1C1E
QColor libraryIconRail();  // Thin left rail — always mirrors libraryTitleBar()
QColor librarySidebar();   // Dark #1C1F28 / Light #F0F0F0 nav well
QColor libraryMain();      // Light: #FFFFFF note list

int surfaceRadiusDp();
int radiusLgDp(); // 10 — modals / large sheets (soft, not pill)
int radiusMdDp(); // 8  — cards / menus / chips

/// Minimum interactive height for primary controls (chips, toolbar slots,
/// search fields). Satisfies finger + stylus without bloating mouse UI.
int touchTargetMinDp();

/// Content inset from a pane or dialog edge.
int edgeInsetDp();
/// Gap between stacked sections.
int sectionGapDp();
/// Gap between neighboring controls, and the horizontal inset of narrow rails.
int controlGapDp();

/// Notion/Obsidian segmented chip control (Format, Light/Dark, Layout, …).
/// Soft accent fill + accent border when checked; works in Light and Dark.
/// Uses BlopTheme (library / Settings / Neue Notiz).
QString segmentQss();

/// Same chip language, but NoteChrome tokens (Seite & Notiz while editing).
/// Avoids Dark-BlopTheme text on a Light editor sheet (and vice versa).
QString noteSegmentQss();

/// Fixed paper chips (dark ink on Notion paper) — use on Settings / Neue Notiz /
/// Seite & Notiz paper panes regardless of app Dark mode.
/// Checked = primary-light fill + accent text/border.
QString paperSegmentQss();

/// Left-rail nav button for large Seite & Notiz modal (active = 3px left
/// accent border + primary-light bg + accent text).
QString paperSideNavQss();

/// Large theme tile (Hell/Dunkel) — 2px accent border when checked.
QString paperThemeTileQss();

/// Outlined secondary chip (Vorlagen / Importieren / Mehr).
QString paperOutlineChipQss();

/// Primary / secondary / input styles for Notion paper surfaces.
QString paperPrimaryButtonQss();
QString paperSecondaryButtonQss();
QString paperDestructiveButtonQss();
QString paperInputQss();

/// Light in-window menu frame + item QSS (white surface, text-main, hover slate).
QString paperMenuFrameQss();
QString paperMenuItemQss(bool destructive = false);
QString paperMenuSeparatorQss();

/// Force a widget (and its QPalette) to paint Notion paper so Dark-mode
/// Window=#000 does not show through transparent parents.
void paintPaperSurface(QWidget *w, const QString &objectName = QString());

/// Soft modal drop shadow (--shadow-modal).
void applyModalShadow(QWidget *card);

/// Quiet icon/tool button: transparent idle, soft hover, press. For title-bar
/// and sidebar icon strips (mouse hover + touch press).
/// minDp defaults to touchTargetMinDp(). Radius defaults to radiusMdDp().
QString quietIconButtonQss(int radiusPx = 8, int minDp = -1);

/// Framed charcoal chrome icon button (sidebar header + / collapse, explorer +).
/// darkChrome: light borders on dark title strip; else dark borders on light.
/// sizeDp defaults to touchTargetMinDp().
QString framedChromeIconButtonQss(bool darkChrome, int radiusPx = 8,
                                  int sizeDp = 40);

/// In-window menu row (Overflow ⋯): theme-aware (paper in Light, Obsidian in Dark).
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
