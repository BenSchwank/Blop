#include "dashboardpage.h"

#include "blop_dialogs.h"
#include "blop_inwindow_menu.h"
#include "blop_modal.h"
#include "blop_theme.h"
#include "bloplocale.h"
#include "blopstyle.h"
#include "calendardayview.h"
#include "calendarservice.h"
#include "dashcanvas.h"
#include "dashpagescroll.h"
#include "dashrightrail.h"
#include "overlayscrollindicator.h"
#include "phonechrome.h"
#include "uiscale.h"

#include <QDate>
#include <QDateTime>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QPushButton>
#include <QResizeEvent>
#include <QShortcut>
#include <QTimer>
#include <QVBoxLayout>

namespace {
QString pageBg() {
  return BlopTheme::instance().isDark()
             ? BlopStyle::obsidianContent().name(QColor::HexRgb)
             : QStringLiteral("#F4F5F7");
}

QString muted() {
  return BlopTheme::instance().isDark()
             ? QColor(255, 255, 255, 150).name(QColor::HexArgb)
             : BlopStyle::paperInkMuted().name(QColor::HexRgb);
}

QString dashPillQss(bool accented) {
  const int rad = UiScale::dp(BlopStyle::radiusMdDp());
  const int minH = UiScale::dp(BlopStyle::touchTargetMinDp() - 4);
  const QColor accC = BlopTheme::accentPrimary();
  auto rgba = [](const QColor &base, qreal a) {
    return QStringLiteral("rgba(%1,%2,%3,%4)")
        .arg(base.red())
        .arg(base.green())
        .arg(base.blue())
        .arg(QString::number(a, 'f', 2));
  };
  if (accented) {
    return QStringLiteral(
               "QPushButton {"
               "  background: %1; color: %2;"
               "  border: 1px solid %3;"
               "  border-radius: %4px; padding: 0 12px; font-weight: 600;"
               "  font-size: 12px; min-height: %5px;"
               "}"
               "QPushButton:hover { background: %6; }"
               "QPushButton:disabled { color: %7; border-color: transparent;"
               "  background: transparent; }")
        .arg(rgba(accC, BlopTheme::instance().isDark() ? 0.16 : 0.10),
             accC.name(QColor::HexRgb),
             rgba(accC, BlopTheme::instance().isDark() ? 0.40 : 0.35),
             QString::number(rad), QString::number(minH),
             rgba(accC, BlopTheme::instance().isDark() ? 0.24 : 0.16),
             muted());
  }
  if (BlopTheme::instance().isDark()) {
    return QStringLiteral(
               "QPushButton {"
               "  background: transparent; color: rgba(255,255,255,0.62);"
               "  border: 1px solid rgba(255,255,255,0.10); border-radius: %1px;"
               "  padding: 0 12px; font-weight: 550; font-size: 12px;"
               "  min-height: %2px;"
               "}"
               "QPushButton:hover { background: rgba(255,255,255,0.06);"
               "  color: rgba(255,255,255,0.92); }"
               "QPushButton:disabled { color: rgba(255,255,255,0.28);"
               "  border-color: transparent; }")
        .arg(rad)
        .arg(minH);
  }
  return QStringLiteral(
             "QPushButton {"
             "  background: transparent; color: %1;"
             "  border: 1px solid rgba(15,23,42,0.10); border-radius: %2px;"
             "  padding: 0 12px; font-weight: 550; font-size: 12px;"
             "  min-height: %3px;"
             "}"
             "QPushButton:hover { background: rgba(15,23,42,0.05);"
             "  color: %4; }"
             "QPushButton:disabled { color: rgba(15,23,42,0.28);"
             "  border-color: transparent; }")
      .arg(muted(), QString::number(rad), QString::number(minH),
           BlopStyle::paperInk().name(QColor::HexRgb));
}

/// Quiet icon-only overflow — Notion-style, no admin chrome.
QString dashMoreBtnQss(bool editing) {
  const int rad = UiScale::dp(BlopStyle::radiusMdDp());
  const int minH = UiScale::dp(BlopStyle::touchTargetMinDp() - 4);
  const QColor accC = BlopTheme::accentPrimary();
  auto rgba = [](const QColor &base, qreal a) {
    return QStringLiteral("rgba(%1,%2,%3,%4)")
        .arg(base.red())
        .arg(base.green())
        .arg(base.blue())
        .arg(QString::number(a, 'f', 2));
  };
  if (editing) {
    return QStringLiteral(
               "QPushButton {"
               "  background: %1; color: %2;"
               "  border: 1px solid %3;"
               "  border-radius: %4px; padding: 0; font-weight: 700;"
               "  font-size: 18px; min-width: %5px; min-height: %5px;"
               "}"
               "QPushButton:hover { background: %6; }")
        .arg(rgba(accC, BlopTheme::instance().isDark() ? 0.16 : 0.10),
             accC.name(QColor::HexRgb),
             rgba(accC, BlopTheme::instance().isDark() ? 0.40 : 0.35),
             QString::number(rad), QString::number(minH),
             rgba(accC, BlopTheme::instance().isDark() ? 0.24 : 0.16));
  }
  if (BlopTheme::instance().isDark()) {
    return QStringLiteral(
               "QPushButton {"
               "  background: transparent; color: rgba(255,255,255,0.55);"
               "  border: 1px solid transparent; border-radius: %1px;"
               "  padding: 0; font-weight: 700; font-size: 18px;"
               "  min-width: %2px; min-height: %2px;"
               "}"
               "QPushButton:hover { background: rgba(255,255,255,0.06);"
               "  color: rgba(255,255,255,0.92); }")
        .arg(rad)
        .arg(minH);
  }
  return QStringLiteral(
             "QPushButton {"
             "  background: transparent; color: %1;"
             "  border: 1px solid transparent; border-radius: %2px;"
             "  padding: 0; font-weight: 700; font-size: 18px;"
             "  min-width: %3px; min-height: %3px;"
             "}"
             "QPushButton:hover { background: rgba(15,23,42,0.05);"
             "  color: %4; }")
      .arg(muted(), QString::number(rad), QString::number(minH),
           BlopStyle::paperInk().name(QColor::HexRgb));
}
} // namespace

DashboardPage::DashboardPage(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("DashboardPage"));
  setAttribute(Qt::WA_StyledBackground, true);

  m_root = new QVBoxLayout(this);
  m_root->setContentsMargins(0, 0, 0, 0);
  m_root->setSpacing(0);

  // Shell row: board column | charcoal right rail (matches left icon rail).
  m_bodyRow = new QWidget(this);
  auto *shellLay = new QHBoxLayout(m_bodyRow);
  shellLay->setContentsMargins(0, 0, 0, 0);
  shellLay->setSpacing(0);

  m_mainCol = new QWidget(m_bodyRow);
  m_mainCol->setObjectName(QStringLiteral("DashMainCol"));
  auto *mainLay = new QVBoxLayout(m_mainCol);
  mainLay->setContentsMargins(0, 0, 0, 0);
  mainLay->setSpacing(0);

  m_scroll = new DashPageScroll(m_mainCol);
  m_scroll->setObjectName(QStringLiteral("DashPageScroll"));
  m_scroll->setWidgetResizable(true);
  m_scroll->setFrameShape(QFrame::NoFrame);
  m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_scroll->setStyleSheet(QStringLiteral(
      "QScrollArea#DashPageScroll { background: transparent; border: none; }"
      "QScrollArea#DashPageScroll > QWidget > QWidget { background: transparent; }"
      "QScrollBar:vertical, QScrollBar:horizontal { width: 0px; height: 0px; }"));
  OverlayScrollIndicator::install(m_scroll);

  m_canvas = new DashCanvas(m_scroll);
  m_scroll->setWidget(m_canvas);
  mainLay->addWidget(m_scroll, 1);

  // Floating controls — no dedicated gray header strip above the banner.
  buildHeader();
  m_header->setParent(m_mainCol);
  m_header->raise();
  shellLay->addWidget(m_mainCol, 1);

  m_rightRail = new DashRightRail(m_bodyRow);
  connect(m_rightRail, &DashRightRail::openNotePath, this,
          &DashboardPage::openNotePath);
  shellLay->addWidget(m_rightRail, 0);
  m_root->addWidget(m_bodyRow, 1);

  connect(m_canvas, &DashCanvas::openNotePath, this, &DashboardPage::openNotePath);
  connect(m_canvas, &DashCanvas::newNoteRequested, this,
          &DashboardPage::newNoteRequested);
  connect(m_canvas, &DashCanvas::snapToNotesRequested, this,
          &DashboardPage::snapToNotesRequested);
  connect(m_canvas, &DashCanvas::studyRequested, this,
          &DashboardPage::studyRequested);
  connect(m_canvas, &DashCanvas::searchLibrary, this,
          &DashboardPage::searchLibrary);
  connect(m_canvas, &DashCanvas::maximizeCalendarRequested, this,
          &DashboardPage::showCalendarMaximized);
  connect(m_canvas, &DashCanvas::specsChanged, this,
          [this](const QVector<DashboardWidgetSpec> &specs) {
            // Undo records the previous committed board whenever a new layout
            // is published during edit mode (does not rely on about-to-change).
            if (m_editMode && !m_undoRestoring &&
                !layoutsEqual(m_committed, specs)) {
              m_undoStack.append(m_committed);
              while (m_undoStack.size() > 40)
                m_undoStack.removeFirst();
              updateUndoButton();
            }
            m_committed = specs;
            DashboardLayoutStore::save(specs);
            syncRailFromBoard();
          });
  connect(m_canvas, &DashCanvas::boardContentChanged, this, [this]() {
    if (m_rightRail)
      m_rightRail->refresh();
  });

  connect(&CalendarService::instance(), &CalendarService::eventsChanged, this,
          &DashboardPage::refresh, Qt::QueuedConnection);
  connect(&BlopTheme::instance(), &BlopTheme::themeChanged, this, [this]() {
    applyChrome();
    refresh();
  });
  connect(&BlopLocale::instance(), &BlopLocale::localeChanged, this,
          &DashboardPage::refresh);

  m_undoShortcut = new QShortcut(QKeySequence::Undo, this);
  m_undoShortcut->setContext(Qt::ApplicationShortcut);
  connect(m_undoShortcut, &QShortcut::activated, this, [this]() {
    if (m_editMode)
      undoLayout();
  });

  m_clockTimer = new QTimer(this);
  connect(m_clockTimer, &QTimer::timeout, this, &DashboardPage::updateHeader);
  m_clockTimer->start(30000);

  applyChrome();
  applyDensity();
  m_canvas->setPhoneMode(usePhone());
  m_canvas->setSpecs(DashboardLayoutStore::load());
  m_canvas->syncWidgets();
  m_committed = m_canvas->specs();
  syncRailFromBoard();
  updateHeader();
  updateUndoButton();
}

bool DashboardPage::usePhone() const {
  return UiScale::isAndroidPhoneUi(const_cast<DashboardPage *>(this)) ||
         UiScale::isPhoneSizedLayout(const_cast<DashboardPage *>(this));
}

void DashboardPage::applyChrome() {
  setStyleSheet(QStringLiteral(
                    "QWidget#DashboardPage { background: %1; }"
                    "QWidget#DashHeader {"
                    "  background: transparent;"
                    "  border: none;"
                    "}")
                    .arg(pageBg()));
  if (m_btnCustomize)
    m_btnCustomize->setStyleSheet(dashPillQss(m_editMode));
  if (m_btnUndo)
    m_btnUndo->setStyleSheet(dashPillQss(false));
  if (m_btnMore)
    m_btnMore->setStyleSheet(dashMoreBtnQss(m_editMode));
}

void DashboardPage::applyDensity() {
  const bool phone = usePhone();
  if (m_header) {
    if (auto *lay = qobject_cast<QBoxLayout *>(m_header->layout())) {
      lay->setContentsMargins(UiScale::dp(8), UiScale::dp(8), UiScale::dp(10),
                              UiScale::dp(8));
      lay->setSpacing(UiScale::dp(6));
    }
  }
  if (m_bodyRow) {
    if (auto *lay = qobject_cast<QBoxLayout *>(m_bodyRow->layout())) {
      lay->setContentsMargins(0, 0, 0, 0);
      lay->setSpacing(0);
    }
  }
  if (m_canvas)
    m_canvas->setPhoneMode(phone);
  if (m_rightRail) {
    m_rightRail->setVisible(!phone);
    if (m_canvas)
      m_canvas->setBesideRail(!phone && m_rightRail->isVisible());
  } else if (m_canvas) {
    m_canvas->setBesideRail(false);
  }
  layoutFloatingHeader();
}

void DashboardPage::buildHeader() {
  m_header = new QWidget(m_mainCol ? m_mainCol : this);
  m_header->setObjectName(QStringLiteral("DashHeader"));
  m_header->setAttribute(Qt::WA_StyledBackground, true);
  auto *lay = new QHBoxLayout(m_header);
  lay->setSpacing(UiScale::dp(6));
  lay->setContentsMargins(UiScale::dp(8), UiScale::dp(8), UiScale::dp(10),
                          UiScale::dp(8));

  m_btnUndo = new QPushButton(QStringLiteral("Rückgängig"), m_header);
  m_btnUndo->setCursor(Qt::PointingHandCursor);
  m_btnUndo->setToolTip(QStringLiteral("Letzte Layout-Änderung rückgängig"));
  m_btnUndo->setVisible(false);
  m_btnUndo->setEnabled(false);
  m_btnUndo->setStyleSheet(dashPillQss(false));
  connect(m_btnUndo, &QPushButton::clicked, this, &DashboardPage::undoLayout);
  lay->addWidget(m_btnUndo, 0, Qt::AlignTop);

  m_btnCustomize = new QPushButton(QStringLiteral("Anpassen"), m_header);
  m_btnCustomize->setCursor(Qt::PointingHandCursor);
  m_btnCustomize->setToolTip(QStringLiteral("Dashboard anpassen"));
  m_btnCustomize->setStyleSheet(dashPillQss(false));
  connect(m_btnCustomize, &QPushButton::clicked, this,
          &DashboardPage::toggleEditMode);
  lay->addWidget(m_btnCustomize, 0, Qt::AlignTop);

  m_btnMore = new QPushButton(QStringLiteral("⋯"), m_header);
  m_btnMore->setCursor(Qt::PointingHandCursor);
  m_btnMore->setToolTip(QStringLiteral("Mehr"));
  m_btnMore->setFixedSize(UiScale::dp(BlopStyle::touchTargetMinDp() - 4),
                          UiScale::dp(BlopStyle::touchTargetMinDp() - 4));
  m_btnMore->setStyleSheet(dashMoreBtnQss(false));
  connect(m_btnMore, &QPushButton::clicked, this,
          &DashboardPage::showOverflowMenu);
  lay->addWidget(m_btnMore, 0, Qt::AlignTop);

  m_header->adjustSize();
  m_header->raise();
}

void DashboardPage::layoutFloatingHeader() {
  if (!m_header || !m_mainCol)
    return;
  m_header->adjustSize();
  const int w = m_header->sizeHint().width();
  const int h = m_header->sizeHint().height();
  const int x = qMax(0, m_mainCol->width() - w - UiScale::dp(4));
  const int y = UiScale::dp(2);
  m_header->setGeometry(x, y, w, h);
  m_header->raise();
}

void DashboardPage::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  layoutFloatingHeader();
}

void DashboardPage::updateHeader() {
  // Greeting/metrics live in the intro board widget; keep board + rail fresh.
  if (m_canvas)
    m_canvas->refreshAll();
  if (m_rightRail)
    m_rightRail->refresh();
  if (m_btnCustomize) {
    m_btnCustomize->setText(m_editMode ? QStringLiteral("Fertig")
                                       : QStringLiteral("Anpassen"));
    m_btnCustomize->setStyleSheet(dashPillQss(m_editMode));
  }
  if (m_btnMore)
    m_btnMore->setStyleSheet(dashMoreBtnQss(m_editMode));
  updateUndoButton();
}

void DashboardPage::refresh() {
  applyDensity();
  if (m_canvas) {
    m_canvas->setPhoneMode(usePhone());
    m_canvas->refreshAll();
    m_canvas->applyPositions();
  }
  syncRailFromBoard();
  if (m_rightRail)
    m_rightRail->refresh();
  updateHeader();
}

void DashboardPage::setEditMode(bool on) {
  if (m_editMode == on)
    return;
  m_editMode = on;
  if (m_btnCustomize) {
    m_btnCustomize->setText(on ? QStringLiteral("Fertig")
                               : QStringLiteral("Anpassen"));
    m_btnCustomize->setStyleSheet(dashPillQss(on));
  }
  if (m_btnMore)
    m_btnMore->setStyleSheet(dashMoreBtnQss(on));
  m_undoStack.clear();
  if (on && m_canvas)
    m_committed = m_canvas->specs();
  updateUndoButton();
  if (m_canvas)
    m_canvas->setEditMode(on);
  if (m_rightRail)
    m_rightRail->setEditMode(on);
  emit customizeToggled(on);
}

void DashboardPage::toggleEditMode() { setEditMode(!m_editMode); }

bool DashboardPage::layoutsEqual(const QVector<DashboardWidgetSpec> &a,
                                 const QVector<DashboardWidgetSpec> &b) {
  if (a.size() != b.size())
    return false;
  for (const auto &x : a) {
    bool found = false;
    for (const auto &y : b) {
      if (y.id != x.id)
        continue;
      found = true;
      if (x.visible != y.visible || x.row != y.row || x.col != y.col ||
          x.sizeClass != y.sizeClass || x.bgEnabled != y.bgEnabled ||
          x.borderEnabled != y.borderEnabled || x.bgColor != y.bgColor ||
          x.borderColor != y.borderColor)
        return false;
      break;
    }
    if (!found)
      return false;
  }
  return true;
}

void DashboardPage::undoLayout() {
  if (!m_editMode || m_undoStack.isEmpty() || !m_canvas)
    return;
  const QVector<DashboardWidgetSpec> specs = m_undoStack.takeLast();
  m_undoRestoring = true;
  m_canvas->restoreSpecs(specs);
  m_undoRestoring = false;
  updateUndoButton();
}

void DashboardPage::updateUndoButton() {
  const bool show = m_editMode;
  const bool can = show && !m_undoStack.isEmpty();
  if (m_btnUndo) {
    m_btnUndo->setVisible(show);
    m_btnUndo->setEnabled(can);
    m_btnUndo->setText(can ? QStringLiteral("Rückgängig (%1)")
                                 .arg(m_undoStack.size())
                           : QStringLiteral("Rückgängig"));
  }
  layoutFloatingHeader();
}

void DashboardPage::syncRailFromBoard() {
  if (!m_rightRail || !m_canvas)
    return;
  QStringList occupied;
  for (const auto &s : m_canvas->specs()) {
    if (s.visible)
      occupied.append(s.id);
  }
  m_rightRail->setBoardOccupiedIds(occupied);
}

void DashboardPage::persistAndApply(const QVector<DashboardWidgetSpec> &specs) {
  if (m_canvas)
    m_canvas->restoreSpecs(specs);
  else
    DashboardLayoutStore::save(specs);
}

void DashboardPage::resetLayout() {
  DashboardLayoutStore::reset();
  persistAndApply(DashboardLayoutStore::defaults());
}

void DashboardPage::showOverflowMenu() {
  if (!m_btnMore)
    return;
  QList<BlopInWindowMenu::Item> items;
  items.push_back(
      {QStringLiteral("Blöcke…"), QIcon(), [this]() { showBlocksMenu(); }});
  if (m_editMode) {
    BlopInWindowMenu::Item sep;
    sep.separator = true;
    items.push_back(sep);
    items.push_back({QStringLiteral("Zurücksetzen"), QIcon(),
                     [this]() { resetLayout(); }, true});
  }
  BlopInWindowMenu::show(
      this, m_btnMore->mapToGlobal(QPoint(0, m_btnMore->height())), items);
}

void DashboardPage::showBlocksMenu() {
  if (!m_canvas || !m_btnMore)
    return;
  auto specs = m_canvas->specs();
  QList<BlopInWindowMenu::Item> items;
  for (const QString &id : DashboardLayoutStore::knownIds()) {
    bool visible = false;
    for (const auto &s : specs) {
      if (s.id == id) {
        visible = s.visible;
        break;
      }
    }
    const QString label =
        QStringLiteral("%1 %2")
            .arg(visible ? QStringLiteral("✓") : QStringLiteral("＋"))
            .arg(DashboardLayoutStore::displayName(id));
    items.push_back({label, QIcon(), [this, id, visible]() {
                       if (m_canvas)
                         m_canvas->setBlockVisible(id, !visible);
                     }});
  }
  BlopInWindowMenu::Item sep;
  sep.separator = true;
  items.push_back(sep);
  BlopInWindowMenu::Item addBanner;
  addBanner.label = QStringLiteral("＋ Banner hinzufügen");
  addBanner.handler = [this]() {
    if (m_canvas)
      m_canvas->addBanner();
  };
  items.push_back(addBanner);
  BlopInWindowMenu::show(
      this, m_btnMore->mapToGlobal(QPoint(0, m_btnMore->height())), items);
}

void DashboardPage::showCalendarMaximized() {
  if (m_calModal) {
    m_calModal->raise();
    return;
  }

  auto *day = new CalendarDayView();
  day->setCompact(false);
  day->setMinimal(false);
  day->setDate(QDate::currentDate());
  day->setMinimumSize(UiScale::dp(720), UiScale::dp(520));
  day->refresh();

  connect(day, &CalendarDayView::dateChanged, this, [this](const QDate &) {
    updateHeader();
  });

  m_calModal = BlopModal::present(this, day, BlopModal::Mode::Card,
                                  QStringLiteral("Kalender"),
                                  UiScale::dp(880));
  connect(m_calModal, &BlopModal::dismissed, this, [this]() {
    m_calModal = nullptr;
  });
}
