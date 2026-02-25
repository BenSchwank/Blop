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
      m_splash(0.0), m_nibReveal(0.0), m_textFade(0.0), m_dripsStart(0.0),
      m_particleTimer(0), m_finished(false) {
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

  // 5. Ink Drips begin falling from logo bottom
  auto *dripsAnim = new QPropertyAnimation(this, "dripsStart", this);
  dripsAnim->setDuration(100); // triggers spawnDrips() on first tick
  dripsAnim->setStartValue(0.0);
  dripsAnim->setEndValue(1.0);
  m_seq->addAnimation(dripsAnim);

  // 6. Text fades in (drips are physics-driven by timer, concurrent with text)
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

// ============= Ink Drip System =============
void IntroScreen::spawnDrips() {
  m_drips.clear();
  // The blob at blobScale=1.5 has bottom points at roughly:
  // bottom-center, lower-left, lower-right, and a mid-bottom
  // Anchors are in screen coords relative to screen center (m_cx, m_cy)
  const qreal bs = 1.5; // final blobScale
  struct AnchorDef {
    qreal ax;
    qreal ay;
    qreal speed;
    qreal w;
    qreal delay;
  };
  QList<AnchorDef> anchors = {
      {-14.0 * bs, 98.0 * bs, 2.8, 4.0, 0},   // bottom-left
      {0.5 * bs, 99.5 * bs, 3.5, 5.5, 180},   // center bottom (biggest)
      {33.5 * bs, 93.0 * bs, 2.2, 3.5, 90},   // bottom-right
      {-31.0 * bs, 79.0 * bs, 1.8, 3.0, 320}, // lower-left shoulder
  };
  for (const auto &a : anchors) {
    InkDrip d;
    d.anchor = QPointF(m_cx + a.ax, m_cy + a.ay);
    d.fallY = -a.delay * 0.05; // stagger start (negative = hasn't started)
    d.speed = a.speed;
    d.width = a.w;
    d.length = 0.0;
    d.splash = 0.0;
    d.landed = false;
    d.landY = d.anchor.y() + 180.0 + (qrand() % 30);
    m_drips.append(d);
  }
}

void IntroScreen::timerEvent(QTimerEvent *) {
  // --- Particles ---
  for (auto &p : m_particles) {
    p.pos += p.vel;
    p.vel.ry() += 0.15;
    p.life -= 0.03;
  }
  m_particles.erase(
      std::remove_if(m_particles.begin(), m_particles.end(),
                     [](const Particle &p) { return p.life <= 0; }),
      m_particles.end());

  // --- Ink Drips ---
  bool anyDripActive = false;
  for (auto &d : m_drips) {
    if (d.fallY < 0) {
      // Delay countdown
      d.fallY += 0.05;
      anyDripActive = true;
      continue;
    }
    if (!d.landed) {
      d.fallY += d.speed;
      d.length = qMin(d.fallY * 0.6, 30.0); // elongate as it falls
      if (d.anchor.y() + d.fallY >= d.landY) {
        d.landed = true;
        d.splash = 0.01;
      }
      anyDripActive = true;
    } else if (d.splash < 1.0) {
      d.splash += 0.04;
      anyDripActive = true;
    }
  }

  if (!m_particles.isEmpty() || anyDripActive)
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

// Logo blob — exact Bezier shape traced from Intro5.png
QPainterPath IntroScreen::buildBlobPath(qreal scale) const {
  // All coordinates were extracted from Intro5.png via Python/OpenCV
  // and are normalized so scale=1.0 matches the original logo proportions.
  // The raw extraction used factor 0.7450; dividing by 134 gives unit coords,
  // then we re-multiply by scale * 130 for rendering.
  const qreal s = scale;
  QPainterPath blob;
  blob.moveTo(-37.24 * s, -100.50 * s);
  blob.quadTo(-52.72 * s, -96.10 * s, -59.60 * s, -86.73 * s);
  blob.quadTo(-66.49 * s, -77.36 * s, -71.19 * s, -61.78 * s);
  blob.quadTo(-75.89 * s, -46.20 * s, -89.96 * s, -40.10 * s);
  blob.quadTo(-104.03 * s, -34.01 * s, -114.11 * s, -24.19 * s);
  blob.quadTo(-124.19 * s, -14.37 * s, -127.11 * s, 1.09 * s);
  blob.quadTo(-130.03 * s, 16.55 * s, -126.41 * s, 27.83 * s);
  blob.quadTo(-122.79 * s, 39.12 * s, -115.68 * s, 46.28 * s);
  blob.quadTo(-108.56 * s, 53.44 * s, -99.32 * s, 57.05 * s);
  blob.quadTo(-90.08 * s, 60.66 * s, -82.32 * s, 60.66 * s);
  blob.quadTo(-74.56 * s, 60.66 * s, -62.44 * s, 55.52 * s);
  blob.quadTo(-50.31 * s, 50.38 * s, -40.81 * s, 58.82 * s);
  blob.quadTo(-31.31 * s, 67.26 * s, -31.68 * s, 78.37 * s);
  blob.quadTo(-32.04 * s, 89.49 * s, -25.63 * s, 94.58 * s);
  blob.quadTo(-19.22 * s, 99.67 * s, -9.38 * s, 99.67 * s);
  blob.quadTo(0.72 * s, 99.67 * s, 10.48 * s, 94.25 * s);
  blob.quadTo(20.24 * s, 88.84 * s, 33.92 * s, 93.08 * s);
  blob.quadTo(47.61 * s, 97.32 * s, 58.09 * s, 96.25 * s);
  blob.quadTo(68.57 * s, 95.18 * s, 72.50 * s, 91.59 * s);
  blob.quadTo(76.43 * s, 87.99 * s, 83.71 * s, 70.05 * s);
  blob.quadTo(90.98 * s, 52.10 * s, 101.46 * s, 45.87 * s);
  blob.quadTo(111.94 * s, 39.64 * s, 116.59 * s, 29.23 * s);
  blob.quadTo(121.24 * s, 18.82 * s, 120.51 * s, 7.58 * s);
  blob.quadTo(119.79 * s, -3.61 * s, 107.63 * s, -16.24 * s);
  blob.quadTo(95.48 * s, -28.87 * s, 90.66 * s, -49.78 * s);
  blob.quadTo(85.84 * s, -70.68 * s, 76.17 * s, -79.11 * s);
  blob.quadTo(66.49 * s, -87.55 * s, 42.95 * s, -84.48 * s);
  blob.quadTo(19.42 * s, -81.41 * s, -1.08 * s, -93.11 * s);
  blob.quadTo(-21.58 * s, -104.82 * s, -37.24 * s, -100.50 * s);
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

    // ====== NIB CUTOUT — exact pen-nib trace from Intro5.png ======
    if (m_nibReveal > 0.0) {
      p.setOpacity(m_nibReveal);
      // Scale matches the blob's current blobScale
      p.scale(blobScale, blobScale);

      // Draw nib cutout (background color over blue = "cut out" look)
      p.setBrush(QColor("#0a0a0f"));
      p.setPen(Qt::NoPen);

      // Exact nib shape traced from Intro5.png via OpenCV, centered at blob
      // centroid
      QPainterPath nib;
      nib.moveTo(-8.56, 2.89);
      nib.quadTo(-9.99, 2.16, -13.94, 2.16);
      nib.quadTo(-17.88, 2.16, -21.12, 3.59);
      nib.quadTo(-24.37, 5.02, -26.49, 7.51);
      nib.quadTo(-28.61, 10.00, -30.03, 10.00);
      nib.quadTo(-31.46, 10.00, -32.88, 11.07);
      nib.quadTo(-34.31, 12.14, -37.92, 20.42);
      nib.quadTo(-41.54, 28.69, -42.25, 33.34);
      nib.quadTo(-42.96, 37.99, -44.38, 39.41);
      nib.quadTo(-45.81, 40.83, -42.96, 44.38);
      nib.quadTo(-40.12, 47.92, -33.63, 53.29);
      nib.quadTo(-27.14, 58.67, -26.07, 58.67);
      nib.quadTo(-25.00, 58.67, -19.69, 54.37);
      nib.quadTo(-14.37, 50.07, -8.56, 47.22);
      nib.quadTo(-2.75, 44.38, -1.44, 40.83);
      nib.quadTo(0.00, 37.27, -0.36, 36.20);
      nib.quadTo(-0.71, 35.13, 2.50, 31.22);
      nib.quadTo(5.72, 27.32, 5.72, 26.60);
      nib.quadTo(5.72, 25.89, 3.57, 25.18);
      nib.quadTo(1.43, 24.46, 3.22, 23.32);
      nib.quadTo(5.02, 22.18, 4.29, 20.75);
      nib.quadTo(3.57, 19.33, 4.29, 17.55);
      nib.quadTo(5.02, 15.77, 2.86, 15.06);
      nib.quadTo(0.71, 14.34, 1.79, 12.20);
      nib.quadTo(2.86, 10.07, 1.79, 10.07);
      nib.quadTo(0.71, 10.07, -2.86, 13.98);
      nib.quadTo(-6.43, 17.88, -7.15, 21.82);
      nib.quadTo(-7.86, 25.75, -9.64, 27.89);
      nib.quadTo(-11.43, 30.03, -14.66, 30.38);
      nib.quadTo(-17.88, 30.74, -19.30, 29.67);
      nib.quadTo(-20.75, 28.60, -21.47, 24.68);
      nib.quadTo(-22.18, 20.75, -21.12, 18.98);
      nib.quadTo(-20.06, 17.20, -16.83, 16.12);
      nib.quadTo(-13.62, 15.06, -11.07, 11.84);
      nib.quadTo(-8.56, 8.56, -7.86, 6.07);
      nib.quadTo(-7.15, 3.59, -8.56, 2.89);
      nib.closeSubpath();
      p.drawPath(nib);

      p.setOpacity(1.0);
    }
  }

  // ====== INK DRIPS: drawn after blob settles ======
  if (!m_drips.isEmpty()) {
    for (const auto &d : m_drips) {
      if (d.fallY <= 0)
        continue; // still in delay

      const QColor dripColor(78, 130, 255);
      const QColor glowColor(100, 160, 255);

      qreal headX = d.anchor.x();
      qreal headY = d.anchor.y() + d.fallY;
      qreal tailY = d.anchor.y() + qMax(0.0, d.fallY - d.length);

      if (!d.landed) {
        // --- Falling drip body (elongated rounded shape) ---
        // Glow around drip
        QPainterPath dripPath;
        dripPath.moveTo(headX, tailY);
        dripPath.quadTo(headX + d.width * 0.7, tailY + d.length * 0.3, headX,
                        headY + d.width * 0.6); // bottom tip
        dripPath.quadTo(headX - d.width * 0.7, tailY + d.length * 0.3, headX,
                        tailY);

        for (int gi = 4; gi >= 1; --gi) {
          qreal alpha = 0.07 * gi;
          QPen gp(QColor(glowColor.red(), glowColor.green(), glowColor.blue(),
                         int(255 * alpha)),
                  gi * 8.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
          p.setPen(gp);
          p.setBrush(Qt::NoBrush);
          p.drawPath(dripPath);
        }

        // Filled drip body
        QLinearGradient dg(headX, tailY, headX, headY);
        dg.setColorAt(0.0, QColor(120, 175, 255, 200));
        dg.setColorAt(1.0, dripColor);
        p.setBrush(dg);
        p.setPen(Qt::NoPen);
        p.drawPath(dripPath);

      } else {
        // --- Splash ring on landing ---
        qreal sp = d.splash; // 0‥1
        qreal ringR = sp * d.width * 9.0;
        qreal alpha = 1.0 - sp;
        if (alpha > 0) {
          p.setOpacity(alpha * 0.85);

          // Flat oval splash pool
          QRadialGradient pool(headX, d.landY, ringR * 0.5);
          pool.setColorAt(0.0, QColor(78, 130, 255, int(180 * alpha)));
          pool.setColorAt(1.0, QColor(78, 130, 255, 0));
          p.setBrush(pool);
          p.setPen(Qt::NoPen);
          p.drawEllipse(QPointF(headX, d.landY), ringR, ringR * 0.35);

          // Outer ring outline
          QPen ringPen(QColor(glowColor.red(), glowColor.green(),
                              glowColor.blue(), int(200 * alpha)),
                       1.5);
          p.setPen(ringPen);
          p.setBrush(Qt::NoBrush);
          p.drawEllipse(QPointF(headX, d.landY), ringR, ringR * 0.35);

          p.setOpacity(1.0);
        }
      }
    }
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
