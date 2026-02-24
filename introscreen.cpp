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
  dropYAnim->setDuration(500);
  dropYAnim->setStartValue(logoBaseY - 800.0);
  dropYAnim->setEndValue(qreal(logoBaseY));
  dropYAnim->setEasingCurve(QEasingCurve::InCubic);

  QPropertyAnimation *dropStretchAnim =
      new QPropertyAnimation(this, "dropStretch", this);
  dropStretchAnim->setDuration(500);
  dropStretchAnim->setStartValue(2.0); // More dramatic stretch
  dropStretchAnim->setEndValue(0.5);   // Squash heavily on impact
  dropStretchAnim->setEasingCurve(QEasingCurve::InExpo);

  fallGroup->addAnimation(dropYAnim);
  fallGroup->addAnimation(dropStretchAnim);

  // 2. Splash / Morph
  QPropertyAnimation *splashAnim =
      new QPropertyAnimation(this, "splashProgress", this);
  splashAnim->setDuration(1000); // Longer for the elastic settle
  splashAnim->setStartValue(0.0);
  splashAnim->setEndValue(1.0);
  splashAnim->setEasingCurve(
      QEasingCurve::OutElastic); // Extremely bouncy, exciting splash

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
      qreal p = m_splashProgress;
      painter.setBrush(electricBlue);

      painter.translate(cx, logoBaseY);

      // Blob scales up elastically
      qreal blobScale = p * 1.5;
      painter.scale(blobScale, blobScale);

      // Zeichne exakte, weiche Bezier-Hüllkurve des originalen Logos
      QPainterPath blob;
      blob.moveTo(-14.0, -38.1);
      blob.quadTo(-20.6, -36.5, -32.7, -23.4);
      blob.quadTo(-44.8, -10.2, -45.3, 1.0);
      blob.quadTo(-45.8, 12.1, -40.3, 17.1);
      blob.quadTo(-34.9, 22.2, -26.1, 22.7);
      blob.quadTo(-17.3, 23.2, -13.7, 29.0);
      blob.quadTo(-10.1, 34.8, 10.8, 38.4);
      blob.quadTo(31.6, 42.0, 31.2, 32.6);
      blob.quadTo(30.9, 23.2, 37.4, 12.4);
      blob.quadTo(43.9, 1.7, 36.9, -13.6);
      blob.quadTo(29.9, -28.9, 11.3, -34.3);
      blob.quadTo(-7.3, -39.7, -14.0, -38.1);

      painter.drawPath(blob);

      // --- NIB CUTOUT & DECORATIONS ---
      // Die Logo-Silhouette wird ausgestanzt
      if (m_logoOpacity > 0.0) {
        // Die Blop Feder zeigt nach unten-links
        painter.rotate(45);

        // 1. Der Cutout der Feder (zeichnet Background-Color auf das Blau)
        painter.setBrush(QColor("#121212"));
        painter.setPen(Qt::NoPen);

        QPainterPath nib;
        nib.moveTo(0, 32);
        nib.lineTo(-12, 5);
        nib.lineTo(-9, -20);
        nib.quadTo(0, -30, 9, -20);
        nib.lineTo(12, 5);
        nib.closeSubpath();

        painter.setOpacity(m_logoOpacity); // Reveal nib smoothly
        painter.drawPath(nib);

        // 2. Das Herz/Loch der Feder und der Schlitz
        painter.setBrush(electricBlue);
        painter.drawEllipse(QRectF(-3.5, -6, 7, 7));

        painter.setPen(QPen(electricBlue, 2.0, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(0, -2, 0, 28);

        // 3. Dekorativer Punkt neben der Feder
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#121212"));
        painter.drawEllipse(QRectF(22, -10, 12, 12));
      }
      painter.resetTransform();
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
