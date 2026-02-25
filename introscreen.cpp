#include "introscreen.h"
#include <QApplication>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QRandomGenerator>
#include <QScreen>
#include <QTimerEvent>
#include <cmath>

// ============= Constructor =============
IntroScreen::IntroScreen(QWidget *parent)
    : QWidget(parent), m_dropY(0.0), m_dropScaleX(1.0), m_dropScaleY(1.0),
      m_splash(0.0), m_nibReveal(0.0), m_textFade(0.0), m_particleTimer(0),
      m_finished(false) {
  setWindowFlags(Qt::Window | Qt::FramelessWindowHint |
                 Qt::WindowStaysOnTopHint);
  setAttribute(Qt::WA_TranslucentBackground);
  setAttribute(Qt::WA_OpaquePaintEvent, false);

  QScreen *screen = QApplication::primaryScreen();
  QRect geo = screen->geometry();
  setGeometry(geo);

  m_cx = geo.width() / 2;
  m_cy = geo.height() / 2;

  // ---- Build animation sequence ----
  m_seq = new QSequentialAnimationGroup(this);

  // 1. FALL: drop accelerates from above; scaleY stretches, scaleX narrows
  auto *fallGroup = new QParallelAnimationGroup(this);

  auto *aDropY = new QPropertyAnimation(this, "dropY", this);
  aDropY->setDuration(550);
  aDropY->setStartValue(qreal(m_cy - geo.height() * 0.55));
  aDropY->setEndValue(qreal(m_cy));
  aDropY->setEasingCurve(QEasingCurve::InQuad);

  auto *aScaleY = new QPropertyAnimation(this, "dropScaleY", this);
  aScaleY->setDuration(550);
  aScaleY->setStartValue(1.6);
  aScaleY->setEndValue(1.0);
  aScaleY->setEasingCurve(QEasingCurve::InCubic);

  auto *aScaleX = new QPropertyAnimation(this, "dropScaleX", this);
  aScaleX->setDuration(550);
  aScaleX->setStartValue(0.55);
  aScaleX->setEndValue(1.0);
  aScaleX->setEasingCurve(QEasingCurve::InCubic);

  fallGroup->addAnimation(aDropY);
  fallGroup->addAnimation(aScaleY);
  fallGroup->addAnimation(aScaleX);
  m_seq->addAnimation(fallGroup);

  // 2. SQUASH on impact:  scaleX wide, scaleY flat
  auto *squashGroup = new QParallelAnimationGroup(this);

  auto *sqX = new QPropertyAnimation(this, "dropScaleX", this);
  sqX->setDuration(120);
  sqX->setStartValue(1.0);
  sqX->setEndValue(2.4);
  sqX->setEasingCurve(QEasingCurve::OutCubic);

  auto *sqY = new QPropertyAnimation(this, "dropScaleY", this);
  sqY->setDuration(120);
  sqY->setStartValue(1.0);
  sqY->setEndValue(0.35);
  sqY->setEasingCurve(QEasingCurve::OutCubic);

  squashGroup->addAnimation(sqX);
  squashGroup->addAnimation(sqY);
  m_seq->addAnimation(squashGroup);

  // 3. SPLASH / MORPH  (starts the particle timer)
  auto *splashAnim = new QPropertyAnimation(this, "splash", this);
  splashAnim->setDuration(900);
  splashAnim->setStartValue(0.0);
  splashAnim->setEndValue(1.0);
  splashAnim->setEasingCurve(QEasingCurve::OutElastic);
  m_seq->addAnimation(splashAnim);

  // 4. NIB / pen icon reveals
  auto *nibAnim = new QPropertyAnimation(this, "nibReveal", this);
  nibAnim->setDuration(500);
  nibAnim->setStartValue(0.0);
  nibAnim->setEndValue(1.0);
  nibAnim->setEasingCurve(QEasingCurve::OutCubic);
  m_seq->addAnimation(nibAnim);

  // 5. Text fades in
  auto *textAnim = new QPropertyAnimation(this, "textFade", this);
  textAnim->setDuration(500);
  textAnim->setStartValue(0.0);
  textAnim->setEndValue(1.0);
  textAnim->setEasingCurve(QEasingCurve::InOutQuad);
  m_seq->addAnimation(textAnim);

  // 6. Hold
  auto *hold = new QPropertyAnimation(this, "windowOpacity", this);
  hold->setDuration(900);
  hold->setStartValue(1.0);
  hold->setEndValue(1.0);
  m_seq->addAnimation(hold);

  // 7. Fade out
  auto *fadeOut = new QPropertyAnimation(this, "windowOpacity", this);
  fadeOut->setDuration(600);
  fadeOut->setStartValue(1.0);
  fadeOut->setEndValue(0.0);
  fadeOut->setEasingCurve(QEasingCurve::InOutQuad);
  m_seq->addAnimation(fadeOut);

  connect(m_seq, &QSequentialAnimationGroup::finished, this,
          &IntroScreen::finishIntro);
}

void IntroScreen::startAnimation() {
  show();
  m_seq->start();
  m_particleTimer = startTimer(16); // 60 fps particle update
}

void IntroScreen::finishIntro() {
  if (m_finished)
    return;
  m_finished = true;
  if (m_particleTimer) {
    killTimer(m_particleTimer);
    m_particleTimer = 0;
  }
  m_seq->stop();
  emit introFinished();
}

void IntroScreen::mousePressEvent(QMouseEvent *e) {
  e->accept();
  finishIntro();
}

// ============= Particle System =============
void IntroScreen::spawnParticles() {
  m_particles.clear();
  auto *rng = QRandomGenerator::global();
  for (int i = 0; i < 20; ++i) {
    Particle p;
    p.pos = QPointF(m_cx + int(rng->bounded(60u)) - 30, m_cy);
    qreal angle = rng->bounded(180u) * M_PI / 180.0; // upward hemisphere
    qreal speed = 2.0 + rng->bounded(40u) / 10.0;
    p.vel = QPointF(std::cos(angle) * speed,
                    -std::abs(std::sin(angle)) * speed * 1.3);
    p.life = 1.0;
    p.size = 2.0 + rng->bounded(30u) / 10.0;
    m_particles.append(p);
  }
}

void IntroScreen::timerEvent(QTimerEvent *) {
  for (auto &p : m_particles) {
    p.pos += p.vel;
    p.vel.ry() += 0.15; // gravity
    p.life -= 0.03;
  }
  m_particles.erase(
      std::remove_if(m_particles.begin(), m_particles.end(),
                     [](const Particle &p) { return p.life <= 0; }),
      m_particles.end());
  if (!m_particles.isEmpty())
    update();
}

// ============= Draw Helpers =============
void IntroScreen::drawGlowShape(QPainter &p, const QPainterPath &path,
                                const QColor &col, int layers) {
  for (int i = layers; i >= 1; --i) {
    qreal w = i * 12.0;
    qreal alpha = 0.08 + 0.04 * (layers - i);
    QPen gpen(QColor(col.red(), col.green(), col.blue(), int(255 * alpha)), w,
              Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(gpen);
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);
  }
}

// Teardrop: center at (0,0), pointing down
QPainterPath IntroScreen::buildDropPath(qreal sx, qreal sy) const {
  // Parametric teardrop based on scale sx (width) and sy (height)
  qreal rw = 22 * sx;
  qreal rh = 28 * sy;
  QPainterPath path;
  // Main body is an ellipse
  path.addEllipse(QRectF(-rw, -rh * 0.5, rw * 2, rh));
  // Tip (triangle pointing down)
  QPainterPath tip;
  tip.moveTo(0, rh * 0.5);
  tip.lineTo(-rw * 0.35, rh * 0.1);
  tip.lineTo(rw * 0.35, rh * 0.1);
  tip.closeSubpath();
  return path.united(tip);
}

// Logo blob at target scale
QPainterPath IntroScreen::buildBlobPath(qreal scale) const {
  // These Bezier points were extracted from the real logo via Python/OpenCV
  QPainterPath blob;
  blob.moveTo(-14.0 * scale, -38.1 * scale);
  blob.quadTo(-20.6 * scale, -36.5 * scale, -32.7 * scale, -23.4 * scale);
  blob.quadTo(-44.8 * scale, -10.2 * scale, -45.3 * scale, 1.0 * scale);
  blob.quadTo(-45.8 * scale, 12.1 * scale, -40.3 * scale, 17.1 * scale);
  blob.quadTo(-34.9 * scale, 22.2 * scale, -26.1 * scale, 22.7 * scale);
  blob.quadTo(-17.3 * scale, 23.2 * scale, -13.7 * scale, 29.0 * scale);
  blob.quadTo(-10.1 * scale, 34.8 * scale, 10.8 * scale, 38.4 * scale);
  blob.quadTo(31.6 * scale, 42.0 * scale, 31.2 * scale, 32.6 * scale);
  blob.quadTo(30.9 * scale, 23.2 * scale, 37.4 * scale, 12.4 * scale);
  blob.quadTo(43.9 * scale, 1.7 * scale, 36.9 * scale, -13.6 * scale);
  blob.quadTo(29.9 * scale, -28.9 * scale, 11.3 * scale, -34.3 * scale);
  blob.quadTo(-7.3 * scale, -39.7 * scale, -14.0 * scale, -38.1 * scale);
  return blob;
}

// ============= Paint Event =============
void IntroScreen::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  // Black background
  p.fillRect(rect(), QColor("#0a0a0f"));

  const QColor deepBlue(32, 66, 220);
  const QColor brightBlue(78, 130, 255);
  const QColor glowBlue(100, 160, 255);

  // ====== PHASE 1 & 2: Falling drop + squash ======
  if (m_splash < 0.01) {
    p.save();
    p.translate(m_cx, m_dropY);

    QPainterPath drop = buildDropPath(m_dropScaleX, m_dropScaleY);

    // Energy trail
    if (m_dropScaleY > 1.0) {
      for (int i = 1; i <= 5; ++i) {
        qreal alpha = (6 - i) * 0.06;
        p.setOpacity(alpha);
        QColor tc(brightBlue.red(), brightBlue.green(), brightBlue.blue(),
                  int(255 * alpha));
        QPainterPath trail;
        qreal ty = -i * 18 * m_dropScaleY;
        trail.addEllipse(
            QRectF(-8 * m_dropScaleX, ty - 10, 16 * m_dropScaleX, 20));
        p.fillPath(trail, tc);
      }
      p.setOpacity(1.0);
    }

    // Glow around drop
    drawGlowShape(p, drop, glowBlue, 4);

    // Gradient fill of drop
    QLinearGradient dropGrad(0, -28 * m_dropScaleY, 0, 28 * m_dropScaleY);
    dropGrad.setColorAt(0.0, QColor(120, 175, 255));
    dropGrad.setColorAt(0.5, brightBlue);
    dropGrad.setColorAt(1.0, deepBlue);
    p.setBrush(dropGrad);
    p.setPen(Qt::NoPen);
    p.drawPath(drop);

    p.restore();
  }

  // ====== PHASE 3: SPLASH → BLOB ======
  if (m_splash > 0.0) {
    // Particles
    for (const auto &part : m_particles) {
      if (part.life <= 0)
        continue;
      p.setOpacity(part.life * 0.9);
      QColor pc(brightBlue.red(), brightBlue.green(), brightBlue.blue(),
                int(255 * part.life));
      p.setBrush(pc);
      p.setPen(Qt::NoPen);
      p.drawEllipse(part.pos, part.size, part.size);
    }
    p.setOpacity(1.0);

    p.save();
    p.translate(m_cx, m_cy);

    // The blob scales from 0→1.5 with elastic overshoot
    qreal blobScale = m_splash * 1.5;

    QPainterPath blob = buildBlobPath(blobScale);

    // Glow
    drawGlowShape(p, blob, glowBlue, 5);

    // Gradient fill
    QRadialGradient blobGrad(0, -20 * blobScale, 80 * blobScale);
    blobGrad.setColorAt(0.0, QColor(110, 165, 255));
    blobGrad.setColorAt(0.6, brightBlue);
    blobGrad.setColorAt(1.0, deepBlue);
    p.setBrush(blobGrad);
    p.setPen(Qt::NoPen);
    p.drawPath(blob);

    // ====== NIB CUTOUT (pen icon) – fades in via nibReveal ======
    if (m_nibReveal > 0.0) {
      p.setOpacity(m_nibReveal);

      // Rotate so nib points lower-left (like real logo)
      p.rotate(40);
      p.scale(blobScale, blobScale);

      // Nib silhouette cutout  (background color = makes it look cut-out)
      p.setBrush(QColor("#0a0a0f"));
      p.setPen(Qt::NoPen);
      QPainterPath nib;
      nib.moveTo(0, 30);
      nib.lineTo(-11, 4);
      nib.lineTo(-8, -18);
      nib.quadTo(0, -28, 8, -18);
      nib.lineTo(11, 4);
      nib.closeSubpath();
      p.drawPath(nib);

      // Breather hole
      p.setBrush(brightBlue);
      p.drawEllipse(QRectF(-3, -5, 6, 6));

      // Center slit
      p.setPen(QPen(brightBlue, 1.8, Qt::SolidLine, Qt::RoundCap));
      p.drawLine(0, -2, 0, 26);

      // Decorative dot
      p.setPen(Qt::NoPen);
      p.setBrush(QColor("#0a0a0f"));
      p.drawEllipse(QRectF(20, -9, 11, 11));

      p.setOpacity(1.0);
    }

    p.restore();
  }

  // ====== PHASE 5: Text ======
  if (m_textFade > 0.0) {
    p.setOpacity(m_textFade);
    QFont font;
    font.setPixelSize(38);
    font.setBold(true);
    font.setLetterSpacing(QFont::AbsoluteSpacing, 6);
    p.setFont(font);
    p.setPen(QColor(220, 235, 255));
    int textY = int(m_cy * 1.0 + 115);
    QRect textRect(0, textY, width(), 55);
    p.drawText(textRect, Qt::AlignCenter, "Blop");
    p.setOpacity(1.0);
  }
}
