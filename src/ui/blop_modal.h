#pragma once

#include <QPointer>
#include <QWidget>

class QPropertyAnimation;
class QFrame;
class QDialog;

/// Reusable, theme-aware in-window modal sheet for Blop. Renders a backdrop
/// scrim over the parent window and a content card / bottom sheet that
/// animates in. Designed to replace top-level `QDialog` usage on Android
/// (where any top-level QWindow can trip the EGL deadlock seen in v3.16.x)
/// and to give desktop a consistent, animated overlay.
///
/// Display modes:
///   * Mode::Card        - centered, rounded card. Default on desktop.
///   * Mode::BottomSheet - slides up from the bottom edge, rounded top
///                         corners only, drag-down-to-dismiss. Default on
///                         Android phones.
///   * Mode::SideSheet   - v3.17.3: slides in from the right edge,
///                         rounded left corners only. Used by wide
///                         dialogs like Settings on tablet/desktop.
///   * Mode::Auto        - BottomSheet on Android phone, Card otherwise.
///
/// API:
///   QWidget *content = new MyForm(this);
///   auto *modal = BlopModal::present(this, content);
///   connect(modal, &BlopModal::dismissed, content, &QObject::deleteLater);
///
/// The returned BlopModal takes ownership of `content` (reparents it).
/// Outside-tap on the backdrop, ESC key (desktop) or drag-down (sheet mode)
/// triggers dismiss with reverse animation, then deleteLater().
class BlopModal : public QWidget {
  Q_OBJECT
public:
  enum class Mode { Auto, Card, BottomSheet, SideSheet };
  Q_ENUM(Mode)

  static BlopModal *present(QWidget *parent, QWidget *content,
                            Mode mode = Mode::Auto,
                            const QString &accessibleTitle = QString());

  /// v3.17.2: synchronous-feel replacement for QDialog::exec() that wraps
  /// any QDialog inside a BlopModal. The dialog is reparented (loses its
  /// top-level QWindow, avoiding the v3.16.x EGL deadlock path on
  /// Android), then a local QEventLoop blocks until the dialog emits
  /// `finished(int)` OR the user dismisses the modal. Returns the
  /// QDialog::DialogCode value (Accepted, Rejected, or any custom code
  /// the dialog passed to done()).
  ///
  /// Use only in code paths that today read `if (dlg.exec() == Accepted)`
  /// and where converting to async would require reshuffling the caller.
  /// For pure async usage prefer `present()` + finished/dismissed
  /// signals.
  static int execBlocking(QWidget *parent, QDialog *dlg,
                          Mode mode = Mode::Auto);

  void dismiss();

  /// Override the default card width on desktop. Ignored in BottomSheet mode.
  void setPreferredCardWidth(int px);

signals:
  void aboutToDismiss();
  void dismissed();

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;

private:
  BlopModal(QWidget *parent, QWidget *content, Mode mode,
            const QString &accessibleTitle);

  void layoutContent();
  void startOpenAnim();
  void startDismissAnim();
  void applyTheme();
  void onParentResized();
  Mode resolveMode(Mode requested) const;

  QWidget *m_content{nullptr};
  QFrame *m_card{nullptr};
  QWidget *m_dragHandle{nullptr}; // BottomSheet only
  QPropertyAnimation *m_backdropAnim{nullptr};
  QPropertyAnimation *m_cardAnim{nullptr};
  Mode m_mode{Mode::Auto};
  int m_preferredCardWidth{520};
  bool m_dismissing{false};
  bool m_dragging{false};
  QPoint m_dragStart;
  int m_dragOffset{0};
  QPointer<QObject> m_parentFilterTarget;
};
