#ifndef BLOP_SCROLL_H
#define BLOP_SCROLL_H

class QAbstractItemView;
class QCoreApplication;
class QWidget;

namespace BlopScroll {

/// Finger / touch flick-scroll on any QAbstractScrollArea:
/// tap without moving → click the control under the finger;
/// tap and drag → scroll the enclosing scrollable (even if the press
/// started on a button, chip, or list row).
///
/// Safe to call more than once. Skips drawing canvases (QGraphicsView) so
/// ink / pan-zoom are unchanged. Text edits and sliders keep native drag.
void enableFingerScroll(QWidget *target);

/// Size an item-view to its rows and turn off inner scrollbars so an
/// outer QScrollArea owns the flick (avoids nested scrollers).
void makeListFitContents(QAbstractItemView *view);

/// Install an app-wide filter so every QAbstractScrollArea that is shown
/// (lists, settings, dialogs, chip rows, …) gets enableFingerScroll, and
/// so press-drag on descendant controls scrolls instead of sticking.
void installApplicationWide(QCoreApplication *app);

/// Set on a QWidget (or ancestor) to opt out of app-wide finger-scroll while
/// the subtree needs custom drag gestures (e.g. dashboard edit mode).
inline constexpr const char *kNoFingerScrollProperty = "blopNoFingerScroll";

} // namespace BlopScroll

#endif // BLOP_SCROLL_H
