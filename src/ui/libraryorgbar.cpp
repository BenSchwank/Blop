#include "libraryorgbar.h"

#include "blop_theme.h"
#include "uiscale.h"

#include <QButtonGroup>
#include <QColor>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSettings>

LibraryOrgBar::LibraryOrgBar(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("LibraryOrgBar"));
  setAttribute(Qt::WA_StyledBackground, true);

  auto *lay = new QHBoxLayout(this);
  lay->setContentsMargins(0, UiScale::dp(4), 0, UiScale::dp(10));
  lay->setSpacing(UiScale::dp(8));

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

  for (const auto &c : chips) {
    auto *btn = new QPushButton(QString::fromUtf8(c.label), this);
    btn->setCheckable(true);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedHeight(UiScale::dp(30));
    btn->setObjectName(QStringLiteral("libraryOrgChip"));
    m_viewGroup->addButton(btn, int(c.view));
    if (c.view == m_view)
      btn->setChecked(true);
    lay->addWidget(btn);
  }
  connect(m_viewGroup, &QButtonGroup::idClicked, this,
          &LibraryOrgBar::onViewClicked);

  lay->addStretch(1);

  m_btnSort = new QPushButton(this);
  m_btnSort->setObjectName(QStringLiteral("libraryOrgSort"));
  m_btnSort->setCursor(Qt::PointingHandCursor);
  m_btnSort->setFixedHeight(UiScale::dp(30));
  m_btnSort->setText(m_sort == SortMode::Modified
                         ? QStringLiteral("Sortierung · Datum")
                         : QStringLiteral("Sortierung · Name"));
  connect(m_btnSort, &QPushButton::clicked, this, &LibraryOrgBar::onSortClicked);
  lay->addWidget(m_btnSort);

  rebuildStyles();
}

void LibraryOrgBar::setAccentColor(const QColor &color) {
  if (!color.isValid())
    return;
  m_accent = color;
  rebuildStyles();
}

void LibraryOrgBar::onViewClicked(int id) {
  m_view = static_cast<SmartView>(id);
  QSettings s(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  s.setValue(QStringLiteral("ui/librarySmartView"), int(m_view));
  emit smartViewChanged(m_view);
}

void LibraryOrgBar::onSortClicked() {
  m_sort = (m_sort == SortMode::Name) ? SortMode::Modified : SortMode::Name;
  m_btnSort->setText(m_sort == SortMode::Modified
                         ? QStringLiteral("Sortierung · Datum")
                         : QStringLiteral("Sortierung · Name"));
  QSettings s(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  s.setValue(QStringLiteral("ui/librarySortMode"), int(m_sort));
  emit sortModeChanged(m_sort);
}

void LibraryOrgBar::rebuildStyles() {
  const QString accent = m_accent.name(QColor::HexRgb);
  const QString text = BlopTheme::textPrimary().name(QColor::HexRgb);
  const QString muted = BlopTheme::textSecondary().name(QColor::HexRgb);
  const QString border = BlopTheme::borderSubtle().name(QColor::HexArgb);
  setStyleSheet(QStringLiteral(
      "QWidget#LibraryOrgBar { background: transparent; }"
      "QPushButton#libraryOrgChip {"
      "  background: rgba(255,255,255,0.04); color: %1;"
      "  border: 1px solid %2; border-radius: 15px;"
      "  padding: 0 12px; font-size: 12px; font-weight: 600;"
      "}"
      "QPushButton#libraryOrgChip:checked {"
      "  background: rgba(%3,%4,%5,0.22); color: %6;"
      "  border: 1px solid %7;"
      "}"
      "QPushButton#libraryOrgChip:hover:!checked {"
      "  background: rgba(255,255,255,0.07);"
      "}"
      "QPushButton#libraryOrgSort {"
      "  background: transparent; color: %1;"
      "  border: 1px solid %2; border-radius: 15px;"
      "  padding: 0 12px; font-size: 12px; font-weight: 600;"
      "}"
      "QPushButton#libraryOrgSort:hover {"
      "  border-color: %7; color: %6;"
      "}")
                    .arg(muted, border)
                    .arg(m_accent.red())
                    .arg(m_accent.green())
                    .arg(m_accent.blue())
                    .arg(text, accent));
}
