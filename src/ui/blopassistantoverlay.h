#pragma once

#include <QPoint>
#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QToolButton;
class QFrame;

#ifdef BLOP_HAS_WEBENGINE
class QWebEngineView;
#endif

// Floating VoiceOS-style pill with Blop chrome. Can sit on a host widget
// or run as a standalone always-on-top companion.
class BlopAssistantOverlay : public QWidget {
  Q_OBJECT
public:
  explicit BlopAssistantOverlay(QWidget *host = nullptr);

  void setStandalone(bool on);
  bool isStandalone() const { return m_standalone; }

  void toggleExpanded();
  void setExpanded(bool expanded);
  bool isExpanded() const { return m_expanded; }
  void reposition();
  void placeOnScreen();
  void setStatus(const QString &text);
  void setHeadline(const QString &text);
  void focusInput();
  void handleSttConsole(const QString &message);
  void refreshChrome();
  void promptConfirm(const QString &prompt);
  void clearConfirm();

signals:
  void utteranceSubmitted(const QString &text);
  void confirmAccepted();
  void confirmRejected();

public slots:
  void submitCurrent();

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  bool eventFilter(QObject *obj, QEvent *event) override;

private:
  void buildUi();
  void applyChrome();
  void startListening();
  void stopListening();
  void applyLayoutMode();
#ifdef BLOP_HAS_WEBENGINE
  void ensureSpeechPage();
#endif

  QWidget *m_host{nullptr};
  QFrame *m_card{nullptr};
  QWidget *m_idleRow{nullptr};
  QLabel *m_orb{nullptr};
  QLabel *m_idleLabel{nullptr};
  QLabel *m_title{nullptr};
  QLabel *m_status{nullptr};
  QLineEdit *m_input{nullptr};
  QToolButton *m_micBtn{nullptr};
  QToolButton *m_sendBtn{nullptr};
  QToolButton *m_closeBtn{nullptr};
  QWidget *m_examples{nullptr};
  QWidget *m_confirmBar{nullptr};
  QLabel *m_confirmLabel{nullptr};
  QPushButton *m_confirmYes{nullptr};
  QPushButton *m_confirmNo{nullptr};
  bool m_expanded{false};
  bool m_listening{false};
  bool m_standalone{false};
  bool m_dragging{false};
  QPoint m_dragOffset;

#ifdef BLOP_HAS_WEBENGINE
  QWebEngineView *m_speechView{nullptr};
#endif
};
