#include "onboardingwizard.h"

#include "blop_dialogs.h"
#include "blopwindowcontrols.h"
#include "cloudlink.h"
#include "googleauthmanager.h"
#include "storageprefs.h"
#include "uiscale.h"

#include <QAudioOutput>
#include <QButtonGroup>
#include <QCheckBox>
#include <QDesktopServices>
#include <QEvent>
#include <QFileDialog>
#include <QFontMetrics>
#include <QFrame>
#include <QHideEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QGuiApplication>
#include <QImage>
#include <QMediaPlayer>
#include <QPainter>
#include <QPaintEvent>
#include <QPixmap>
#include <QPushButton>
#include <QScreen>
#include <QRadialGradient>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QShowEvent>
#include <QSizePolicy>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStringList>
#include <QtMath>
#include <QLinearGradient>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QVideoFrame>
#include <QVideoSink>

// Paints intro frames in the Qt widget stack. QVideoWidget is a native
// window that stays at the first small size and swallows input, so the
// intro looked frozen.
class IntroStage : public QWidget {
public:
  explicit IntroStage(QWidget *parent = nullptr) : QWidget(parent) {
    setAutoFillBackground(true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::black);
    setPalette(pal);
  }

  void setFrame(const QImage &image) {
    m_frame = image;
    update();
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.fillRect(rect(), Qt::black);
    if (m_frame.isNull() || width() < 2 || height() < 2)
      return;
    const QSize fitted = m_frame.size().scaled(size(), Qt::KeepAspectRatio);
    p.drawImage(QRect(QPoint((width() - fitted.width()) / 2,
                             (height() - fitted.height()) / 2),
                      fitted),
                m_frame);
  }

private:
  QImage m_frame;
};

namespace {

// Brand palette for the intro only. Does not touch the user's theme.
const QColor kBg(0x10, 0x0E, 0x18);
const QColor kPanel(0x1C, 0x17, 0x30);
const QColor kInk(0xF4, 0xF1, 0xFF);

const QString kTitleStyle = QStringLiteral(
    "font-size: 28px; font-weight: 650; background: transparent; color: #F4F1FF;");
const QString kBodyStyle = QStringLiteral(
    "font-size: 15px; background: transparent; color: #C4B6E0;");

QLabel *titleLabel(const QString &text, QWidget *parent) {
  auto *h = new QLabel(text, parent);
  h->setWordWrap(true);
  h->setStyleSheet(kTitleStyle);
  return h;
}

QLabel *bodyLabel(const QString &text, QWidget *parent) {
  auto *b = new QLabel(text, parent);
  b->setWordWrap(true);
  b->setStyleSheet(kBodyStyle);
  return b;
}

QPixmap featheredLogo(const QPixmap &src, int side) {
  const QPixmap scaled =
      src.scaled(side, side, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  QPixmap out(scaled.size());
  out.fill(Qt::transparent);
  QPainter p(&out);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setRenderHint(QPainter::SmoothPixmapTransform, true);
  p.drawPixmap(0, 0, scaled);
  const QPointF c(out.width() / 2.0, out.height() / 2.0);
  QRadialGradient fade(c, out.width() * 0.58);
  fade.setColorAt(0.40, QColor(0, 0, 0, 0));
  fade.setColorAt(0.72, QColor(0, 0, 0, 70));
  fade.setColorAt(1.0, QColor(0, 0, 0, 255));
  p.setCompositionMode(QPainter::CompositionMode_DestinationOut);
  p.fillRect(out.rect(), fade);
  return out;
}

/// Field behind the card. Same dark violet base as before, with a slow
/// drift of the brand purples. Hidden while the intro video is playing.
class BackdropWash : public QWidget {
public:
  explicit BackdropWash(QWidget *parent = nullptr) : QWidget(parent) {
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    m_timer.setInterval(50);
    connect(&m_timer, &QTimer::timeout, this, [this]() {
      m_phase += 0.0055;
      if (m_phase > 1.0)
        m_phase -= 1.0;
      update();
    });
  }

protected:
  void showEvent(QShowEvent *event) override {
    QWidget::showEvent(event);
    m_timer.start();
  }
  void hideEvent(QHideEvent *event) override {
    m_timer.stop();
    QWidget::hideEvent(event);
  }
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QRect r = rect();
    QLinearGradient base(r.topLeft(), r.bottomLeft());
    base.setColorAt(0.0, QColor(0x14, 0x12, 0x1C));
    base.setColorAt(1.0, QColor(0x0A, 0x09, 0x10));
    p.fillRect(r, base);
    const qreal t = m_phase * 6.28318530718;
    const qreal w = r.width();
    const qreal h = r.height();
    const qreal reach = qMax(w, h) * 0.85;
    const auto wave = [&](const QPointF &crest, const QPointF &focal,
                          const QColor &ink) {
      QRadialGradient g(crest, reach);
      g.setFocalPoint(focal);
      QColor soft = ink;
      soft.setAlpha(ink.alpha() / 2);
      QColor edge = ink;
      edge.setAlpha(ink.alpha() / 5);
      g.setColorAt(0.0, soft);
      g.setColorAt(0.28, ink);
      g.setColorAt(0.62, edge);
      g.setColorAt(1.0, QColor(0x0A, 0x09, 0x10, 0));
      p.fillRect(r, g);
    };
    const QPointF crestA(w * (0.15 + 0.22 * qCos(t)),
                         h * (0.50 + 0.28 * qSin(t * 0.65)));
    wave(crestA, crestA + QPointF(w * 0.18 * qCos(t), h * 0.08),
         QColor(0x7C, 0x5C, 0xFC, 108));
    const QPointF crestB(w * (0.85 + 0.18 * qCos(t + 2.4)),
                         h * (0.42 + 0.30 * qSin(t * 0.55 + 1.2)));
    wave(crestB, crestB + QPointF(-w * 0.16 * qSin(t), h * 0.10),
         QColor(0x95, 0x7A, 0xFF, 82));
  }

private:
  QTimer m_timer;
  qreal m_phase{0.2};
};

/// Page inside the centered card. The card itself is the measure.
QWidget *centered(QWidget *parent, QWidget *column) {
  auto *w = new QWidget(parent);
  w->setStyleSheet(QStringLiteral("background: transparent;"));
  auto *v = new QVBoxLayout(w);
  v->setContentsMargins(UiScale::dp(28), UiScale::dp(4), UiScale::dp(28),
                        UiScale::dp(4));
  column->setParent(w);
  v->addWidget(column);
  v->addStretch(1);
  return w;
}

class ScrollFit : public QObject {
public:
  QScrollArea *scroll{nullptr};
  QLabel *body{nullptr};
  explicit ScrollFit(QObject *parent) : QObject(parent) {}
  bool eventFilter(QObject *watched, QEvent *event) override {
    if (scroll && body && watched == scroll->viewport() &&
        event->type() == QEvent::Resize)
      fit();
    return QObject::eventFilter(watched, event);
  }
  void fit() {
    const int w = qMax(80, scroll->viewport()->width() - 8);
    body->setFixedWidth(w);
    const QFontMetrics fm(body->font());
    const QRect br = fm.boundingRect(QRect(0, 0, w - 36, 10000),
                                     Qt::TextWordWrap, body->text());
    body->setFixedHeight(br.height() + 40);
    scroll->verticalScrollBar()->setValue(0);
  }
};

QScrollArea *textScroll(QLabel *body, QWidget *parent) {
  body->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  body->setWordWrap(true);
  const QString pad = body->styleSheet();
  body->setStyleSheet(pad + QStringLiteral("padding: 16px 18px;"));
  auto *scroll = new QScrollArea(parent);
  scroll->setWidgetResizable(false);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setAlignment(Qt::AlignTop | Qt::AlignLeft);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  scroll->setMinimumHeight(UiScale::dp(220));
  scroll->setWidget(body);
  scroll->setStyleSheet(QStringLiteral(
      "QScrollArea { background: #1A191F; border: none; border-radius: 12px; }"
      "QScrollBar:vertical { width: 10px; background: transparent; margin: 6px 2px; }"
      "QScrollBar::handle:vertical { background: #7C5CFC; border-radius: 4px;"
      "  min-height: 28px; }"
      "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
      "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
      "  background: transparent; }"));
  if (scroll->viewport()) {
    scroll->viewport()->setStyleSheet(QStringLiteral("background: #1A191F;"));
    auto *fit = new ScrollFit(scroll);
    fit->scroll = scroll;
    fit->body = body;
    scroll->viewport()->installEventFilter(fit);
  }
  return scroll;
}

QPushButton *choiceButton(const QString &title, const QString &why,
                          QWidget *parent) {
  auto *b = new QPushButton(parent);
  b->setCheckable(true);
  b->setCursor(Qt::PointingHandCursor);
  b->setText(title + QStringLiteral("\n") + why);
  b->setStyleSheet(QStringLiteral(
      "QPushButton {"
      "  text-align: left; padding: 14px 16px; border-radius: 14px;"
      "  border: 1px solid rgba(124,92,252,0.40);"
      "  background: #1A191F; color: #F4F1FF; font-size: 13px;"
      "}"
      "QPushButton:hover { border-color: #957AFF; }"
      "QPushButton:checked {"
      "  border: 2px solid #7C5CFC; background: rgba(124,92,252,0.28);"
      "}"));
  return b;
}

QString primaryButtonQss() {
  return QStringLiteral(
      "QPushButton { background: #7C5CFC; color: white; border: none;"
      "  border-radius: 12px; padding: 12px 18px; font-weight: 700; }"
      "QPushButton:hover { background: #957AFF; }"
      "QPushButton:disabled { background: rgba(124,92,252,0.35); color: #E6DFFF; }");
}

QString ghostButtonQss() {
  return QStringLiteral(
      "QPushButton { background: transparent; color: #C4B6E0; border: none;"
      "  border-radius: 10px; padding: 10px 12px; font-weight: 600; }"
      "QPushButton:hover { color: #F4F1FF; background: rgba(255,255,255,0.04); }");
}

} // namespace

OnboardingWizard::OnboardingWizard(QWidget *parent) : QDialog(parent) {
  setObjectName(QStringLiteral("OnboardingWizard"));
  setProperty("blopPreventDismiss", true);
  setWindowTitle(QStringLiteral("Willkommen bei Blop"));
  setModal(true);
  setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
  setWindowModality(Qt::ApplicationModal);
  setAttribute(Qt::WA_StyledBackground, true);
  setAutoFillBackground(true);
  QPalette pal = palette();
  pal.setColor(QPalette::Window, QColor(0x0E, 0x0C, 0x14));
  pal.setColor(QPalette::WindowText, kInk);
  pal.setColor(QPalette::Base, kPanel);
  pal.setColor(QPalette::Text, kInk);
  pal.setColor(QPalette::Button, kPanel);
  pal.setColor(QPalette::ButtonText, kInk);
  setPalette(pal);
  setStyleSheet(QStringLiteral(
      "QDialog#OnboardingWizard {"
      "  background-color: #0E0C14;"
      "  background-image: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
      "    stop:0 #14121C, stop:1 #0A0910);"
      "}"
      "QLabel { color: #F4F1FF; background: transparent; }"
      "QCheckBox { color: #F4F1FF; background: transparent; spacing: 8px; }"
      "QCheckBox::indicator { width: 18px; height: 18px; border-radius: 4px;"
      "  border: 1px solid #7C5CFC; background: #1C1730; }"
      "QCheckBox::indicator:checked { background: #7C5CFC; }"));

  m_backdrop = new BackdropWash(this);
  m_backdrop->hide();

  m_card = new QFrame(this);
  m_card->setObjectName(QStringLiteral("OnboardingCard"));
  m_card->setAttribute(Qt::WA_StyledBackground, true);
  m_card->setStyleSheet(QStringLiteral(
      "QFrame#OnboardingCard {"
      "  background: #101014;"
      "  border: 1px solid rgba(255,255,255,0.10);"
      "  border-radius: 16px;"
      "}"));
  auto *cardLay = new QVBoxLayout(m_card);
  cardLay->setContentsMargins(UiScale::dp(8), UiScale::dp(18), UiScale::dp(8),
                              UiScale::dp(16));
  cardLay->setSpacing(UiScale::dp(8));

  m_brand = new QWidget(m_card);
  m_brand->setStyleSheet(QStringLiteral("background: transparent;"));
  auto *steps = new QHBoxLayout(m_brand);
  steps->setContentsMargins(UiScale::dp(20), 0, UiScale::dp(20), 0);
  steps->setSpacing(UiScale::dp(8));
  const QStringList stepNames = {
      QStringLiteral("Willkommen"), QStringLiteral("Hinweise"),
      QStringLiteral("Speicher"),    QStringLiteral("Anbieter"),
      QStringLiteral("Neue Notiz"),  QStringLiteral("Bibliothek"),
      QStringLiteral("Einstellungen")};
  for (const QString &name : stepNames) {
    auto *cell = new QWidget(m_brand);
    cell->setMinimumWidth(0);
    cell->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    auto *v = new QVBoxLayout(cell);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(6);
    auto *dot = new QFrame(cell);
    dot->setFixedSize(8, 8);
    dot->setAttribute(Qt::WA_StyledBackground, true);
    dot->setStyleSheet(QStringLiteral(
        "background: #3A3350; border: none; border-radius: 4px;"));
    auto *lab = new QLabel(name, cell);
    lab->setAlignment(Qt::AlignHCenter);
    lab->setProperty("stepName", name);
    lab->setMinimumWidth(0);
    lab->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    lab->setStyleSheet(QStringLiteral(
        "font-size: 12px; color: #6E6484; background: transparent;"));
    v->addWidget(dot, 0, Qt::AlignHCenter);
    v->addWidget(lab);
    steps->addWidget(cell, 1);
    m_stepDots.append(dot);
    m_stepLabels.append(lab);
  }
  cardLay->addWidget(m_brand);

  m_loginStatus = new QLabel(m_card);
  m_loginStatus->setAlignment(Qt::AlignHCenter);
  m_loginStatus->setWordWrap(true);
  m_loginStatus->setVisible(false);
  m_loginStatus->setStyleSheet(QStringLiteral(
      "font-size: 14px; font-weight: 650; color: #C4B6FF;"
      "background: transparent; padding: 2px 16px 0 16px;"));
  cardLay->addWidget(m_loginStatus);

  m_stepLabel = new QLabel(m_card);
  m_stepLabel->setVisible(false);

  m_stack = new QStackedWidget(m_card);
  m_stack->setStyleSheet(QStringLiteral(
      "QStackedWidget { background: transparent; border: none; }"));
  cardLay->addWidget(m_stack, 1);
  buildPages();

  m_footer = new QWidget(m_card);
  m_footer->setStyleSheet(QStringLiteral("background: transparent;"));
  auto *row = new QHBoxLayout(m_footer);
  row->setContentsMargins(UiScale::dp(28), UiScale::dp(4), UiScale::dp(28),
                          UiScale::dp(8));
  m_btnBack = new QPushButton(QStringLiteral("Zurück"), m_footer);
  m_btnBack->setCursor(Qt::PointingHandCursor);
  m_btnBack->setStyleSheet(ghostButtonQss());
  m_btnNext = new QPushButton(QStringLiteral("Weiter"), m_footer);
  m_btnNext->setCursor(Qt::PointingHandCursor);
  m_btnNext->setDefault(true);
  m_btnNext->setMinimumWidth(UiScale::dp(148));
  m_btnNext->setFixedHeight(UiScale::dp(40));
  m_btnNext->setStyleSheet(primaryButtonQss());
  row->addWidget(m_btnBack);
  row->addStretch(1);
  row->addWidget(m_btnNext);
  cardLay->addWidget(m_footer);

#ifndef Q_OS_ANDROID
  m_winControls = new BlopWindowControls(this);
  m_winControls->setChrome(QColor(0xF4, 0xF1, 0xFF), false);
  connect(m_winControls, &BlopWindowControls::minimizeRequested, this, [this]() {
    if (QWidget *host = parentWidget())
      host->showMinimized();
    showMinimized();
  });
  connect(m_winControls, &BlopWindowControls::maximizeRequested, this, [this]() {
    QWidget *host = parentWidget();
    if (!host)
      return;
    if (host->isMaximized())
      host->showNormal();
    else
      host->showMaximized();
    m_winControls->setMaximized(host->isMaximized());
    QTimer::singleShot(0, this, [this]() { fitToHost(); });
  });
  connect(m_winControls, &BlopWindowControls::closeRequested, this,
          &OnboardingWizard::requestQuit);
#endif

  connect(m_btnBack, &QPushButton::clicked, this, &OnboardingWizard::onBack);
  connect(m_btnNext, &QPushButton::clicked, this, &OnboardingWizard::onNext);
  connect(&GoogleAuthManager::instance(), &GoogleAuthManager::authenticated,
          this, [this]() { refreshWelcome(); });
  connect(&GoogleAuthManager::instance(), &GoogleAuthManager::userInfoUpdated,
          this, [this]() { refreshWelcome(); });
  connect(&GoogleAuthManager::instance(),
          &GoogleAuthManager::authenticationFailed, this,
          [this](const QString &) {
            if (m_welcomeStatus)
              m_welcomeStatus->setText(
                  QStringLiteral("Anmeldung nicht abgeschlossen. "
                                 "Du kannst es erneut versuchen oder ohne "
                                 "Konto weitergehen."));
          });

  if (parent)
    parent->installEventFilter(this);
  goTo(PageVideo);
}

void OnboardingWizard::showEvent(QShowEvent *event) {
  fitToHost();
  QDialog::showEvent(event);
  QTimer::singleShot(0, this, [this]() {
    fitToHost();
    raise();
    activateWindow();
    if (m_page == PageVideo)
      startIntroVideo();
  });
}

void OnboardingWizard::placeWindowControls() {
#ifndef Q_OS_ANDROID
  if (m_winControls) {
    const QSize s = m_winControls->size();
    const int margin = UiScale::dp(12);
    m_winControls->setGeometry(width() - s.width() - margin, margin, s.width(),
                              s.height());
    m_winControls->raise();
  }
#endif
  if (m_btnSkip) {
    m_btnSkip->setVisible(m_page == PageVideo);
    m_btnSkip->adjustSize();
    const int margin = UiScale::dp(20);
    m_btnSkip->move(width() - m_btnSkip->width() - margin,
                    height() - m_btnSkip->height() - margin);
    m_btnSkip->raise();
  }
}

void OnboardingWizard::requestQuit() {
  QWidget *host = parentWidget();
  QDialog::done(QDialog::Rejected);
  if (host)
    QTimer::singleShot(0, host, [host]() { host->close(); });
}

void OnboardingWizard::fitToHost() {
  if (m_fitting)
    return;
  m_fitting = true;

  QWidget *host = parentWidget();
  QScreen *screen = host ? host->screen() : this->screen();
  if (!screen)
    screen = QGuiApplication::primaryScreen();
  const QRect avail =
      screen ? screen->availableGeometry() : QRect(0, 0, 1280, 800);

  // The frameless host often opens at its size hint. Stretch it before the
  // intro copies that rect, otherwise the video sits in a half window.
  if (host && host->geometry() != avail)
    host->setGeometry(avail);
  if (geometry() != avail)
    setGeometry(avail);
  layoutCard();
  m_fitting = false;
}

void OnboardingWizard::resizeEvent(QResizeEvent *event) {
  QDialog::resizeEvent(event);
  layoutCard();
}

void OnboardingWizard::layoutCard() {
  if (!m_card)
    return;
  const bool video = m_page == PageVideo;
  if (m_backdrop) {
    m_backdrop->setGeometry(rect());
    m_backdrop->setVisible(!video);
    m_backdrop->lower();
  }
  if (video) {
    setStyleSheet(QStringLiteral(
        "QDialog#OnboardingWizard { background-color: #000000; background-image: none; }"
        "QLabel { color: #F4F1FF; background: transparent; }"
        "QCheckBox { color: #F4F1FF; background: transparent; spacing: 8px; }"
        "QCheckBox::indicator { width: 18px; height: 18px; border-radius: 4px;"
        "  border: 1px solid #7C5CFC; background: #1C1730; }"
        "QCheckBox::indicator:checked { background: #7C5CFC; }"));
    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::black);
    setPalette(pal);
    m_card->setStyleSheet(QStringLiteral(
        "QFrame#OnboardingCard { background: #000000; border: none;"
        "  border-radius: 0px; }"));
    m_card->setGeometry(rect());
    placeWindowControls();
    return;
  }
  setStyleSheet(QStringLiteral(
      "QDialog#OnboardingWizard {"
      "  background-color: #0E0C14;"
      "  background-image: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
      "    stop:0 #14121C, stop:1 #0A0910);"
      "}"
      "QLabel { color: #F4F1FF; background: transparent; }"
      "QCheckBox { color: #F4F1FF; background: transparent; spacing: 8px; }"
      "QCheckBox::indicator { width: 18px; height: 18px; border-radius: 4px;"
      "  border: 1px solid #7C5CFC; background: #1C1730; }"
      "QCheckBox::indicator:checked { background: #7C5CFC; }"));
  QPalette pal = palette();
  pal.setColor(QPalette::Window, QColor(0x0E, 0x0C, 0x14));
  setPalette(pal);
  m_card->setStyleSheet(QStringLiteral(
      "QFrame#OnboardingCard {"
      "  background: #101014;"
      "  border: 1px solid rgba(255,255,255,0.10);"
      "  border-radius: 16px;"
      "}"));
  const int margin = UiScale::dp(48);
  const int availW = qMax(UiScale::dp(280), width() - UiScale::dp(32));
  const int availH = qMax(UiScale::dp(280), height() - UiScale::dp(32));
  const int w = qBound(qMin(UiScale::dp(640), availW),
                       qMin(width() - 2 * margin, UiScale::dp(980)), availW);
  const int h = qBound(qMin(UiScale::dp(460), availH),
                       qMin(height() - 2 * margin, UiScale::dp(640)), availH);
  m_card->setProperty("stepsInner", qMax(80, w - UiScale::dp(56)));
  m_card->setGeometry(qMax(0, (width() - w) / 2), qMax(0, (height() - h) / 2), w,
                      h);
  placeWindowControls();
  syncSteps();
}

bool OnboardingWizard::eventFilter(QObject *watched, QEvent *event) {
  if (watched == parentWidget() &&
      (event->type() == QEvent::Resize || event->type() == QEvent::Move))
    fitToHost();
  return QDialog::eventFilter(watched, event);
}

void OnboardingWizard::reject() {}

void OnboardingWizard::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape || event->key() == Qt::Key_Back)
    return;
  QDialog::keyPressEvent(event);
}

bool OnboardingWizard::hasSignedInName() const {
  return GoogleAuthManager::instance().isAuthenticated() &&
         (!GoogleAuthManager::instance().userName().trimmed().isEmpty() ||
          !GoogleAuthManager::instance().userEmail().trimmed().isEmpty());
}

void OnboardingWizard::refreshWelcome() {
  if (!m_welcomeTitle)
    return;
  m_welcomeTitle->setText(QStringLiteral("Willkommen bei Blop"));
  QString name = GoogleAuthManager::instance().userName().trimmed();
  if (name.isEmpty())
    name = GoogleAuthManager::instance().userEmail().trimmed();
  const bool google = GoogleAuthManager::instance().isAuthenticated();
  if (google) {
    m_accountLine = name.isEmpty()
                        ? QStringLiteral("Mit Google angemeldet")
                        : QStringLiteral("Mit Google angemeldet · %1").arg(name);
  }
  const bool showLine = !m_accountLine.isEmpty();
  if (m_loginStatus) {
    m_loginStatus->setText(m_accountLine);
    m_loginStatus->setVisible(showLine && m_page != PageVideo);
  }
  if (m_welcomeStatus) {
    m_welcomeStatus->setVisible(showLine);
    if (showLine)
      m_welcomeStatus->setText(m_accountLine);
  }
  if (m_btnGoogle) {
    m_btnGoogle->setEnabled(!google);
    m_btnGoogle->setText(google ? QStringLiteral("Mit Google angemeldet")
                                : QStringLiteral("Mit Google anmelden"));
  }
  if (m_page == PageWelcome && m_btnNext)
    m_btnNext->setText(QStringLiteral("Weiter"));
}

void OnboardingWizard::startIntroVideo() {
  if (!m_player || m_introDone || m_introStarted)
    return;
  m_introStarted = true;
  m_introSawFrame = false;
  m_player->setSource(QUrl(QStringLiteral("qrc:/assets/Intro Blop.mp4")));
  m_player->play();
  // No frame means the clip did not start. Leave the black page instead of
  // sitting there until the long safety timer.
  QTimer::singleShot(2500, this, [this]() {
    if (m_introDone || m_page != PageVideo || m_introSawFrame)
      return;
    m_introDone = true;
    stopIntroVideo();
    goTo(PageWelcome);
  });
  QTimer::singleShot(90000, this, [this]() {
    if (m_introDone || m_page != PageVideo)
      return;
    m_introDone = true;
    stopIntroVideo();
    goTo(PageWelcome);
  });
}

void OnboardingWizard::stopIntroVideo() {
  if (m_player)
    m_player->stop();
}

void OnboardingWizard::buildPages() {
  auto *video = new QWidget(m_stack);
  video->setAutoFillBackground(true);
  video->setStyleSheet(QStringLiteral("background-color: #000000;"));
  auto *videoLay = new QVBoxLayout(video);
  videoLay->setContentsMargins(0, 0, 0, 0);
  videoLay->setSpacing(0);
  m_introView = new IntroStage(video);
  videoLay->addWidget(m_introView, 1);
  m_stack->addWidget(video);

  m_btnSkip = new QPushButton(QStringLiteral("Überspringen"), this);
  m_btnSkip->setCursor(Qt::PointingHandCursor);
  m_btnSkip->setStyleSheet(QStringLiteral(
      "QPushButton { color: #F4F1FF; background: rgba(0,0,0,0.45);"
      "  border: 1px solid rgba(255,255,255,0.28); border-radius: 8px;"
      "  padding: 8px 14px; }"));
  connect(m_btnSkip, &QPushButton::clicked, this, [this]() {
    m_introDone = true;
    stopIntroVideo();
    goTo(PageWelcome);
  });

  m_player = new QMediaPlayer(this);
  m_audio = new QAudioOutput(this);
  m_audio->setVolume(0.8);
  m_player->setAudioOutput(m_audio);
  auto *sink = new QVideoSink(this);
  m_player->setVideoSink(sink);
  connect(sink, &QVideoSink::videoFrameChanged, this,
          [this](const QVideoFrame &frame) {
            if (!m_introView)
              return;
            const QImage image = frame.toImage();
            if (image.isNull())
              return;
            m_introSawFrame = true;
            m_introView->setFrame(image);
          });
  connect(m_player, &QMediaPlayer::mediaStatusChanged, this,
          [this](QMediaPlayer::MediaStatus status) {
            if (m_introDone || status != QMediaPlayer::EndOfMedia)
              return;
            m_introDone = true;
            stopIntroVideo();
            goTo(PageWelcome);
          });
  connect(m_player, &QMediaPlayer::positionChanged, this,
          [this](qint64 pos) {
            if (m_introDone || m_page != PageVideo || !m_player)
              return;
            const qint64 dur = m_player->duration();
            if (dur > 800 && pos > 400 && pos + 120 >= dur) {
              m_introDone = true;
              stopIntroVideo();
              goTo(PageWelcome);
            }
          });
  connect(m_player, &QMediaPlayer::errorOccurred, this,
          [this](QMediaPlayer::Error, const QString &) {
            if (m_introDone)
              return;
            m_introDone = true;
            stopIntroVideo();
            goTo(PageWelcome);
          });

  auto *welcomeCol = new QWidget();
  welcomeCol->setFixedWidth(UiScale::dp(320));
  welcomeCol->setStyleSheet(QStringLiteral("background: transparent;"));
  auto *welcomeLay = new QVBoxLayout(welcomeCol);
  welcomeLay->setContentsMargins(0, 0, 0, 0);
  welcomeLay->setSpacing(UiScale::dp(10));
  auto *mark = new QLabel(welcomeCol);
  mark->setAlignment(Qt::AlignCenter);
  mark->setStyleSheet(QStringLiteral("background: transparent;"));
  const QPixmap logo(QStringLiteral(":/assets/logo.jpg"));
  if (!logo.isNull())
    mark->setPixmap(featheredLogo(logo, UiScale::dp(220)));
  welcomeLay->addWidget(mark, 0, Qt::AlignHCenter);
  welcomeLay->addSpacing(UiScale::dp(6));
  m_welcomeTitle = titleLabel(QStringLiteral("Willkommen bei Blop"), welcomeCol);
  m_welcomeTitle->setAlignment(Qt::AlignHCenter);
  welcomeLay->addWidget(m_welcomeTitle);
  auto *lead = bodyLabel(
      QStringLiteral(
          "Die Anmeldung zeigt nur, wer du bist. Den Speicher wählst du "
          "gleich danach — dann liegen neue Notizen in der Cloud."),
      welcomeCol);
  lead->setAlignment(Qt::AlignHCenter);
  welcomeLay->addWidget(lead);
  m_welcomeStatus = bodyLabel(QString(), welcomeCol);
  m_welcomeStatus->setAlignment(Qt::AlignHCenter);
  m_welcomeStatus->setVisible(false);
  welcomeLay->addWidget(m_welcomeStatus);
  welcomeLay->addSpacing(UiScale::dp(10));

  const QString outline = QStringLiteral(
      "QPushButton { background: transparent; color: #F4F1FF;"
      "  border: 1px solid rgba(255,255,255,0.18); border-radius: 12px;"
      "  padding: 0 16px; font-weight: 650; }"
      "QPushButton:hover { background: rgba(255,255,255,0.06); }");

  auto *btnGo = new QPushButton(QStringLiteral("Weiter"), welcomeCol);
  btnGo->setCursor(Qt::PointingHandCursor);
  btnGo->setFixedHeight(UiScale::dp(44));
  btnGo->setStyleSheet(primaryButtonQss());
  connect(btnGo, &QPushButton::clicked, this, [this]() { onNext(); });
  welcomeLay->addWidget(btnGo);

  auto *btnGoogle =
      new QPushButton(QStringLiteral("Mit Google anmelden"), welcomeCol);
  m_btnGoogle = btnGoogle;
  btnGoogle->setCursor(Qt::PointingHandCursor);
  btnGoogle->setFixedHeight(UiScale::dp(44));
  btnGoogle->setStyleSheet(outline);
  connect(btnGoogle, &QPushButton::clicked, this, [this]() {
    if (m_welcomeStatus) {
      m_welcomeStatus->setVisible(true);
      m_welcomeStatus->setText(
          QStringLiteral("Browser öffnet sich zur Anmeldung…"));
    }
    if (m_loginStatus) {
      m_loginStatus->setVisible(true);
      m_loginStatus->setText(
          QStringLiteral("Browser öffnet sich zur Anmeldung…"));
    }
    GoogleAuthManager::instance().login();
  });
  welcomeLay->addWidget(btnGoogle);

  auto *btnStudy =
      new QPushButton(QStringLiteral("Study-Konto"), welcomeCol);
  btnStudy->setCursor(Qt::PointingHandCursor);
  btnStudy->setFixedHeight(UiScale::dp(44));
  btnStudy->setStyleSheet(outline);
  connect(btnStudy, &QPushButton::clicked, this, [this]() {
    m_accountLine = QStringLiteral("Mit Study-Konto angemeldet");
    refreshWelcome();
    QDesktopServices::openUrl(
        QUrl(QStringLiteral("https://www.blop-study.com/login")));
  });
  welcomeLay->addWidget(btnStudy);

  auto *welcomeStage = new QWidget(m_stack);
  welcomeStage->setObjectName(QStringLiteral("WelcomeStage"));
  welcomeStage->setAttribute(Qt::WA_StyledBackground, true);
  welcomeStage->setStyleSheet(QStringLiteral(
      "QWidget#WelcomeStage {"
      "  background: qradialgradient(cx:0.5, cy:0.40, radius:0.72,"
      "    stop:0 #2A2468, stop:0.32 #181532, stop:0.68 #121016, stop:1 #101014);"
      "}"));
  auto *stageLay = new QVBoxLayout(welcomeStage);
  stageLay->setContentsMargins(UiScale::dp(28), 0, UiScale::dp(28), 0);
  stageLay->addStretch(1);
  stageLay->addWidget(welcomeCol, 0, Qt::AlignHCenter);
  stageLay->addStretch(1);
  m_stack->addWidget(welcomeStage);
  refreshWelcome();

  auto *agbCol = new QWidget();
  auto *agbLay = new QVBoxLayout(agbCol);
  agbLay->setContentsMargins(0, 0, 0, 0);
  agbLay->setSpacing(UiScale::dp(12));
  agbLay->addWidget(titleLabel(QStringLiteral("Hinweise zur Nutzung"), agbCol));
  auto *agbBody = bodyLabel(
      QStringLiteral(
          "Deine Notizen gehören dir. Blop speichert sie als Dateien in dem "
          "Ordner, den du im nächsten Schritt wählst (lokal, in einem "
          "Cloud-Sync-Ordner oder in beiden).\n\n"
          "Blop verkauft deine Notizen nicht und lädt sie nicht automatisch "
          "auf einen Blop-Server. Ein optionales Blop-Study-Konto betrifft nur "
          "Anmeldung, Lernen und Teilen über Study — nicht den Notizordner.\n\n"
          "Wenn du einen Cloud-Ordner wählst (zum Beispiel Google Drive oder "
          "Nextcloud), gelten zusätzlich die Bedingungen dieses Anbieters, "
          "weil die Dateien dann in dessen Sync-Ordner liegen.\n\n"
          "Du kannst die Speicherart später unter Einstellungen → Speicher "
          "ändern."),
      agbCol);
  auto *scroll = textScroll(agbBody, agbCol);
  agbLay->addWidget(scroll, 1);
  m_agbCheck = new QCheckBox(
      QStringLiteral("Ich habe die Hinweise gelesen."), agbCol);
  m_agbCheck->setStyleSheet(QStringLiteral(
      "font-size: 14px; background: transparent; color: #F4F1FF;"));
  agbLay->addWidget(m_agbCheck);
  connect(m_agbCheck, &QCheckBox::toggled, this, [this](bool) {
    if (m_page == PageAgb && m_btnNext)
      m_btnNext->setEnabled(m_agbCheck->isChecked());
  });
  m_stack->addWidget(centered(m_stack, agbCol));

  auto *storageCol = new QWidget();
  auto *storageLay = new QVBoxLayout(storageCol);
  storageLay->setContentsMargins(0, 0, 0, 0);
  storageLay->setSpacing(UiScale::dp(10));
  storageLay->addWidget(
      titleLabel(QStringLiteral("Wo sollen Notizen liegen?"), storageCol));
  m_storageGroup = new QButtonGroup(this);
  m_storageGroup->setExclusive(true);
  auto *bLocal = choiceButton(
      QStringLiteral("Nur lokal"),
      QStringLiteral("Schnell und offline. Liegt unter Dokumente/BlopNotizen "
                     "und belegt Speicher auf diesem Gerät."),
      storageCol);
  auto *bCloud = choiceButton(
      QStringLiteral("Nur Cloud-Ordner"),
      QStringLiteral("Schreibt in einen Sync-Ordner (Drive, Nextcloud, …). "
                     "Der lokale App-Ordner bleibt schlank — der Anbieter "
                     "speichert die Dateien."),
      storageCol);
  auto *bBoth = choiceButton(
      QStringLiteral("Lokal und Cloud"),
      QStringLiteral("Schreiben auf dem Gerät und Spiegel in den Cloud-Ordner. "
                     "Offline nutzbar, zusätzlich gesichert, braucht beide "
                     "Speicherplätze."),
      storageCol);
  m_storageGroup->addButton(bLocal, int(StoragePrefs::Mode::LocalOnly));
  m_storageGroup->addButton(bCloud, int(StoragePrefs::Mode::CloudOnly));
  m_storageGroup->addButton(bBoth, int(StoragePrefs::Mode::LocalAndCloud));
  bLocal->setChecked(true);
  m_storageMode = int(StoragePrefs::Mode::LocalOnly);
  storageLay->addWidget(bLocal);
  storageLay->addWidget(bCloud);
  storageLay->addWidget(bBoth);
#ifdef Q_OS_ANDROID
  storageLay->addWidget(bodyLabel(
      QStringLiteral(
          "Auf dem Handy bleiben Notizen auf dem Gerät. Web-Clouds öffnest "
          "du später in der Sidebar — ein Sync-Ordner wird hier nicht "
          "verknüpft. „Nur Cloud“ und „Beides“ merken wir uns als lokal, "
          "bis ein Ordner verfügbar ist."),
      storageCol));
#endif
  connect(m_storageGroup, &QButtonGroup::idClicked, this,
          [this](int id) { m_storageMode = id; });
  m_stack->addWidget(centered(m_stack, storageCol));

  auto *folderCol = new QWidget();
  auto *folderLay = new QVBoxLayout(folderCol);
  folderLay->setContentsMargins(0, 0, 0, 0);
  folderLay->setSpacing(UiScale::dp(12));
  folderLay->addWidget(titleLabel(QStringLiteral("Cloud verbinden"), folderCol));
  folderLay->addWidget(bodyLabel(
      QStringLiteral(
          "Wähle hier den Speicher. Neue Notizen legt Blop dann dort ab, "
          "im Ordner „BlopNotizen“.\n\n"
          "Google Drive, Nextcloud und OneDrive machen das über ihre API. "
          "iCloud nutzt den Ordner von iCloud für Windows."),
      folderCol));
  const QString cloudBtn = QStringLiteral(
      "QPushButton { background: transparent; color: #F4F1FF;"
      "  border: 1px solid rgba(255,255,255,0.18); border-radius: 12px;"
      "  padding: 0 16px; font-weight: 650; }"
      "QPushButton:hover { background: rgba(255,255,255,0.06); }");
  const struct {
    const char *type;
    const char *label;
    bool primary;
  } providers[] = {
      {"googledrive", "Google Drive verbinden", true},
      {"nextcloud", "Nextcloud verbinden", false},
      {"onedrive", "OneDrive verbinden", false},
      {"icloud", "iCloud verbinden", false},
  };
  for (const auto &p : providers) {
    auto *btn = new QPushButton(QString::fromUtf8(p.label), folderCol);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedHeight(UiScale::dp(44));
    btn->setStyleSheet(p.primary ? primaryButtonQss() : cloudBtn);
    const QString type = QString::fromLatin1(p.type);
    connect(btn, &QPushButton::clicked, this, [this, type]() {
      if (m_folderLabel)
        m_folderLabel->setText(QStringLiteral("Verbindung läuft…"));
      CloudLinkHub::instance().connectProvider(type, this);
    });
    folderLay->addWidget(btn);
  }
  auto *pick = new QPushButton(QStringLiteral("Ordner wählen…"), folderCol);
  pick->setCursor(Qt::PointingHandCursor);
  pick->setStyleSheet(cloudBtn);
  pick->setFixedHeight(UiScale::dp(44));
  connect(pick, &QPushButton::clicked, this, [this]() { pickCloudFolder(); });
  folderLay->addWidget(pick);
  m_folderLabel = bodyLabel(QStringLiteral("Noch nichts verbunden."), folderCol);
  folderLay->addWidget(m_folderLabel);
  connect(&CloudLinkHub::instance(), &CloudLinkHub::connectFinished, this,
          [this](const QString &type, bool ok, const QString &detail) {
            if (m_folderLabel)
              m_folderLabel->setText(detail);
            if (!ok)
              return;
            m_linkedProvider = type;
            m_cloudFolder.clear();
            if (m_page == PageFolder && m_btnNext)
              m_btnNext->setEnabled(true);
          });
  m_stack->addWidget(centered(m_stack, folderCol));

  auto *noteCol = new QWidget();
  auto *noteLay = new QVBoxLayout(noteCol);
  noteLay->setContentsMargins(0, 0, 0, 0);
  noteLay->setSpacing(UiScale::dp(12));
  noteLay->addWidget(titleLabel(QStringLiteral("Neue Notiz"), noteCol));
  noteLay->addWidget(bodyLabel(
      QStringLiteral(
          "Über „Neue Notiz“ (Seitenleiste oder Plus) legst du ein Heft an: "
          "unendliche Leinwand, DIN A4 oder Struktur.\n\n"
          "Das Format bleibt danach fest. Papier und Tags wählst du beim "
          "Anlegen."),
      noteCol));
  m_stack->addWidget(centered(m_stack, noteCol));

  auto *libCol = new QWidget();
  auto *libLay = new QVBoxLayout(libCol);
  libLay->setContentsMargins(0, 0, 0, 0);
  libLay->setSpacing(UiScale::dp(12));
  libLay->addWidget(
      titleLabel(QStringLiteral("Bibliothek, Papierkorb, Tags"), libCol));
  libLay->addWidget(bodyLabel(
      QStringLiteral(
          "Die Bibliothek zeigt deine Dateien. Mehrere Notizen markierst du "
          "und verschiebst sie mit Entf in den Papierkorb — dort kannst du "
          "sie wiederherstellen.\n\n"
          "Tags filterst du in der Sidebar unter CLOUD. CLOUD selbst öffnet "
          "Web-Clouds und synchronisiert die Notizen nicht von allein."),
      libCol));
  m_stack->addWidget(centered(m_stack, libCol));

  auto *setCol = new QWidget();
  auto *setLay = new QVBoxLayout(setCol);
  setLay->setContentsMargins(0, 0, 0, 0);
  setLay->setSpacing(UiScale::dp(12));
  setLay->addWidget(titleLabel(QStringLiteral("Einstellungen"), setCol));
  setLay->addWidget(bodyLabel(
      QStringLiteral(
          "Unter Einstellungen änderst du Speicher, Erscheinungsbild und "
          "Tastaturkürzel. Diese Einrichtung kannst du dort erneut öffnen.\n\n"
          "Damit bist du startklar — die Notizen erscheinen erst jetzt."),
      setCol));
  m_stack->addWidget(centered(m_stack, setCol));
}

void OnboardingWizard::syncSteps() {
  const int current = m_page - int(PageWelcome);
  for (int i = 0; i < m_stepLabels.size(); ++i) {
    const bool on = i == current;
    const bool done = i < current && current >= 0;
    QString labelColor = QStringLiteral("#6E6484");
    QString weight = QStringLiteral("500");
    QString dot = QStringLiteral("#3A3350");
    if (on) {
      labelColor = QStringLiteral("#F4F1FF");
      weight = QStringLiteral("650");
      dot = QStringLiteral("#7C5CFC");
    } else if (done) {
      labelColor = QStringLiteral("#A89BC4");
      dot = QStringLiteral("#5B4A96");
    }
    m_stepLabels.at(i)->setStyleSheet(
        QStringLiteral("font-size: 13px; font-weight: %1; color: %2;"
                       "background: transparent;")
            .arg(weight, labelColor));
    const int inner = m_card ? m_card->property("stepsInner").toInt() : 0;
    const int cellW =
        m_stepLabels.isEmpty() ? 0 : qMax(36, inner / m_stepLabels.size());
    const QString full = m_stepLabels.at(i)->property("stepName").toString();
    QLabel *lab = m_stepLabels.at(i);
    if (cellW > 8) {
      if (QWidget *cell = lab->parentWidget())
        cell->setFixedWidth(cellW);
      lab->setFixedWidth(qMax(8, cellW - 4));
      if (!full.isEmpty()) {
        const QFontMetrics fm(lab->font());
        lab->setText(fm.elidedText(full, Qt::ElideRight, qMax(8, cellW - 8)));
      }
    }
    if (i < m_stepDots.size() && m_stepDots.at(i)) {
      m_stepDots.at(i)->setStyleSheet(
          QStringLiteral("background: %1; border: none; border-radius: 4px;")
              .arg(dot));
    }
  }
}

void OnboardingWizard::goTo(int page) {
  m_page = qBound(0, page, m_stack->count() - 1);
  m_stack->setCurrentIndex(m_page);
  const bool video = m_page == PageVideo;
  if (m_brand)
    m_brand->setVisible(!video);
  if (m_loginStatus)
    m_loginStatus->setVisible(!video && !m_accountLine.isEmpty());
  if (m_footer)
    m_footer->setVisible(!video && m_page != PageWelcome);
  layoutCard();
  syncSteps();
  if (m_btnBack)
    m_btnBack->setVisible(m_page > PageWelcome);
  const bool last = m_page == m_stack->count() - 1;
  if (m_page == PageWelcome)
    m_btnNext->setText(QStringLiteral("Weiter"));
  else
    m_btnNext->setText(last ? QStringLiteral("Fertig")
                            : QStringLiteral("Weiter"));
  bool enable = true;
  if (m_page == PageAgb)
    enable = m_agbCheck && m_agbCheck->isChecked();
  if (m_page == PageFolder)
    enable = !m_cloudFolder.isEmpty() || !m_linkedProvider.isEmpty();
  m_btnNext->setEnabled(enable);
  if (m_page == PageWelcome)
    refreshWelcome();
}

bool OnboardingWizard::pickCloudFolder() {
  QString start = StoragePrefs::bestSuggestedRootForProvider(
      QStringLiteral("custom"));
  if (start.isEmpty())
    start = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
  const QString folder = QFileDialog::getExistingDirectory(
      this, QStringLiteral("Sync-Ordner für Blop-Notizen"), start);
  if (folder.isEmpty())
    return false;
  m_cloudFolder = folder;
  m_linkedProvider.clear();
  if (m_folderLabel)
    m_folderLabel->setText(folder);
  if (m_page == PageFolder && m_btnNext)
    m_btnNext->setEnabled(true);
  return true;
}

bool OnboardingWizard::applyStorageChoice() {
#ifdef Q_OS_ANDROID
  Q_UNUSED(m_cloudFolder);
  StoragePrefs::setMode(StoragePrefs::Mode::LocalOnly);
  return true;
#else
  const auto mode = static_cast<StoragePrefs::Mode>(m_storageMode);
  if (mode == StoragePrefs::Mode::LocalOnly) {
    StoragePrefs::setMode(StoragePrefs::Mode::LocalOnly);
    return true;
  }
  if (!m_linkedProvider.isEmpty() &&
      CloudLinkHub::instance().providerReady(m_linkedProvider)) {
    StoragePrefs::setPrimaryCloudId(m_linkedProvider);
    StoragePrefs::setMode(mode);
    return true;
  }
  if (m_cloudFolder.isEmpty() ||
      !StoragePrefs::connectProviderForNotes(QStringLiteral("custom"),
                                             m_cloudFolder)) {
    BlopDialogs::notify(
        this, QStringLiteral("Speicher"),
        QStringLiteral("Der Ordner konnte nicht verknüpft werden."));
    return false;
  }
  if (mode == StoragePrefs::Mode::CloudOnly)
    StoragePrefs::setMode(StoragePrefs::Mode::CloudOnly);
  return true;
#endif
}

void OnboardingWizard::onBack() {
  if (m_page <= PageWelcome)
    return;
  int prev = m_page - 1;
  if (prev == PageFolder &&
      m_storageMode == int(StoragePrefs::Mode::LocalOnly))
    prev = PageStorage;
#ifdef Q_OS_ANDROID
  if (prev == PageFolder)
    prev = PageStorage;
#endif
  if (prev == PageVideo)
    return;
  goTo(prev);
}

void OnboardingWizard::onNext() {
  if (m_page == PageVideo) {
    m_introDone = true;
    stopIntroVideo();
    goTo(PageWelcome);
    return;
  }

  if (m_page == PageAgb && !(m_agbCheck && m_agbCheck->isChecked()))
    return;

  if (m_page == PageStorage) {
#ifdef Q_OS_ANDROID
    goTo(PageNewNote);
#else
    if (m_storageMode == int(StoragePrefs::Mode::LocalOnly))
      goTo(PageNewNote);
    else
      goTo(PageFolder);
#endif
    return;
  }

  if (m_page == PageFolder) {
    if (m_cloudFolder.isEmpty() && m_linkedProvider.isEmpty())
      return;
    goTo(PageNewNote);
    return;
  }

  if (m_page >= m_stack->count() - 1) {
    if (!applyStorageChoice())
      return;
    QSettings st(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
    st.setValue(QStringLiteral("ui/onboardingDone"), true);
    m_completed = true;
    accept();
    return;
  }

  goTo(m_page + 1);
}
