#include "blopassistantoverlay.h"

#include "uiscale.h"

#include <QEvent>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScreen>
#include <QToolButton>
#include <QVBoxLayout>

#ifdef BLOP_HAS_WEBENGINE
#include <QWebEnginePage>
#include <QtWebEngineWidgets/QWebEngineView>
#endif

namespace {

const char *kSpeechHtml = R"HTML(
<!DOCTYPE html><html><head><meta charset="utf-8"></head><body>
<script>
(function () {
  const Rec = window.SpeechRecognition || window.webkitSpeechRecognition;
  window.blopStart = function (lang) {
    if (!Rec) { console.log('BLOP_STT_ERR:unsupported'); return; }
    try {
      const r = new Rec();
      r.lang = lang || 'de-DE';
      r.interimResults = true;
      r.continuous = false;
      r.onresult = function (e) {
        let t = '';
        for (let i = e.resultIndex; i < e.results.length; i++)
          t += e.results[i][0].transcript;
        const last = e.results[e.results.length - 1];
        console.log((last.isFinal ? 'BLOP_STT_FINAL:' : 'BLOP_STT_INTERIM:') + t);
      };
      r.onerror = function (e) { console.log('BLOP_STT_ERR:' + (e.error || 'error')); };
      r.onend = function () { console.log('BLOP_STT_END'); };
      r.start();
    } catch (err) {
      console.log('BLOP_STT_ERR:' + (err && err.message ? err.message : 'start'));
    }
  };
})();
</script>
</body></html>
)HTML";

#ifdef BLOP_HAS_WEBENGINE
class AssistantSpeechPage : public QWebEnginePage {
public:
  explicit AssistantSpeechPage(BlopAssistantOverlay *owner)
      : QWebEnginePage(owner), m_owner(owner) {}

protected:
  void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel,
                                const QString &message, int,
                                const QString &) override {
    if (m_owner)
      m_owner->handleSttConsole(message);
  }

private:
  BlopAssistantOverlay *m_owner{nullptr};
};
#endif

QToolButton *makeIconBtn(QWidget *parent, const QString &tip) {
  auto *b = new QToolButton(parent);
  b->setCursor(Qt::PointingHandCursor);
  b->setToolTip(tip);
  b->setAutoRaise(true);
  b->setFocusPolicy(Qt::NoFocus);
  return b;
}

} // namespace

BlopAssistantOverlay::BlopAssistantOverlay(QWidget *host)
    : QWidget(host), m_host(host) {
  setObjectName(QStringLiteral("BlopAssistantOverlay"));
  setAttribute(Qt::WA_StyledBackground, true);
  setAttribute(Qt::WA_TranslucentBackground, true);
  buildUi();
  applyChrome();
  if (host)
    hide();
}

void BlopAssistantOverlay::setStandalone(bool on) {
  m_standalone = on;
  if (!on)
    return;
  setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                 Qt::Tool);
  setAttribute(Qt::WA_TranslucentBackground, true);
  setWindowTitle(QStringLiteral("Blop Assistant"));
  setExpanded(false);
  placeOnScreen();
  show();
  raise();
}

void BlopAssistantOverlay::buildUi() {
  auto *outer = new QVBoxLayout(this);
  outer->setContentsMargins(UiScale::dp(8), UiScale::dp(8), UiScale::dp(8),
                            UiScale::dp(8));
  outer->setSpacing(0);

  m_card = new QFrame(this);
  m_card->setObjectName(QStringLiteral("BlopAssistantCard"));
  outer->addWidget(m_card);

  auto *lay = new QVBoxLayout(m_card);
  lay->setContentsMargins(UiScale::dp(14), UiScale::dp(12), UiScale::dp(14),
                          UiScale::dp(12));
  lay->setSpacing(UiScale::dp(8));

  m_idleRow = new QWidget(m_card);
  auto *idle = new QHBoxLayout(m_idleRow);
  idle->setContentsMargins(0, 0, 0, 0);
  idle->setSpacing(UiScale::dp(10));
  m_orb = new QLabel(m_idleRow);
  m_orb->setObjectName(QStringLiteral("BlopAssistantOrb"));
  m_orb->setFixedSize(UiScale::dp(18), UiScale::dp(18));
  m_idleLabel = new QLabel(QStringLiteral("Blop  ·  klicken oder sprechen"),
                           m_idleRow);
  m_idleLabel->setObjectName(QStringLiteral("BlopAssistantIdle"));
  idle->addWidget(m_orb, 0);
  idle->addWidget(m_idleLabel, 1);
  lay->addWidget(m_idleRow);

  auto *top = new QHBoxLayout();
  top->setContentsMargins(0, 0, 0, 0);
  top->setSpacing(UiScale::dp(8));
  m_title = new QLabel(QStringLiteral("Blop Assistant"), m_card);
  m_title->setObjectName(QStringLiteral("BlopAssistantTitle"));
  m_closeBtn = makeIconBtn(m_card, QStringLiteral("Einklappen (Esc)"));
  m_closeBtn->setText(QStringLiteral("–"));
  m_closeBtn->setFixedSize(UiScale::dp(28), UiScale::dp(28));
  top->addWidget(m_title, 1);
  top->addWidget(m_closeBtn, 0);
  lay->addLayout(top);

  m_status = new QLabel(
      QStringLiteral("Sag oder tippe, was ich tun soll."), m_card);
  m_status->setObjectName(QStringLiteral("BlopAssistantStatus"));
  m_status->setWordWrap(true);
  lay->addWidget(m_status);

  auto *row = new QHBoxLayout();
  row->setContentsMargins(0, 0, 0, 0);
  row->setSpacing(UiScale::dp(8));
  m_input = new QLineEdit(m_card);
  m_input->setObjectName(QStringLiteral("BlopAssistantInput"));
  m_input->setPlaceholderText(
      QStringLiteral("neue Notiz Einkauf  ·  öffne YouTube  ·  starte Chrome"));
  m_micBtn = makeIconBtn(m_card, QStringLiteral("Mikrofon"));
  m_micBtn->setObjectName(QStringLiteral("BlopAssistantMic"));
  m_micBtn->setText(QStringLiteral("●"));
  m_micBtn->setFixedSize(UiScale::dp(36), UiScale::dp(36));
  m_sendBtn = makeIconBtn(m_card, QStringLiteral("Ausführen"));
  m_sendBtn->setObjectName(QStringLiteral("BlopAssistantSend"));
  m_sendBtn->setText(QStringLiteral("→"));
  m_sendBtn->setFixedSize(UiScale::dp(36), UiScale::dp(36));
  row->addWidget(m_input, 1);
  row->addWidget(m_micBtn, 0);
  row->addWidget(m_sendBtn, 0);
  lay->addLayout(row);

  m_examples = new QWidget(m_card);
  auto *ex = new QHBoxLayout(m_examples);
  ex->setContentsMargins(0, 0, 0, 0);
  ex->setSpacing(UiScale::dp(6));
  const QStringList chips = {QStringLiteral("Notiz"), QStringLiteral("Browser"),
                             QStringLiteral("App"), QStringLiteral("Hilfe")};
  for (const QString &c : chips) {
    auto *b = new QPushButton(c, m_examples);
    b->setCursor(Qt::PointingHandCursor);
    b->setFlat(true);
    b->setObjectName(QStringLiteral("BlopAssistantChip"));
    connect(b, &QPushButton::clicked, this, [this, c]() {
      if (c == QLatin1String("Hilfe")) {
        m_input->setText(QStringLiteral("Hilfe"));
        submitCurrent();
        return;
      }
      if (c == QLatin1String("Notiz"))
        m_input->setText(QStringLiteral("neue Notiz "));
      else if (c == QLatin1String("Browser"))
        m_input->setText(QStringLiteral("öffne "));
      else
        m_input->setText(QStringLiteral("starte "));
      m_input->setFocus();
      m_input->end(false);
    });
    ex->addWidget(b, 0);
  }
  ex->addStretch(1);
  lay->addWidget(m_examples);

  m_confirmBar = new QWidget(m_card);
  m_confirmBar->setObjectName(QStringLiteral("BlopAssistantConfirm"));
  auto *cf = new QHBoxLayout(m_confirmBar);
  cf->setContentsMargins(0, 0, 0, 0);
  cf->setSpacing(UiScale::dp(8));
  m_confirmLabel = new QLabel(m_confirmBar);
  m_confirmLabel->setObjectName(QStringLiteral("BlopAssistantConfirmLabel"));
  m_confirmLabel->setWordWrap(true);
  m_confirmYes = new QPushButton(QStringLiteral("Ausführen"), m_confirmBar);
  m_confirmYes->setObjectName(QStringLiteral("BlopAssistantConfirmYes"));
  m_confirmYes->setCursor(Qt::PointingHandCursor);
  m_confirmNo = new QPushButton(QStringLiteral("Abbrechen"), m_confirmBar);
  m_confirmNo->setObjectName(QStringLiteral("BlopAssistantConfirmNo"));
  m_confirmNo->setCursor(Qt::PointingHandCursor);
  cf->addWidget(m_confirmLabel, 1);
  cf->addWidget(m_confirmNo, 0);
  cf->addWidget(m_confirmYes, 0);
  m_confirmBar->hide();
  lay->addWidget(m_confirmBar);

  connect(m_sendBtn, &QToolButton::clicked, this,
          &BlopAssistantOverlay::submitCurrent);
  connect(m_input, &QLineEdit::returnPressed, this,
          &BlopAssistantOverlay::submitCurrent);
  connect(m_closeBtn, &QToolButton::clicked, this,
          [this]() { setExpanded(false); });
  connect(m_micBtn, &QToolButton::clicked, this, [this]() {
    if (m_listening)
      stopListening();
    else
      startListening();
  });
  connect(m_confirmYes, &QPushButton::clicked, this, [this]() {
    clearConfirm();
    emit confirmAccepted();
  });
  connect(m_confirmNo, &QPushButton::clicked, this, [this]() {
    clearConfirm();
    setStatus(QStringLiteral("Abgebrochen."));
    emit confirmRejected();
  });

  m_idleRow->installEventFilter(this);
  m_input->installEventFilter(this);
}

void BlopAssistantOverlay::applyChrome() {
  setStyleSheet(QStringLiteral(
      "QWidget#BlopAssistantOverlay { background: transparent; }"
      "QFrame#BlopAssistantCard {"
      "  background: #16131F;"
      "  border: 1px solid rgba(124,92,252,0.45);"
      "  border-radius: 22px;"
      "}"
      "QLabel#BlopAssistantOrb {"
      "  background: qlineargradient(x1:0,y1:0,x2:1,y2:1,"
      "    stop:0 #7C5CFC, stop:1 #5B9DFF);"
      "  border-radius: 9px;"
      "}"
      "QLabel#BlopAssistantIdle, QLabel#BlopAssistantTitle {"
      "  color: #F4F5FB; font-weight: 800; font-size: 13px; background: transparent;"
      "}"
      "QLabel#BlopAssistantIdle { font-weight: 650; color: rgba(244,245,251,0.88); }"
      "QLabel#BlopAssistantStatus, QLabel#BlopAssistantConfirmLabel {"
      "  color: rgba(200,196,230,0.86); font-size: 12px; background: transparent;"
      "}"
      "QLineEdit#BlopAssistantInput {"
      "  background: #211C30; color: #F4F5FB; border: 1px solid rgba(91,157,255,0.28);"
      "  border-radius: 14px; padding: 9px 12px; font-size: 13px;"
      "}"
      "QLineEdit#BlopAssistantInput:focus { border: 1px solid #5B9DFF; }"
      "QToolButton { color: #F4F5FB; background: transparent; border: none;"
      "  font-size: 16px; font-weight: 700; border-radius: 10px; }"
      "QToolButton:hover { background: rgba(124,92,252,0.22); }"
      "QToolButton#BlopAssistantMic { color: #5B9DFF; }"
      "QPushButton#BlopAssistantChip {"
      "  background: rgba(124,92,252,0.16); color: #EDE9FF; border: none;"
      "  border-radius: 11px; padding: 6px 11px; font-size: 11px; font-weight: 650;"
      "}"
      "QPushButton#BlopAssistantChip:hover { background: rgba(91,157,255,0.28); }"
      "QPushButton#BlopAssistantConfirmYes {"
      "  background: #5B9DFF; color: #0B1220; border: none; border-radius: 11px;"
      "  padding: 7px 12px; font-weight: 800;"
      "}"
      "QPushButton#BlopAssistantConfirmNo {"
      "  background: rgba(255,255,255,0.06); color: #EDE9FF; border: 1px solid #3A3550;"
      "  border-radius: 11px; padding: 7px 12px; font-weight: 650;"
      "}"));
}

void BlopAssistantOverlay::applyLayoutMode() {
  const bool exp = m_expanded;
  if (m_idleRow)
    m_idleRow->setVisible(!exp && m_standalone);
  if (m_title)
    m_title->setVisible(exp);
  if (m_closeBtn)
    m_closeBtn->setVisible(exp);
  if (m_status)
    m_status->setVisible(exp);
  if (m_input)
    m_input->setVisible(exp);
  if (m_micBtn)
    m_micBtn->setVisible(exp);
  if (m_sendBtn)
    m_sendBtn->setVisible(exp);
  if (m_examples)
    m_examples->setVisible(exp);
  if (m_confirmBar && !exp)
    m_confirmBar->hide();
}

void BlopAssistantOverlay::placeOnScreen() {
  QScreen *screen = QGuiApplication::primaryScreen();
  if (!screen)
    return;
  const QRect geo = screen->availableGeometry();
  const int w = m_expanded ? UiScale::dp(520) : UiScale::dp(280);
  const int h = m_expanded ? UiScale::dp(228) : UiScale::dp(56);
  const int x = geo.x() + (geo.width() - w) / 2;
  const int y = geo.y() + geo.height() - h - UiScale::dp(28);
  setFixedSize(w, h);
  move(x, y);
}

void BlopAssistantOverlay::reposition() {
  if (m_standalone) {
    placeOnScreen();
    show();
    raise();
    return;
  }
  if (!m_host)
    return;
  const int margin = UiScale::dp(16);
  const int w = qBound(UiScale::dp(320), int(m_host->width() * 0.52),
                       UiScale::dp(560));
  const int h = m_expanded ? UiScale::dp(188) : UiScale::dp(0);
  if (!m_expanded) {
    hide();
    return;
  }
  const int x = qMax(margin, (m_host->width() - w) / 2);
  const int y = qMax(margin, m_host->height() - h - UiScale::dp(28));
  setGeometry(x, y, w, h);
  show();
  raise();
}

void BlopAssistantOverlay::setExpanded(bool expanded) {
  m_expanded = expanded;
  if (!expanded)
    stopListening();
  applyLayoutMode();
  applyChrome();
  if (m_standalone) {
    placeOnScreen();
    show();
    raise();
    if (expanded)
      focusInput();
    return;
  }
  if (!expanded) {
    hide();
    return;
  }
  reposition();
  focusInput();
}

void BlopAssistantOverlay::toggleExpanded() { setExpanded(!m_expanded); }

void BlopAssistantOverlay::setStatus(const QString &text) {
  if (m_status)
    m_status->setText(text);
  if (m_idleLabel && !m_expanded)
    m_idleLabel->setText(text.isEmpty()
                             ? QStringLiteral("Blop  ·  klicken oder sprechen")
                             : text);
}

void BlopAssistantOverlay::setHeadline(const QString &text) {
  if (m_title)
    m_title->setText(text);
}

void BlopAssistantOverlay::focusInput() {
  if (m_input && m_expanded) {
    m_input->setFocus(Qt::OtherFocusReason);
    m_input->selectAll();
  }
}

void BlopAssistantOverlay::refreshChrome() {
  applyChrome();
  reposition();
}

void BlopAssistantOverlay::promptConfirm(const QString &prompt) {
  setExpanded(true);
  if (m_confirmLabel)
    m_confirmLabel->setText(prompt);
  if (m_confirmBar)
    m_confirmBar->show();
}

void BlopAssistantOverlay::clearConfirm() {
  if (m_confirmBar)
    m_confirmBar->hide();
}

void BlopAssistantOverlay::submitCurrent() {
  if (!m_input)
    return;
  const QString text = m_input->text().trimmed();
  if (text.isEmpty())
    return;
  stopListening();
  clearConfirm();
  emit utteranceSubmitted(text);
}

void BlopAssistantOverlay::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  QPainterPath path;
  path.addRoundedRect(rect().adjusted(4, 4, -4, -4), 22, 22);
  QLinearGradient g(0, 0, width(), height());
  g.setColorAt(0.0, QColor(124, 92, 252, 55));
  g.setColorAt(1.0, QColor(91, 157, 255, 40));
  p.fillPath(path, g);
}

void BlopAssistantOverlay::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton && m_standalone) {
    m_dragging = true;
    m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
    if (!m_expanded)
      setExpanded(true);
    event->accept();
    return;
  }
  QWidget::mousePressEvent(event);
}

void BlopAssistantOverlay::mouseMoveEvent(QMouseEvent *event) {
  if (m_dragging && (event->buttons() & Qt::LeftButton)) {
    move(event->globalPosition().toPoint() - m_dragOffset);
    event->accept();
    return;
  }
  QWidget::mouseMoveEvent(event);
}

void BlopAssistantOverlay::mouseReleaseEvent(QMouseEvent *event) {
  m_dragging = false;
  QWidget::mouseReleaseEvent(event);
}

void BlopAssistantOverlay::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape) {
    if (m_confirmBar && m_confirmBar->isVisible()) {
      m_confirmNo->click();
      event->accept();
      return;
    }
    setExpanded(false);
    event->accept();
    return;
  }
  QWidget::keyPressEvent(event);
}

bool BlopAssistantOverlay::eventFilter(QObject *obj, QEvent *event) {
  if (obj == m_idleRow && event->type() == QEvent::MouseButtonRelease) {
    setExpanded(true);
    return true;
  }
  if (obj == m_input && event->type() == QEvent::KeyPress) {
    auto *ke = static_cast<QKeyEvent *>(event);
    if (ke->key() == Qt::Key_Escape) {
      setExpanded(false);
      return true;
    }
  }
  return QWidget::eventFilter(obj, event);
}

void BlopAssistantOverlay::startListening() {
#ifdef BLOP_HAS_WEBENGINE
  ensureSpeechPage();
  m_listening = true;
  m_micBtn->setText(QStringLiteral("■"));
  setStatus(QStringLiteral("Ich höre zu…"));
  if (m_speechView && m_speechView->page()) {
    m_speechView->page()->runJavaScript(
        QStringLiteral("window.blopStart && window.blopStart('de-DE');"));
  }
#else
  setStatus(QStringLiteral(
      "Spracheingabe braucht Desktop-WebEngine — tippe den Befehl."));
#endif
}

void BlopAssistantOverlay::stopListening() {
  m_listening = false;
  if (m_micBtn)
    m_micBtn->setText(QStringLiteral("●"));
}

void BlopAssistantOverlay::handleSttConsole(const QString &message) {
  if (message.startsWith(QLatin1String("BLOP_STT_FINAL:"))) {
    const QString t = message.mid(14).trimmed();
    if (m_input)
      m_input->setText(t);
    setStatus(QStringLiteral("Verstanden."));
    submitCurrent();
    return;
  }
  if (message.startsWith(QLatin1String("BLOP_STT_INTERIM:"))) {
    if (m_input)
      m_input->setText(message.mid(16));
    setStatus(QStringLiteral("Ich höre zu…"));
    return;
  }
  if (message.startsWith(QLatin1String("BLOP_STT_ERR:"))) {
    m_listening = false;
    if (m_micBtn)
      m_micBtn->setText(QStringLiteral("●"));
    setStatus(QStringLiteral("Mikrofon: %1 — tippe den Befehl stattdessen.")
                  .arg(message.mid(12)));
    return;
  }
  if (message == QLatin1String("BLOP_STT_END")) {
    m_listening = false;
    if (m_micBtn)
      m_micBtn->setText(QStringLiteral("●"));
  }
}

#ifdef BLOP_HAS_WEBENGINE
void BlopAssistantOverlay::ensureSpeechPage() {
  if (m_speechView)
    return;
  m_speechView = new QWebEngineView(this);
  m_speechView->setFixedSize(1, 1);
  m_speechView->hide();
  auto *page = new AssistantSpeechPage(this);
  page->setParent(m_speechView);
  m_speechView->setPage(page);
  m_speechView->setHtml(QString::fromUtf8(kSpeechHtml));
}
#endif
