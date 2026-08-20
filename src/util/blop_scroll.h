#ifndef BLOP_SCROLL_H
#define BLOP_SCROLL_H

class QCoreApplication;
class QWidget;

namespace BlopScroll {

/// Finger / touch flick-scroll on any QAbstractScrollArea:
/// tap without moving → click the control under the finger;
/// tap and drag → scroll.
///
/// Safe to call more than once. Skips drawing canvases (QGraphicsView) so
/// ink / pan-zoom are unchanged. Text edits get touch-scroll only so mouse
/// drag still selects text.
void enableFingerScroll(QWidget *target);

/// Install an app-wide filter so every QAbstractScrollArea that is shown
/// (lists, settings, dialogs, chip rows, …) gets enableFingerScroll.
void installApplicationWide(QCoreApplication *app);

} // namespace BlopScroll

#endif // BLOP_SCROLL_H
