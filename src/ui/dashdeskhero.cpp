#include "dashdeskhero.h"

#include "blop_inwindow_menu.h"
#include "blop_theme.h"
#include "blopstyle.h"
#include "uiscale.h"

#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QImage>
#include <QImageReader>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPointer>
#include <QPushButton>
#include <QRadialGradient>
#include <QResizeEvent>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QWheelEvent>

namespace {
QString rgba(const QColor &c) {
  return QStringLiteral("rgba(%1,%2,%3,%4)")
      .arg(c.red())
      .arg(c.green())
      .arg(c.blue())
      .arg(QString::number(c.alphaF(), 'f', 3));
}

QString chipQss(bool dark, const QColor &acc, bool compact) {
  const QString ink =
      dark ? QStringLiteral("rgba(255,255,255,0.92)")
           : BlopStyle::paperInk().name(QColor::HexRgb);
  const QString bg =
      dark ? QStringLiteral("rgba(0,0,0,0.40)")
           : QStringLiteral("rgba(255,255,255,0.92)");
  const QString hover =
      dark ? QStringLiteral("rgba(0,0,0,0.55)")
           : QStringLiteral("rgba(255,255,255,1.0)");
  const QString pad =
      compact ? QStringLiteral("4px 8px") : QStringLiteral("4px 10px");
  return QStringLiteral("QPushButton {"
                        "  background: %1; color: %2;"
                        "  border: 1px solid %3; border-radius: %4px;"
                        "  padding: %5; font-size: 12px; font-weight: 600;"
                        "}"
                        "QPushButton:hover { background: %6; }")
      .arg(bg, ink, rgba(acc), QString::number(UiScale::dp(BlopStyle::radiusMdDp())),
           pad, hover);
}
} // namespace

DashDeskHero::DashDeskHero(const QString &bannerId, QWidget *parent)
    : QWidget(parent), m_bannerId(bannerId) {
  setObjectName(QStringLiteral("DashDeskHero"));
  setAttribute(Qt::WA_StyledBackground, true);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setMinimumHeight(UiScale::dp(48));
  setCursor(Qt::ArrowCursor);
  setMouseTracking(true);

  m_logo = QPixmap(QStringLiteral(":/assets/logo.jpg"));

  m_btnPick = new QPushButton(QStringLiteral("Bild"), this);
  m_btnPick->setCursor(Qt::PointingHandCursor);
  m_btnPick->setVisible(false);
  m_btnPick->setFixedHeight(UiScale::dp(28));
  m_btnPick->setToolTip(QStringLiteral("Wash oder eigenes Bild"));
  connect(m_btnPick, &QPushButton::clicked, this,
          &DashDeskHero::showLibraryMenu);

  m_btnMore = new QPushButton(QStringLiteral("⋯"), this);
  m_btnMore->setCursor(Qt::PointingHandCursor);
  m_btnMore->setVisible(false);
  m_btnMore->setFixedSize(UiScale::dp(28), UiScale::dp(28));
  m_btnMore->setToolTip(QStringLiteral("Banner-Optionen"));
  connect(m_btnMore, &QPushButton::clicked, this,
          &DashDeskHero::showSecondaryMenu);

  connect(&BlopTheme::instance(), &BlopTheme::themeChanged, this, [this]() {
    styleEditBtns();
    update();
  });

  if (m_bannerId == QLatin1String("banner")) {
    QSettings st(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
    const QString legacy =
        st.value(QStringLiteral("dashboard/coverPath")).toString();
    if (!legacy.isEmpty() &&
        st.value(coverSettingsKey()).toString().isEmpty()) {
      st.setValue(coverSettingsKey(), legacy);
      st.remove(QStringLiteral("dashboard/coverPath"));
    }
  }

  loadPrefs();
  styleEditBtns();
}

void DashDeskHero::setEditMode(bool on) {
  if (m_editMode == on)
    return;
  m_editMode = on;
  if (m_btnPick)
    m_btnPick->setVisible(on);
  if (m_btnMore)
    m_btnMore->setVisible(on);
  setCursor(on ? Qt::PointingHandCursor : Qt::ArrowCursor);
  layoutEditBtns();
  update();
}

void DashDeskHero::mousePressEvent(QMouseEvent *event) {
  if (!m_editMode) {
    QWidget::mousePressEvent(event);
    return;
  }
  if (event->button() == Qt::RightButton) {
    showSecondaryMenu();
    event->accept();
    return;
  }
  if (event->button() == Qt::LeftButton) {
    if (!m_cover.isNull()) {
      m_panning = true;
      m_panLast = event->pos();
      setCursor(Qt::ClosedHandCursor);
      event->accept();
      return;
    }
    showLibraryMenu();
    event->accept();
    return;
  }
  QWidget::mousePressEvent(event);
}

void DashDeskHero::mouseMoveEvent(QMouseEvent *event) {
  if (m_panning && !m_cover.isNull() && width() > 0 && height() > 0) {
    const QPoint delta = event->pos() - m_panLast;
    m_panLast = event->pos();
    // Dragging right moves focus left (image follows the hand).
    m_focusX = qBound(0.0, m_focusX - qreal(delta.x()) / qreal(width()), 1.0);
    m_focusY = qBound(0.0, m_focusY - qreal(delta.y()) / qreal(height()), 1.0);
    update();
    event->accept();
    return;
  }
  QWidget::mouseMoveEvent(event);
}

void DashDeskHero::mouseReleaseEvent(QMouseEvent *event) {
  if (m_panning) {
    m_panning = false;
    saveCropPrefs();
    setCursor(m_editMode ? Qt::PointingHandCursor : Qt::ArrowCursor);
    event->accept();
    return;
  }
  QWidget::mouseReleaseEvent(event);
}

void DashDeskHero::wheelEvent(QWheelEvent *event) {
  if (!m_editMode || m_cover.isNull()) {
    QWidget::wheelEvent(event);
    return;
  }
  const qreal step = event->angleDelta().y() > 0 ? 0.08 : -0.08;
  const qreal next = qBound(1.0, m_zoom + step, 2.5);
  if (!qFuzzyCompare(next, m_zoom)) {
    m_zoom = next;
    saveCropPrefs();
    update();
  }
  event->accept();
}

void DashDeskHero::reloadCover() {
  loadPrefs();
  update();
}

void DashDeskHero::styleEditBtns() {
  const bool dark = BlopTheme::instance().isDark();
  const QColor acc = BlopTheme::accentPrimary();
  if (m_btnPick)
    m_btnPick->setStyleSheet(chipQss(dark, acc, false));
  if (m_btnMore)
    m_btnMore->setStyleSheet(chipQss(dark, acc, true));
}

void DashDeskHero::layoutEditBtns() {
  if (!m_editMode)
    return;
  const int m = UiScale::dp(10);
  const int gap = UiScale::dp(6);
  int x = width() - m;
  if (m_btnMore && m_btnMore->isVisible()) {
    x -= m_btnMore->width();
    m_btnMore->move(x, height() - m_btnMore->height() - m);
    m_btnMore->raise();
    x -= gap;
  }
  if (m_btnPick && m_btnPick->isVisible()) {
    m_btnPick->adjustSize();
    x -= m_btnPick->width();
    m_btnPick->move(x, height() - m_btnPick->height() - m);
    m_btnPick->raise();
  }
}

void DashDeskHero::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  layoutEditBtns();
}

QString DashDeskHero::coverSettingsKey() const {
  return QStringLiteral("dashboard/bannerCover/%1").arg(m_bannerId);
}
QString DashDeskHero::washSettingsKey() const {
  return QStringLiteral("dashboard/bannerWash/%1").arg(m_bannerId);
}
QString DashDeskHero::focusXKey() const {
  return QStringLiteral("dashboard/bannerFocus/%1/x").arg(m_bannerId);
}
QString DashDeskHero::focusYKey() const {
  return QStringLiteral("dashboard/bannerFocus/%1/y").arg(m_bannerId);
}
QString DashDeskHero::zoomKey() const {
  return QStringLiteral("dashboard/bannerZoom/%1").arg(m_bannerId);
}

QString DashDeskHero::coverStorageDir() const {
  return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
         QStringLiteral("/dashboard/banners/") + m_bannerId;
}

QString DashDeskHero::washId(Wash w) const {
  switch (w) {
  case Wash::Mist:
    return QStringLiteral("mist");
  case Wash::Dusk:
    return QStringLiteral("dusk");
  case Wash::Soft:
  default:
    return QStringLiteral("soft");
  }
}

DashDeskHero::Wash DashDeskHero::washFromId(const QString &id) const {
  if (id == QLatin1String("mist"))
    return Wash::Mist;
  if (id == QLatin1String("dusk"))
    return Wash::Dusk;
  return Wash::Soft;
}

void DashDeskHero::loadPrefs() {
  QSettings st(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  m_wash = washFromId(st.value(washSettingsKey()).toString());
  m_focusX = qBound(0.0, st.value(focusXKey(), 0.5).toReal(), 1.0);
  m_focusY = qBound(0.0, st.value(focusYKey(), 0.5).toReal(), 1.0);
  m_zoom = qBound(1.0, st.value(zoomKey(), 1.0).toReal(), 2.5);
  loadCoverPixmap();
}

void DashDeskHero::saveCropPrefs() const {
  QSettings st(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  st.setValue(focusXKey(), m_focusX);
  st.setValue(focusYKey(), m_focusY);
  st.setValue(zoomKey(), m_zoom);
  st.sync();
}

void DashDeskHero::loadCoverPixmap() {
  m_cover = QPixmap();
  const QString path =
      QSettings(QStringLiteral("Blop"), QStringLiteral("BlopApp"))
          .value(coverSettingsKey())
          .toString();
  if (path.isEmpty())
    return;
  const QFileInfo fi(path);
  if (!fi.exists() || !fi.isFile())
    return;

  // Prefer QImageReader — more reliable than QPixmap(path) for large JPEGs
  // and surfaces format/plugin issues clearly.
  QImageReader reader(fi.absoluteFilePath());
  reader.setAutoTransform(true);
  QImage img = reader.read();
  if (img.isNull()) {
    // Fallback: direct load (helps when a plugin path is odd).
    img = QImage(fi.absoluteFilePath());
  }
  if (img.isNull())
    return;
  m_cover = QPixmap::fromImage(img);
}

void DashDeskHero::clearCoverFile() {
  QSettings st(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  const QString path = st.value(coverSettingsKey()).toString();
  st.remove(coverSettingsKey());
  st.sync();
  if (!path.isEmpty() && path.startsWith(coverStorageDir()) &&
      QFile::exists(path))
    QFile::remove(path);
  m_cover = QPixmap();
}

void DashDeskHero::showLibraryMenu() {
  QList<BlopInWindowMenu::Item> items;

  auto addWash = [this, &items](const QString &label, Wash w) {
    BlopInWindowMenu::Item it;
    it.label = label;
    it.handler = [this, w]() { applyWash(w); };
    items.push_back(it);
  };
  addWash(QStringLiteral("Wash: Soft"), Wash::Soft);
  addWash(QStringLiteral("Wash: Mist"), Wash::Mist);
  addWash(QStringLiteral("Wash: Dusk"), Wash::Dusk);

  BlopInWindowMenu::Item sep;
  sep.separator = true;
  items.push_back(sep);

  BlopInWindowMenu::Item pick;
  pick.label = QStringLiteral("Eigenes Bild…");
  // Wait until the in-window menu finish-dismisses before the native
  // Windows explorer opens — otherwise it can abort silently.
  pick.handler = [this]() {
    QTimer::singleShot(BlopMotion::kFast + 60, this,
                       [this]() { pickCover(); });
  };
  items.push_back(pick);

  const QPoint g =
      m_btnPick && m_btnPick->isVisible()
          ? m_btnPick->mapToGlobal(QPoint(0, m_btnPick->height()))
          : mapToGlobal(QPoint(width() - UiScale::dp(40), height() / 2));
  BlopInWindowMenu::show(this, g, items);
}

void DashDeskHero::showSecondaryMenu() {
  QList<BlopInWindowMenu::Item> items;

  if (!m_cover.isNull()) {
    BlopInWindowMenu::Item crop;
    crop.label = QStringLiteral("Position zurücksetzen");
    crop.handler = [this]() { resetCrop(); };
    items.push_back(crop);
  }

  BlopInWindowMenu::Item reset;
  reset.label = QStringLiteral("Bild / Wash zurücksetzen");
  reset.handler = [this]() { resetCover(); };
  items.push_back(reset);

  BlopInWindowMenu::Item remove;
  remove.label = QStringLiteral("Banner entfernen");
  remove.destructive = true;
  remove.handler = [this]() { emit removeRequested(); };
  items.push_back(remove);

  const QPoint g =
      m_btnMore && m_btnMore->isVisible()
          ? m_btnMore->mapToGlobal(QPoint(0, m_btnMore->height()))
          : mapToGlobal(QPoint(width() - UiScale::dp(40), height() / 2));
  BlopInWindowMenu::show(this, g, items);
}

void DashDeskHero::applyWash(Wash wash) {
  clearCoverFile();
  m_wash = wash;
  resetCrop();
  QSettings st(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  st.setValue(washSettingsKey(), washId(wash));
  st.sync();
  update();
  layoutEditBtns();
}

void DashDeskHero::pickCover() {
  // Capture everything needed so a board refresh that deletes this widget
  // during the modal dialog cannot lose the chosen file.
  QPointer<DashDeskHero> self(this);
  const QString bannerId = m_bannerId;
  const QString settingsKey =
      QStringLiteral("dashboard/bannerCover/%1").arg(bannerId);
  const QString storageDir =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
      QStringLiteral("/dashboard/banners/") + bannerId;
  QWidget *parentWin = window() ? window() : nullptr;

  const QString path = QFileDialog::getOpenFileName(
      parentWin, QStringLiteral("Banner-Bild"), QString(),
      QStringLiteral("Bilder (*.png *.jpg *.jpeg *.webp *.bmp)"));
  if (path.isEmpty())
    return;

  QDir().mkpath(storageDir);
  const QFileInfo fi(path);
  const QString dest =
      storageDir + QStringLiteral("/cover") +
      (fi.suffix().isEmpty() ? QStringLiteral(".jpg")
                             : QStringLiteral(".") + fi.suffix().toLower());
  if (QFile::exists(dest))
    QFile::remove(dest);

  QString stored = QFileInfo(path).absoluteFilePath();
  if (QFile::copy(path, dest))
    stored = QFileInfo(dest).absoluteFilePath();

  // Persist first — survives even if this hero is gone after the dialog.
  QSettings st(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  st.setValue(settingsKey, stored);
  st.setValue(QStringLiteral("dashboard/bannerFocus/%1/x").arg(bannerId), 0.5);
  st.setValue(QStringLiteral("dashboard/bannerFocus/%1/y").arg(bannerId), 0.5);
  st.setValue(QStringLiteral("dashboard/bannerZoom/%1").arg(bannerId), 1.0);
  st.sync();

  auto applyTo = [](DashDeskHero *hero) {
    if (!hero)
      return;
    hero->m_focusX = 0.5;
    hero->m_focusY = 0.5;
    hero->m_zoom = 1.0;
    hero->loadCoverPixmap();
    hero->update();
    hero->layoutEditBtns();
  };

  if (self) {
    applyTo(self.data());
    return;
  }

  // Hero was rebuilt under us — push the new cover onto whatever banner is live.
  if (parentWin) {
    const QList<DashDeskHero *> heroes =
        parentWin->findChildren<DashDeskHero *>();
    for (DashDeskHero *h : heroes) {
      if (h->m_bannerId == bannerId)
        applyTo(h);
    }
  }
}

void DashDeskHero::resetCrop() {
  m_focusX = 0.5;
  m_focusY = 0.5;
  m_zoom = 1.0;
  saveCropPrefs();
  update();
}

void DashDeskHero::resetCover() {
  clearCoverFile();
  m_wash = Wash::Soft;
  QSettings st(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  st.setValue(washSettingsKey(), washId(Wash::Soft));
  st.sync();
  resetCrop();
  update();
}

void DashDeskHero::paintWash(QPainter &p, Wash wash, const QColor &acc,
                             bool dark) {
  QColor desk = dark ? QColor(0x1A, 0x1D, 0x26) : QColor(0xF7, 0xF8, 0xFA);
  QColor washA = acc;
  QColor washB = acc;

  switch (wash) {
  case Wash::Mist:
    desk = dark ? QColor(0x16, 0x1E, 0x28) : QColor(0xEE, 0xF4, 0xF8);
    washA = dark ? QColor(0x5B, 0xC0, 0xBE) : QColor(0x3D, 0x8B, 0xA8);
    washB = dark ? QColor(0x7A, 0x9C, 0xCF) : QColor(0x6B, 0x8C, 0xCE);
    break;
  case Wash::Dusk:
    desk = dark ? QColor(0x22, 0x18, 0x1C) : QColor(0xFA, 0xF3, 0xF0);
    washA = dark ? QColor(0xE0, 0x7A, 0x5F) : QColor(0xC4, 0x6B, 0x4A);
    washB = dark ? QColor(0xC9, 0x86, 0x56) : QColor(0xD4, 0xA3, 0x73);
    break;
  case Wash::Soft:
  default:
    washA = acc;
    washB = acc;
    break;
  }
  p.fillRect(rect(), desk);

  QRadialGradient g1(width() * 0.18, height() * 0.4,
                     qMax(width(), height()) * 0.75);
  QColor c0 = washA;
  c0.setAlpha(dark ? 48 : 32);
  QColor c1 = washA;
  c1.setAlpha(dark ? 12 : 8);
  g1.setColorAt(0.0, c0);
  g1.setColorAt(0.55, c1);
  g1.setColorAt(1.0, Qt::transparent);
  p.fillRect(rect(), g1);

  QRadialGradient g2(width() * 0.92, height() * 0.2, width() * 0.48);
  QColor c2 = washB;
  c2.setAlpha(dark ? 28 : 18);
  g2.setColorAt(0.0, c2);
  g2.setColorAt(1.0, Qt::transparent);
  p.fillRect(rect(), g2);

  if (!m_logo.isNull()) {
    const int mark = UiScale::dp(qBound(28, height() / 3, 48));
    QPixmap logo = m_logo.scaled(mark, mark, Qt::KeepAspectRatio,
                                 Qt::SmoothTransformation);
    const int mx = UiScale::dp(20);
    const int my = (height() - logo.height()) / 2;
    QPainterPath round;
    round.addEllipse(QRectF(mx, my, logo.width(), logo.height()));
    p.save();
    p.setClipPath(round, Qt::IntersectClip);
    p.drawPixmap(mx, my, logo);
    p.restore();

    QPen ring(washA);
    ring.setWidthF(1.2);
    QColor ringC = washA;
    ringC.setAlpha(dark ? 140 : 150);
    ring.setColor(ringC);
    p.setPen(ring);
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QRectF(mx, my, logo.width(), logo.height()));
  }
}

void DashDeskHero::paintCover(QPainter &p, const QColor &acc, bool dark) {
  if (m_cover.isNull() || width() <= 0 || height() <= 0)
    return;

  const qreal zoom = qBound(1.0, m_zoom, 2.5);
  const qreal sx = qreal(width()) / qreal(m_cover.width());
  const qreal sy = qreal(height()) / qreal(m_cover.height());
  const qreal s = qMax(sx, sy) * zoom;
  const qreal dw = m_cover.width() * s;
  const qreal dh = m_cover.height() * s;
  qreal x = qreal(width()) * 0.5 - m_focusX * dw;
  qreal y = qreal(height()) * 0.5 - m_focusY * dh;
  x = qBound(qreal(width()) - dw, x, 0.0);
  y = qBound(qreal(height()) - dh, y, 0.0);
  p.drawPixmap(QRectF(x, y, dw, dh), m_cover, m_cover.rect());

  QLinearGradient scrim(0, height() * 0.45, 0, height());
  QColor tip = acc;
  tip.setAlpha(dark ? 80 : 40);
  QColor base = dark ? QColor(0, 0, 0, 100) : QColor(15, 23, 42, 28);
  scrim.setColorAt(0.0, Qt::transparent);
  scrim.setColorAt(0.6, tip);
  scrim.setColorAt(1.0, base);
  p.fillRect(rect(), scrim);

  if (m_editMode) {
    p.setPen(QPen(QColor(255, 255, 255, dark ? 180 : 200)));
    QFont f = font();
    f.setPixelSize(UiScale::dp(11));
    f.setWeight(QFont::Medium);
    p.setFont(f);
    p.drawText(QRect(UiScale::dp(12), height() - UiScale::dp(36),
                     width() - UiScale::dp(100), UiScale::dp(24)),
               Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("Ziehen = Position · Rad = Zoom"));
  }
}

void DashDeskHero::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setRenderHint(QPainter::SmoothPixmapTransform, true);

  const int rad = UiScale::dp(BlopStyle::radiusMdDp());
  QPainterPath clip;
  clip.addRoundedRect(QRectF(rect()), rad, rad);
  p.setClipPath(clip);

  const QColor acc = BlopTheme::accentPrimary();
  const bool dark = BlopTheme::instance().isDark();

  if (!m_cover.isNull())
    paintCover(p, acc, dark);
  else
    paintWash(p, m_wash, acc, dark);
}
