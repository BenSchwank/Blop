#pragma once

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QToolButton;
class QFrame;
class QPropertyAnimation;

#ifdef BLOP_HAS_WEBENGINE
class QWebEngineView;
class QWebEnginePage;
#endif

// VoiceOS-style floating assistant: type or speak a command, Blop acts.
class BlopAssistantOverlay : public QWidget {
  Q_OBJECT
public:
  explicit BlopAssistantOverlay(QWidget *host);

  void toggleExpanded();
  void setExpanded(bool expanded);
  bool isExpanded() const { return m_expanded; }
  void reposition();
  void setStatus(const QString &text);
  void setHeadline(const QString &text);
  void focusInput();
  void handleSttConsole(const QString &message);
  void refreshChrome();

signals:
  void utteranceSubmitted(const QString &text);

public slots:
  void submitCurrent();

protected:
  void resizeEvent(QResizeEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  bool eventFilter(QObject *obj, QEvent *event) override;

private:
  void buildUi();
  void applyChrome();
  void startListening();
  void stopListening();
#ifdef BLOP_HAS_WEBENGINE
  void ensureSpeechPage();
#endif

  QWidget *m_host{nullptr};
  QFrame *m_card{nullptr};
  QLabel *m_title{nullptr};
  QLabel *m_status{nullptr};
  QLineEdit *m_input{nullptr};
  QToolButton *m_micBtn{nullptr};
  QToolButton *m_sendBtn{nullptr};
  QToolButton *m_closeBtn{nullptr};
  QWidget *m_examples{nullptr};
  bool m_expanded{false};
  bool m_listening{false};

#ifdef BLOP_HAS_WEBENGINE
  QWebEngineView *m_speechView{nullptr};
#endif
};
