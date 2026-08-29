#include "dashboardpage.h"

#include "blop_scroll.h"
#include "blop_inwindow_menu.h"
#include "blop_modal.h"
#include "blop_theme.h"
#include "blopstyle.h"
#include "calendardayview.h"
#include "calendarservice.h"
#include "dashboardlayoutstore.h"
#include "libraryorgstore.h"
#include "todostore.h"
#include "uiscale.h"

#include <QCheckBox>
#include <QCursor>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSizePolicy>
#include <QPainter>
#include <QResizeEvent>
#include <QDate>
#include <QLocale>
#include <QTime>
#include <QTimer>
#include <QtMath>
#include <QApplication>
#include <QMetaObject>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QMap>
#include <QTouchEvent>
#include <climits>
#include <optional>

namespace {
/// Complete Notion-style dashboard palette. Light and Dark are separate
/// tables — never mix tokens across modes.
struct DashTheme {
  QColor pageBg;
  QColor cardBg;
  QColor ink;
  QColor muted;
  QColor border;
  QColor hover;
  QColor divider;
  QColor inputBg;
  QColor borderHover;
  QColor scrollHandle;
};

DashTheme dashLight() {
  return {
      QColor(0xF9, 0xF9, 0xF9), // pageBg
      QColor(0xFF, 0xFF, 0xFF), // cardBg
      QColor(0x37, 0x35, 0x2F), // ink  #37352F
      QColor(0x50, 0x50, 0x50), // muted
      QColor(0xE5, 0xE5, 0xE5), // border
      QColor(0xF1, 0xF1, 0xEF), // hover
      QColor(55, 53, 47, 16),   // divider
      QColor(0xFF, 0xFF, 0xFF), // inputBg
      QColor(0xD3, 0xD3, 0xD3), // borderHover
      QColor(55, 53, 47, 40),   // scrollHandle
  };
}

DashTheme dashDark() {
  return {
      QColor(0x19, 0x19, 0x19),       // pageBg
      QColor(0x25, 0x25, 0x25),       // cardBg
      QColor(255, 255, 255, 207),     // ink ~0.81
      QColor(255, 255, 255, 153),     // muted ~0.60
      QColor(0x33, 0x33, 0x33),       // border
      QColor(255, 255, 255, 13),      // hover ~0.05
      QColor(255, 255, 255, 16),      // divider
      QColor(0x1F, 0x1F, 0x1F),       // inputBg
      QColor(0x40, 0x40, 0x40),       // borderHover
      QColor(255, 255, 255, 46),      // scrollHandle
  };
}

const DashTheme &dash() {
  static DashTheme light = dashLight();
  static DashTheme dark = dashDark();
  return BlopTheme::instance().isDark() ? dark : light;
}

QString hex(const QColor &c) {
  if (c.alpha() >= 250)
    return c.name(QColor::HexRgb);
  return QStringLiteral("rgba(%1,%2,%3,%4)")
      .arg(c.red())
      .arg(c.green())
      .arg(c.blue())
      .arg(QString::number(c.alphaF(), 'f', 3));
}

QString ink() { return hex(dash().ink); }
QString muted() { return hex(dash().muted); }
QString accent() { return BlopTheme::accentPrimary().name(QColor::HexRgb); }
QString paper() { return hex(dash().pageBg); }
QString card() { return hex(dash().cardBg); }
QString border() { return hex(dash().border); }
QString hover() { return hex(dash().hover); }

QString sectionTitleQss() {
  return QStringLiteral(
             "color: %1; font-size: 15px; font-weight: 600;"
             "letter-spacing: -0.2px; background: transparent;")
      .arg(ink());
}

QString bodyTextQss(bool strike = false) {
  return QStringLiteral(
             "color: %1; font-size: 13px; font-weight: 400;"
             "background: transparent;%2")
      .arg(strike ? muted() : ink(),
           strike ? QStringLiteral(" text-decoration: line-through;")
                  : QString());
}

QString compactPrimaryQss() {
  const QColor acc = BlopTheme::accentPrimary();
  return QStringLiteral(
             "QPushButton {"
             "  background: %1; color: #FFFFFF; border: none;"
             "  border-radius: 6px; padding: 5px 12px;"
             "  font-size: 12px; font-weight: 600; min-height: 28px;"
             "}"
             "QPushButton:hover { background: %2; }"
             "QPushButton:pressed { background: %3; }")
      .arg(acc.name(QColor::HexRgb), acc.lighter(108).name(QColor::HexRgb),
           acc.darker(108).name(QColor::HexRgb));
}

QString compactGhostQss() {
  return QStringLiteral(
             "QPushButton {"
             "  background: transparent; color: %1; border: none;"
             "  border-radius: 6px; padding: 5px 10px;"
             "  font-size: 12px; font-weight: 500; min-height: 28px;"
             "}"
             "QPushButton:hover { color: %2; background: %3; }")
      .arg(muted(), ink(), hover());
}

QString softBlockQss(bool editing, const QString &blockId = QString()) {
  Q_UNUSED(blockId);
  const DashTheme &t = dash();
  // Quiet Notion-style cards: soft fill + hairline, never rainbow.
  if (editing) {
    return QStringLiteral(
               "QFrame#DashBlock {"
               "  background: %1;"
               "  border: 1px dashed %2;"
               "  border-radius: 10px;"
               "}"
               "QFrame#DashBlock:hover { border-color: %3; }")
        .arg(hex(t.cardBg), hex(t.border), hex(BlopTheme::accentBorder()));
  }
  return QStringLiteral(
             "QFrame#DashBlock {"
             "  background: %1;"
             "  border: 1px solid %2;"
             "  border-radius: 10px;"
             "}"
             "QFrame#DashBlock:hover { border-color: %3; }"
             "QFrame#DashBlock QLabel#DashDragGrip { color: transparent; }"
             "QFrame#DashBlock:hover QLabel#DashDragGrip { color: %4; }")
      .arg(hex(t.cardBg), hex(t.border), hex(t.borderHover), hex(t.muted));
}

QString rowQss() {
  const DashTheme &t = dash();
  return QStringLiteral(
             "QFrame#DashRow {"
             "  background: transparent; border: none;"
             "  border-bottom: 1px solid %1;"
             "}"
             "QFrame#DashRow:hover { background: %2; border-radius: 4px; }")
      .arg(hex(t.divider), hex(t.hover));
}

QString checkBoxQss() {
  const DashTheme &t = dash();
  const QString acc = accent();
  return QStringLiteral(
             "QCheckBox { spacing: 8px; color: %1; background: transparent; }"
             "QCheckBox::indicator {"
             "  width: 14px; height: 14px;"
             "  border: 1px solid %2;"
             "  border-radius: 3px;"
             "  background: %3;"
             "}"
             "QCheckBox::indicator:hover { border-color: %4; }"
             "QCheckBox::indicator:checked {"
             "  background: %4; border-color: %4;"
             "}")
      .arg(hex(t.ink), hex(t.border), hex(t.cardBg), acc);
}

QString badgeQss() {
  const QColor acc = BlopTheme::accentPrimary();
  QColor bg = acc;
  bg.setAlpha(38);
  return QStringLiteral(
             "QLabel#DashBadge {"
             "  background: %1; color: %2;"
             "  border: none; border-radius: 4px;"
             "  padding: 1px 7px; font-size: 11px; font-weight: 500;"
             "}")
      .arg(hex(bg), acc.name(QColor::HexRgb));
}

QString dashInputQss() {
  const DashTheme &t = dash();
  return QStringLiteral(
             "QLineEdit {"
             "  background: %1; color: %2;"
             "  border: 1px solid %3; border-radius: 6px;"
             "  padding: 6px 10px; font-size: 13px;"
             "  selection-background-color: %4;"
             "}"
             "QLineEdit:focus { border-color: %4; }"
             "QLineEdit::placeholder { color: %5; }")
      .arg(hex(t.inputBg), hex(t.ink), hex(t.border), accent(), hex(t.muted));
}

QString statMetricQss() {
  return QStringLiteral(
             "color: %1; font-size: 12px; font-weight: 400;"
             "background: transparent;")
      .arg(muted());
}

QString quietBtnQss() {
  return QStringLiteral(
             "QPushButton {"
             "  background: transparent; color: %1; border: none;"
             "  font-weight: 500; font-size: 12px; padding: 4px 8px;"
             "  border-radius: 4px;"
             "}"
             "QPushButton:hover { color: %2; background: %3; }")
      .arg(muted(), ink(), hover());
}

QString editChipQss() {
  QColor tint = BlopTheme::accentPrimary();
  tint.setAlpha(28);
  return QStringLiteral(
             "QPushButton {"
             "  background: %1; color: %2; border: 1px solid %3;"
             "  border-radius: 4px; padding: 3px 8px; font-size: 11px;"
             "  font-weight: 500;"
             "}"
             "QPushButton:hover { background: %4; color: %5; }")
      .arg(hover(), muted(), border(), hex(tint), accent());
}

QString gripQss(bool alwaysVisible) {
  if (alwaysVisible) {
    return QStringLiteral(
               "color: %1; font-size: 13px; padding: 2px 4px;"
               "background: transparent; letter-spacing: -1px;")
        .arg(muted());
  }
  return QStringLiteral(
      "color: transparent; font-size: 13px; padding: 2px 4px;"
      "background: transparent; letter-spacing: -1px;");
}

QString scrollBarQss() {
  return QStringLiteral(
             "QScrollArea#DashBlockScroll { background: transparent; border: none; }"
             "QScrollBar:vertical { width: 6px; background: transparent; margin: 2px; }"
             "QScrollBar::handle:vertical {"
             "  background: %1; border-radius: 3px; min-height: 20px;"
             "}"
             "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
             "  height: 0; border: none; }"
             "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
             "  background: transparent; }")
      .arg(hex(dash().scrollHandle));
}

QString shortcutCardQss() {
  const DashTheme &t = dash();
  return QStringLiteral(
             "QPushButton {"
             "  background: %1; border: 1px solid %2;"
             "  border-radius: 8px; text-align: left; padding: 10px 12px;"
             "}"
             "QPushButton:hover {"
             "  border-color: %3; background: %4;"
             "}")
      .arg(hex(t.cardBg), hex(t.border), hex(t.borderHover), hex(t.hover));
}

QString greetingHour() {
  const int h = QTime::currentTime().hour();
  if (h < 11)
    return QStringLiteral("Guten Morgen");
  if (h < 17)
    return QStringLiteral("Guten Tag");
  return QStringLiteral("Guten Abend");
}

QString formatToday() {
  return QLocale(QLocale::German, QLocale::Germany)
      .toString(QDate::currentDate(), QStringLiteral("dddd, d. MMMM yyyy"));
}

QString formatClock() {
  return QTime::currentTime().toString(QStringLiteral("HH:mm"));
}

class DashAnalogClock : public QWidget {
public:
  explicit DashAnalogClock(QWidget *parent = nullptr) : QWidget(parent) {
    setObjectName(QStringLiteral("DashAnalogClock"));
    setMinimumSize(UiScale::dp(120), UiScale::dp(120));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const int side = qMin(width(), height()) - UiScale::dp(8);
    const QRectF face((width() - side) / 2.0, (height() - side) / 2.0, side,
                      side);
    const QPointF c = face.center();
    const qreal r = side / 2.0;

    const bool dark = BlopTheme::instance().isDark();
    p.setPen(Qt::NoPen);
    p.setBrush(dark ? QColor(255, 255, 255, 18) : QColor(91, 157, 255, 28));
    p.drawEllipse(face);
    p.setPen(QPen(dark ? QColor(255, 255, 255, 40) : QColor(91, 157, 255, 90),
                  1.5));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(face.adjusted(1, 1, -1, -1));

    p.setPen(QPen(BlopTheme::accentPrimary(), 2.0, Qt::SolidLine, Qt::RoundCap));
    for (int i = 0; i < 12; ++i) {
      const qreal a = qDegreesToRadians(i * 30.0 - 90.0);
      const QPointF outer(c.x() + qCos(a) * (r - 4), c.y() + qSin(a) * (r - 4));
      const QPointF inner(c.x() + qCos(a) * (r - 12),
                          c.y() + qSin(a) * (r - 12));
      p.drawLine(inner, outer);
    }

    const QTime now = QTime::currentTime();
    auto hand = [&](qreal angleDeg, qreal len, qreal width, const QColor &col) {
      const qreal a = qDegreesToRadians(angleDeg - 90.0);
      p.setPen(QPen(col, width, Qt::SolidLine, Qt::RoundCap));
      p.drawLine(c, QPointF(c.x() + qCos(a) * len, c.y() + qSin(a) * len));
    };
    const QColor inkCol =
        dark ? QColor(255, 255, 255, 220) : QColor(0x37, 0x35, 0x2F);
    hand((now.hour() % 12 + now.minute() / 60.0) * 30.0, r * 0.50, 3.2, inkCol);
    hand((now.minute() + now.second() / 60.0) * 6.0, r * 0.68, 2.4, inkCol);
    hand(now.second() * 6.0, r * 0.78, 1.4, BlopTheme::accentPrimary());
    p.setPen(Qt::NoPen);
    p.setBrush(BlopTheme::accentPrimary());
    p.drawEllipse(c, 3.5, 3.5);
  }
};

bool clockAnalogPref() {
  return QSettings(QStringLiteral("Blop"), QStringLiteral("BlopApp"))
      .value(QStringLiteral("dashboard/clock_analog"), false)
      .toBool();
}

void setClockAnalogPref(bool on) {
  QSettings(QStringLiteral("Blop"), QStringLiteral("BlopApp"))
      .setValue(QStringLiteral("dashboard/clock_analog"), on);
}

QString dashboardUserName() {
  QSettings st(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  QString name = st.value(QStringLiteral("username")).toString().trimmed();
  if (name.isEmpty() ||
      name.compare(QLatin1String("Gast"), Qt::CaseInsensitive) == 0)
    return QStringLiteral("du");
  return name;
}

bool isGridBlock(const QString &id) {
  return id != QLatin1String("greeting") && id != QLatin1String("actions");
}

std::optional<QPoint> pointerPosInWidget(const QWidget *w, const QEvent *event) {
  switch (event->type()) {
  case QEvent::MouseButtonPress:
  case QEvent::MouseButtonRelease:
  case QEvent::MouseMove: {
    const auto *me = static_cast<const QMouseEvent *>(event);
    return me->position().toPoint();
  }
  case QEvent::TouchBegin:
  case QEvent::TouchUpdate:
  case QEvent::TouchEnd: {
    const auto *te = static_cast<const QTouchEvent *>(event);
    if (te->points().isEmpty())
      return std::nullopt;
    return w->mapFromGlobal(
        te->points().constFirst().globalPosition().toPoint());
  }
  default:
    return std::nullopt;
  }
}

bool isPointerPress(const QEvent *event) {
  return event->type() == QEvent::MouseButtonPress ||
         event->type() == QEvent::TouchBegin;
}

bool isPointerMove(const QEvent *event) {
  return event->type() == QEvent::MouseMove ||
         event->type() == QEvent::TouchUpdate;
}

bool isPointerRelease(const QEvent *event) {
  return event->type() == QEvent::MouseButtonRelease ||
         event->type() == QEvent::TouchEnd ||
         event->type() == QEvent::TouchCancel;
}

QPoint globalPosFromEvent(const QEvent *event) {
  switch (event->type()) {
  case QEvent::MouseMove:
  case QEvent::MouseButtonPress:
  case QEvent::MouseButtonRelease: {
    const auto *me = static_cast<const QMouseEvent *>(event);
    return me->globalPosition().toPoint();
  }
  case QEvent::TouchBegin:
  case QEvent::TouchUpdate:
  case QEvent::TouchEnd:
  case QEvent::TouchCancel: {
    const auto *te = static_cast<const QTouchEvent *>(event);
    if (te->points().isEmpty())
      return {};
    return te->points().constFirst().globalPosition().toPoint();
  }
  default:
    return {};
  }
}

QVector<DashboardWidgetSpec> currentSpecs() {
  return DashboardLayoutStore::load();
}
} // namespace

class DashSnapOverlay : public QWidget {
public:
  explicit DashSnapOverlay(QWidget *host) : QWidget(host) {
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    hide();
  }
  void setGridLines(const QVector<int> &colX, const QVector<int> &rowY) {
    m_colX = colX;
    m_rowY = rowY;
    update();
  }
  void setHighlight(const QRect &r) {
    m_highlight = r;
    show();
    raise();
    update();
  }
  void clearHighlight() { hide(); }

protected:
  void resizeEvent(QResizeEvent *) override {
    if (parentWidget())
      setGeometry(parentWidget()->rect());
  }
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(QColor(91, 157, 255, 35), 1));
    for (int x : m_colX)
      p.drawLine(x, m_rowY.isEmpty() ? 0 : m_rowY.first(),
                 x, m_rowY.isEmpty() ? height() : m_rowY.last());
    for (int y : m_rowY)
      p.drawLine(m_colX.isEmpty() ? 0 : m_colX.first(), y,
                 m_colX.isEmpty() ? width() : m_colX.last(), y);
    if (!m_highlight.isValid())
      return;
    p.setPen(QPen(QColor(91, 157, 255, 120), 2));
    p.setBrush(QColor(91, 157, 255, 28));
    p.drawRoundedRect(m_highlight, UiScale::dp(14), UiScale::dp(14));
  }

private:
  QRect m_highlight;
  QVector<int> m_colX;
  QVector<int> m_rowY;
};

DashboardPage::DashboardPage(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("DashboardPage"));
  setAttribute(Qt::WA_StyledBackground, true);

  m_rootLay = new QVBoxLayout(this);
  m_rootLay->setContentsMargins(0, 0, 0, 0);
  m_rootLay->setSpacing(0);

  m_editBar = new QWidget(this);
  m_editBar->setObjectName(QStringLiteral("DashEditBar"));
  m_editBar->hide();
  m_editBarLay = new QHBoxLayout(m_editBar);
  m_editBarLay->setContentsMargins(UiScale::dp(28), UiScale::dp(8),
                                   UiScale::dp(28), UiScale::dp(8));
  m_rootLay->addWidget(m_editBar, 0);

  m_persistentHeader = new QWidget(this);
  m_persistentHeader->setObjectName(QStringLiteral("DashPersistentHeader"));
  m_rootLay->addWidget(m_persistentHeader, 0);

  m_clockTimer = new QTimer(this);
  connect(m_clockTimer, &QTimer::timeout, this, [this]() {
    if (m_lblClock)
      m_lblClock->setText(formatClock());
    // Tick analog faces + digital labels inside clock blocks.
    const auto clocks =
        findChildren<QWidget *>(QStringLiteral("DashAnalogClock"));
    for (QWidget *c : clocks)
      c->update();
    const auto digis =
        findChildren<QLabel *>(QStringLiteral("DashDigitalClock"));
    for (QLabel *l : digis)
      l->setText(formatClock());
  });
  m_clockTimer->start(1000);

  auto *scroll = new QScrollArea(this);
  m_scroll = scroll;
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scroll->viewport()->installEventFilter(this);

  m_host = new QWidget(scroll);
  m_host->setObjectName(QStringLiteral("DashHost"));
  m_gridLay = new QGridLayout(m_host);
  m_gridLay->setContentsMargins(UiScale::dp(48), UiScale::dp(12), UiScale::dp(48),
                                UiScale::dp(48));
  m_gridLay->setHorizontalSpacing(UiScale::dp(28));
  m_gridLay->setVerticalSpacing(UiScale::dp(12));
  for (int c = 0; c < 12; ++c)
    m_gridLay->setColumnStretch(c, 1);
  scroll->setWidget(m_host);
  m_rootLay->addWidget(scroll, 1);
  scroll->installEventFilter(this);

  m_snapOverlay = new DashSnapOverlay(m_host);
  m_snapOverlay->hide();

  connect(&CalendarService::instance(), &CalendarService::eventsChanged, this,
          &DashboardPage::refresh);
  connect(&BlopTheme::instance(), &BlopTheme::themeChanged, this, [this]() {
    applyChromeStyles();
    refresh();
  });

  applyChromeStyles();
  rebuildEditBar();
  ensurePersistentHeader();
  updatePersistentHeader();
  rebuildWidgets();
}

void DashboardPage::applyChromeStyles() {
  setStyleSheet(QStringLiteral("QWidget#DashboardPage { background: %1; }")
                    .arg(paper()));
  if (m_editBar) {
    m_editBar->setStyleSheet(QStringLiteral(
                                 "QWidget#DashEditBar {"
                                 "  background: %1;"
                                 "  border-bottom: 1px solid %2;"
                                 "}")
                                 .arg(card(), border()));
  }
  if (m_persistentHeader) {
    m_persistentHeader->setStyleSheet(QStringLiteral(
                                          "QWidget#DashPersistentHeader {"
                                          "  background: transparent;"
                                          "  border: none;"
                                          "}"));
  }
  if (m_scroll) {
    m_scroll->setStyleSheet(
        QStringLiteral("QScrollArea { background: transparent; border: none; }"));
  }
  if (m_host)
    m_host->setStyleSheet(QStringLiteral("background: transparent;"));

  if (m_lblHello) {
    m_lblHello->setStyleSheet(QStringLiteral(
                                  "color: %1; font-size: 32px; font-weight: 700;"
                                  "letter-spacing: -0.8px; background: transparent;")
                                  .arg(ink()));
  }
  if (m_lblDate) {
    m_lblDate->setStyleSheet(QStringLiteral(
                                 "color: %1; font-size: 14px; font-weight: 400;"
                                 "background: transparent;")
                                 .arg(muted()));
  }
  if (m_lblClock) {
    m_lblClock->setStyleSheet(QStringLiteral(
                                  "color: %1; font-size: 13px; font-weight: 500;"
                                  "font-variant-numeric: tabular-nums;"
                                  "background: transparent;")
                                  .arg(ink()));
  }
  if (m_lblMetrics) {
    m_lblMetrics->setStyleSheet(QStringLiteral(
                                    "color: %1; font-size: 12px; font-weight: 400;"
                                    "background: transparent;")
                                    .arg(muted()));
  }
  if (m_btnBlocks)
    m_btnBlocks->setStyleSheet(compactGhostQss());
  if (m_btnEdit)
    m_btnEdit->setStyleSheet(compactGhostQss());
}

void DashboardPage::rebuildEditBar() {
  if (!m_editBarLay)
    return;
  while (QLayoutItem *it = m_editBarLay->takeAt(0)) {
    if (it->widget())
      it->widget()->deleteLater();
    delete it;
  }

  auto *editHint = new QLabel(
      QStringLiteral(
          "Layout anpassen — ⋮⋮ verschieben, Kantenpunkte zum Vergrößern"),
      m_editBar);
  editHint->setStyleSheet(QStringLiteral(
      "color: %1; font-size: 12px; font-weight: 400; background: transparent;")
                              .arg(muted()));
  m_editBarLay->addWidget(editHint, 1);

  auto *btnReset = new QPushButton(QStringLiteral("Zurücksetzen"), m_editBar);
  btnReset->setCursor(Qt::PointingHandCursor);
  btnReset->setStyleSheet(BlopStyle::paperSecondaryButtonQss());
  connect(btnReset, &QPushButton::clicked, this, [this]() {
    DashboardLayoutStore::reset();
    refresh();
  });
  m_editBarLay->addWidget(btnReset, 0);

  auto *btnDone = new QPushButton(QStringLiteral("Fertig"), m_editBar);
  btnDone->setCursor(Qt::PointingHandCursor);
  btnDone->setStyleSheet(BlopStyle::paperPrimaryButtonQss());
  connect(btnDone, &QPushButton::clicked, this, &DashboardPage::toggleEditMode);
  m_editBarLay->addWidget(btnDone, 0);
}

void DashboardPage::showBlocksMenu(QPushButton *anchor) {
  if (!anchor)
    return;
  QList<BlopInWindowMenu::Item> items;
  for (const QString &id : DashboardLayoutStore::knownIds()) {
    if (!isGridBlock(id))
      continue;
    bool visible = true;
    for (const auto &s : currentSpecs()) {
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
                       updateSpec(id, [visible](DashboardWidgetSpec &s) {
                         s.visible = !visible;
                       });
                     }});
  }
  BlopInWindowMenu::show(this, anchor->mapToGlobal(QPoint(0, anchor->height())),
                         items);
}

void DashboardPage::setEditMode(bool on) {
  if (m_editMode == on)
    return;
  m_editMode = on;
  m_dragging = false;
  m_gesture = DashGesture::None;
  m_dragBlockId.clear();
  m_dragFrame = nullptr;
  clearSnapOverlay();
  setScrollLocked(false);
  if (m_appFilterInstalled) {
    qApp->removeEventFilter(this);
    m_appFilterInstalled = false;
  }
  if (m_editBar)
    m_editBar->setVisible(m_editMode);
  if (m_scroll) {
    // Block finger-flick only (conflicts with drag grips); wheel/scrollbar OK.
    m_scroll->setProperty(BlopScroll::kNoFingerScrollProperty, m_editMode);
    m_scroll->verticalScrollBar()->setEnabled(true);
    m_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  }
  if (m_editMode)
    rebuildEditBar();
  updatePersistentHeader();
  emit customizeToggled(m_editMode);
  rebuildWidgets();
}

void DashboardPage::toggleEditMode() { setEditMode(!m_editMode); }

void DashboardPage::refresh() {
  updatePersistentHeader();
  rebuildWidgets();
}

void DashboardPage::persistSpecs() {
  DashboardLayoutStore::save(currentSpecs());
}

void DashboardPage::updateSpec(
    const QString &id,
    const std::function<void(DashboardWidgetSpec &)> &mutator) {
  auto specs = currentSpecs();
  for (auto &s : specs) {
    if (s.id == id) {
      mutator(s);
      break;
    }
  }
  resolveOverlaps(specs, id, PushDir::Auto);
  commitSpecs(std::move(specs));
}

void DashboardPage::commitSpecs(QVector<DashboardWidgetSpec> specs) {
  DashboardLayoutStore::save(specs);
  refresh();
}

void DashboardPage::resolveOverlaps(QVector<DashboardWidgetSpec> &specs,
                                    const QString &primaryId,
                                    PushDir dir) const {
  DashboardWidgetSpec *primary = nullptr;
  for (auto &s : specs) {
    if (s.id == primaryId) {
      primary = &s;
      break;
    }
  }
  if (!primary || !primary->visible || !isGridBlock(primary->id))
    return;

  // Primary geometry is authoritative — never shrink it to resolve collisions.
  primary->col = qBound(0, primary->col, 11);
  primary->colSpan = qBound(1, primary->colSpan, 12 - primary->col);
  primary->row = qMax(0, primary->row);
  primary->rowSpan = qBound(1, primary->rowSpan, 4);

  auto overlaps = [](const DashboardWidgetSpec &a,
                     const DashboardWidgetSpec &b) {
    if (a.id == b.id || !a.visible || !b.visible)
      return false;
    if (!isGridBlock(a.id) || !isGridBlock(b.id))
      return false;
    return a.row < b.row + b.rowSpan && b.row < a.row + a.rowSpan &&
           a.col < b.col + b.colSpan && b.col < a.col + a.colSpan;
  };

  for (int pass = 0; pass < 12; ++pass) {
    bool changed = false;
    for (auto &other : specs) {
      if (!overlaps(*primary, other))
        continue;

      const int pRight = primary->col + primary->colSpan;
      const int pBottom = primary->row + primary->rowSpan;
      const int oRight = other.col + other.colSpan;
      const int oBottom = other.row + other.rowSpan;
      const int overlapW =
          qMin(pRight, oRight) - qMax(primary->col, other.col);
      const int overlapH =
          qMin(pBottom, oBottom) - qMax(primary->row, other.row);

      PushDir use = dir;
      if (use == PushDir::Auto) {
        // Prefer the axis of deeper intrusion; default to pushing the
        // neighbor that sits further along that axis.
        if (overlapW >= overlapH) {
          use = (other.col + other.colSpan / 2 >=
                 primary->col + primary->colSpan / 2)
                    ? PushDir::East
                    : PushDir::West;
        } else {
          use = (other.row + other.rowSpan / 2 >=
                 primary->row + primary->rowSpan / 2)
                    ? PushDir::South
                    : PushDir::North;
        }
      }

      switch (use) {
      case PushDir::East:
        // Primary grew right → neighbor loses its left side.
        other.col = qMin(11, pRight);
        other.colSpan = qBound(1, oRight - other.col, 12 - other.col);
        break;
      case PushDir::West:
        // Primary grew left → neighbor loses its right side.
        other.colSpan = qMax(1, primary->col - other.col);
        break;
      case PushDir::South:
        // Primary grew down → neighbor is pushed/shrunk from the top.
        other.row = pBottom;
        other.rowSpan = qBound(1, oBottom - other.row, 4);
        break;
      case PushDir::North:
        // Primary grew up → neighbor loses its bottom.
        other.rowSpan = qMax(1, primary->row - other.row);
        break;
      case PushDir::Auto:
        break;
      }
      changed = true;
    }
    if (!changed)
      break;
  }
}

QWidget *DashboardPage::buildEditChrome(const QString &id) {
  auto *bar = new QWidget();
  bar->setObjectName(QStringLiteral("DashEditChrome"));
  auto *lay = new QHBoxLayout(bar);
  lay->setContentsMargins(0, 0, 0, UiScale::dp(6));
  lay->setSpacing(UiScale::dp(4));

  auto *grip = new QLabel(QStringLiteral("⋮⋮"), bar);
  grip->setObjectName(QStringLiteral("DashDragGrip"));
  grip->setCursor(Qt::SizeAllCursor);
  grip->setToolTip(QStringLiteral("Ziehen zum Verschieben"));
  grip->setAttribute(Qt::WA_AcceptTouchEvents, true);
  grip->installEventFilter(this);
  grip->setStyleSheet(gripQss(true));
  lay->addWidget(grip, 0);

  auto *title = new QLabel(DashboardLayoutStore::displayName(id), bar);
  title->setStyleSheet(sectionTitleQss());
  lay->addWidget(title, 1);

  auto addBtn = [&](const QString &label, auto slot) {
    auto *b = new QPushButton(label, bar);
    b->setCursor(Qt::PointingHandCursor);
    b->setStyleSheet(editChipQss());
    connect(b, &QPushButton::clicked, this, slot);
    lay->addWidget(b, 0);
  };

  addBtn(QStringLiteral("←"), [this, id]() {
    updateSpec(id, [](DashboardWidgetSpec &s) {
      s.col = qMax(0, s.col - 1);
    });
  });
  addBtn(QStringLiteral("→"), [this, id]() {
    updateSpec(id, [](DashboardWidgetSpec &s) {
      s.col = qMin(12 - s.colSpan, s.col + 1);
    });
  });
  addBtn(QStringLiteral("↑"), [this, id]() {
    updateSpec(id, [](DashboardWidgetSpec &s) {
      s.row = qMax(0, s.row - 1);
    });
  });
  addBtn(QStringLiteral("↓"), [this, id]() {
    updateSpec(id, [](DashboardWidgetSpec &s) { s.row += 1; });
  });

  auto *size = new QComboBox(bar);
  size->setCursor(Qt::PointingHandCursor);
  size->addItem(QStringLiteral("Schmal"), 4);
  size->addItem(QStringLiteral("Halb"), 6);
  size->addItem(QStringLiteral("Breit"), 8);
  size->addItem(QStringLiteral("Voll"), 12);
  for (const auto &s : currentSpecs()) {
    if (s.id == id) {
      const int idx = size->findData(s.colSpan);
      if (idx >= 0)
        size->setCurrentIndex(idx);
      break;
    }
  }
  connect(size, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this, id, size](int) {
            const int span = size->currentData().toInt();
            updateSpec(id, [span](DashboardWidgetSpec &s) {
              s.colSpan = span;
              if (s.col + s.colSpan > 12)
                s.col = qMax(0, 12 - s.colSpan);
            });
          });
  lay->addWidget(size, 0);

  auto *height = new QComboBox(bar);
  height->setCursor(Qt::PointingHandCursor);
  height->addItem(QStringLiteral("Kompakt"), 1);
  height->addItem(QStringLiteral("Normal"), 2);
  height->addItem(QStringLiteral("Hoch"), 3);
  for (const auto &s : currentSpecs()) {
    if (s.id == id) {
      const int idx = height->findData(s.rowSpan);
      if (idx >= 0)
        height->setCurrentIndex(idx);
      break;
    }
  }
  connect(height, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this, id, height](int) {
            const int span = height->currentData().toInt();
            updateSpec(id, [span](DashboardWidgetSpec &s) { s.rowSpan = span; });
          });
  lay->addWidget(height, 0);

  if (id == QLatin1String("calendar") || id == QLatin1String("todos") ||
      id == QLatin1String("recent")) {
    auto *limit = new QComboBox(bar);
    limit->setCursor(Qt::PointingHandCursor);
    if (id == QLatin1String("calendar")) {
      limit->addItem(QStringLiteral("5 Termine"), 5);
      limit->addItem(QStringLiteral("10 Termine"), 10);
      limit->addItem(QStringLiteral("15 Termine"), 15);
    } else if (id == QLatin1String("todos")) {
      limit->addItem(QStringLiteral("5 Todos"), 5);
      limit->addItem(QStringLiteral("8 Todos"), 8);
      limit->addItem(QStringLiteral("12 Todos"), 12);
    } else {
      limit->addItem(QStringLiteral("4 Notizen"), 4);
      limit->addItem(QStringLiteral("6 Notizen"), 6);
      limit->addItem(QStringLiteral("8 Notizen"), 8);
    }
    for (const auto &s : currentSpecs()) {
      if (s.id == id) {
        const int lim = s.itemLimit > 0 ? s.itemLimit : limit->itemData(1).toInt();
        const int idx = limit->findData(lim);
        if (idx >= 0)
          limit->setCurrentIndex(idx);
        break;
      }
    }
    connect(limit, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, id, limit](int) {
              const int n = limit->currentData().toInt();
              updateSpec(id, [n](DashboardWidgetSpec &s) { s.itemLimit = n; });
            });
    lay->addWidget(limit, 0);
  }

  if (id == QLatin1String("clock")) {
    auto *style = new QComboBox(bar);
    style->setCursor(Qt::PointingHandCursor);
    style->addItem(QStringLiteral("Digital"), 0);
    style->addItem(QStringLiteral("Analog"), 1);
    style->setCurrentIndex(clockAnalogPref() ? 1 : 0);
    connect(style, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, style](int) {
              setClockAnalogPref(style->currentData().toInt() == 1);
              refresh();
            });
    lay->addWidget(style, 0);
  }

  auto *hideBtn = new QPushButton(QStringLiteral("Ausblenden"), bar);
  hideBtn->setCursor(Qt::PointingHandCursor);
  hideBtn->setStyleSheet(editChipQss());
  connect(hideBtn, &QPushButton::clicked, this, [this, id]() {
    updateSpec(id, [](DashboardWidgetSpec &s) { s.visible = false; });
  });
  lay->addWidget(hideBtn, 0);
  return bar;
}

void DashboardPage::ensurePersistentHeader() {
  if (m_headerBuilt || !m_persistentHeader)
    return;

  auto *lay = new QHBoxLayout(m_persistentHeader);
  lay->setContentsMargins(UiScale::dp(48), UiScale::dp(28), UiScale::dp(48),
                          UiScale::dp(8));
  lay->setSpacing(UiScale::dp(16));

  auto *textHost = new QWidget(m_persistentHeader);
  textHost->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  auto *textCol = new QVBoxLayout(textHost);
  textCol->setContentsMargins(0, 0, 0, 0);
  textCol->setSpacing(UiScale::dp(6));

  m_lblHello = new QLabel(textHost);
  m_lblDate = new QLabel(textHost);
  m_lblClock = new QLabel(textHost);
  m_lblClock->hide(); // clock block owns time; keep header uncluttered
  m_lblMetrics = new QLabel(textHost);

  textCol->addWidget(m_lblHello);
  textCol->addWidget(m_lblDate);
  textCol->addWidget(m_lblClock);
  textCol->addWidget(m_lblMetrics);
  lay->addWidget(textHost, 1);

  auto *actionsHost = new QWidget(m_persistentHeader);
  actionsHost->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
  auto *actions = new QHBoxLayout(actionsHost);
  actions->setSpacing(UiScale::dp(4));
  actions->setContentsMargins(0, 0, 0, 0);

  auto *btnNotes = new QPushButton(QStringLiteral("Notizen"), actionsHost);
  btnNotes->setCursor(Qt::PointingHandCursor);
  btnNotes->setStyleSheet(compactGhostQss());
  connect(btnNotes, &QPushButton::clicked, this,
          &DashboardPage::snapToNotesRequested);

  m_btnBlocks = new QPushButton(QStringLiteral("Blöcke"), actionsHost);
  m_btnBlocks->setCursor(Qt::PointingHandCursor);
  m_btnBlocks->setToolTip(QStringLiteral("Dashboard-Blöcke ein- und ausblenden"));
  m_btnBlocks->setStyleSheet(compactGhostQss());
  connect(m_btnBlocks, &QPushButton::clicked, this,
          [this]() { showBlocksMenu(m_btnBlocks); });

  m_btnEdit = new QPushButton(QStringLiteral("Bearbeiten"), actionsHost);
  m_btnEdit->setCursor(Qt::PointingHandCursor);
  m_btnEdit->setStyleSheet(compactGhostQss());
  connect(m_btnEdit, &QPushButton::clicked, this, &DashboardPage::toggleEditMode);

  actions->addWidget(btnNotes, 0);
  actions->addWidget(m_btnBlocks, 0);
  actions->addWidget(m_btnEdit, 0);
  lay->addWidget(actionsHost, 0, Qt::AlignTop);

  m_headerBuilt = true;
  applyChromeStyles();
}

void DashboardPage::updatePersistentHeader() {
  if (!m_headerBuilt)
    ensurePersistentHeader();
  if (!m_lblHello)
    return;

  m_lblHello->setText(
      QStringLiteral("%1, %2").arg(greetingHour(), dashboardUserName()));
  m_lblDate->setText(formatToday());
  m_lblClock->setText(formatClock());
  if (m_lblMetrics)
    m_lblMetrics->clear(); // Notion page: no stat strip under the title

  if (m_btnEdit)
    m_btnEdit->setText(m_editMode ? QStringLiteral("Fertig")
                                  : QStringLiteral("Bearbeiten"));
  applyChromeStyles();
}

void DashboardPage::purgeFloatingHostWidgets() {
  if (!m_host)
    return;
  const auto children = m_host->children();
  for (QObject *child : children) {
    if (child == m_snapOverlay)
      continue;
    auto *w = qobject_cast<QWidget *>(child);
    if (!w)
      continue;
    if (m_dragging && w == m_dragFrame)
      continue;
    if (m_gridLay && m_gridLay->indexOf(w) >= 0)
      continue;
    delete w;
  }
}

QWidget *DashboardPage::buildEmptyStatePanel() {
  auto *panel = new QFrame(m_host);
  panel->setObjectName(QStringLiteral("DashEmptyState"));
  panel->setAttribute(Qt::WA_StyledBackground, true);
  panel->setStyleSheet(QStringLiteral(
                           "QFrame#DashEmptyState {"
                           "  background: %1;"
                           "  border: 1px dashed %2;"
                           "  border-radius: 8px;"
                           "}")
                           .arg(card(), border()));

  auto *lay = new QVBoxLayout(panel);
  lay->setContentsMargins(UiScale::dp(32), UiScale::dp(36), UiScale::dp(32),
                          UiScale::dp(36));
  lay->setSpacing(UiScale::dp(10));
  lay->setAlignment(Qt::AlignCenter);

  auto *title = new QLabel(QStringLiteral("Noch keine Dashboard-Blöcke"), panel);
  title->setAlignment(Qt::AlignCenter);
  title->setStyleSheet(QStringLiteral(
                           "color: %1; font-size: 16px; font-weight: 600;"
                           "background: transparent;")
                           .arg(ink()));

  auto *sub = new QLabel(
      QStringLiteral(
          "Füge Aufgaben, Kalender, Zuletzt geöffnet oder Schnellzugriff hinzu."),
      panel);
  sub->setAlignment(Qt::AlignCenter);
  sub->setWordWrap(true);
  sub->setStyleSheet(QStringLiteral(
                         "color: %1; font-size: 13px; background: transparent;")
                         .arg(muted()));

  auto *btnAdd = new QPushButton(QStringLiteral("Blöcke hinzufügen"), panel);
  btnAdd->setCursor(Qt::PointingHandCursor);
  btnAdd->setStyleSheet(BlopStyle::paperPrimaryButtonQss());
  connect(btnAdd, &QPushButton::clicked, this,
          [this, btnAdd]() { showBlocksMenu(btnAdd); });

  lay->addWidget(title);
  lay->addWidget(sub);
  lay->addSpacing(UiScale::dp(4));
  lay->addWidget(btnAdd, 0, Qt::AlignHCenter);
  return panel;
}

QWidget *DashboardPage::wrapBlock(const QString &id, QWidget *content,
                                   int minHeight, bool showTitle) {
  Q_UNUSED(minHeight);
  auto *frame = new QFrame(m_host);
  frame->setObjectName(QStringLiteral("DashBlock"));
  frame->setAttribute(Qt::WA_StyledBackground, true);
  frame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  frame->setStyleSheet(softBlockQss(m_editMode, id));

  auto *lay = new QVBoxLayout(frame);
  lay->setContentsMargins(UiScale::dp(14), UiScale::dp(12), UiScale::dp(14),
                          UiScale::dp(14));
  lay->setSpacing(UiScale::dp(8));
  if (m_editMode)
    lay->addWidget(buildEditChrome(id), 0);
  else if (showTitle) {
    auto *hdr = new QWidget(frame);
    auto *hl = new QHBoxLayout(hdr);
    hl->setContentsMargins(0, 0, 0, UiScale::dp(2));
    hl->setSpacing(UiScale::dp(6));
    auto *grip = new QLabel(QStringLiteral("⋮⋮"), hdr);
    grip->setObjectName(QStringLiteral("DashDragGrip"));
    grip->setStyleSheet(gripQss(false));
    grip->setToolTip(QStringLiteral("Zum Verschieben: Bearbeiten aktivieren"));
    auto *title = new QLabel(DashboardLayoutStore::displayName(id), hdr);
    title->setStyleSheet(sectionTitleQss());
    hl->addWidget(grip, 0);
    hl->addWidget(title, 1);
    lay->addWidget(hdr, 0);
  }

  // Scroll only for list-heavy blocks. Clock/shortcuts otherwise show thin
  // scrollbar tracks that look like stray gray bars across the card.
  const bool needsScroll = id == QLatin1String("todos") ||
                           id == QLatin1String("calendar") ||
                           id == QLatin1String("recent");
  if (needsScroll) {
    auto *scroller = new QScrollArea(frame);
    scroller->setObjectName(QStringLiteral("DashBlockScroll"));
    scroller->setWidgetResizable(true);
    scroller->setFrameShape(QFrame::NoFrame);
    scroller->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroller->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroller->setStyleSheet(scrollBarQss());
    content->setParent(scroller);
    scroller->setWidget(content);
    lay->addWidget(scroller, 1);
  } else {
    content->setParent(frame);
    lay->addWidget(content, 1);
  }

  frame->setProperty("dashBlockId", id);
  if (m_editMode) {
    frame->setAttribute(Qt::WA_AcceptTouchEvents, true);
    frame->installEventFilter(this);
    attachResizeHandles(frame);
  }
  return frame;
}

void DashboardPage::attachResizeHandles(QFrame *frame) {
  const auto make = [&](const char *edge, Qt::CursorShape cursor) {
    auto *h = new QWidget(frame);
    h->setObjectName(QStringLiteral("DashResizeHandle"));
    h->setProperty("resizeEdge", QString::fromLatin1(edge));
    h->setAttribute(Qt::WA_AcceptTouchEvents, true);
    h->setCursor(cursor);
    h->setStyleSheet(QStringLiteral(
        "QWidget#DashResizeHandle {"
        "  background: %1; border: 1px solid %2;"
        "  border-radius: 3px;"
        "}"
        "QWidget#DashResizeHandle:hover { background: %3; }")
                         .arg(accent(), card(),
                              BlopTheme::accentHover().name(QColor::HexRgb)));
    h->installEventFilter(this);
    h->raise();
  };
  make("N", Qt::SizeVerCursor);
  make("E", Qt::SizeHorCursor);
  make("S", Qt::SizeVerCursor);
  make("W", Qt::SizeHorCursor);
  layoutResizeHandles(frame);
}

void DashboardPage::layoutResizeHandles(QFrame *frame) const {
  if (!frame)
    return;
  const int grip = UiScale::dp(16);
  const int w = frame->width();
  const int h = frame->height();
  for (auto *handle : frame->findChildren<QWidget *>(QStringLiteral("DashResizeHandle"))) {
    const QString edge = handle->property("resizeEdge").toString();
    if (edge == QLatin1String("N"))
      handle->setGeometry((w - grip) / 2, 0, grip, grip);
    else if (edge == QLatin1String("S"))
      handle->setGeometry((w - grip) / 2, h - grip, grip, grip);
    else if (edge == QLatin1String("W"))
      handle->setGeometry(0, (h - grip) / 2, grip, grip);
    else if (edge == QLatin1String("E"))
      handle->setGeometry(w - grip, (h - grip) / 2, grip, grip);
  }
}

int DashboardPage::gridRowUnit() const { return UiScale::dp(100); }

int DashboardPage::cellHeightForSpan(int rowSpan) const {
  const int span = qMax(1, rowSpan);
  const int vsp = m_gridLay ? m_gridLay->verticalSpacing() : UiScale::dp(16);
  return span * gridRowUnit() + (span - 1) * vsp;
}

int DashboardPage::minRowSpanForBlock(const QString &id) {
  if (id == QLatin1String("shortcuts"))
    return 2;
  if (id == QLatin1String("clock"))
    return 2;
  if (id == QLatin1String("todos") || id == QLatin1String("calendar"))
    return 2;
  return 1;
}

void DashboardPage::applyBlockCellSize(QWidget *block, int rowSpan) const {
  if (!block)
    return;
  const QString id = block->property("dashBlockId").toString();
  const int span = qMax(rowSpan, minRowSpanForBlock(id));
  const int h = cellHeightForSpan(span);
  block->setMinimumHeight(h);
  block->setMaximumHeight(h);
  block->setFixedHeight(h);
}

void DashboardPage::gridColumnEdges(QVector<int> &out) const {
  out.resize(13);
  if (!m_gridLay || !m_host) {
    out.fill(0);
    return;
  }
  const QMargins mg = m_gridLay->contentsMargins();
  const int hsp = m_gridLay->horizontalSpacing();
  const int availW = qMax(12, m_host->width() - mg.left() - mg.right());
  const int colW = qMax(1, (availW - 11 * hsp) / 12);
  for (int c = 0; c < 12; ++c)
    out[c] = mg.left() + c * (colW + hsp);
  out[12] = out[11] + colW;
}

void DashboardPage::gridRowEdges(QVector<int> &out) const {
  if (!m_gridLay || !m_host) {
    out.clear();
    return;
  }
  const QMargins mg = m_gridLay->contentsMargins();
  const int vsp = m_gridLay->verticalSpacing();
  const int rowPitch = gridRowUnit() + vsp;
  // Enough rows for current layout + a few empty slots below.
  const int rows = qBound(4, maxSnapRow() + 4, 28);
  out.resize(rows + 1);
  out[0] = mg.top();
  for (int r = 0; r < rows; ++r)
    out[r + 1] = out[r] + rowPitch;
}

int DashboardPage::snapRowFromY(int hostY) const {
  QVector<int> rowY;
  gridRowEdges(rowY);
  if (rowY.size() < 2)
    return 0;
  // Nearest row by top edge (used for move + resize anchors).
  int best = 0;
  int bestDist = INT_MAX;
  for (int r = 0; r + 1 < rowY.size(); ++r) {
    const int dist = qAbs(hostY - rowY[r]);
    if (dist < bestDist) {
      bestDist = dist;
      best = r;
    }
  }
  return best;
}

int DashboardPage::snapColFromX(int hostX, int colSpan) const {
  QVector<int> colX;
  gridColumnEdges(colX);
  if (colX.size() < 2)
    return 0;
  // Nearest column by left edge. colSpan only clamps the max start col.
  const int maxCol = qBound(0, 12 - colSpan, colX.size() - 2);
  int best = 0;
  int bestDist = INT_MAX;
  for (int c = 0; c <= maxCol; ++c) {
    const int dist = qAbs(hostX - colX[c]);
    if (dist < bestDist) {
      bestDist = dist;
      best = c;
    }
  }
  return best;
}

void DashboardPage::setScrollLocked(bool locked) {
  if (!m_scroll)
    return;
  auto *bar = m_scroll->verticalScrollBar();
  if (locked) {
    m_frozenScrollY = bar->value();
    bar->setEnabled(false);
    QObject::disconnect(m_scrollFreezeConn);
    m_scrollFreezeConn = connect(bar, &QScrollBar::valueChanged, this,
                                 [this](int) {
                                   if (!m_dragging || !m_scroll)
                                     return;
                                   QSignalBlocker block(m_scroll->verticalScrollBar());
                                   m_scroll->verticalScrollBar()->setValue(m_frozenScrollY);
                                 });
  } else {
    QObject::disconnect(m_scrollFreezeConn);
    if (!m_editMode)
      bar->setEnabled(true);
  }
}

void DashboardPage::beginGesture(QFrame *frame, const QString &blockId,
                                 DashGesture gesture) {
  DashboardWidgetSpec spec;
  for (const auto &s : currentSpecs()) {
    if (s.id == blockId) {
      spec = s;
      break;
    }
  }

  m_dragging = true;
  m_gesture = gesture;
  m_dragBlockId = blockId;
  m_dragFrame = frame;
  m_dragStartRow = spec.row;
  m_dragStartCol = spec.col;
  m_dragStartColSpan = spec.colSpan;
  m_dragStartRowSpan = spec.rowSpan;
  m_dragColSpan = spec.colSpan;
  m_dragRowSpan = spec.rowSpan;
  m_previewRow = spec.row;
  m_previewCol = spec.col;
  m_previewColSpan = spec.colSpan;
  m_previewRowSpan = spec.rowSpan;
  m_floatActive = false;

  setScrollLocked(true);
  if (!m_appFilterInstalled) {
    qApp->installEventFilter(this);
    m_appFilterInstalled = true;
  }

  // Lift the block out of the grid so it follows the pointer 1:1.
  if (gesture == DashGesture::Move && frame && m_gridLay && m_host) {
    const QRect geom = frame->geometry();
    m_dragOriginHost = geom.topLeft();
    if (!m_havePressHostPos)
      m_pressHostPos = m_host->mapFromGlobal(QCursor::pos());
    m_havePressHostPos = true;
    applyBlockCellSize(frame, m_dragRowSpan);
    m_gridLay->removeWidget(frame);
    frame->setParent(m_host);
    frame->setGeometry(QRect(geom.topLeft(),
                             QSize(geom.width(), cellHeightForSpan(m_dragRowSpan))));
    frame->show();
    frame->raise();
    m_floatActive = true;
  }

  if (frame) {
    frame->setMouseTracking(true);
    frame->grabMouse();
  }
  if (m_host)
    m_host->setMouseTracking(true);
  if (m_snapOverlay) {
    m_snapOverlay->setGeometry(m_host ? m_host->rect() : QRect());
    m_snapOverlay->raise();
    if (frame && m_floatActive)
      frame->raise();
    updateSnapOverlay(spec.row, spec.col, spec.colSpan, spec.rowSpan);
  }
}

void DashboardPage::handleGestureRelease() {
  if (!m_dragging || m_dragBlockId.isEmpty() || !m_host)
    return;

  PushDir dir = PushDir::Auto;
  if (m_gesture == DashGesture::ResizeE)
    dir = PushDir::East;
  else if (m_gesture == DashGesture::ResizeW)
    dir = PushDir::West;
  else if (m_gesture == DashGesture::ResizeS)
    dir = PushDir::South;
  else if (m_gesture == DashGesture::ResizeN)
    dir = PushDir::North;

  if (m_gesture == DashGesture::Move) {
    QPoint topLeft = m_dragOriginHost;
    if (m_dragFrame && m_floatActive)
      topLeft = m_dragFrame->pos();
    else {
      const QPoint hostPos = m_host->mapFromGlobal(QCursor::pos());
      topLeft = m_dragOriginHost + (hostPos - m_pressHostPos);
    }
    snapGridFromPos(topLeft, m_dragColSpan, m_dragRowSpan, m_previewRow,
                    m_previewCol);
    const QString id = m_dragBlockId;
    const int row = qBound(0, m_previewRow, 24);
    const int col = qBound(0, m_previewCol, 12 - m_dragColSpan);
    finishGesture([this, id, row, col, dir]() {
      auto specs = currentSpecs();
      for (auto &s : specs) {
        if (s.id == id) {
          s.row = row;
          s.col = col;
          break;
        }
      }
      resolveOverlaps(specs, id, dir);
      commitSpecs(std::move(specs));
    });
    return;
  }

  applyResizePreview(m_host->mapFromGlobal(QCursor::pos()));
  const QString id = m_dragBlockId;
  const int pr = qBound(0, m_previewRow, 24);
  const int pc = qBound(0, m_previewCol, 12 - m_previewColSpan);
  const int pcs = qBound(1, m_previewColSpan, 12);
  const int prs = qBound(1, m_previewRowSpan, 4);
  finishGesture([this, id, pr, pc, pcs, prs, dir]() {
    auto specs = currentSpecs();
    for (auto &s : specs) {
      if (s.id == id) {
        s.row = pr;
        s.col = pc;
        s.colSpan = pcs;
        s.rowSpan = prs;
        break;
      }
    }
    resolveOverlaps(specs, id, dir);
    commitSpecs(std::move(specs));
  });
}

void DashboardPage::finishGesture(const std::function<void()> &commit) {
  if (!m_dragging)
    return;

  if (m_appFilterInstalled) {
    qApp->removeEventFilter(this);
    m_appFilterInstalled = false;
  }
  if (QWidget *grabber = QWidget::mouseGrabber())
    grabber->releaseMouse();
  if (m_dragFrame)
    m_dragFrame->setMouseTracking(false);
  if (m_host)
    m_host->setMouseTracking(false);

  clearSnapOverlay();
  m_dragging = false;
  m_gesture = DashGesture::None;
  m_dragBlockId.clear();
  m_dragFrame = nullptr;
  m_floatActive = false;
  m_havePressHostPos = false;
  m_pressHostPos = QPoint();
  m_dragOriginHost = QPoint();
  setScrollLocked(false);

  QTimer::singleShot(0, this, commit);
}

QWidget *DashboardPage::buildClockBlock() {
  auto *body = new QWidget();
  auto *lay = new QVBoxLayout(body);
  lay->setContentsMargins(UiScale::dp(4), UiScale::dp(8), UiScale::dp(4),
                          UiScale::dp(8));
  lay->setSpacing(UiScale::dp(4));

  const bool analog = clockAnalogPref();
  if (analog) {
    auto *face = new DashAnalogClock(body);
    face->setMinimumHeight(UiScale::dp(120));
    lay->addWidget(face, 1);
  } else {
    auto *timeLbl = new QLabel(formatClock(), body);
    timeLbl->setObjectName(QStringLiteral("DashDigitalClock"));
    timeLbl->setAlignment(Qt::AlignCenter);
    timeLbl->setStyleSheet(QStringLiteral(
                               "color: %1; font-size: 40px; font-weight: 600;"
                               "letter-spacing: -1px; background: transparent;")
                               .arg(ink()));
    auto *dateLbl = new QLabel(
        QLocale(QLocale::German, QLocale::Germany)
            .toString(QDate::currentDate(), QStringLiteral("d. MMMM")),
        body);
    dateLbl->setAlignment(Qt::AlignCenter);
    dateLbl->setStyleSheet(QStringLiteral(
                               "color: %1; font-size: 12px; font-weight: 400;"
                               "background: transparent;")
                               .arg(muted()));
    lay->addStretch(1);
    lay->addWidget(timeLbl, 0, Qt::AlignHCenter);
    lay->addWidget(dateLbl, 0, Qt::AlignHCenter);
    lay->addStretch(1);
  }

  return wrapBlock(QStringLiteral("clock"), body, 0, true);
}

QWidget *DashboardPage::buildTodosBlock() {
  auto *body = new QWidget();
  auto *lay = new QVBoxLayout(body);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(UiScale::dp(4));

  const int limit =
      DashboardLayoutStore::itemLimitFor(QStringLiteral("todos"), 8);
  const auto items = TodoStore::load();
  int shown = 0;
  for (const TodoItem &t : items) {
    if (shown >= limit)
      break;
    auto *row = new QFrame(body);
    row->setObjectName(QStringLiteral("DashRow"));
    row->setStyleSheet(rowQss());
    auto *rl = new QHBoxLayout(row);
    rl->setContentsMargins(UiScale::dp(10), UiScale::dp(8), UiScale::dp(10),
                           UiScale::dp(8));
    rl->setSpacing(UiScale::dp(8));
    auto *cb = new QCheckBox(row);
    cb->setChecked(t.done);
    cb->setStyleSheet(checkBoxQss());
    auto *lbl = new QLabel(t.title, row);
    lbl->setWordWrap(true);
    lbl->setStyleSheet(bodyTextQss(t.done));
    const QString id = t.id;
    connect(cb, &QCheckBox::toggled, this, [this, id](bool on) {
      TodoStore::setDone(id, on);
      refresh();
    });
    rl->addWidget(cb, 0);
    rl->addWidget(lbl, 1);
    lay->addWidget(row);
    ++shown;
  }
  if (shown == 0) {
    auto *empty = new QLabel(QStringLiteral("Keine offenen Aufgaben."), body);
    empty->setStyleSheet(QStringLiteral(
                             "color: %1; font-size: 13px; background: transparent;")
                             .arg(muted()));
    lay->addWidget(empty);
  }

  auto *addRow = new QHBoxLayout();
  auto *input = new QLineEdit(body);
  input->setPlaceholderText(QStringLiteral("Neue Aufgabe…"));
  input->setMinimumHeight(UiScale::dp(36));
  input->setStyleSheet(dashInputQss());
  auto *addBtn = new QPushButton(QStringLiteral("+"), body);
  addBtn->setFixedSize(UiScale::dp(32), UiScale::dp(32));
  addBtn->setCursor(Qt::PointingHandCursor);
  addBtn->setStyleSheet(QStringLiteral(
                            "QPushButton { background: %1; color: #FFF; border: none;"
                            "  border-radius: 6px; font-weight: 700; font-size: 16px; }"
                            "QPushButton:hover { background: %2; }")
                            .arg(accent(),
                                 BlopTheme::accentHover().name(QColor::HexRgb)));
  auto doAdd = [this, input]() {
    if (input->text().trimmed().isEmpty())
      return;
    TodoStore::add(input->text());
    input->clear();
    refresh();
  };
  connect(addBtn, &QPushButton::clicked, this, doAdd);
  connect(input, &QLineEdit::returnPressed, this, doAdd);
  addRow->addWidget(input, 1);
  addRow->addWidget(addBtn, 0);
  lay->addLayout(addRow);
  lay->addStretch(1);
  return wrapBlock(QStringLiteral("todos"), body);
}

QWidget *DashboardPage::buildCalendarBlock(bool maximizedChrome) {
  auto *body = new QWidget();
  auto *lay = new QVBoxLayout(body);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(UiScale::dp(6));

  auto *hdr = new QHBoxLayout();
  auto *btnConnect = new QPushButton(
      CalendarService::instance().hasGoogleAccess()
          ? QStringLiteral("Sync")
          : QStringLiteral("Google verbinden"),
      body);
  btnConnect->setFlat(true);
  btnConnect->setCursor(Qt::PointingHandCursor);
  btnConnect->setStyleSheet(quietBtnQss());
  connect(btnConnect, &QPushButton::clicked, this, []() {
    if (CalendarService::instance().hasGoogleAccess())
      CalendarService::instance().refreshGoogle();
    else
      CalendarService::instance().connectGoogle();
  });
  hdr->addStretch(1);
  hdr->addWidget(btnConnect, 0);
  if (!maximizedChrome) {
    auto *btnMax = new QPushButton(QStringLiteral("Maximieren"), body);
    btnMax->setFlat(true);
    btnMax->setCursor(Qt::PointingHandCursor);
    btnMax->setStyleSheet(quietBtnQss());
    connect(btnMax, &QPushButton::clicked, this,
            &DashboardPage::showCalendarMaximized);
    hdr->addWidget(btnMax, 0);
  }
  auto *btnNew = new QPushButton(QStringLiteral("Termin +"), body);
  btnNew->setFlat(true);
  btnNew->setCursor(Qt::PointingHandCursor);
  btnNew->setStyleSheet(quietBtnQss());
  connect(btnNew, &QPushButton::clicked, this, [this]() {
    openCreateEventDialog(QDateTime());
  });
  hdr->addWidget(btnNew, 0);
  lay->addLayout(hdr);

  auto *dayView = new CalendarDayView(body);
  dayView->setCompact(!maximizedChrome);
  dayView->setMinimumHeight(
      UiScale::dp(maximizedChrome ? 420 : 280));
  connect(dayView, &CalendarDayView::createAt, this,
          [this](const QDateTime &dt) { openCreateEventDialog(dt); });
  lay->addWidget(dayView, 1);

  if (maximizedChrome)
    return body;
  return wrapBlock(QStringLiteral("calendar"), body);
}

QWidget *DashboardPage::buildRecentBlock() {
  auto *body = new QWidget();
  auto *lay = new QVBoxLayout(body);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(UiScale::dp(4));

  const int limit =
      DashboardLayoutStore::itemLimitFor(QStringLiteral("recent"), 6);
  const QStringList recent = LibraryOrgStore::recentPaths(limit);
  if (recent.isEmpty()) {
    auto *empty =
        new QLabel(QStringLiteral("Noch keine Notizen geöffnet."), body);
    empty->setStyleSheet(QStringLiteral(
                             "color: %1; font-size: 13px; background: transparent;")
                             .arg(muted()));
    lay->addWidget(empty);
  } else {
    for (const QString &path : recent) {
      auto *row = new QFrame(body);
      row->setObjectName(QStringLiteral("DashRow"));
      row->setStyleSheet(rowQss());
      auto *rl = new QHBoxLayout(row);
      rl->setContentsMargins(UiScale::dp(10), UiScale::dp(8), UiScale::dp(10),
                             UiScale::dp(8));
      const QString name = QFileInfo(path).completeBaseName();
      auto *dot = new QLabel(name.left(1).toUpper(), row);
      dot->setObjectName(QStringLiteral("DashBadge"));
      dot->setFixedSize(UiScale::dp(24), UiScale::dp(24));
      dot->setAlignment(Qt::AlignCenter);
      dot->setStyleSheet(badgeQss() + QStringLiteral(
                                          "QLabel#DashBadge {"
                                          "  padding: 0; font-size: 11px; font-weight: 600;"
                                          "  qproperty-alignment: AlignCenter;"
                                          "}"));
      auto *t = new QLabel(name.isEmpty() ? QStringLiteral("Notiz") : name, row);
      t->setStyleSheet(QStringLiteral(
                           "color: %1; font-size: 13px; font-weight: 500;"
                           "background: transparent;")
                           .arg(ink()));
      auto *open = new QPushButton(QStringLiteral("Öffnen"), row);
      open->setFlat(true);
      open->setCursor(Qt::PointingHandCursor);
      open->setStyleSheet(quietBtnQss());
      connect(open, &QPushButton::clicked, this,
              [this, path]() { emit openNotePath(path); });
      rl->addWidget(dot, 0);
      rl->addWidget(t, 1);
      rl->addWidget(open, 0);
      lay->addWidget(row);
    }
  }
  return wrapBlock(QStringLiteral("recent"), body);
}

QWidget *DashboardPage::buildShortcutsBlock() {
  auto *body = new QWidget();
  auto *grid = new QGridLayout(body);
  grid->setContentsMargins(0, 0, 0, 0);
  grid->setHorizontalSpacing(UiScale::dp(10));
  grid->setVerticalSpacing(UiScale::dp(10));

  struct Card {
    QString title;
    QString subtitle;
    std::function<void()> action;
  };
  const QVector<Card> cards = {
      {QStringLiteral("Neue Notiz"),
       QStringLiteral("Schnell festhalten — Meeting, Skizze, Idee"),
       [this]() { emit newNoteRequested(); }},
      {QStringLiteral("Bibliothek"),
       QStringLiteral("Projekte, Ordner und Referenzen"),
       [this]() { emit snapToNotesRequested(); }},
      {QStringLiteral("Kalender"),
       QStringLiteral("Termine und Fokusfenster"),
       [this]() { showCalendarMaximized(); }},
      {QStringLiteral("Recherche"),
       QStringLiteral("Quellen, Papers und Wissensbasis"),
       [this]() { emit studyRequested(); }},
  };

  int col = 0;
  for (const Card &item : cards) {
    auto *btn = new QPushButton(body);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setMinimumHeight(UiScale::dp(76));
    btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    btn->setStyleSheet(shortcutCardQss());
    auto *vl = new QVBoxLayout(btn);
    vl->setContentsMargins(UiScale::dp(12), UiScale::dp(10), UiScale::dp(12),
                           UiScale::dp(10));
    vl->setSpacing(UiScale::dp(4));
    auto *t = new QLabel(item.title, btn);
    t->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    t->setStyleSheet(QStringLiteral(
                         "color: %1; font-size: 13px; font-weight: 600;"
                         "background: transparent; border: none;")
                         .arg(ink()));
    auto *s = new QLabel(item.subtitle, btn);
    s->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    s->setWordWrap(true);
    s->setStyleSheet(QStringLiteral(
                         "color: %1; font-size: 11px; font-weight: 400;"
                         "background: transparent; border: none;")
                         .arg(muted()));
    vl->addWidget(t);
    vl->addWidget(s);
    vl->addStretch(1);
    connect(btn, &QPushButton::clicked, this, item.action);
    grid->addWidget(btn, 0, col++);
  }
  for (int i = 0; i < 4; ++i)
    grid->setColumnStretch(i, 1);
  return wrapBlock(QStringLiteral("shortcuts"), body, 0, true);
}

QWidget *DashboardPage::buildActionsBlock() {
  auto *body = new QWidget();
  auto *lay = new QHBoxLayout(body);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(UiScale::dp(8));

  auto makeBtn = [&](const QString &text, auto slot) {
    auto *b = new QPushButton(text, body);
    b->setCursor(Qt::PointingHandCursor);
    b->setMinimumHeight(UiScale::dp(BlopStyle::touchTargetMinDp()));
    b->setStyleSheet(BlopStyle::paperSecondaryButtonQss());
    connect(b, &QPushButton::clicked, this, slot);
    lay->addWidget(b, 0);
  };
  makeBtn(QStringLiteral("Neue Notiz"),
          [this]() { emit newNoteRequested(); });
  makeBtn(QStringLiteral("Bibliothek durchsuchen"), [this]() {
    emit searchLibrary(QString());
    emit snapToNotesRequested();
  });
  makeBtn(QStringLiteral("Kalender maximieren"),
          [this]() { showCalendarMaximized(); });
  lay->addStretch(1);
  return wrapBlock(QStringLiteral("actions"), body);
}

QWidget *DashboardPage::buildContentFor(const QString &id, bool maximizedChrome) {
  if (id == QLatin1String("greeting"))
    return nullptr;
  if (id == QLatin1String("clock"))
    return buildClockBlock();
  if (id == QLatin1String("todos"))
    return buildTodosBlock();
  if (id == QLatin1String("calendar"))
    return buildCalendarBlock(maximizedChrome);
  if (id == QLatin1String("recent"))
    return buildRecentBlock();
  if (id == QLatin1String("shortcuts"))
    return buildShortcutsBlock();
  if (id == QLatin1String("actions"))
    return buildActionsBlock();
  return nullptr;
}

void DashboardPage::rebuildWidgets() {
  if (!m_gridLay)
    return;
  purgeFloatingHostWidgets();
  while (QLayoutItem *it = m_gridLay->takeAt(0)) {
    if (it->widget())
      delete it->widget();
    delete it;
  }

  // Clear old stretch/min so leftover rows don't keep expanding.
  for (int r = 0; r < 48; ++r) {
    m_gridLay->setRowMinimumHeight(r, 0);
    m_gridLay->setRowStretch(r, 0);
  }

  const auto specs = currentSpecs();
  int maxRow = 0;
  int visibleBlocks = 0;
  for (const auto &s : specs) {
    if (!s.visible || !isGridBlock(s.id))
      continue;
    QWidget *w = buildContentFor(s.id);
    if (!w)
      continue;
    ++visibleBlocks;
    const int rowSpan = qMax(s.rowSpan, minRowSpanForBlock(s.id));
    applyBlockCellSize(w, rowSpan);
    m_gridLay->addWidget(w, s.row, s.col, rowSpan, s.colSpan);
    maxRow = qMax(maxRow, s.row + rowSpan);
  }
  if (visibleBlocks == 0) {
    QWidget *empty = buildEmptyStatePanel();
    applyBlockCellSize(empty, 2);
    m_gridLay->addWidget(empty, 0, 0, 2, 12);
    maxRow = 2;
  }
  for (int r = 0; r < maxRow; ++r) {
    m_gridLay->setRowMinimumHeight(r, gridRowUnit());
    m_gridLay->setRowStretch(r, 0);
  }
  // In edit mode leave ~2 empty rows below so blocks can be placed further down.
  if (m_editMode) {
    auto *pad = new QWidget(m_host);
    pad->setObjectName(QStringLiteral("DashEditTailPad"));
    pad->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    pad->setFixedHeight(gridRowUnit() * 2 + m_gridLay->verticalSpacing());
    pad->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    pad->setStyleSheet(QStringLiteral("background: transparent;"));
    m_gridLay->addWidget(pad, maxRow, 0, 1, 12);
    ++maxRow;
  }
  // Trailing stretch row absorbs leftover viewport height (no content growth).
  m_gridLay->setRowStretch(maxRow, 1);
  if (m_snapOverlay) {
    m_snapOverlay->setGeometry(m_host->rect());
    m_snapOverlay->raise();
  }
}

void DashboardPage::snapGridFromPos(const QPoint &hostPos, int colSpan, int rowSpan,
                                    int &outRow, int &outCol) const {
  Q_UNUSED(rowSpan);
  if (!m_gridLay)
    return;
  // hostPos is the intended top-left of the block in host coordinates.
  outCol = snapColFromX(hostPos.x(), colSpan);
  outRow = snapRowFromY(hostPos.y());
  outCol = qBound(0, outCol, 12 - colSpan);
  outRow = qBound(0, outRow, qMax(0, maxSnapRow() + 2));
}

int DashboardPage::maxSnapRow() const {
  int maxRow = 0;
  for (const auto &s : currentSpecs()) {
    if (!s.visible || !isGridBlock(s.id))
      continue;
    maxRow = qMax(maxRow, s.row + s.rowSpan);
  }
  if (m_gridLay) {
    for (int i = 0; i < m_gridLay->count(); ++i) {
      int r = 0, c = 0, rs = 1, cs = 1;
      m_gridLay->getItemPosition(i, &r, &c, &rs, &cs);
      maxRow = qMax(maxRow, r + rs);
    }
  }
  return maxRow;
}

int DashboardPage::rowTopForSnap(int row) const {
  QVector<int> rowY;
  gridRowEdges(rowY);
  if (row >= 0 && row < rowY.size())
    return rowY[row];
  return 0;
}

QRect DashboardPage::snapHighlightRect(int row, int col, int colSpan,
                                       int rowSpan) const {
  if (!m_gridLay || !m_host)
    return {};

  // While still on the start cell, hug the live widget so preview matches 1:1.
  if (m_dragFrame && m_dragging && row == m_dragStartRow &&
      col == m_dragStartCol && colSpan == m_dragStartColSpan &&
      rowSpan == m_dragStartRowSpan)
    return m_dragFrame->geometry();

  QVector<int> colX;
  QVector<int> rowY;
  gridColumnEdges(colX);
  gridRowEdges(rowY);
  while (rowY.size() <= row + rowSpan) {
    const int vsp = m_gridLay->verticalSpacing();
    rowY.push_back(rowY.isEmpty() ? 0 : rowY.last() + gridRowUnit() + vsp);
  }
  if (colX.size() < 2 || rowY.size() < 2)
    return {};

  const int c0 = qBound(0, col, colX.size() - 2);
  const int c1 = qBound(c0 + 1, col + colSpan, colX.size() - 1);
  const int r0 = qBound(0, row, rowY.size() - 2);
  const int r1 = qBound(r0 + 1, row + rowSpan, rowY.size() - 1);
  return QRect(QPoint(colX[c0], rowY[r0]), QPoint(colX[c1], rowY[r1]));
}

void DashboardPage::updateSnapOverlay(int row, int col, int colSpan, int rowSpan) {
  if (!m_snapOverlay || !m_host)
    return;
  m_snapOverlay->setGeometry(m_host->rect());
  QVector<int> colX;
  QVector<int> rowY;
  gridColumnEdges(colX);
  gridRowEdges(rowY);
  m_snapOverlay->setGridLines(colX, rowY);
  m_snapOverlay->setHighlight(snapHighlightRect(row, col, colSpan, rowSpan));
  m_snapOverlay->raise();
}

void DashboardPage::clearSnapOverlay() {
  if (m_snapOverlay)
    m_snapOverlay->clearHighlight();
}

int DashboardPage::snapGridLineX(int hostX) const {
  QVector<int> colX;
  gridColumnEdges(colX);
  if (colX.isEmpty())
    return 0;
  int best = 0;
  int bestDist = INT_MAX;
  for (int i = 0; i < colX.size(); ++i) {
    const int dist = qAbs(hostX - colX[i]);
    if (dist < bestDist) {
      bestDist = dist;
      best = i;
    }
  }
  return best; // 0 = left of col 0, 12 = right of col 11
}

int DashboardPage::snapGridLineY(int hostY) const {
  QVector<int> rowY;
  gridRowEdges(rowY);
  if (rowY.isEmpty())
    return 0;
  int best = 0;
  int bestDist = INT_MAX;
  for (int i = 0; i < rowY.size(); ++i) {
    const int dist = qAbs(hostY - rowY[i]);
    if (dist < bestDist) {
      bestDist = dist;
      best = i;
    }
  }
  return best;
}

void DashboardPage::applyResizePreview(const QPoint &hostPos) {
  m_previewRow = m_dragStartRow;
  m_previewCol = m_dragStartCol;
  m_previewColSpan = m_dragStartColSpan;
  m_previewRowSpan = m_dragStartRowSpan;

  // Snap the dragged EDGE to grid lines (not cell centers / left tops),
  // otherwise east/south resizes often commit smaller than the preview.
  if (m_gesture == DashGesture::ResizeS) {
    const int bottom = snapGridLineY(hostPos.y());
    m_previewRowSpan =
        qBound(1, bottom - m_dragStartRow, 4);
  } else if (m_gesture == DashGesture::ResizeN) {
    const int bottom = m_dragStartRow + m_dragStartRowSpan;
    const int top = qMin(snapGridLineY(hostPos.y()), bottom - 1);
    m_previewRow = qMax(0, top);
    m_previewRowSpan = qBound(1, bottom - m_previewRow, 4);
  } else if (m_gesture == DashGesture::ResizeE) {
    const int right = snapGridLineX(hostPos.x());
    m_previewColSpan =
        qBound(1, right - m_dragStartCol, 12 - m_dragStartCol);
  } else if (m_gesture == DashGesture::ResizeW) {
    const int right = m_dragStartCol + m_dragStartColSpan;
    const int left = qMin(snapGridLineX(hostPos.x()), right - 1);
    m_previewCol = qMax(0, left);
    m_previewColSpan = qBound(1, right - m_previewCol, 12 - m_previewCol);
  }

  if (!m_snapOverlay)
    return;

  QVector<int> colX;
  QVector<int> rowY;
  gridColumnEdges(colX);
  gridRowEdges(rowY);
  const int vsp = m_gridLay ? m_gridLay->verticalSpacing() : UiScale::dp(16);
  while (rowY.size() <= m_previewRow + m_previewRowSpan)
    rowY.push_back(rowY.last() + gridRowUnit() + vsp);

  const int c0 = qBound(0, m_previewCol, colX.size() - 2);
  const int c1 = qBound(c0 + 1, m_previewCol + m_previewColSpan, colX.size() - 1);
  const int r0 = qBound(0, m_previewRow, rowY.size() - 2);
  const int r1 = qBound(r0 + 1, m_previewRow + m_previewRowSpan, rowY.size() - 1);
  const QRect target(colX[c0], rowY[r0], colX[c1] - colX[c0],
                     rowY[r1] - rowY[r0]);

  QRect rect = target;
  if (m_dragFrame) {
    const QRect frame = m_dragFrame->geometry();
    switch (m_gesture) {
    case DashGesture::ResizeE:
      rect = QRect(frame.left(), frame.top(),
                   qMax(frame.width(), target.right() - frame.left() + 1),
                   frame.height());
      break;
    case DashGesture::ResizeS:
      rect = QRect(frame.left(), frame.top(), frame.width(),
                   qMax(frame.height(), target.bottom() - frame.top() + 1));
      break;
    case DashGesture::ResizeN:
      rect = QRect(frame.left(), target.top(), frame.width(),
                   frame.bottom() - target.top() + 1);
      break;
    case DashGesture::ResizeW:
      rect = QRect(target.left(), frame.top(),
                   frame.right() - target.left() + 1, frame.height());
      break;
    default:
      break;
    }
  }

  m_snapOverlay->setGridLines(colX, rowY);
  m_snapOverlay->setHighlight(rect);
}

void DashboardPage::applyMovePreview(const QPoint &hostPos) {
  if (!m_dragFrame)
    return;
  // Keep the grabbed point under the cursor (delta from press).
  const QPoint topLeft = m_dragOriginHost + (hostPos - m_pressHostPos);
  if (m_floatActive)
    m_dragFrame->move(topLeft);
  snapGridFromPos(topLeft, m_dragColSpan, m_dragRowSpan, m_previewRow,
                  m_previewCol);
  updateSnapOverlay(m_previewRow, m_previewCol, m_dragColSpan, m_dragRowSpan);
  if (m_snapOverlay)
    m_snapOverlay->stackUnder(m_dragFrame);
  m_dragFrame->raise();
}

bool DashboardPage::eventFilter(QObject *watched, QEvent *event) {
  if (m_dragging) {
    switch (event->type()) {
    case QEvent::MouseMove:
    case QEvent::HoverMove:
    case QEvent::TouchUpdate: {
      QPoint global = globalPosFromEvent(event);
      if (global.isNull())
        global = QCursor::pos();
      if (m_host) {
        const QPoint hostPos = m_host->mapFromGlobal(global);
        if (m_gesture == DashGesture::Move)
          applyMovePreview(hostPos);
        else
          applyResizePreview(hostPos);
      }
      event->accept();
      return true;
    }
    case QEvent::MouseButtonRelease:
    case QEvent::TouchEnd:
    case QEvent::TouchCancel:
      if (event->type() == QEvent::MouseButtonRelease) {
        const auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() != Qt::LeftButton)
          return true;
      }
      handleGestureRelease();
      event->accept();
      return true;
    case QEvent::Wheel:
    case QEvent::Scroll:
    case QEvent::NativeGesture:
    case QEvent::Gesture:
    case QEvent::MouseButtonPress:
    case QEvent::TouchBegin:
      event->accept();
      return true;
    default:
      break;
    }
  }

  if (!m_editMode)
    return QWidget::eventFilter(watched, event);

  // Wheel/scroll are allowed in edit mode so the tail padding is reachable.
  // (Active drag already swallows wheel above.)

  auto *local = qobject_cast<QWidget *>(watched);
  const bool isHandle =
      local && local->objectName() == QLatin1String("DashResizeHandle");
  const bool isGrip =
      local && local->objectName() == QLatin1String("DashDragGrip");

  QFrame *frame = nullptr;
  if (isHandle)
    frame = qobject_cast<QFrame *>(local->parentWidget());
  else if (isGrip)
    frame = qobject_cast<QFrame *>(local->parentWidget()->parentWidget());
  else
    frame = qobject_cast<QFrame *>(watched);

  if (frame && frame->objectName() == QLatin1String("DashBlock") &&
      event->type() == QEvent::Resize) {
    layoutResizeHandles(frame);
    return QWidget::eventFilter(watched, event);
  }

  if (!frame || !frame->property("dashBlockId").isValid())
    return QWidget::eventFilter(watched, event);

  const QString blockId = frame->property("dashBlockId").toString();
  const QString resizeEdge =
      isHandle ? local->property("resizeEdge").toString() : QString();

  if (isPointerPress(event)) {
    if (event->type() == QEvent::MouseButtonPress) {
      auto *me = static_cast<QMouseEvent *>(event);
      if (me->button() != Qt::LeftButton)
        return QWidget::eventFilter(watched, event);
    }
    const QWidget *target = isHandle ? local : (isGrip ? local : frame);
    const auto pos = pointerPosInWidget(target, event);
    if (!pos)
      return QWidget::eventFilter(watched, event);

    if (isHandle) {
      DashGesture g = DashGesture::None;
      if (resizeEdge == QLatin1String("N"))
        g = DashGesture::ResizeN;
      else if (resizeEdge == QLatin1String("E"))
        g = DashGesture::ResizeE;
      else if (resizeEdge == QLatin1String("S"))
        g = DashGesture::ResizeS;
      else if (resizeEdge == QLatin1String("W"))
        g = DashGesture::ResizeW;
      if (g == DashGesture::None)
        return QWidget::eventFilter(watched, event);
      m_dragOffset = *pos;
      beginGesture(frame, blockId, g);
      event->accept();
      return true;
    }

    if (isGrip) {
      m_pressHostPos = m_host->mapFromGlobal(globalPosFromEvent(event));
      m_havePressHostPos = true;
      beginGesture(frame, blockId, DashGesture::Move);
      event->accept();
      return true;
    }

    QWidget *grip = frame->findChild<QWidget *>(QStringLiteral("DashDragGrip"));
    if (!grip || !grip->geometry().contains(*pos))
      return QWidget::eventFilter(watched, event);

    m_pressHostPos = m_host->mapFromGlobal(globalPosFromEvent(event));
    m_havePressHostPos = true;
    beginGesture(frame, blockId, DashGesture::Move);
    event->accept();
    return true;
  }

  return QWidget::eventFilter(watched, event);
}

void DashboardPage::showCalendarMaximized() {
  if (m_calMaxDlg) {
    m_calMaxDlg->raise();
    return;
  }
  auto *dlg = new QDialog(window());
  m_calMaxDlg = dlg;
  dlg->setWindowTitle(QStringLiteral("Kalender"));
  dlg->setMinimumSize(UiScale::dp(560), UiScale::dp(480));
  auto *lay = new QVBoxLayout(dlg);
  lay->setContentsMargins(UiScale::dp(12), UiScale::dp(12), UiScale::dp(12),
                          UiScale::dp(12));
  lay->addWidget(buildCalendarBlock(true), 1);
  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, dlg);
  connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
  lay->addWidget(buttons);
  connect(dlg, &QDialog::finished, this, [this]() { m_calMaxDlg = nullptr; });
  BlopModal::execBlocking(window(), dlg, BlopModal::Mode::Card,
                          UiScale::dp(680));
  m_calMaxDlg = nullptr;
}

void DashboardPage::openCreateEventDialog(const QDateTime &presetStart) {
  QDialog dlg(window());
  dlg.setWindowTitle(QStringLiteral("Neuer Termin"));
  auto *form = new QFormLayout(&dlg);
  auto *title = new QLineEdit(&dlg);
  title->setPlaceholderText(QStringLiteral("Titel"));
  const QDateTime startDt =
      presetStart.isValid() ? presetStart : QDateTime::currentDateTime();
  auto *start = new QDateTimeEdit(startDt, &dlg);
  start->setCalendarPopup(true);
  auto *end = new QDateTimeEdit(startDt.addSecs(3600), &dlg);
  end->setCalendarPopup(true);
  form->addRow(QStringLiteral("Titel"), title);
  form->addRow(QStringLiteral("Start"), start);
  form->addRow(QStringLiteral("Ende"), end);
  auto *buttons =
      new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  form->addRow(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  if (BlopModal::execBlocking(window(), &dlg, BlopModal::Mode::Card,
                              UiScale::dp(420)) != QDialog::Accepted)
    return;
  CalendarService::instance().createEvent(title->text(), start->dateTime(),
                                          end->dateTime());
  refresh();
}
