#include "introscreen.h"
#include <QApplication>
#include <QPainter>
#include <QScreen>


IntroScreen::IntroScreen(QWidget *parent)
    : QWidget(parent), m_frameProgress(0.0), m_finished(false) {
  // Modern frameless window setup, stays on top
  setWindowFlags(Qt::Window | Qt::FramelessWindowHint |
                 Qt::WindowStaysOnTopHint);
  setAttribute(Qt::WA_TranslucentBackground);

  // Full screen size, centered
  QScreen *screen = QApplication::primaryScreen();
  QRect screenGeom = screen->geometry();
  setGeometry(screenGeom);

  // Load sequence images
  QStringList imagePaths = {":/assets/Intro1.png", ":/assets/Intro2.png",
                            ":/assets/Intro3.png", ":/assets/Intro4.png",
                            ":/assets/Intro5.png"};

  // Wir skalieren die Bilder auf eine feste Größe relativ zum Bildschirm, z.B.
  // 800x600 oder 1000x1000, um die Animation scharf und performant zu halten.
  int targetSize = qMin(screenGeom.width(), screenGeom.height()) * 0.8;

  for (const QString &path : imagePaths) {
    QPixmap pix(path);
    if (!pix.isNull()) {
      m_images.append(pix.scaled(targetSize, targetSize, Qt::KeepAspectRatio,
                                 Qt::SmoothTransformation));
    }
  }

  // Setup Animation Sequence
  m_mainSequence = new QSequentialAnimationGroup(this);

  // 1. Image sequence crossfade (0.0 to 4.0)
  QPropertyAnimation *sequenceAnim =
      new QPropertyAnimation(this, "frameProgress", this);
  sequenceAnim->setDuration(1600); // 1.6 seconds for the morphing drop
  sequenceAnim->setStartValue(0.0);
  sequenceAnim->setEndValue(qreal(qMax(0, m_images.size() - 1)));
  sequenceAnim->setEasingCurve(QEasingCurve::InOutQuad);

  // Pause before transition
  QPropertyAnimation *pauseAnim =
      new QPropertyAnimation(this, "windowOpacity", this);
  pauseAnim->setDuration(1200); // Wait 1.2s to show final logo
  pauseAnim->setStartValue(1.0);
  pauseAnim->setEndValue(1.0); // essentially a delay

  // 2. Screen fade out (Widget property)
  QPropertyAnimation *screenFadeOut =
      new QPropertyAnimation(this, "windowOpacity", this);
  screenFadeOut->setDuration(600);
  screenFadeOut->setStartValue(1.0);
  screenFadeOut->setEndValue(0.0);
  screenFadeOut->setEasingCurve(QEasingCurve::InOutQuad);

  // Add everything to the sequence
  m_mainSequence->addAnimation(sequenceAnim);
  m_mainSequence->addAnimation(pauseAnim);
  m_mainSequence->addAnimation(screenFadeOut);

  connect(m_mainSequence, &QSequentialAnimationGroup::finished, this,
          &IntroScreen::finishIntro);
}

IntroScreen::~IntroScreen() {}

void IntroScreen::startAnimation() {
  if (m_images.isEmpty()) {
    finishIntro();
    return;
  }
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
  // Skip animation on click
  finishIntro();
}

void IntroScreen::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  // Paint a solid dark background for the splash
  painter.fillRect(rect(), QColor("#121212"));

  if (m_images.isEmpty())
    return;

  int cx = width() / 2;
  int cy = height() / 2;

  int maxIndex = m_images.size() - 1;

  // Aktueller und nächster Index
  int lowerIndex = qBound(0, static_cast<int>(m_frameProgress), maxIndex);
  int upperIndex = qBound(0, lowerIndex + 1, maxIndex);

  // Wie weit sind wir zwischen lower und upper?
  qreal fraction = m_frameProgress - lowerIndex;

  const QPixmap &pixLower = m_images[lowerIndex];
  int lw = pixLower.width();
  int lh = pixLower.height();
  QRect rectLower(cx - lw / 2, cy - lh / 2, lw, lh);

  if (fraction <= 0.0 || lowerIndex == maxIndex) {
    // Exakt auf einem Frame, render voll
    painter.drawPixmap(rectLower, pixLower);
  } else {
    // Crossfade zwischen 2 Frames zeichnen
    const QPixmap &pixUpper = m_images[upperIndex];
    int uw = pixUpper.width();
    int uh = pixUpper.height();
    QRect rectUpper(cx - uw / 2, cy - uh / 2, uw, uh);

    // Unteres Bild ausblenden
    painter.setOpacity(1.0 - fraction);
    painter.drawPixmap(rectLower, pixLower);

    // Oberes Bild einblenden
    painter.setOpacity(fraction);
    painter.drawPixmap(rectUpper, pixUpper);
  }
}
