#include "libraryorgbar.h"

#include "blop_inwindow_menu.h"
#include "blop_theme.h"
#include "blopstyle.h"
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
  // Inset strokes so RoundCap never clips at the pixmap edge.
  p.setPen(QPen(color, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p.setBrush(Qt::NoBrush);
  switch (view) {
  case LibraryOrgBar::SmartView::All: {
    p.drawRoundedRect(QRectF(7, 7, 7, 7), 1.4, 1.4);
    p.drawRoundedRect(QRectF(18, 7, 7, 7), 1.4, 1.4);
    p.drawRoundedRect(QRectF(7, 18, 7, 7), 1.4, 1.4);
    p.drawRoundedRect(QRectF(18, 18, 7, 7), 1.4, 1.4);
    break;
  }
  case LibraryOrgBar::SmartView::Favorites: {
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    const QPointF c(16, 16.5);
    QPolygonF star;
    for (int i = 0; i < 5; ++i) {
      const qreal a = -M_PI / 2 + i * 2 * M_PI / 5;
      star << QPointF(c.x() + qCos(a) * 9.0, c.y() + qSin(a) * 9.0);
      const qreal b = a + M_PI / 5;
      star << QPointF(c.x() + qCos(b) * 3.8, c.y() + qSin(b) * 3.8);
    }
    p.drawPolygon(star);
    break;
  }
  case LibraryOrgBar::SmartView::Recent: {
    p.drawEllipse(QRectF(7.5, 7.5, 17, 17));
    p.drawLine(QPointF(16, 11), QPointF(16, 16.5));
    p.drawLine(QPointF(16, 16.5), QPointF(20.5, 19));
    break;
  }
  case LibraryOrgBar::SmartView::Untagged: {
    QPainterPath tag;
    tag.moveTo(8, 11);
    tag.lineTo(16, 7);
    tag.lineTo(24, 11);
    tag.lineTo(24, 21);
    tag.lineTo(16, 25);
    tag.lineTo(8, 21);
    tag.closeSubpath();
    p.drawPath(tag);
    p.drawEllipse(QPointF(16, 12.5), 1.5, 1.5);
    break;
  }
  }
  return QIcon(pm);
}
} // namespace

LibraryOrgBar::LibraryOrgBar(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("LibraryOrgBar"));
  setAttribute(Qt::WA_StyledBackground, true);
  // Fixed height so a tall icon-mode list cannot squeeze this row to its
  // minimum and paint the chips on top of the notes.
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  setMinimumWidth(0);
  m_accent = BlopTheme::accentPrimary();

  auto *root = new QHBoxLayout(this);
  root->setContentsMargins(0, 0, 0, UiScale::dp(BlopStyle::controlGapDp()));
  root->setSpacing(UiScale::dp(BlopStyle::controlGapDp()));

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

  QHBoxLayout *chipLay = nullptr;
  auto *scroll = new QScrollArea(this);
  scroll->setObjectName(QStringLiteral("LibraryOrgChipScroll"));
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setWidgetResizable(false);
  scroll->setMinimumWidth(0);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  scroll->setFixedHeight(UiScale::dp(32));
  scroll->setStyleSheet(
      QStringLiteral("QScrollArea { background: transparent; border: none; }"));
  auto *host = new QWidget(scroll);
  host->setObjectName(QStringLiteral("LibraryOrgChipHost"));
  host->setStyleSheet(QStringLiteral("background: transparent;"));
  host->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
  chipLay = new QHBoxLayout(host);
  chipLay->setContentsMargins(0, 0, 0, 0);
  chipLay->setSpacing(UiScale::dp(BlopStyle::controlGapDp()));
  chipLay->setSizeConstraint(QLayout::SetMinAndMaxSize);

  for (const auto &c : chips) {
    auto *btn = new QPushButton(QString::fromUtf8(c.label), host);
    btn->setCheckable(true);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedHeight(UiScale::dp(32));
    btn->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    btn->setObjectName(QStringLiteral("libraryOrgChip"));
    switch (c.view) {
    case SmartView::All:
      btn->setToolTip(QStringLiteral("Smart View: alle Notizen in diesem Ordner"));
      break;
    case SmartView::Favorites:
      btn->setToolTip(QStringLiteral("Smart View: nur Favoriten"));
      break;
    case SmartView::Recent:
      btn->setToolTip(QStringLiteral("Smart View: zuletzt geöffnete Notizen"));
      break;
    case SmartView::Untagged:
      btn->setToolTip(QStringLiteral("Smart View: Notizen ohne Tags"));
      break;
    }
    m_viewGroup->addButton(btn, int(c.view));
    btn->setIconSize(QSize(UiScale::dp(14), UiScale::dp(14)));
    if (c.view == m_view)
      btn->setChecked(true);
    chipLay->addWidget(btn);
  }
  host->adjustSize();
  scroll->setWidget(host);
  root->addWidget(scroll, 1);
  const int barH = UiScale::dp(32 + BlopStyle::controlGapDp());
  setMinimumHeight(barH);
  setMaximumHeight(barH);
  BlopScroll::enableFingerScroll(scroll);
  connect(m_viewGroup, &QButtonGroup::idClicked, this,
          &LibraryOrgBar::onViewClicked);

  m_btnSort = new QPushButton(this);
  m_btnSort->setObjectName(QStringLiteral("libraryOrgSort"));
  m_btnSort->setCursor(Qt::PointingHandCursor);
  m_btnSort->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
  m_btnSort->setFixedHeight(UiScale::dp(32));
  connect(m_btnSort, &QPushButton::clicked, this, &LibraryOrgBar::showSortMenu);
  root->addWidget(m_btnSort);

  m_viewModeHost = new QWidget(this);
  m_viewModeHost->setObjectName(QStringLiteral("LibraryOrgViewModeHost"));
  m_viewModeLay = new QHBoxLayout(m_viewModeHost);
  m_viewModeLay->setContentsMargins(0, 0, 0, 0);
  m_viewModeLay->setSpacing(0);
  m_viewModeHost->setFixedHeight(UiScale::dp(32));
  root->addWidget(m_viewModeHost, 0);

  rebuildStyles();
  refreshSortLabel();
}

void LibraryOrgBar::placeViewModeButtons(QPushButton *grid, QPushButton *list) {
  if (!m_viewModeLay)
    return;
  if (grid) {
    grid->setParent(m_viewModeHost);
    m_viewModeLay->addWidget(grid);
  }
  if (list) {
    list->setParent(m_viewModeHost);
    m_viewModeLay->addWidget(list);
  }
}

void LibraryOrgBar::placeInspectorButton(QPushButton *btn) {
  if (!btn)
    return;
  auto *root = qobject_cast<QHBoxLayout *>(layout());
  if (!root)
    return;
  btn->setParent(this);
  root->addWidget(btn, 0, Qt::AlignVCenter);
}

void LibraryOrgBar::placeSortInActionBar(QLayout *actionRow) {
  if (!actionRow || !m_btnSort)
    return;
  if (auto *parentLay = qobject_cast<QHBoxLayout *>(layout()))
    parentLay->removeWidget(m_btnSort);
  actionRow->addWidget(m_btnSort);
  rebuildStyles();
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

void LibraryOrgBar::setSortMode(SortMode mode) {
  if (m_sort == mode)
    return;
  m_sort = mode;
  refreshSortLabel();
  QSettings s(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  s.setValue(QStringLiteral("ui/librarySortMode"), int(m_sort));
  emit sortModeChanged(m_sort);
}

void LibraryOrgBar::cycleSortMode() {
  setSortMode(m_sort == SortMode::Name ? SortMode::Modified : SortMode::Name);
}

void LibraryOrgBar::refreshSortLabel() {
  if (!m_btnSort)
    return;
  const QString which = m_sort == SortMode::Modified
                            ? QStringLiteral("Zuletzt geändert")
                            : QStringLiteral("Name");
  m_btnSort->setText(QStringLiteral("Sortieren: %1 \u25BE").arg(which));
  m_btnSort->setToolTip(QStringLiteral("Sortieren · %1").arg(which));
}

void LibraryOrgBar::showSortMenu() {
  if (!m_btnSort)
    return;
  QList<BlopInWindowMenu::Item> items;
  {
    BlopInWindowMenu::Item recent;
    recent.label = (m_sort == SortMode::Modified ? QStringLiteral("✓  ")
                                                 : QStringLiteral("    ")) +
                   QStringLiteral("Zuletzt geändert");
    recent.handler = [this]() { setSortMode(SortMode::Modified); };
    items.push_back(recent);
  }
  {
    BlopInWindowMenu::Item byName;
    byName.label = (m_sort == SortMode::Name ? QStringLiteral("✓  ")
                                             : QStringLiteral("    ")) +
                   QStringLiteral("Name A–Z");
    byName.handler = [this]() { setSortMode(SortMode::Name); };
    items.push_back(byName);
  }
  const QPoint global =
      m_btnSort->mapToGlobal(QPoint(m_btnSort->width() - UiScale::dp(8),
                                    m_btnSort->height()));
  BlopInWindowMenu::show(m_btnSort, global, items);
}

void LibraryOrgBar::rebuildStyles() {
  const QString accent = m_accent.name(QColor::HexRgb);
  const bool paperLibrary = !BlopTheme::instance().isDark();
  const QString text = paperLibrary ? BlopStyle::paperInk().name(QColor::HexRgb)
                                    : BlopTheme::textPrimary().name(QColor::HexRgb);
  const QString muted = paperLibrary
                            ? BlopStyle::paperInkMuted().name(QColor::HexRgb)
                            : BlopTheme::textSecondary().name(QColor::HexRgb);
  const QString border =
      paperLibrary ? BlopStyle::paperBorder().name(QColor::HexRgb)
                   : BlopTheme::borderDefault().name(QColor::HexArgb);
  const QString hoverIdle =
      paperLibrary ? BlopStyle::paperHover().name(QColor::HexRgb)
                   : QStringLiteral("rgba(255,255,255,0.07)");
  const QString sortIdle =
      paperLibrary ? QStringLiteral("#FFFFFF")
                   : QStringLiteral("rgba(255,255,255,0.06)");
  const QString sortHover =
      paperLibrary ? BlopStyle::paperHover().name(QColor::HexRgb)
                   : QStringLiteral("rgba(255,255,255,0.10)");
  const QString sortBorder =
      paperLibrary ? QStringLiteral("#E5E7EB")
                   : QStringLiteral("rgba(255,255,255,0.10)");
  const int chipMinH = UiScale::dp(32);
  const int sortPx = UiScale::dp(32);
  const int chipRad = UiScale::dp(6);
  const QString chipIdleBg =
      paperLibrary ? QStringLiteral("#FFFFFF") : QStringLiteral("transparent");
  const QString chipIdleFg =
      paperLibrary ? QStringLiteral("#4B5563") : muted;
  const QString chipIdleBd =
      paperLibrary ? QStringLiteral("#E5E7EB") : border;
  const QString chipOnBg =
      paperLibrary ? QStringLiteral("#EFF6FF")
                   : QStringLiteral("rgba(%1,%2,%3,0.18)")
                         .arg(m_accent.red())
                         .arg(m_accent.green())
                         .arg(m_accent.blue());
  const QString chipOnFg =
      paperLibrary ? QStringLiteral("#1D4ED8") : text;
  const QString chipOnBd =
      paperLibrary ? QStringLiteral("#BFDBFE") : accent;
  setStyleSheet(
      QStringLiteral(
          "QWidget#LibraryOrgBar { background: transparent; }"
          "QPushButton#libraryOrgChip {"
          "  background: %1; color: %2;"
          "  border: 1px solid %3; border-radius: %4px;"
          "  padding: 0 10px; font-size: 12px; font-weight: 600;"
          "  min-height: %5px; max-height: %5px;"
          "}"
          "QPushButton#libraryOrgChip:checked {"
          "  background: %6; color: %7; border: 1px solid %8;"
          "}"
          "QPushButton#libraryOrgChip:hover:!checked {"
          "  background: %9;"
          "}"
          "QPushButton#libraryOrgSort {"
          "  background: %11; color: %2;"
          "  border: 1px solid %3; border-radius: %4px;"
          "  padding: 0 10px; font-size: 12px; font-weight: 600;"
          "  min-height: %10px; max-height: %10px;"
          "}"
          "QPushButton#libraryOrgSort:hover {"
          "  background: %9;"
          "}")
          .arg(chipIdleBg, chipIdleFg, chipIdleBd)
          .arg(chipRad)
          .arg(chipMinH)
          .arg(chipOnBg, chipOnFg, chipOnBd, hoverIdle)
          .arg(sortPx)
          .arg(sortIdle));
  if (m_viewModeHost) {
    m_viewModeHost->setStyleSheet(QStringLiteral(
        "QWidget#LibraryOrgViewModeHost {"
        "  background: %3; border: 1px solid %1; border-radius: %2px;"
        "}"
        "QPushButton {"
        "  background: transparent; border: none; border-radius: 6px;"
        "}")
                                       .arg(sortBorder)
                                       .arg(chipRad)
                                       .arg(sortIdle));
  }

  // When the sort control is reparented into the library header, this
  // widget stylesheet no longer reaches it — paint the soft plate there too.
  if (m_btnSort && m_btnSort->parentWidget() != this) {
    m_btnSort->setStyleSheet(QStringLiteral(
        "QPushButton#libraryOrgSort {"
        "  background: %1; color: %2;"
        "  border: 1px solid %3; border-radius: %6px;"
        "  padding: 0 10px; font-size: 12px; font-weight: 600;"
        "}"
        "QPushButton#libraryOrgSort:hover {"
        "  background: %4; border-color: %5; color: %5;"
        "}")
                                 .arg(sortIdle, text, sortBorder, sortHover,
                                      accent)
                                 .arg(chipRad));
  }

  if (!m_viewGroup)
    return;
  const QColor idle =
      paperLibrary ? BlopStyle::paperInkMuted() : BlopTheme::textSecondary();
  const QList<QAbstractButton *> btns = m_viewGroup->buttons();
  for (QAbstractButton *b : btns) {
    if (!b)
      continue;
    const auto view = static_cast<SmartView>(m_viewGroup->id(b));
    const QColor ic = b->isChecked() ? m_accent : idle;
    b->setIcon(chipGlyph(view, ic));
  }
}
