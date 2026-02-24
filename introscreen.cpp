#include "introscreen.h"
#include <QApplication>
#include <QFontDatabase>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>

IntroScreen::IntroScreen(QWidget *parent)
    : QWidget(parent), m_dropY(-200.0), m_dropStretch(1.0),
      m_splashProgress(0.0), m_logoOpacity(0.0), m_textOpacity(0.0),
      m_finished(false) {
  setWindowFlags(Qt::Window | Qt::FramelessWindowHint |
                 Qt::WindowStaysOnTopHint);
  setAttribute(Qt::WA_TranslucentBackground);

  QScreen *screen = QApplication::primaryScreen();
  QRect screenGeom = screen->geometry();
  setGeometry(screenGeom);

  // Start Y is way off screen top, end Y is center
  int cy = screenGeom.height() / 2;
  int logoBaseY = cy - 40;

  m_mainSequence = new QSequentialAnimationGroup(this);

  // 1. Drop falling
  QParallelAnimationGroup *fallGroup = new QParallelAnimationGroup(this);
  QPropertyAnimation *dropYAnim = new QPropertyAnimation(this, "dropY", this);
  dropYAnim->setDuration(600);
  dropYAnim->setStartValue(logoBaseY - 600.0);
  dropYAnim->setEndValue(qreal(logoBaseY));
  dropYAnim->setEasingCurve(QEasingCurve::InCubic);

  QPropertyAnimation *dropStretchAnim =
      new QPropertyAnimation(this, "dropStretch", this);
  dropStretchAnim->setDuration(600);
  dropStretchAnim->setStartValue(1.8);
  dropStretchAnim->setEndValue(1.0);
  dropStretchAnim->setEasingCurve(QEasingCurve::OutQuad);

  fallGroup->addAnimation(dropYAnim);
  fallGroup->addAnimation(dropStretchAnim);

  // 2. Splash / Morph
  QPropertyAnimation *splashAnim =
      new QPropertyAnimation(this, "splashProgress", this);
  splashAnim->setDuration(500);
  splashAnim->setStartValue(0.0);
  splashAnim->setEndValue(1.0);
  splashAnim->setEasingCurve(QEasingCurve::OutBack); // Bouncy splash

  // 3. Logo & Text Reveal
  QParallelAnimationGroup *revealGroup = new QParallelAnimationGroup(this);
  QPropertyAnimation *logoAnim =
      new QPropertyAnimation(this, "logoOpacity", this);
  logoAnim->setDuration(600);
  logoAnim->setStartValue(0.0);
  logoAnim->setEndValue(1.0);

  QPropertyAnimation *textAnim =
      new QPropertyAnimation(this, "textOpacity", this);
  textAnim->setDuration(600);
  textAnim->setStartValue(0.0);
  textAnim->setEndValue(1.0);
  textAnim->setEasingCurve(QEasingCurve::InOutQuad);

  revealGroup->addAnimation(logoAnim);
  revealGroup->addAnimation(textAnim);

  // 4. Final pause and screen fade
  QPropertyAnimation *pauseAnim =
      new QPropertyAnimation(this, "windowOpacity", this);
  pauseAnim->setDuration(800);
  pauseAnim->setStartValue(1.0);
  pauseAnim->setEndValue(1.0);

  QPropertyAnimation *fadeOut =
      new QPropertyAnimation(this, "windowOpacity", this);
  fadeOut->setDuration(600);
  fadeOut->setStartValue(1.0);
  fadeOut->setEndValue(0.0);
  fadeOut->setEasingCurve(QEasingCurve::InOutQuad);

  m_mainSequence->addAnimation(fallGroup);
  m_mainSequence->addAnimation(splashAnim);
  m_mainSequence->addAnimation(revealGroup);
  m_mainSequence->addAnimation(pauseAnim);
  m_mainSequence->addAnimation(fadeOut);

  connect(m_mainSequence, &QSequentialAnimationGroup::finished, this,
          &IntroScreen::finishIntro);
}

IntroScreen::~IntroScreen() {}

void IntroScreen::startAnimation() {
  show();
  m_mainSequence->start();
}

void IntroScreen::finishIntro() {
  if (m_finished)
    return;
  m_finished = true;
  m_mainSequence->stop();
  emit introFinished();
}

void IntroScreen::mousePressEvent(QMouseEvent *event) {
  event->accept();
  finishIntro();
}

void IntroScreen::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  // Schwarz füllen
  painter.fillRect(rect(), QColor("#121212"));

  int cx = width() / 2;
  int cy = height() / 2;
  int logoBaseY = cy - 40;

  QColor electricBlue(47, 82, 224); // Angenähertes Blop-Blau

  // Zeichne Vektor-Formen für Fall und Impact
  if (m_logoOpacity < 1.0) {
    painter.setPen(Qt::NoPen);

    // Wenn das Morphing noch nicht läuft, zeichnen wir den Tropfen
    if (m_splashProgress == 0.0) {
      painter.setBrush(electricBlue);
      int dWidth = 30 / m_dropStretch;
      int dHeight = 30 * m_dropStretch;

      painter.drawEllipse(
          QRectF(cx - dWidth / 2.0, m_dropY - dHeight / 2.0, dWidth, dHeight));

      // Schleppe einen kleinen Schweif mit
      QPainterPath tail;
      tail.moveTo(cx - dWidth / 2.0, m_dropY);
      tail.lineTo(cx, m_dropY - dHeight * 1.5);
      tail.lineTo(cx + dWidth / 2.0, m_dropY);
      painter.drawPath(tail);

    } else if (m_splashProgress > 0.0) {
      // Splash Interpolation
      // Zentraler Kreis wächst und "morpht" zu der breiteren Blop-Form
      qreal p = m_splashProgress;
      qreal baseR = 15 + (130 * p);

      painter.setBrush(electricBlue);

      // Center blob
      painter.drawEllipse(
          QRectF(cx - baseR / 2.0, logoBaseY - baseR / 2.0, baseR, baseR));

      // Satelliten-Kleckse, die eine Wolke formen
      qreal r1 = baseR * 0.75 * p;
      painter.drawEllipse(QRectF(cx - baseR * 0.65 - r1 / 2.0,
                                 logoBaseY - baseR * 0.2 - r1 / 2.0, r1, r1));

      qreal r2 = baseR * 0.6 * p;
      painter.drawEllipse(QRectF(cx + baseR * 0.6 - r2 / 2.0,
                                 logoBaseY - baseR * 0.1 - r2 / 2.0, r2, r2));

      qreal r3 = baseR * 0.5 * p;
      painter.drawEllipse(QRectF(cx + baseR * 0.25 - r3 / 2.0,
                                 logoBaseY + baseR * 0.45 - r3 / 2.0, r3, r3));

      qreal r4 = baseR * 0.45 * p;
      painter.drawEllipse(QRectF(cx - baseR * 0.35 - r4 / 2.0,
                                 logoBaseY + baseR * 0.4 - r4 / 2.0, r4, r4));

      // --- NIB CUTOUT & DECORATIONS ---
      // Die Logo-Silhouette (Füllfederhalter + Punkt) wird als Cutout aus dem
      // Blob generiert
      if (m_logoOpacity > 0.0) {
        painter.setOpacity(m_logoOpacity);

        painter.translate(cx, logoBaseY);
        qreal s = baseR / 130.0;
        painter.scale(s, s);

        // Die Blop Feder zeigt nach unten-links
        painter.rotate(45);

        // 1. Der Cutout der Feder (zeichnet Background-Color auf das Blau)
        painter.setBrush(QColor("#121212"));
        painter.setPen(Qt::NoPen);

        QPainterPath nib;
        nib.moveTo(0, 35);           // Tip of pen
        nib.lineTo(-14, 5);          // Left shoulder
        nib.lineTo(-10, -25);        // Left base
        nib.quadTo(0, -35, 10, -25); // Base curve
        nib.lineTo(14, 5);           // Right shoulder
        nib.closeSubpath();

        painter.drawPath(nib);

        // 2. Das Herz/Loch der Feder und der Schlitz (wieder Blau in den Cutout
        // gemalt)
        painter.setBrush(electricBlue);
        painter.drawEllipse(QRectF(-4, -6, 8, 8)); // Breather hole

        painter.setPen(QPen(electricBlue, 2.5, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(0, -2, 0, 32); // Slit

        // 3. Dekorativer Cutout-Punkt ("Dot") neben der Feder
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#121212"));
        painter.drawEllipse(QRectF(22, -10, 14, 14));

        // Reset transformation state for the text rendering
        painter.resetTransform();
        painter.setOpacity(1.0);
      }
    }
  }

  // Zeige den Text "BLOP"
  if (m_textOpacity > 0.0) {
    painter.setOpacity(m_textOpacity);
    QFont font = painter.font();
    font.setPixelSize(36);
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(QColor("#FFFFFF"));

    int textYOffset = 15 * (1.0 - m_textOpacity);
    QRect textRect(0, logoBaseY + 105 + textYOffset, width(), 50);
    painter.drawText(textRect, Qt::AlignCenter, "BLOP");

    painter.setOpacity(1.0);
  }
}
