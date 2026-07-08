#include "bloommenu.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QTimer>
#include <QVariantAnimation>
#include <QtMath>

#include "blop_theme.h"
#include "blopstyle.h"
#include "moderntoolbar.h"
#include "uiscale.h"

// ===========================================================================
// BloomPetal - one squircle tool button flying out of the anchor point.
// ===========================================================================
class BloomPetal : public QWidget {
  Q_OBJECT
  Q_PROPERTY(double appear READ appear WRITE setAppear)
public:
  BloomPetal(BloomMenu *menu, const BloomItem &item, bool active,
             const QColor &accent)
      : QWidget(menu), m_item(item), m_active(active), m_accent(accent) {
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_TranslucentBackground, true);
    const int s = UiScale::dp(46);
    setFixedSize(s, s);
  }

  BloomItem item() const { return m_item; }

  double appear() const { return m_appear; }
  void setAppear(double v) {
    m_appear = v;
    update();
  }

signals:
  void picked();

protected:
  void mousePressEvent(QMouseEvent *e) override {
    if (e->button() == Qt::LeftButton || e->button() == Qt::NoButton)
      emit picked();
  }

  void paintEvent(QPaintEvent *) override {
    if (m_appear <= 0.01)
      return;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setOpacity(qBound(0.0, m_appear, 1.0));

    // Scale the petal around its center while it blooms in.
    const qreal cx = width() / 2.0;
    const qreal cy = height() / 2.0;
    const qreal sc = 0.55 + 0.45 * qMin(1.0, m_appear);
    p.translate(cx, cy);
    p.scale(sc, sc);
    p.translate(-cx, -cy);

    QRectF r = rect().adjusted(1.5, 1.5, -1.5, -1.5);
    const qreal rad = UiScale::dp(14);
    QPainterPath bg;
    bg.addRoundedRect(r, rad, rad);

    QColor surface = m_active ? m_accent : BlopStyle::surfaceBg();
    p.fillPath(bg, surface);
    QColor border = m_active ? m_accent : BlopStyle::surfaceBorder();
    p.setPen(QPen(border, m_active ? 2.0 : 1.0));
    p.drawPath(bg);

    const QColor fg = m_active ? QColor(255, 255, 255)
                               : QColor(236, 240, 252, 250);
    p.save();
    p.translate(cx, cy);
    const qreal g = qMin(width(), height()) * 0.72 / 64.0;
    p.scale(g, g);
    p.translate(-32, -32);
    blopDrawToolbarGlyph64(&p, m_item.icon, fg);
    p.restore();
  }

private:
  BloomItem m_item;
  bool m_active{false};
  QColor m_accent;
  double m_appear{0.0};
};

// ===========================================================================
// BloomMenu
// ===========================================================================
BloomMenu::BloomMenu(QWidget *win) : QWidget(win) {
  setAttribute(Qt::WA_DeleteOnClose);
  setAttribute(Qt::WA_NoSystemBackground, true);
  setAttribute(Qt::WA_TranslucentBackground, true);
  setFocusPolicy(Qt::NoFocus);
  resize(win->size());
  move(0, 0);
}

BloomMenu *BloomMenu::popup(QWidget *win, const QPoint &anchorWin,
                            const QVector<BloomItem> &items, ToolMode current,
                            const QColor &accent) {
  if (!win || items.isEmpty())
    return nullptr;
  auto *menu = new BloomMenu(win);
  menu->buildPetals(anchorWin, items, current, accent);
  menu->show();
  menu->raise();

  auto *fade = new QVariantAnimation(menu);
  fade->setDuration(BlopMotion::kFast);
  fade->setStartValue(0.0);
  fade->setEndValue(1.0);
  fade->setEasingCurve(BlopMotion::kEaseStandard);
  connect(fade, &QVariantAnimation::valueChanged, menu, [menu](const QVariant &v) {
    menu->m_scrim = v.toReal();
    menu->update();
  });
  fade->start(QAbstractAnimation::DeleteWhenStopped);
  return menu;
}

void BloomMenu::buildPetals(const QPoint &anchorWin,
                            const QVector<BloomItem> &items, ToolMode current,
                            const QColor &accent) {
  const int n = items.size();
  const int margin = UiScale::dp(10);

  // Blop's signature bloom: petals fan across an upward arc whose radius
  // gently grows petal by petal (a soft spiral, not Drawboard's closed
  // ring), each one popping in with its own staggered overshoot.
  const qreal r0 = UiScale::dp(96);
  const qreal rStep = UiScale::dp(5);
  const qreal startDeg = 196.0;
  const qreal endDeg = -16.0;

  for (int i = 0; i < n; ++i) {
    auto *petal = new BloomPetal(this, items[i], items[i].mode == current,
                                 accent);
    const qreal t = (n == 1) ? 0.5 : (qreal)i / (n - 1);
    const qreal deg = startDeg + (endDeg - startDeg) * t;
    const qreal rad = qDegreesToRadians(deg);
    const qreal radius = r0 + i * rStep;
    QPoint target(anchorWin.x() + qRound(radius * qCos(rad)) -
                      petal->width() / 2,
                  anchorWin.y() - qRound(radius * qSin(rad)) -
                      petal->height() / 2);
    target.setX(qBound(margin, target.x(), width() - petal->width() - margin));
    target.setY(qBound(margin, target.y(),
                       height() - petal->height() - margin));

    const QPoint from(anchorWin.x() - petal->width() / 2,
                      anchorWin.y() - petal->height() / 2);
    petal->move(from);
    petal->show();

    connect(petal, &BloomPetal::picked, this, [this, petal]() {
      if (m_closing)
        return;
      emit toolSelected(petal->item().mode);
      dismiss();
    });

    // Staggered entrance: each petal starts a beat after the previous one.
    QTimer::singleShot(i * 26, petal, [petal, from, target]() {
      auto *pos = new QPropertyAnimation(petal, "pos", petal);
      pos->setDuration(BlopMotion::kEmphasis);
      pos->setStartValue(from);
      pos->setEndValue(target);
      pos->setEasingCurve(BlopMotion::kEaseOvershoot);
      pos->start(QAbstractAnimation::DeleteWhenStopped);

      auto *in = new QPropertyAnimation(petal, "appear", petal);
      in->setDuration(BlopMotion::kStandard);
      in->setStartValue(0.0);
      in->setEndValue(1.0);
      in->setEasingCurve(BlopMotion::kEaseStandard);
      in->start(QAbstractAnimation::DeleteWhenStopped);
    });

    m_petals.push_back(petal);
  }
}

void BloomMenu::dismiss(bool instant) {
  if (m_closing)
    return;
  m_closing = true;
  if (instant) {
    close();
    return;
  }
  for (BloomPetal *petal : m_petals) {
    auto *out = new QPropertyAnimation(petal, "appear", petal);
    out->setDuration(BlopMotion::kFast);
    out->setStartValue(petal->appear());
    out->setEndValue(0.0);
    out->setEasingCurve(BlopMotion::kEaseStandard);
    out->start(QAbstractAnimation::DeleteWhenStopped);
  }
  auto *fade = new QVariantAnimation(this);
  fade->setDuration(BlopMotion::kFast);
  fade->setStartValue(m_scrim);
  fade->setEndValue(0.0);
  connect(fade, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
    m_scrim = v.toReal();
    update();
  });
  connect(fade, &QVariantAnimation::finished, this, [this]() { close(); });
  fade->start(QAbstractAnimation::DeleteWhenStopped);
}

void BloomMenu::paintEvent(QPaintEvent *) {
  QPainter p(this);
  QColor scrim = BlopStyle::backdrop(/*forAndroid=*/true);
  scrim.setAlphaF(scrim.alphaF() * 0.72 * m_scrim);
  p.fillRect(rect(), scrim);
}

void BloomMenu::mousePressEvent(QMouseEvent *) { dismiss(); }

#include "bloommenu.moc"
