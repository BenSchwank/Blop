#include "dashboardpage.h"

#include "blop_inwindow_menu.h"
#include "blop_theme.h"
#include "blopstyle.h"
#include "calendardayview.h"
#include "calendarservice.h"
#include "dashcanvas.h"
#include "dashwidget.h"
#include "phonechrome.h"
#include "todostore.h"
#include "uiscale.h"

#include <QDate>
#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QScrollArea>
#include <QTime>
#include <QTimer>
#include <QVBoxLayout>

namespace {
QString pageBg() {
  // Slightly cooler desk so white cards read as elevated sheets.
  return BlopTheme::instance().isDark()
             ? QColor(0x16, 0x18, 0x1E).name(QColor::HexRgb)
             : QStringLiteral("#EEF0F4");
}
QString ink() {
  return BlopTheme::instance().isDark()
             ? QColor(255, 255, 255, 220).name(QColor::HexArgb)
             : BlopStyle::paperInk().name(QColor::HexRgb);
}
QString muted() {
  return BlopTheme::instance().isDark()
             ? QColor(255, 255, 255, 150).name(QColor::HexArgb)
             : BlopStyle::paperInkMuted().name(QColor::HexRgb);
}

QString greetingText() {
  const int h = QTime::currentTime().hour();
  if (h < 11)
    return QStringLiteral("Guten Morgen");
  if (h < 17)
    return QStringLiteral("Guten Tag");
  return QStringLiteral("Guten Abend");
}
} // namespace

DashboardPage::DashboardPage(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("DashboardPage"));
  setAttribute(Qt::WA_StyledBackground, true);

  m_root = new QVBoxLayout(this);
  m_root->setContentsMargins(0, 0, 0, 0);
  m_root->setSpacing(0);

  buildHeader();
  m_root->addWidget(m_header, 0);

  m_scroll = new QScrollArea(this);
  m_scroll->setObjectName(QStringLiteral("DashPageScroll"));
  m_scroll->setWidgetResizable(true);
  m_scroll->setFrameShape(QFrame::NoFrame);
  m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_scroll->setStyleSheet(QStringLiteral(
      "QScrollArea#DashPageScroll { background: transparent; border: none; }"));

  m_canvas = new DashCanvas(m_scroll);
  m_scroll->setWidget(m_canvas);
  m_root->addWidget(m_scroll, 1);

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
            DashboardLayoutStore::save(specs);
          });

  connect(&CalendarService::instance(), &CalendarService::eventsChanged, this,
          &DashboardPage::refresh, Qt::QueuedConnection);
  connect(&BlopTheme::instance(), &BlopTheme::themeChanged, this, [this]() {
    applyChrome();
    refresh();
  });

  m_clockTimer = new QTimer(this);
  connect(m_clockTimer, &QTimer::timeout, this, &DashboardPage::updateHeader);
  m_clockTimer->start(30000);

  applyChrome();
  applyDensity();
  m_canvas->setPhoneMode(usePhone());
  m_canvas->setSpecs(DashboardLayoutStore::load());
  m_canvas->syncWidgets();
  updateHeader();
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
                    "  border-bottom: 1px solid %2;"
                    "}")
                    .arg(pageBg(),
                         BlopTheme::instance().isDark()
                             ? QStringLiteral("rgba(255,255,255,0.06)")
                             : QStringLiteral("rgba(15,23,42,0.06)")));
}

void DashboardPage::applyDensity() {
  const bool phone = usePhone();
  const int side = UiScale::dp(phone ? 16 : 48);
  if (m_header) {
    if (auto *lay = qobject_cast<QBoxLayout *>(m_header->layout())) {
      lay->setContentsMargins(side, UiScale::dp(phone ? 20 : 32), side,
                              UiScale::dp(phone ? 8 : 12));
    }
  }
  if (m_canvas)
    m_canvas->setPhoneMode(phone);
}

void DashboardPage::buildHeader() {
  m_header = new QWidget(this);
  m_header->setObjectName(QStringLiteral("DashHeader"));
  auto *lay = new QHBoxLayout(m_header);
  lay->setSpacing(UiScale::dp(16));

  auto *textCol = new QWidget(m_header);
  auto *textLay = new QVBoxLayout(textCol);
  textLay->setContentsMargins(0, 0, 0, 0);
  textLay->setSpacing(UiScale::dp(4));

  m_hello = new QLabel(textCol);
  m_hello->setStyleSheet(QStringLiteral(
                             "color: %1; font-size: 30px; font-weight: 680;"
                             "letter-spacing: -0.7px; background: transparent;")
                             .arg(ink()));
  m_date = new QLabel(textCol);
  m_date->setStyleSheet(QStringLiteral(
                            "color: %1; font-size: 13px; font-weight: 400;"
                            "background: transparent;")
                            .arg(muted()));
  m_metrics = new QLabel(textCol);
  m_metrics->setStyleSheet(QStringLiteral(
                               "color: %1; font-size: 12px; font-weight: 550;"
                               "background: transparent;"
                               "padding: 4px 0;")
                               .arg(muted()));
  textLay->addWidget(m_hello);
  textLay->addWidget(m_date);
  textLay->addWidget(m_metrics);
  lay->addWidget(textCol, 1);

  auto *actions = new QWidget(m_header);
  auto *actLay = new QHBoxLayout(actions);
  actLay->setContentsMargins(0, 0, 0, 0);
  actLay->setSpacing(UiScale::dp(8));

  m_btnBlocks = new QPushButton(QStringLiteral("Blöcke"), actions);
  m_btnBlocks->setCursor(Qt::PointingHandCursor);
  m_btnBlocks->setMinimumHeight(UiScale::dp(BlopStyle::touchTargetMinDp()));
  m_btnBlocks->setStyleSheet(BlopStyle::paperSecondaryButtonQss());
  connect(m_btnBlocks, &QPushButton::clicked, this,
          &DashboardPage::showBlocksMenu);
  actLay->addWidget(m_btnBlocks);

  m_btnReset = new QPushButton(QStringLiteral("Zurücksetzen"), actions);
  m_btnReset->setCursor(Qt::PointingHandCursor);
  m_btnReset->setMinimumHeight(UiScale::dp(BlopStyle::touchTargetMinDp()));
  m_btnReset->setStyleSheet(BlopStyle::paperSecondaryButtonQss());
  m_btnReset->hide();
  connect(m_btnReset, &QPushButton::clicked, this, &DashboardPage::resetLayout);
  actLay->addWidget(m_btnReset);

  m_btnEdit = new QPushButton(QStringLiteral("Anpassen"), actions);
  m_btnEdit->setCursor(Qt::PointingHandCursor);
  m_btnEdit->setMinimumHeight(UiScale::dp(BlopStyle::touchTargetMinDp()));
  m_btnEdit->setStyleSheet(BlopStyle::paperPrimaryButtonQss());
  connect(m_btnEdit, &QPushButton::clicked, this,
          &DashboardPage::toggleEditMode);
  actLay->addWidget(m_btnEdit);

  lay->addWidget(actions, 0, Qt::AlignTop);
}

void DashboardPage::updateHeader() {
  if (m_hello)
    m_hello->setText(greetingText());
  if (m_date) {
    m_date->setText(QLocale(QLocale::German, QLocale::Germany)
                        .toString(QDate::currentDate(),
                                  QStringLiteral("dddd, d. MMMM yyyy")));
  }
  if (m_metrics) {
    const auto todos = TodoStore::load();
    int open = 0;
    for (const auto &t : todos) {
      if (!t.done)
        ++open;
    }
    const int events =
        CalendarService::instance().eventsForDay(QDate::currentDate()).size();
    m_metrics->setText(
        QStringLiteral("%1 Aufgaben offen · %2 Termine heute")
            .arg(open)
            .arg(events));
  }
  // Refresh ink colors after theme change.
  if (m_hello)
    m_hello->setStyleSheet(QStringLiteral(
                               "color: %1; font-size: 30px; font-weight: 680;"
                               "letter-spacing: -0.7px; background: transparent;")
                               .arg(ink()));
  if (m_date)
    m_date->setStyleSheet(QStringLiteral(
                              "color: %1; font-size: 13px; font-weight: 400;"
                              "background: transparent;")
                              .arg(muted()));
  if (m_metrics)
    m_metrics->setStyleSheet(QStringLiteral(
                                 "color: %1; font-size: 12px; font-weight: 550;"
                                 "background: transparent; padding: 4px 0;")
                                 .arg(muted()));
}

void DashboardPage::refresh() {
  applyDensity();
  updateHeader();
  if (m_canvas) {
    m_canvas->setPhoneMode(usePhone());
    m_canvas->refreshAll();
    m_canvas->applyPositions();
  }
}

void DashboardPage::setEditMode(bool on) {
  if (m_editMode == on)
    return;
  m_editMode = on;
  if (m_btnEdit) {
    m_btnEdit->setText(on ? QStringLiteral("Fertig")
                          : QStringLiteral("Anpassen"));
    m_btnEdit->setStyleSheet(on ? BlopStyle::paperSecondaryButtonQss()
                                : BlopStyle::paperPrimaryButtonQss());
  }
  if (m_btnReset)
    m_btnReset->setVisible(on);
  if (m_canvas)
    m_canvas->setEditMode(on);
  emit customizeToggled(on);
}

void DashboardPage::toggleEditMode() { setEditMode(!m_editMode); }

void DashboardPage::persistAndApply(const QVector<DashboardWidgetSpec> &specs) {
  DashboardLayoutStore::save(specs);
  if (m_canvas) {
    m_canvas->setSpecs(specs);
    m_canvas->syncWidgets();
  }
}

void DashboardPage::resetLayout() {
  DashboardLayoutStore::reset();
  persistAndApply(DashboardLayoutStore::defaults());
}

void DashboardPage::showBlocksMenu() {
  if (!m_btnBlocks || !m_canvas)
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
                       auto specs = m_canvas->specs();
                       for (auto &s : specs) {
                         if (s.id == id) {
                           s.visible = !visible;
                           break;
                         }
                       }
                       persistAndApply(specs);
                     }});
  }
  BlopInWindowMenu::show(
      this, m_btnBlocks->mapToGlobal(QPoint(0, m_btnBlocks->height())), items);
}

void DashboardPage::showCalendarMaximized() {
  if (m_calMaxDlg) {
    m_calMaxDlg->raise();
    m_calMaxDlg->activateWindow();
    return;
  }
  m_calMaxDlg = new QDialog(this);
  m_calMaxDlg->setWindowTitle(QStringLiteral("Kalender"));
  m_calMaxDlg->resize(UiScale::dp(920), UiScale::dp(640));
  auto *lay = new QVBoxLayout(m_calMaxDlg);
  auto *day = new CalendarDayView(m_calMaxDlg);
  day->setCompact(false);
  day->setDate(QDate::currentDate());
  day->refresh();
  lay->addWidget(day);
  connect(m_calMaxDlg, &QDialog::finished, this, [this]() {
    m_calMaxDlg = nullptr;
  });
  m_calMaxDlg->setAttribute(Qt::WA_DeleteOnClose, true);
  m_calMaxDlg->show();
}
