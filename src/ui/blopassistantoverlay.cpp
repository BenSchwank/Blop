#include "blopassistantoverlay.h"

#include "uiscale.h"

#include <QAbstractAnimation>
#include <QCursor>
#include <QEasingCurve>
#include <QEnterEvent>
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
#include <QPen>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QRegion>
#include <QScrollArea>
#include <QScrollBar>
#include <QScreen>
#include <QTimer>
#include <QToolButton>
#include <QTransform>
#include <QVariantAnimation>
#include <QVBoxLayout>
#include <QWindow>

#ifdef BLOP_HAS_WEBENGINE
#include <QWebEnginePage>
#include <QtWebEngineWidgets/QWebEngineView>
#endif

namespace {

// Idle cloud is about a cursor; hover fills this widget; click opens chat.
const int kCloudWDp = 38;
const int kCloudHDp = 26;
const qreal kCloudRestScale = 0.78;

QPainterPath makeCloudPath(const QRectF &bounds) {
  // Blop-logo blob: overlapping lobes + a short stem into the bezel.
  const QRectF r = bounds.adjusted(0.5, -1.0, -0.5, -0.5);
  if (r.width() < 2.0 || r.height() < 2.0)
    return QPainterPath();

  auto puff = [&](qreal nx, qreal ny, qreal nw, qreal nh) {
    QPainterPath e;
    e.addEllipse(QRectF(r.left() + nx * r.width(), r.top() + ny * r.height(),
                        nw * r.width(), nh * r.height()));
    return e;
  };

  QPainterPath path = puff(0.30, 0.10, 0.46, 0.70);
  path = path.united(puff(0.06, 0.30, 0.42, 0.62));
  path = path.united(puff(0.52, 0.28, 0.44, 0.64));
  path = path.united(puff(0.18, 0.48, 0.64, 0.50));

  QPainterPath stem;
  const qreal stemW = qMax(4.0, r.width() * 0.18);
  stem.addRoundedRect(
      QRectF(r.center().x() - stemW * 0.5, r.top() - 2.0, stemW,
             r.height() * 0.42),
      stemW * 0.5, stemW * 0.5);
  path = path.united(stem);
  return path;
}

class NotchIsland : public QWidget {
public:
  explicit NotchIsland(QWidget *parent = nullptr) : QWidget(parent) {}

  void setGrow(qreal grow) {
    const qreal g = qBound(0.0, grow, 1.0);
    if (qFuzzyCompare(m_grow + 1.0, g + 1.0))
      return;
    m_grow = g;
    update();
  }
  qreal grow() const { return m_grow; }

  QRectF cloudRect() const {
    const QRectF r = QRectF(rect()).adjusted(1.0, 0.0, -1.0, -1.0);
    const qreal s = kCloudRestScale + (1.0 - kCloudRestScale) * m_grow;
    QRectF c(0, 0, r.width() * s, r.height() * s);
    c.moveCenter(QPointF(r.center().x(), r.top() + c.height() * 0.5));
    return c;
  }

  QPainterPath cloudPath() const { return makeCloudPath(cloudRect()); }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QPainterPath path = cloudPath();
    if (path.isEmpty())
      return;
    const QRectF br = path.boundingRect();

    QTransform glowXf;
    glowXf.translate(br.center().x(), br.center().y());
    glowXf.scale(1.16 + 0.10 * m_grow, 1.16 + 0.10 * m_grow);
    glowXf.translate(-br.center().x(), -br.center().y());
    p.setOpacity(0.28 + 0.32 * m_grow);
    p.fillPath(glowXf.map(path), QColor(124, 108, 255));
    p.setOpacity(1.0);

    QLinearGradient g(br.topLeft(), br.bottomRight());
    g.setColorAt(0.0, QColor(186, 176, 255));
    g.setColorAt(0.45, QColor(118, 104, 250));
    g.setColorAt(1.0, QColor(72, 62, 214));
    p.fillPath(path, g);

    QRectF spec(0, 0, br.width() * 0.22, br.height() * 0.18);
    spec.moveCenter(QPointF(br.left() + br.width() * 0.38,
                            br.top() + br.height() * 0.32));
    p.setOpacity(0.22 + 0.28 * m_grow);
    p.setBrush(QColor(255, 255, 255));
    p.setPen(Qt::NoPen);
    p.drawEllipse(spec);
    p.setOpacity(1.0);

    QPen pen(QColor(255, 255, 255, int(36 + 40 * m_grow)));
    pen.setWidthF(1.0);
    p.strokePath(path, pen);
  }

private:
  qreal m_grow{0.0};
};

static NotchIsland *asCloud(QWidget *w) {
  return static_cast<NotchIsland *>(w);
}

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
                 Qt::Tool | Qt::NoDropShadowWindowHint);
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
  // Otherwise the WM parks the lip below the panel and leaves a gap.
  setWindowFlag(Qt::X11BypassWindowManagerHint, true);
#endif
  setAttribute(Qt::WA_TranslucentBackground, true);
  setAttribute(Qt::WA_Hover, true);
  setMouseTracking(true);
  setWindowTitle(QStringLiteral("Blop Assistant"));
  if (QScreen *screen = QGuiApplication::primaryScreen()) {
    connect(screen, &QScreen::geometryChanged, this,
            &BlopAssistantOverlay::placeOnScreen, Qt::UniqueConnection);
    connect(screen, &QScreen::availableGeometryChanged, this,
            &BlopAssistantOverlay::placeOnScreen, Qt::UniqueConnection);
  }
  if (!m_hoverPoll) {
    m_hoverPoll = new QTimer(this);
    m_hoverPoll->setInterval(32);
    connect(m_hoverPoll, &QTimer::timeout, this, [this]() {
      if (!m_standalone || m_expanded)
        return;
      setCloudHover(frameGeometry().contains(QCursor::pos()));
    });
    m_hoverPoll->start();
  }
  if (QScreen *screen = QGuiApplication::primaryScreen()) {
    connect(screen, &QScreen::geometryChanged, this,
            &BlopAssistantOverlay::placeOnScreen, Qt::UniqueConnection);
    connect(screen, &QScreen::availableGeometryChanged, this,
            &BlopAssistantOverlay::placeOnScreen, Qt::UniqueConnection);
  }
  setExpanded(false);
  placeOnScreen();
  show();
  raise();
}

void BlopAssistantOverlay::buildUi() {
  auto *outer = new QVBoxLayout(this);
  outer->setContentsMargins(0, 0, 0, 0);
  outer->setSpacing(UiScale::dp(6));
  outer->setAlignment(Qt::AlignHCenter | Qt::AlignTop);

  m_notch = new NotchIsland(this);
  m_notch->setObjectName(QStringLiteral("BlopAssistantNotch"));
  m_notch->setAttribute(Qt::WA_StyledBackground, false);
  m_notch->setAttribute(Qt::WA_TranslucentBackground, true);
  m_notch->setAutoFillBackground(false);
  m_notch->setFixedSize(UiScale::dp(kCloudWDp), UiScale::dp(kCloudHDp));
  m_notch->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  m_notch->setCursor(Qt::PointingHandCursor);
  m_notch->setToolTip(QStringLiteral("Blop"));
  outer->addWidget(m_notch, 0, Qt::AlignHCenter);

  m_chat = new QFrame(this);
  m_chat->setObjectName(QStringLiteral("BlopAssistantCard"));
  outer->addWidget(m_chat, 1);

  auto *lay = new QVBoxLayout(m_chat);
  lay->setContentsMargins(UiScale::dp(12), UiScale::dp(10), UiScale::dp(12),
                          UiScale::dp(10));
  lay->setSpacing(UiScale::dp(8));

  auto *top = new QHBoxLayout();
  top->setContentsMargins(0, 0, 0, 0);
  m_title = new QLabel(QStringLiteral("Blop"), m_chat);
  m_title->setObjectName(QStringLiteral("BlopAssistantTitle"));
  top->addWidget(m_title, 1);
  lay->addLayout(top);

  m_empty = new QLabel(
      QStringLiteral("Frag mich irgendwas. Oder halt Push-to-talk."), m_chat);
  m_empty->setObjectName(QStringLiteral("BlopAssistantEmpty"));
  m_empty->setWordWrap(true);
  m_empty->setAlignment(Qt::AlignCenter);
  lay->addWidget(m_empty);

  m_scroll = new QScrollArea(m_chat);
  m_scroll->setObjectName(QStringLiteral("BlopAssistantScroll"));
  m_scroll->setWidgetResizable(true);
  m_scroll->setFrameShape(QFrame::NoFrame);
  m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_scroll->viewport()->setAutoFillBackground(false);
  m_scroll->viewport()->setObjectName(QStringLiteral("BlopAssistantScrollView"));
  m_transcript = new QWidget(m_scroll);
  m_transcript->setObjectName(QStringLiteral("BlopAssistantTranscript"));
  m_transcript->setAutoFillBackground(false);
  m_transcriptLay = new QVBoxLayout(m_transcript);
  m_transcriptLay->setContentsMargins(0, 0, 2, 0);
  m_transcriptLay->setSpacing(UiScale::dp(6));
  m_transcriptLay->addStretch(1);
  m_scroll->setWidget(m_transcript);
  m_scroll->hide();
  lay->addWidget(m_scroll, 1);

  m_examples = new QWidget(m_chat);
  auto *ex = new QHBoxLayout(m_examples);
  ex->setContentsMargins(0, 0, 0, 0);
  ex->setSpacing(UiScale::dp(6));
  const QStringList chips = {QStringLiteral("Hallo"), QStringLiteral("Notiz"),
                             QStringLiteral("YouTube")};
  for (const QString &c : chips) {
    auto *b = new QPushButton(c, m_examples);
    b->setCursor(Qt::PointingHandCursor);
    b->setFlat(true);
    b->setObjectName(QStringLiteral("BlopAssistantChip"));
    connect(b, &QPushButton::clicked, this, [this, c]() {
      if (c == QLatin1String("Hallo")) {
        m_input->setText(QStringLiteral("hallo"));
        submitCurrent();
        return;
      }
      if (c == QLatin1String("Notiz"))
        m_input->setText(QStringLiteral("neue Notiz "));
      else
        m_input->setText(QStringLiteral("öffne YouTube"));
      m_input->setFocus();
      m_input->end(false);
    });
    ex->addWidget(b, 0);
  }
  ex->addStretch(1);
  lay->addWidget(m_examples);

  m_confirmBar = new QWidget(m_chat);
  m_confirmBar->setObjectName(QStringLiteral("BlopAssistantConfirm"));
  auto *cf = new QHBoxLayout(m_confirmBar);
  cf->setContentsMargins(0, 0, 0, 0);
  cf->setSpacing(UiScale::dp(8));
  m_confirmLabel = new QLabel(m_confirmBar);
  m_confirmLabel->setObjectName(QStringLiteral("BlopAssistantConfirmLabel"));
  m_confirmLabel->setWordWrap(true);
  m_confirmYes = new QPushButton(QStringLiteral("Ja"), m_confirmBar);
  m_confirmYes->setObjectName(QStringLiteral("BlopAssistantConfirmYes"));
  m_confirmYes->setCursor(Qt::PointingHandCursor);
  m_confirmNo = new QPushButton(QStringLiteral("Nein"), m_confirmBar);
  m_confirmNo->setObjectName(QStringLiteral("BlopAssistantConfirmNo"));
  m_confirmNo->setCursor(Qt::PointingHandCursor);
  cf->addWidget(m_confirmLabel, 1);
  cf->addWidget(m_confirmNo, 0);
  cf->addWidget(m_confirmYes, 0);
  m_confirmBar->hide();
  lay->addWidget(m_confirmBar);

  auto *row = new QHBoxLayout();
  row->setContentsMargins(0, 0, 0, 0);
  row->setSpacing(UiScale::dp(6));
  m_input = new QLineEdit(m_chat);
  m_input->setObjectName(QStringLiteral("BlopAssistantInput"));
  m_input->setPlaceholderText(QStringLiteral("Frag Blop…"));
  m_micBtn = makeIconBtn(m_chat, QStringLiteral("Mikrofon"));
  m_micBtn->setObjectName(QStringLiteral("BlopAssistantMic"));
  m_micBtn->setText(QStringLiteral("●"));
  m_micBtn->setFixedSize(UiScale::dp(32), UiScale::dp(32));
  m_sendBtn = makeIconBtn(m_chat, QStringLiteral("Senden"));
  m_sendBtn->setObjectName(QStringLiteral("BlopAssistantSend"));
  m_sendBtn->setText(QStringLiteral("↑"));
  m_sendBtn->setFixedSize(UiScale::dp(32), UiScale::dp(32));
  row->addWidget(m_input, 1);
  row->addWidget(m_micBtn, 0);
  row->addWidget(m_sendBtn, 0);
  lay->addLayout(row);

  m_hint = new QLabel(m_chat);
  m_hint->setObjectName(QStringLiteral("BlopAssistantHint"));
  m_hint->hide();
  lay->addWidget(m_hint);

  connect(m_sendBtn, &QToolButton::clicked, this,
          &BlopAssistantOverlay::submitCurrent);
  connect(m_input, &QLineEdit::returnPressed, this,
          &BlopAssistantOverlay::submitCurrent);
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
    addAssistantMessage(QStringLiteral("Alles klar, abgebrochen."));
    emit confirmRejected();
  });

  m_input->installEventFilter(this);
}

void BlopAssistantOverlay::applyChrome() {
  setStyleSheet(QStringLiteral(
      "QWidget#BlopAssistantOverlay { background: transparent; }"
      "QWidget#BlopAssistantNotch {"
      "  background: transparent;"
      "  border: none;"
      "}"
      "QFrame#BlopAssistantCard {"
      "  background: #1E1E1E;"
      "  border: 1px solid rgba(124,108,255,0.35);"
      "  border-radius: 18px;"
      "}"
      "QLabel#BlopAssistantTitle {"
      "  color: #E8E8E8; font-weight: 700; font-size: 13px; background: transparent;"
      "}"
      "QLabel#BlopAssistantEmpty, QLabel#BlopAssistantHint,"
      "QLabel#BlopAssistantConfirmLabel {"
      "  color: #8E8E8E; font-size: 12px; background: transparent;"
      "}"
      "QLabel#BlopAssistantEmpty { padding: 18px 8px; }"
      "QScrollArea#BlopAssistantScroll, QWidget#BlopAssistantScrollView,"
      "QWidget#BlopAssistantTranscript { background: transparent; border: none; }"
      "QLineEdit#BlopAssistantInput {"
      "  background: #262626; color: #E8E8E8; border: 1px solid #404040;"
      "  border-radius: 12px; padding: 8px 12px; font-size: 13px;"
      "}"
      "QLineEdit#BlopAssistantInput:focus { border: 1px solid #5B9DFF; }"
      "QToolButton { color: #E8E8E8; background: transparent; border: none;"
      "  font-size: 15px; font-weight: 700; border-radius: 10px; }"
      "QToolButton:hover { background: rgba(91,157,255,0.18); }"
      "QToolButton#BlopAssistantMic { color: #5B9DFF; }"
      "QToolButton#BlopAssistantSend { background: #5B9DFF; color: #0B1220; }"
      "QPushButton#BlopAssistantChip {"
      "  background: #262626; color: #C8C8C8; border: 1px solid #3A3A3A;"
      "  border-radius: 11px; padding: 5px 10px; font-size: 11px;"
      "}"
      "QPushButton#BlopAssistantChip:hover { border-color: #5B9DFF; color: #E8E8E8; }"
      "QPushButton#BlopAssistantConfirmYes {"
      "  background: #5B9DFF; color: #0B1220; border: none; border-radius: 10px;"
      "  padding: 6px 12px; font-weight: 700;"
      "}"
      "QPushButton#BlopAssistantConfirmNo {"
      "  background: transparent; color: #E8E8E8; border: 1px solid #404040;"
      "  border-radius: 10px; padding: 6px 12px;"
      "}"
      "QScrollBar:vertical { background: transparent; width: 8px; margin: 0; }"
      "QScrollBar::handle:vertical { background: rgba(255,255,255,0.14);"
      "  border-radius: 4px; min-height: 24px; }"
      "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"));
}

void BlopAssistantOverlay::applyLayoutMode() {
  if (m_chat)
    m_chat->setVisible(m_expanded);
}

void BlopAssistantOverlay::setCloudHover(bool on) {
  if (m_expanded)
    on = true;
  if (m_cloudHover == on && m_hoverAnim &&
      m_hoverAnim->state() != QAbstractAnimation::Running)
    return;
  m_cloudHover = on;
  if (!m_notch)
    return;
  if (!m_hoverAnim) {
    m_hoverAnim = new QVariantAnimation(this);
    m_hoverAnim->setDuration(140);
    m_hoverAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_hoverAnim, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &v) {
              if (NotchIsland *cloud = asCloud(m_notch))
                cloud->setGrow(v.toReal());
              if (!m_expanded)
                updateCollapsedMask();
            });
  }
  const qreal current = asCloud(m_notch) ? asCloud(m_notch)->grow() : 0.0;
  const qreal target = on ? 1.0 : 0.0;
  if (qFuzzyCompare(current + 1.0, target + 1.0))
    return;
  m_hoverAnim->stop();
  m_hoverAnim->setStartValue(current);
  m_hoverAnim->setEndValue(target);
  m_hoverAnim->start();
}

void BlopAssistantOverlay::updateCollapsedMask() {
  if (m_expanded || !m_notch) {
    clearMask();
    return;
  }
  NotchIsland *cloud = asCloud(m_notch);
  if (!cloud) {
    clearMask();
    return;
  }
  QPainterPath path = cloud->cloudPath();
  QTransform xf;
  xf.translate(m_notch->x(), m_notch->y());
  path = xf.map(path);
  if (path.isEmpty())
    clearMask();
  else
    setMask(QRegion(path.toFillPolygon().toPolygon()));
}

void BlopAssistantOverlay::applyWindowGeometry(const QRect &target, bool animate) {
  m_pendingGeom = target;
  const bool canAnim =
      animate && m_standalone && isVisible() && geometry().isValid() &&
      geometry().width() > 8 && geometry() != target;
  if (!canAnim) {
    setMinimumSize(0, 0);
    setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    setFixedSize(target.size());
    move(target.topLeft());
    if (layout())
      layout()->activate();
    if (QWindow *win = windowHandle())
      win->setPosition(target.topLeft());
    updateCollapsedMask();
    return;
  }
  if (m_expanded)
    clearMask();
  setMinimumSize(0, 0);
  setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
  if (!m_geoAnim) {
    m_geoAnim = new QPropertyAnimation(this, "geometry", this);
    m_geoAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_geoAnim, &QAbstractAnimation::finished, this, [this]() {
      if (!m_pendingGeom.isValid())
        return;
      setFixedSize(m_pendingGeom.size());
      move(m_pendingGeom.topLeft());
      updateCollapsedMask();
    });
  }
  m_geoAnim->stop();
  m_geoAnim->setDuration(m_expanded ? 220 : 160);
  m_geoAnim->setStartValue(geometry());
  m_geoAnim->setEndValue(target);
  m_geoAnim->start();
}

void BlopAssistantOverlay::placeOnScreen() {
  QScreen *screen = QGuiApplication::primaryScreen();
  if (!screen)
    return;
  const QRect full = screen->geometry();
  const int cloudH = m_notch ? m_notch->height() : UiScale::dp(kCloudHDp);
  auto *outer = qobject_cast<QVBoxLayout *>(layout());
  if (outer) {
    const int side = m_expanded ? UiScale::dp(6) : 0;
    const int bottom = m_expanded ? UiScale::dp(6) : UiScale::dp(2);
    outer->setContentsMargins(side, 0, side, bottom);
    outer->setSpacing(m_expanded ? UiScale::dp(8) : 0);
  }
  const int w =
      m_expanded ? UiScale::dp(360)
                 : (m_notch ? m_notch->width() : UiScale::dp(kCloudWDp));
  const int h =
      m_expanded ? (cloudH + UiScale::dp(8) + UiScale::dp(400))
                 : cloudH + UiScale::dp(2);
  const int x = full.x() + (full.width() - w) / 2;
  const QRect avail = screen->availableGeometry();
  int y = full.y();
  if (avail.y() >= full.y() + UiScale::dp(20))
    y = avail.y();
  applyWindowGeometry(QRect(x, y, w, h), isVisible());
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
  if (!m_expanded) {
    hide();
    return;
  }
  const int margin = UiScale::dp(16);
  const int w = qBound(UiScale::dp(300), int(m_host->width() * 0.48),
                       UiScale::dp(420));
  const int h = UiScale::dp(360);
  const int x = qMax(margin, (m_host->width() - w) / 2);
  const int y = margin;
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
    setCloudHover(expanded || frameGeometry().contains(QCursor::pos()));
    placeOnScreen();
    show();
    raise();
    if (expanded) {
      activateWindow();
      grabKeyboard();
      focusInput();
    } else {
      releaseKeyboard();
    }
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
  if (m_hint) {
    m_hint->setText(text);
    m_hint->setVisible(!text.isEmpty() && m_expanded);
  }
}

void BlopAssistantOverlay::setHeadline(const QString &text) {
  if (m_title)
    m_title->setText(text);
}

void BlopAssistantOverlay::setEmptyHint(const QString &text) {
  if (m_empty)
    m_empty->setText(text);
}

void BlopAssistantOverlay::setBusy(bool busy) {
  if (m_input)
    m_input->setEnabled(!busy);
  if (m_sendBtn)
    m_sendBtn->setEnabled(!busy);
  if (busy)
    setStatus(QStringLiteral("Ich denk kurz nach…"));
  else if (m_hint && m_hint->text().contains(QLatin1String("denk kurz")))
    setStatus(QString());
}

void BlopAssistantOverlay::focusInput() {
  if (m_input && m_expanded) {
    m_input->setFocus(Qt::OtherFocusReason);
  }
}

void BlopAssistantOverlay::refreshChrome() {
  applyChrome();
  reposition();
}

void BlopAssistantOverlay::promptConfirm(const QString &prompt) {
  setExpanded(true);
  addAssistantMessage(prompt);
  if (m_confirmLabel)
    m_confirmLabel->setText(QStringLiteral("Ausführen?"));
  if (m_confirmBar)
    m_confirmBar->show();
}

void BlopAssistantOverlay::clearConfirm() {
  if (m_confirmBar)
    m_confirmBar->hide();
}

void BlopAssistantOverlay::addUserMessage(const QString &text) {
  addBubble(text, true);
}

void BlopAssistantOverlay::addAssistantMessage(const QString &text) {
  addBubble(text, false);
}

void BlopAssistantOverlay::addBubble(const QString &text, bool fromUser) {
  if (text.trimmed().isEmpty() || !m_transcriptLay)
    return;
  if (m_empty)
    m_empty->hide();
  if (m_examples)
    m_examples->hide();
  if (m_scroll)
    m_scroll->show();
  auto *lab = new QLabel(text, m_transcript);
  lab->setWordWrap(true);
  lab->setTextFormat(Qt::PlainText);
  lab->setTextInteractionFlags(Qt::TextSelectableByMouse);
  lab->setStyleSheet(fromUser ? QStringLiteral(
                                    "QLabel { background: #5B9DFF; color: #0B1220;"
                                    " border-radius: 12px; padding: 8px 10px;"
                                    " font-size: 12px; }")
                              : QStringLiteral(
                                    "QLabel { background: #2A2A2A; color: #E8E8E8;"
                                    " border-radius: 12px; padding: 8px 10px;"
                                    " font-size: 12px; }"));
  auto *row = new QHBoxLayout();
  row->setContentsMargins(0, 0, 0, 0);
  if (fromUser)
    row->addStretch(1);
  row->addWidget(lab, 0);
  if (!fromUser)
    row->addStretch(1);
  m_transcriptLay->insertLayout(m_transcriptLay->count() - 1, row);
  scrollChatToEnd();
}

void BlopAssistantOverlay::scrollChatToEnd() {
  if (!m_scroll)
    return;
  QTimer::singleShot(0, this, [this]() {
    if (auto *bar = m_scroll->verticalScrollBar())
      bar->setValue(bar->maximum());
  });
}

void BlopAssistantOverlay::submitCurrent() {
  if (!m_input)
    return;
  const QString text = m_input->text().trimmed();
  if (text.isEmpty())
    return;
  stopListening();
  clearConfirm();
  addUserMessage(text);
  m_input->clear();
  emit utteranceSubmitted(text);
}

void BlopAssistantOverlay::startPushToTalk() {
  setExpanded(true);
  startListening();
}

void BlopAssistantOverlay::endPushToTalk() { stopListening(); }

void BlopAssistantOverlay::paintEvent(QPaintEvent *) {}

void BlopAssistantOverlay::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton && m_standalone) {
    m_pressOnNotch = !m_expanded ||
                     (m_notch && m_notch->geometry().contains(event->pos()));
    event->accept();
    return;
  }
  QWidget::mousePressEvent(event);
}

void BlopAssistantOverlay::mouseMoveEvent(QMouseEvent *event) {
  if (m_standalone && !m_expanded)
    setCloudHover(true);
  QWidget::mouseMoveEvent(event);
}

void BlopAssistantOverlay::enterEvent(QEnterEvent *event) {
  if (m_standalone)
    setCloudHover(true);
  QWidget::enterEvent(event);
}

void BlopAssistantOverlay::leaveEvent(QEvent *event) {
  if (m_standalone && !m_expanded)
    setCloudHover(false);
  QWidget::leaveEvent(event);
}

void BlopAssistantOverlay::mouseReleaseEvent(QMouseEvent *event) {
  if (m_standalone && event->button() == Qt::LeftButton) {
    if (m_pressOnNotch)
      setExpanded(!m_expanded);
    m_pressOnNotch = false;
    event->accept();
    return;
  }
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
  setStatus(QStringLiteral("Zuhören… loslassen zum Senden."));
  if (m_speechView && m_speechView->page()) {
    m_speechView->page()->runJavaScript(
        QStringLiteral("window.blopStart && window.blopStart('de-DE');"));
  }
#else
  setStatus(QStringLiteral("Mikrofon braucht WebEngine — tippe einfach."));
#endif
}

void BlopAssistantOverlay::stopListening() {
  m_listening = false;
  if (m_micBtn)
    m_micBtn->setText(QStringLiteral("●"));
  if (m_hint && m_hint->text().startsWith(QLatin1String("Zuhören")))
    m_hint->hide();
}

void BlopAssistantOverlay::handleSttConsole(const QString &message) {
  if (message.startsWith(QLatin1String("BLOP_STT_FINAL:"))) {
    const QString t = message.mid(14).trimmed();
    if (m_input)
      m_input->setText(t);
    setStatus(QString());
    submitCurrent();
    return;
  }
  if (message.startsWith(QLatin1String("BLOP_STT_INTERIM:"))) {
    if (m_input)
      m_input->setText(message.mid(16));
    return;
  }
  if (message.startsWith(QLatin1String("BLOP_STT_ERR:"))) {
    m_listening = false;
    if (m_micBtn)
      m_micBtn->setText(QStringLiteral("●"));
    setStatus(QStringLiteral("Mikrofon nicht verfügbar — tippe stattdessen."));
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
