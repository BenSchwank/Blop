#include "dashwidget.h"

#include "blop_theme.h"
#include "blopstyle.h"
#include "calendardayview.h"
#include "calendarservice.h"
#include "dashboardlayoutstore.h"
#include "libraryorgstore.h"
#include "todostore.h"
#include "uiscale.h"

#include <QCheckBox>
#include <QDate>
#include <QEvent>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QBoxLayout>
#include <functional>
#include <QLabel>
#include <QLocale>
#include <QMouseEvent>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {
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
QString cardBg() {
  return BlopTheme::instance().isDark()
             ? QColor(0x27, 0x2A, 0x31).name(QColor::HexRgb)
             : QStringLiteral("#FFFFFF");
}
QString cardBorder() {
  return BlopTheme::instance().isDark()
             ? QColor(255, 255, 255, 22).name(QColor::HexArgb)
             : QColor(20, 24, 44, 18).name(QColor::HexArgb);
}
QString accent() { return QStringLiteral("#5B9DFF"); }

QLabel *makeMutedLine(const QString &text, QWidget *parent) {
  auto *l = new QLabel(text, parent);
  l->setWordWrap(true);
  l->setStyleSheet(QStringLiteral(
                       "color: %1; font-size: 13px; font-weight: 400;"
                       "background: transparent;")
                       .arg(muted()));
  return l;
}
} // namespace

DashWidget::DashWidget(const QString &id, QWidget *parent)
    : QFrame(parent), m_id(id) {
  setObjectName(QStringLiteral("DashWidget"));
  setAttribute(Qt::WA_StyledBackground, true);
  setAttribute(Qt::WA_Hover, true);
  setProperty("dashBlockId", id);

  m_root = new QVBoxLayout(this);
  m_root->setContentsMargins(UiScale::dp(18), UiScale::dp(16), UiScale::dp(18),
                             UiScale::dp(16));
  m_root->setSpacing(UiScale::dp(12));

  m_header = new QWidget(this);
  auto *hdrLay = new QHBoxLayout(m_header);
  hdrLay->setContentsMargins(0, 0, 0, 0);
  hdrLay->setSpacing(UiScale::dp(8));

  m_grip = new QLabel(QStringLiteral("⋮⋮"), m_header);
  m_grip->setObjectName(QStringLiteral("DashDragGrip"));
  m_grip->setCursor(Qt::SizeAllCursor);
  m_grip->setVisible(false);
  m_grip->installEventFilter(this);
  m_grip->setStyleSheet(QStringLiteral(
                            "color: %1; font-size: 14px; font-weight: 700;"
                            "background: transparent; padding: 2px 4px;")
                            .arg(muted()));
  hdrLay->addWidget(m_grip, 0);

  m_title = new QLabel(DashboardLayoutStore::displayName(id), m_header);
  m_title->setStyleSheet(QStringLiteral(
                             "color: %1; font-size: 12px; font-weight: 650;"
                             "letter-spacing: 0.6px; background: transparent;")
                             .arg(muted()));
  hdrLay->addWidget(m_title, 1);

  m_sizeRow = new QWidget(m_header);
  m_sizeRow->setVisible(false);
  m_sizeLay = new QHBoxLayout(m_sizeRow);
  m_sizeLay->setContentsMargins(0, 0, 0, 0);
  m_sizeLay->setSpacing(UiScale::dp(4));
  const struct {
    const char *label;
    DashSizeClass size;
  } chips[] = {{"S", DashSizeClass::S},
               {"M", DashSizeClass::M},
               {"¾", DashSizeClass::Q3},
               {"L", DashSizeClass::L},
               {"H", DashSizeClass::Tall},
               {"XL", DashSizeClass::XL}};
  for (const auto &c : chips) {
    auto *b = new QPushButton(QString::fromUtf8(c.label), m_sizeRow);
    b->setCheckable(true);
    b->setCursor(Qt::PointingHandCursor);
    b->setFixedHeight(UiScale::dp(24));
    b->setMinimumWidth(UiScale::dp(28));
    b->setProperty("dashSize", static_cast<int>(c.size));
    b->setStyleSheet(BlopStyle::segmentQss());
    connect(b, &QPushButton::clicked, this, [this, size = c.size]() {
      emit sizeClassPicked(size);
    });
    m_sizeLay->addWidget(b, 0);
  }
  hdrLay->addWidget(m_sizeRow, 0);

  m_root->addWidget(m_header, 0);

  m_bodyHost = new QWidget(this);
  m_bodyHost->setObjectName(QStringLiteral("DashWidgetBody"));
  m_bodyLay = new QVBoxLayout(m_bodyHost);
  m_bodyLay->setContentsMargins(0, 0, 0, 0);
  m_bodyLay->setSpacing(0);
  m_root->addWidget(m_bodyHost, 1);

  m_resizeHandle = new QWidget(this);
  m_resizeHandle->setObjectName(QStringLiteral("DashResizeHandle"));
  m_resizeHandle->setCursor(Qt::SizeFDiagCursor);
  m_resizeHandle->setVisible(false);
  m_resizeHandle->installEventFilter(this);
  m_resizeHandle->setStyleSheet(QStringLiteral(
      "QWidget#DashResizeHandle {"
      "  background: %1;"
      "  border: 1px solid %2;"
      "  border-radius: %3px;"
      "}"
      "QWidget#DashResizeHandle:hover { background: %4; }")
                                    .arg(accent(),
                                         QColor(255, 255, 255, 200).name(QColor::HexArgb),
                                         QString::number(UiScale::dp(4)),
                                         QColor(91, 157, 255).lighter(110).name(
                                             QColor::HexRgb)));

  applyChrome();
  syncSizeChips();
  rebuildBody();
}

DashWidget *DashWidget::create(const QString &id, QWidget *parent) {
  return new DashWidget(id, parent);
}

void DashWidget::applyChrome() {
  const int r = UiScale::dp(14);
  // Soft lift without QGraphicsDropShadowEffect (that paints black slabs on Win).
  const QString border =
      m_lifted ? QColor(91, 157, 255, 170).name(QColor::HexArgb)
               : (m_editMode ? QColor(91, 157, 255, 70).name(QColor::HexArgb)
                             : cardBorder());
  const QString bg =
      m_lifted
          ? (BlopTheme::instance().isDark() ? QStringLiteral("#2C3038")
                                            : QStringLiteral("#FFFFFF"))
          : cardBg();
  setStyleSheet(QStringLiteral(
                    "QFrame#DashWidget {"
                    "  background: %1;"
                    "  border: 1px solid %2;"
                    "  border-radius: %3px;"
                    "}"
                    "QFrame#DashWidget:hover {"
                    "  border-color: %4;"
                    "}")
                    .arg(bg, border, QString::number(r),
                         m_editMode ? accent()
                                    : QColor(20, 24, 44, 32).name(QColor::HexArgb)));
  setGraphicsEffect(nullptr);
}

void DashWidget::setEditMode(bool on) {
  if (m_editMode == on)
    return;
  m_editMode = on;
  if (m_grip)
    m_grip->setVisible(on);
  if (m_sizeRow)
    m_sizeRow->setVisible(on);
  if (m_resizeHandle) {
    m_resizeHandle->setVisible(on);
    if (on)
      layoutResizeHandle();
  }
  applyChrome();
}

void DashWidget::setLifted(bool lifted) {
  if (m_lifted == lifted)
    return;
  m_lifted = lifted;
  applyChrome();
}

void DashWidget::setSizeClass(DashSizeClass sizeClass) {
  if (m_sizeClass == sizeClass) {
    syncSizeChips();
    return;
  }
  const bool wasNarrow = DashboardLayoutStore::colSpanFor(m_sizeClass) <= 3;
  m_sizeClass = sizeClass;
  syncSizeChips();
  const bool isNarrow = DashboardLayoutStore::colSpanFor(m_sizeClass) <= 3;
  if (wasNarrow != isNarrow)
    rebuildBody();
}

void DashWidget::syncSizeChips() {
  if (!m_sizeRow)
    return;
  const int cs = DashboardLayoutStore::colSpanFor(m_sizeClass);
  const int rs = DashboardLayoutStore::rowSpanFor(m_sizeClass);
  for (auto *b : m_sizeRow->findChildren<QPushButton *>()) {
    const auto sc =
        static_cast<DashSizeClass>(b->property("dashSize").toInt());
    // Highlight nearest preset chip for the current footprint.
    bool on = false;
    if (sc == DashSizeClass::S)
      on = (cs == 3 && rs <= 2);
    else if (sc == DashSizeClass::M)
      on = (cs == 6 && rs <= 2);
    else if (sc == DashSizeClass::Q3)
      on = (cs == 9 && rs <= 2);
    else if (sc == DashSizeClass::L)
      on = (cs == 12 && rs <= 2);
    else if (sc == DashSizeClass::Tall)
      on = (cs == 6 && rs >= 3);
    else if (sc == DashSizeClass::XL)
      on = (cs == 12 && rs >= 3);
    b->setChecked(on);
  }
}

void DashWidget::layoutResizeHandle() {
  if (!m_resizeHandle)
    return;
  const int grip = UiScale::dp(16);
  const int inset = UiScale::dp(6);
  m_resizeHandle->setGeometry(width() - grip - inset, height() - grip - inset,
                              grip, grip);
  m_resizeHandle->raise();
}

void DashWidget::setBody(QWidget *body) {
  while (QLayoutItem *it = m_bodyLay->takeAt(0)) {
    if (it->widget())
      delete it->widget();
    delete it;
  }
  if (body) {
    body->setParent(m_bodyHost);
    m_bodyLay->addWidget(body, 1);
  }
}

void DashWidget::refreshContent() { rebuildBody(); }

void DashWidget::rebuildBody() {
  QWidget *body = nullptr;
  if (m_id == QLatin1String("today"))
    body = buildToday();
  else if (m_id == QLatin1String("todos"))
    body = buildTodos();
  else if (m_id == QLatin1String("calendar"))
    body = buildCalendar();
  else if (m_id == QLatin1String("recent"))
    body = buildRecent();
  else if (m_id == QLatin1String("shortcuts"))
    body = buildShortcuts();
  setBody(body);
}

bool DashWidget::eventFilter(QObject *watched, QEvent *event) {
  if (m_editMode && event->type() == QEvent::MouseButtonPress) {
    const auto *me = static_cast<QMouseEvent *>(event);
    if (me->button() == Qt::LeftButton) {
      if (watched == m_grip) {
        emit dragHandlePressed(me->globalPosition().toPoint());
        return true;
      }
      if (watched == m_resizeHandle) {
        emit resizeHandlePressed(me->globalPosition().toPoint());
        return true;
      }
    }
  }
  return QFrame::eventFilter(watched, event);
}

void DashWidget::resizeEvent(QResizeEvent *event) {
  QFrame::resizeEvent(event);
  if (m_editMode)
    layoutResizeHandle();
}

QWidget *DashWidget::buildToday() {
  auto *body = new QWidget();
  auto *lay = new QVBoxLayout(body);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(UiScale::dp(8));

  const QDate today = QDate::currentDate();
  const auto todos = TodoStore::load();
  int open = 0;
  int overdue = 0;
  int dueToday = 0;
  for (const auto &t : todos) {
    if (t.done)
      continue;
    ++open;
    if (t.due.isValid()) {
      if (t.due.date() < today)
        ++overdue;
      else if (t.due.date() == today)
        ++dueToday;
    }
  }
  const auto events = CalendarService::instance().eventsForDay(today);

  auto *stats = new QLabel(body);
  stats->setText(QStringLiteral("%1 offen · %2 heute · %3 Termine")
                     .arg(open)
                     .arg(dueToday)
                     .arg(events.size()));
  stats->setStyleSheet(QStringLiteral(
                           "color: %1; font-size: 22px; font-weight: 600;"
                           "letter-spacing: -0.4px; background: transparent;")
                           .arg(ink()));
  stats->setWordWrap(true);
  lay->addWidget(stats);

  if (overdue > 0) {
    auto *warn = new QLabel(
        QStringLiteral("%1 überfällig").arg(overdue), body);
    warn->setStyleSheet(QStringLiteral(
                            "color: #E06C75; font-size: 12px; font-weight: 500;"
                            "background: transparent;")
                            );
    lay->addWidget(warn);
  }

  int shown = 0;
  for (const auto &t : todos) {
    if (t.done || shown >= 4)
      continue;
    if (t.due.isValid() && t.due.date() > today)
      continue;
    auto *row = new QWidget(body);
    auto *rowLay = new QHBoxLayout(row);
    rowLay->setContentsMargins(0, 0, 0, 0);
    rowLay->setSpacing(UiScale::dp(8));
    auto *cb = new QCheckBox(row);
    cb->setChecked(false);
    const QString tid = t.id;
    connect(cb, &QCheckBox::toggled, this, [this, tid](bool on) {
      if (on) {
        TodoStore::setDone(tid, true);
        emit contentChanged();
        refreshContent();
      }
    });
    rowLay->addWidget(cb, 0);
    auto *lbl = new QLabel(t.title, row);
    lbl->setStyleSheet(QStringLiteral(
                           "color: %1; font-size: 13px; background: transparent;")
                           .arg(ink()));
    lbl->setWordWrap(true);
    rowLay->addWidget(lbl, 1);
    lay->addWidget(row);
    ++shown;
  }
  if (shown == 0)
    lay->addWidget(makeMutedLine(QStringLiteral("Nichts Dringendes — guter Tag."),
                                 body));
  lay->addStretch(1);
  return body;
}

QWidget *DashWidget::buildTodos() {
  auto *body = new QWidget();
  auto *lay = new QVBoxLayout(body);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(UiScale::dp(6));

  auto *scroll = new QScrollArea(body);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scroll->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
  auto *list = new QWidget();
  auto *listLay = new QVBoxLayout(list);
  listLay->setContentsMargins(0, 0, 0, 0);
  listLay->setSpacing(UiScale::dp(4));

  const auto todos = TodoStore::load();
  int shown = 0;
  for (const auto &t : todos) {
    if (t.done || shown >= 8)
      continue;
    auto *row = new QWidget(list);
    auto *rowLay = new QHBoxLayout(row);
    rowLay->setContentsMargins(0, UiScale::dp(2), 0, UiScale::dp(2));
    rowLay->setSpacing(UiScale::dp(8));
    auto *cb = new QCheckBox(row);
    const QString tid = t.id;
    connect(cb, &QCheckBox::toggled, this, [this, tid](bool on) {
      TodoStore::setDone(tid, on);
      emit contentChanged();
      refreshContent();
    });
    rowLay->addWidget(cb, 0);
    auto *lbl = new QLabel(t.title, row);
    lbl->setStyleSheet(QStringLiteral(
                           "color: %1; font-size: 13px; background: transparent;")
                           .arg(ink()));
    lbl->setWordWrap(true);
    rowLay->addWidget(lbl, 1);
    listLay->addWidget(row);
    ++shown;
  }
  if (shown == 0)
    listLay->addWidget(
        makeMutedLine(QStringLiteral("Keine offenen Aufgaben."), list));
  listLay->addStretch(1);
  scroll->setWidget(list);
  lay->addWidget(scroll, 1);

  auto *add = new QPushButton(QStringLiteral("+ Aufgabe"), body);
  add->setCursor(Qt::PointingHandCursor);
  add->setMinimumHeight(UiScale::dp(BlopStyle::touchTargetMinDp() - 4));
  add->setStyleSheet(BlopStyle::paperSecondaryButtonQss());
  connect(add, &QPushButton::clicked, this, [this]() {
    TodoStore::add(QStringLiteral("Neue Aufgabe"));
    emit contentChanged();
    refreshContent();
  });
  lay->addWidget(add, 0);
  return body;
}

QWidget *DashWidget::buildCalendar() {
  auto *body = new QWidget();
  auto *lay = new QVBoxLayout(body);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(UiScale::dp(6));

  const int cs = DashboardLayoutStore::colSpanFor(m_sizeClass);
  const int rs = DashboardLayoutStore::rowSpanFor(m_sizeClass);
  const bool tiny = cs <= 3;

  if (!tiny) {
    auto *maxBtn = new QPushButton(QStringLiteral("Maximieren"), body);
    maxBtn->setCursor(Qt::PointingHandCursor);
    maxBtn->setFlat(true);
    maxBtn->setStyleSheet(QStringLiteral(
                              "QPushButton {"
                              "  color: %1; font-size: 12px; font-weight: 600;"
                              "  background: transparent; border: none;"
                              "  text-align: left; padding: 0;"
                              "}"
                              "QPushButton:hover { color: %2; }")
                              .arg(accent(),
                                   QColor(91, 157, 255).darker(110).name(
                                       QColor::HexRgb)));
    connect(maxBtn, &QPushButton::clicked, this,
            &DashWidget::maximizeCalendarRequested);
    lay->addWidget(maxBtn, 0, Qt::AlignLeft);
  }

  auto *day = new CalendarDayView(body);
  day->setCompact(true);
  day->setMinimal(tiny || rs <= 2);
  day->setDate(QDate::currentDate());
  day->refresh();
  lay->addWidget(day, 1);
  return body;
}

QWidget *DashWidget::buildRecent() {
  auto *body = new QWidget();
  auto *lay = new QVBoxLayout(body);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(UiScale::dp(4));

  const QStringList paths = LibraryOrgStore::recentPaths(6);
  if (paths.isEmpty()) {
    lay->addWidget(
        makeMutedLine(QStringLiteral("Noch keine kürzlich geöffneten Notizen."),
                      body));
    lay->addStretch(1);
    return body;
  }
  for (const QString &path : paths) {
    const QFileInfo fi(path);
    auto *btn = new QPushButton(fi.completeBaseName(), body);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFlat(true);
    btn->setStyleSheet(QStringLiteral(
                           "QPushButton {"
                           "  text-align: left; padding: 8px 6px;"
                           "  color: %1; font-size: 13px; font-weight: 500;"
                           "  background: transparent; border: none;"
                           "  border-radius: 6px;"
                           "}"
                           "QPushButton:hover { background: %2; }")
                           .arg(ink(), BlopStyle::paperHover().name(QColor::HexRgb)));
    connect(btn, &QPushButton::clicked, this, [this, path]() {
      emit openNotePath(path);
    });
    lay->addWidget(btn, 0);
  }
  lay->addStretch(1);
  return body;
}

QWidget *DashWidget::buildShortcuts() {
  auto *body = new QWidget();
  const bool narrow = DashboardLayoutStore::colSpanFor(m_sizeClass) <= 3;
  QBoxLayout *lay = nullptr;
  if (narrow)
    lay = new QVBoxLayout(body);
  else
    lay = new QHBoxLayout(body);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(UiScale::dp(narrow ? 6 : 10));

  struct Item {
    const char *label;
    std::function<void()> fire;
  };
  const QList<Item> items = {
      {"Neue Notiz", [this]() { emit newNoteRequested(); }},
      {"Bibliothek", [this]() { emit snapToNotesRequested(); }},
      {"Study", [this]() { emit studyRequested(); }},
  };

  for (const auto &it : items) {
    auto *card = new QPushButton(QString::fromUtf8(it.label), body);
    card->setCursor(Qt::PointingHandCursor);
    card->setMinimumHeight(UiScale::dp(narrow ? 36 : 72));
    card->setStyleSheet(QStringLiteral(
                            "QPushButton {"
                            "  background: %1;"
                            "  border: 1px solid %2;"
                            "  border-radius: 12px;"
                            "  color: %3; font-size: %6px; font-weight: 600;"
                            "  padding: %7px;"
                            "}"
                            "QPushButton:hover {"
                            "  background: %5;"
                            "  border-color: %4;"
                            "}")
                            .arg(BlopTheme::instance().isDark()
                                     ? QStringLiteral("#2C3038")
                                     : QStringLiteral("#F7F9FC"),
                                 cardBorder(), ink(), accent(),
                                 BlopTheme::instance().isDark()
                                     ? QStringLiteral("#32363F")
                                     : BlopStyle::paperPrimaryLight().name(
                                           QColor::HexRgb),
                                 QString::number(narrow ? 12 : 13),
                                 QString::number(narrow ? 8 : 14)));
    connect(card, &QPushButton::clicked, this, it.fire);
    lay->addWidget(card, 1);
  }
  return body;
}
