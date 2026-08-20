#include "libraryorgbar.h"

#include "blop_theme.h"
#include "blop_scroll.h"
#include "uiscale.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QColor>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QtMath>

namespace {
QIcon chipGlyph(LibraryOrgBar::SmartView view, const QColor &color) {
  const int s = 32;
  QPixmap pm(s, s);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setPen(QPen(color, 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p.setBrush(Qt::NoBrush);
  switch (view) {
  case LibraryOrgBar::SmartView::All: {
    p.drawRoundedRect(QRectF(6, 6, 8, 8), 1.6, 1.6);
    p.drawRoundedRect(QRectF(18, 6, 8, 8), 1.6, 1.6);
    p.drawRoundedRect(QRectF(6, 18, 8, 8), 1.6, 1.6);
    p.drawRoundedRect(QRectF(18, 18, 8, 8), 1.6, 1.6);
    break;
  }
  case LibraryOrgBar::SmartView::Favorites: {
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    const QPointF c(16, 16.5);
    QPolygonF star;
    for (int i = 0; i < 5; ++i) {
      const qreal a = -M_PI / 2 + i * 2 * M_PI / 5;
      star << QPointF(c.x() + qCos(a) * 10.0, c.y() + qSin(a) * 10.0);
      const qreal b = a + M_PI / 5;
      star << QPointF(c.x() + qCos(b) * 4.2, c.y() + qSin(b) * 4.2);
    }
    p.drawPolygon(star);
    break;
  }
  case LibraryOrgBar::SmartView::Recent: {
    p.drawEllipse(QRectF(6, 6, 20, 20));
    p.drawLine(QPointF(16, 10), QPointF(16, 16));
    p.drawLine(QPointF(16, 16), QPointF(21, 19));
    break;
  }
  case LibraryOrgBar::SmartView::Untagged: {
    QPainterPath tag;
    tag.moveTo(7, 11);
    tag.lineTo(16, 6);
    tag.lineTo(25, 11);
    tag.lineTo(25, 22);
    tag.lineTo(16, 26);
    tag.lineTo(7, 22);
    tag.closeSubpath();
    p.drawPath(tag);
    p.drawEllipse(QPointF(16, 12), 1.6, 1.6);
    break;
  }
  }
  return QIcon(pm);
}
} // namespace

LibraryOrgBar::LibraryOrgBar(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("LibraryOrgBar"));
  setAttribute(Qt::WA_StyledBackground, true);

  auto *root = new QHBoxLayout(this);
  root->setContentsMargins(0, UiScale::dp(2), 0, UiScale::dp(8));
  root->setSpacing(UiScale::dp(8));

  m_viewGroup = new QButtonGroup(this);
  m_viewGroup->setExclusive(true);

  const struct {
    SmartView view;
    const char *label;
  } chips[] = {
      {SmartView::All, "Alle"},
      {SmartView::Favorites, "Favoriten"},
      {SmartView::Recent, "Zuletzt"},
      {SmartView::Untagged, "Ohne Tags"},
  };

  QSettings s(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  m_view = static_cast<SmartView>(
      s.value(QStringLiteral("ui/librarySmartView"), int(SmartView::All))
          .toInt());
  m_sort = static_cast<SortMode>(
      s.value(QStringLiteral("ui/librarySortMode"), int(SortMode::Name))
          .toInt());
  if (m_view < SmartView::All || m_view > SmartView::Untagged)
    m_view = SmartView::All;
  if (m_sort != SortMode::Name && m_sort != SortMode::Modified)
    m_sort = SortMode::Name;

  const bool phone = UiScale::isAndroidPhoneUi(parentWidget());
  QHBoxLayout *chipLay = root;
  if (phone) {
    auto *scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("LibraryOrgChipScroll"));
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFixedHeight(UiScale::dp(48));
    scroll->setStyleSheet(
        QStringLiteral("QScrollArea { background: transparent; border: none; }"));
    auto *host = new QWidget(scroll);
    host->setObjectName(QStringLiteral("LibraryOrgChipHost"));
    host->setStyleSheet(QStringLiteral("background: transparent;"));
    chipLay = new QHBoxLayout(host);
    chipLay->setContentsMargins(0, 0, 0, 0);
    chipLay->setSpacing(UiScale::dp(8));
    scroll->setWidget(host);
    root->addWidget(scroll, 1);
    BlopScroll::enableFingerScroll(scroll);
  }

  for (const auto &c : chips) {
    auto *btn = new QPushButton(QString::fromUtf8(c.label), this);
    btn->setCheckable(true);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedHeight(UiScale::dp(phone ? 44 : 28));
    btn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    btn->setObjectName(QStringLiteral("libraryOrgChip"));
    m_viewGroup->addButton(btn, int(c.view));
    btn->setIconSize(QSize(UiScale::dp(14), UiScale::dp(14)));
    if (c.view == m_view)
      btn->setChecked(true);
    chipLay->addWidget(btn);
  }
  connect(m_viewGroup, &QButtonGroup::idClicked, this,
          &LibraryOrgBar::onViewClicked);

  if (!phone)
    root->addStretch(1);

  m_btnSort = new QPushButton(this);
  m_btnSort->setObjectName(QStringLiteral("libraryOrgSort"));
  m_btnSort->setCursor(Qt::PointingHandCursor);
  m_btnSort->setFixedHeight(UiScale::dp(phone ? 44 : 34));
  refreshSortLabel();
  connect(m_btnSort, &QPushButton::clicked, this, &LibraryOrgBar::cycleSortMode);
  // Sort starts in this bar; placeSortInActionBar() can lift it into the header.
  root->addWidget(m_btnSort);

  rebuildStyles();
}

void LibraryOrgBar::placeSortInActionBar(QLayout *actionRow) {
  if (!actionRow || !m_btnSort)
    return;
  if (auto *parentLay = qobject_cast<QHBoxLayout *>(layout()))
    parentLay->removeWidget(m_btnSort);
  actionRow->addWidget(m_btnSort);
}

void LibraryOrgBar::setAccentColor(const QColor &color) {
  if (!color.isValid())
    return;
  m_accent = color;
  rebuildStyles();
}

void LibraryOrgBar::setSmartView(SmartView view) {
  if (view < SmartView::All || view > SmartView::Untagged)
    view = SmartView::All;
  if (m_view == view) {
    if (m_viewGroup) {
      if (QAbstractButton *b = m_viewGroup->button(int(view)))
        b->setChecked(true);
    }
    return;
  }
  m_view = view;
  if (m_viewGroup) {
    if (QAbstractButton *b = m_viewGroup->button(int(view)))
      b->setChecked(true);
  }
  QSettings s(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  s.setValue(QStringLiteral("ui/librarySmartView"), int(m_view));
  emit smartViewChanged(m_view);
  rebuildStyles();
}

void LibraryOrgBar::onViewClicked(int id) {
  m_view = static_cast<SmartView>(id);
  QSettings s(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  s.setValue(QStringLiteral("ui/librarySmartView"), int(m_view));
  rebuildStyles();
  emit smartViewChanged(m_view);
}

void LibraryOrgBar::cycleSortMode() {
  m_sort = (m_sort == SortMode::Name) ? SortMode::Modified : SortMode::Name;
  refreshSortLabel();
  QSettings s(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  s.setValue(QStringLiteral("ui/librarySortMode"), int(m_sort));
  emit sortModeChanged(m_sort);
}

void LibraryOrgBar::refreshSortLabel() {
  if (!m_btnSort)
    return;
  m_btnSort->setText(m_sort == SortMode::Modified
                         ? QStringLiteral("Datum")
                         : QStringLiteral("Name"));
  m_btnSort->setToolTip(m_sort == SortMode::Modified
                            ? QStringLiteral("Sortierung: zuletzt geändert")
                            : QStringLiteral("Sortierung: Name A–Z"));
}

void LibraryOrgBar::rebuildStyles() {
  const QString accent = m_accent.name(QColor::HexRgb);
  const QString text = BlopTheme::textPrimary().name(QColor::HexRgb);
  const QString muted = BlopTheme::textSecondary().name(QColor::HexRgb);
  const QString border = BlopTheme::borderSubtle().name(QColor::HexArgb);
  setStyleSheet(BlopTheme::themed(
      QStringLiteral(
          "QWidget#LibraryOrgBar { background: transparent; }"
          "QPushButton#libraryOrgChip {"
          "  background: rgba(255,255,255,0.07); color: %1;"
          "  border: 1px solid %2; border-radius: 14px;"
          "  padding: 0 12px 0 10px; font-size: 12px; font-weight: 600;"
          "}"
          "QPushButton#libraryOrgChip:checked {"
          "  background: %7; color: #FFFFFF;"
          "  border: 1px solid %7;"
          "}"
          "QPushButton#libraryOrgChip:hover:!checked {"
          "  background: rgba(255,255,255,0.11);"
          "}"
          "QPushButton#libraryOrgSort {"
          "  background: rgba(255,255,255,0.07); color: %6;"
          "  border: 1px solid %2; border-radius: 12px;"
          "  padding: 0 14px; font-size: 12px; font-weight: 600;"
          "  min-width: 72px;"
          "}"
          "QPushButton#libraryOrgSort:hover {"
          "  border-color: %7; background: rgba(%3,%4,%5,0.18);"
          "}")
          .arg(muted, border)
          .arg(m_accent.red())
          .arg(m_accent.green())
          .arg(m_accent.blue())
          .arg(text, accent)));

  if (!m_viewGroup)
    return;
  const QColor idle = BlopTheme::textSecondary();
  const QList<QAbstractButton *> btns = m_viewGroup->buttons();
  for (QAbstractButton *b : btns) {
    if (!b)
      continue;
    const auto view = static_cast<SmartView>(m_viewGroup->id(b));
    const QColor ic = b->isChecked() ? QColor(Qt::white) : idle;
    b->setIcon(chipGlyph(view, ic));
  }
}
