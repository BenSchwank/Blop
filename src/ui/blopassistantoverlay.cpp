#include "blopassistantoverlay.h"

#include "notechrome.h"
#include "uiscale.h"

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

#ifdef BLOP_HAS_WEBENGINE
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
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
  hide();
  buildUi();
  applyChrome();
}

void BlopAssistantOverlay::buildUi() {
  auto *outer = new QVBoxLayout(this);
  outer->setContentsMargins(0, 0, 0, 0);
  outer->setSpacing(0);

  m_card = new QFrame(this);
  m_card->setObjectName(QStringLiteral("BlopAssistantCard"));
  outer->addWidget(m_card);

  auto *lay = new QVBoxLayout(m_card);
  lay->setContentsMargins(UiScale::dp(14), UiScale::dp(12), UiScale::dp(14),
                          UiScale::dp(12));
  lay->setSpacing(UiScale::dp(8));

  auto *top = new QHBoxLayout();
  top->setContentsMargins(0, 0, 0, 0);
  top->setSpacing(UiScale::dp(8));
  m_title = new QLabel(QStringLiteral("Blop Assistant"), m_card);
  m_title->setObjectName(QStringLiteral("BlopAssistantTitle"));
  m_closeBtn = makeIconBtn(m_card, QStringLiteral("Schließen (Esc)"));
  m_closeBtn->setText(QStringLiteral("×"));
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
      QStringLiteral("neue Notiz Einkauf  ·  öffne Physik  ·  Hilfe"));
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
  const QStringList chips = {QStringLiteral("neue Notiz"),
                             QStringLiteral("öffne …"),
                             QStringLiteral("suche …"),
                             QStringLiteral("Hilfe")};
  for (const QString &c : chips) {
    auto *b = new QPushButton(c, m_examples);
    b->setCursor(Qt::PointingHandCursor);
    b->setFlat(true);
    b->setObjectName(QStringLiteral("BlopAssistantChip"));
    connect(b, &QPushButton::clicked, this, [this, c]() {
      if (c.startsWith(QStringLiteral("Hilfe"))) {
        m_input->setText(QStringLiteral("Hilfe"));
        submitCurrent();
        return;
      }
      if (c.startsWith(QStringLiteral("neue"))) {
        m_input->setText(QStringLiteral("neue Notiz "));
      } else if (c.startsWith(QStringLiteral("öffne"))) {
        m_input->setText(QStringLiteral("öffne "));
      } else {
        m_input->setText(QStringLiteral("suche "));
      }
      m_input->setFocus();
    });
    ex->addWidget(b, 0);
  }
  ex->addStretch(1);
  lay->addWidget(m_examples);

  connect(m_sendBtn, &QToolButton::clicked, this,
          &BlopAssistantOverlay::submitCurrent);
  connect(m_input, &QLineEdit::returnPressed, this,
          &BlopAssistantOverlay::submitCurrent);
  connect(m_closeBtn, &QToolButton::clicked, this, [this]() { setExpanded(false); });
  connect(m_micBtn, &QToolButton::clicked, this, [this]() {
    if (m_listening)
      stopListening();
    else
      startListening();
  });

  m_input->installEventFilter(this);
}

void BlopAssistantOverlay::applyChrome() {
  const QString bg = NoteChrome::panelElevated().name(QColor::HexRgb);
  const QString border = NoteChrome::notchBorder().name(QColor::HexRgb);
  const QString text = NoteChrome::textPrimary().name(QColor::HexRgb);
  const QString muted = NoteChrome::textSecondary().name(QColor::HexRgb);
  const QString accent = NoteChrome::accent().name(QColor::HexRgb);
  const QString inputBg = NoteChrome::panelBg().name(QColor::HexRgb);

  setStyleSheet(QStringLiteral(
      "QWidget#BlopAssistantOverlay { background: transparent; }"
      "QFrame#BlopAssistantCard {"
      "  background: %1;"
      "  border: 1px solid %2;"
      "  border-radius: 18px;"
      "}"
      "QLabel#BlopAssistantTitle {"
      "  color: %3; font-weight: 800; font-size: 13px; background: transparent;"
      "}"
      "QLabel#BlopAssistantStatus {"
      "  color: %4; font-size: 12px; background: transparent;"
      "}"
      "QLineEdit#BlopAssistantInput {"
      "  background: %5; color: %3; border: 1px solid %2; border-radius: 12px;"
      "  padding: 8px 12px; font-size: 13px;"
      "}"
      "QLineEdit#BlopAssistantInput:focus { border: 1px solid %6; }"
      "QToolButton { color: %3; background: transparent; border: none;"
      "  font-size: 16px; font-weight: 700; border-radius: 10px; }"
      "QToolButton:hover { background: rgba(91,157,255,0.16); }"
      "QToolButton#BlopAssistantMic { color: %6; }"
      "QPushButton#BlopAssistantChip {"
      "  background: rgba(91,157,255,0.12); color: %3; border: none;"
      "  border-radius: 10px; padding: 6px 10px; font-size: 11px; font-weight: 600;"
      "}"
      "QPushButton#BlopAssistantChip:hover { background: rgba(91,157,255,0.22); }")
                    .arg(bg, border, text, muted, inputBg, accent));
}

void BlopAssistantOverlay::reposition() {
  if (!m_host)
    return;
  const int margin = UiScale::dp(16);
  const int w = qBound(UiScale::dp(320),
                       int(m_host->width() * 0.52), UiScale::dp(560));
  const int h = m_expanded ? UiScale::dp(188) : UiScale::dp(0);
  if (!m_expanded) {
    hide();
    return;
  }
  const int x = qMax(margin, (m_host->width() - w) / 2);
  const int y = qMax(margin, m_host->height() - h - UiScale::dp(28) -
                                 UiScale::safeBottomPx(m_host));
  setGeometry(x, y, w, h);
  show();
  raise();
}

void BlopAssistantOverlay::setExpanded(bool expanded) {
  m_expanded = expanded;
  if (m_examples)
    m_examples->setVisible(expanded);
  if (!expanded) {
    stopListening();
    hide();
    return;
  }
  applyChrome();
  reposition();
  focusInput();
}

void BlopAssistantOverlay::toggleExpanded() { setExpanded(!m_expanded); }

void BlopAssistantOverlay::setStatus(const QString &text) {
  if (m_status)
    m_status->setText(text);
}

void BlopAssistantOverlay::setHeadline(const QString &text) {
  if (m_title)
    m_title->setText(text);
}

void BlopAssistantOverlay::focusInput() {
  if (m_input) {
    m_input->setFocus(Qt::OtherFocusReason);
    m_input->selectAll();
  }
}

void BlopAssistantOverlay::refreshChrome() {
  applyChrome();
  if (m_expanded)
    reposition();
}

void BlopAssistantOverlay::submitCurrent() {
  if (!m_input)
    return;
  const QString text = m_input->text().trimmed();
  if (text.isEmpty())
    return;
  stopListening();
  emit utteranceSubmitted(text);
}

void BlopAssistantOverlay::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
}

void BlopAssistantOverlay::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape) {
    setExpanded(false);
    event->accept();
    return;
  }
  QWidget::keyPressEvent(event);
}

bool BlopAssistantOverlay::eventFilter(QObject *obj, QEvent *event) {
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
