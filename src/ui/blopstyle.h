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
// Visual contract (canonical):
//   - surface bg:      rgba(28, 30, 46, 0.94)
//   - surface border:  rgba(124, 92, 252, 0.22)  (accent-Lila hairline)
//   - surface radius:  18 dp
//   - surface shadow:  0 px,  12 px offset, 32 px blur, rgba(0,0,0,0.45)
//   - backdrop desk:   rgba(6,  8, 16, 0.50)
//   - backdrop droid:  rgba(6,  8, 16, 0.78)
//   - slide-in:        280 ms OutCubic, y +24 dp -> 0
//   - opacity-in:      200 ms OutCubic, 0 -> 1
//
// Use `BlopStyle::applySurface(card)` on any QFrame/QWidget that should look
// like a Blop sheet, `BlopStyle::applyBackdrop(host)` on the dim layer, and
// `BlopStyle::installPresentAnim(card)` to wire a one-shot slide-in to the
// next showEvent.

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

int surfaceRadiusDp();

// Stylesheet snippet that styles a QFrame/QWidget with objectName == cardName
// as a Blop sheet surface. The caller is responsible for setting
// `card->setObjectName(cardName)` first.
QString surfaceStyle(const QString &cardName);

// One-call helpers. Each applies a stylesheet + drop-shadow effect.
void applySurface(QWidget *card, const QString &objectName = QStringLiteral("BlopSurface"));
void applyBackdrop(QWidget *host, bool forAndroid = false);

// Installs a one-shot slide-in animation that runs on the next showEvent of
// `card`. The animation translates the card from (y + 24dp) to (y) over
// 280ms with OutCubic, plus a 200ms opacity fade via the windowOpacity
// property when `card` is a top-level (Window/Dialog) or via a
// QGraphicsOpacityEffect-free alpha path otherwise.
void installPresentAnim(QWidget *card);

// Same idea but explicitly anchored: instead of "translate +24dp -> 0",
// the card slides up from below by `slideDistanceDp` dp.
void installPresentAnimFromBottom(QWidget *card, int slideDistanceDp = 24);

} // namespace BlopStyle
