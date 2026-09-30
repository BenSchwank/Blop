#include "dashrightrail.h"

#include "blop_dialogs.h"
#include "blop_inwindow_menu.h"
#include "blop_scroll.h"
#include "blop_theme.h"
#include "bloplocale.h"
#include "blopstyle.h"
#include "calendarservice.h"
#include "libraryorgstore.h"
#include "overlayscrollindicator.h"
#include "todostore.h"
#include "uiscale.h"
#include "weatherservice.h"

#include <QCheckBox>
#include <QDate>
#include <QDateTime>
#include <QFileInfo>
#include <QFrame>
#include <QHash>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPaintEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QStyle>
#include <QStyleFactory>
#include <QTimer>
#include <QTime>
#include <QVBoxLayout>
#include <functional>

namespace {
constexpr const char *kOrderKey = "dashboard/rightRail.moduleOrder";
constexpr const char *kHiddenKey = "dashboard/rightRail.hiddenModules";
constexpr int kRailWidthDp = 188;

enum class SegStyle { Plain, Soft, Callout };

QColor railFill() {
  if (BlopTheme::instance().isDark())
    return QColor(0x27, 0x29, 0x2E);
  return BlopStyle::paperBg();
}
QString railInk() {
  return BlopTheme::instance().isDark()
             ? QColor(255, 255, 255, 230).name(QColor::HexArgb)
             : BlopStyle::paperInk().name(QColor::HexRgb);
}
QString railMuted() {
  return BlopTheme::instance().isDark()
             ? QColor(255, 255, 255, 120).name(QColor::HexArgb)
             : QColor(55, 53, 47, 140).name(QColor::HexArgb);
}
QString hairline() {
  return BlopTheme::instance().isDark()
             ? QStringLiteral("rgba(255,255,255,0.08)")
             : QStringLiteral("rgba(55,53,47,0.10)");
}

SegStyle styleForModule(const QString &id) {
  if (id == QLatin1String("weather") || id == QLatin1String("focus"))
    return SegStyle::Callout;
  if (id == QLatin1String("clock") || id == QLatin1String("shortcuts") ||
      id == QLatin1String("favorites") || id == QLatin1String("recent"))
    return SegStyle::Plain;
  return SegStyle::Soft;
}

QString calloutWash(const QString &id) {
  if (id == QLatin1String("weather")) {
    return BlopTheme::instance().isDark()
               ? QStringLiteral("rgba(91,157,255,0.14)")
               : QStringLiteral("rgba(91,157,255,0.11)");
  }
  if (id == QLatin1String("focus")) {
    return BlopTheme::instance().isDark()
               ? QStringLiteral("rgba(52,211,153,0.14)")
               : QStringLiteral("rgba(16,185,129,0.10)");
  }
  return BlopTheme::instance().isDark()
             ? QStringLiteral("rgba(255,255,255,0.04)")
             : QStringLiteral("rgba(15,23,42,0.03)");
}

QString softWash() {
  return BlopTheme::instance().isDark()
             ? QStringLiteral("rgba(255,255,255,0.04)")
             : QStringLiteral("rgba(55,53,47,0.04)");
}

QString fieldBorder() {
  return BlopTheme::instance().isDark()
             ? QStringLiteral("1px solid rgba(255,255,255,0.10)")
             : QStringLiteral("1px solid rgba(55,53,47,0.12)");
}

QString linkBtnQss() {
  return QStringLiteral(
             "QPushButton { color: %1; font-size: 12px; font-weight: 500;"
             "  background: transparent; border: none; text-align: left;"
             "  padding: 2px 0; }"
             "QPushButton:hover { color: %2; }")
      .arg(railMuted(), BlopTheme::accentPrimary().name(QColor::HexRgb));
}

QString rowBtnQss() {
  const QString hover = BlopTheme::instance().isDark()
                            ? QStringLiteral("rgba(255,255,255,0.06)")
                            : BlopStyle::paperHover().name(QColor::HexRgb);
  return QStringLiteral(
             "QPushButton {"
             "  text-align: left; padding: 5px 4px;"
             "  color: %1; font-size: 13px; font-weight: 450;"
             "  background: transparent; border: none;"
             "  border-radius: %3px;"
             "}"
             "QPushButton:hover { background: %2; }")
      .arg(railInk(), hover, QString::number(UiScale::dp(6)));
}

QString todoCheckQss() {
  const int s = UiScale::dp(16);
  const int r = UiScale::dp(5);
  return QStringLiteral(
             "QCheckBox { color: %1; font-size: 12px; spacing: 8px;"
             "  background: transparent; }"
             "QCheckBox::indicator { width: %2px; height: %2px; }"
             "QCheckBox::indicator:unchecked {"
             "  border: 1.5px solid %3; border-radius: %4px;"
             "  background: transparent; }"
             "QCheckBox::indicator:checked {"
             "  border: 1.5px solid %5; border-radius: %4px;"
             "  background: %5; }")
      .arg(railInk(), QString::number(s), railMuted(), QString::number(r),
           BlopTheme::accentPrimary().name(QColor::HexRgb));
}
} // namespace

QStringList DashRightRail::knownModules() {
  return {QStringLiteral("weather"),   QStringLiteral("clock"),
          QStringLiteral("nextup"),    QStringLiteral("focus"),
          QStringLiteral("capture"),   QStringLiteral("shortcuts"),
          QStringLiteral("search"),    QStringLiteral("favorites"),
          QStringLiteral("recent"),    QStringLiteral("todos")};
}

QString DashRightRail::moduleTitle(const QString &id) {
  static const QHash<QString, QString> names = {
      {QStringLiteral("weather"), QStringLiteral("Wetter")},
      {QStringLiteral("clock"), QStringLiteral("Uhr")},
      {QStringLiteral("nextup"), QStringLiteral("Als Nächstes")},
      {QStringLiteral("focus"), QStringLiteral("Fokus")},
      {QStringLiteral("capture"), QStringLiteral("Schnellnotiz")},
      {QStringLiteral("shortcuts"), QStringLiteral("Shortcuts")},
      {QStringLiteral("search"), QStringLiteral("Suche")},
      {QStringLiteral("favorites"), QStringLiteral("Favoriten")},
      {QStringLiteral("recent"), QStringLiteral("Zuletzt")},
      {QStringLiteral("todos"), QStringLiteral("Aufgaben")},
  };
  return names.value(id, id);
}

DashRightRail::DashRightRail(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("DashRightRail"));
  setAttribute(Qt::WA_StyledBackground, true);
  setFixedWidth(UiScale::dp(kRailWidthDp));
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(UiScale::dp(14), UiScale::dp(12), UiScale::dp(12),
                           UiScale::dp(14));
  root->setSpacing(UiScale::dp(4));

  m_chrome = new QWidget(this);
  m_chrome->setObjectName(QStringLiteral("DashRailChrome"));
  m_chrome->setStyleSheet(QStringLiteral("background: transparent;"));
  auto *chromeLay = new QHBoxLayout(m_chrome);
  chromeLay->setContentsMargins(0, 0, 0, 0);
  chromeLay->setSpacing(UiScale::dp(4));

  m_btnCustomize = new QPushButton(QStringLiteral("Anpassen"), m_chrome);
  m_btnCustomize->setCursor(Qt::PointingHandCursor);
  m_btnCustomize->setFlat(true);
  m_btnCustomize->setToolTip(QStringLiteral("Dashboard anpassen"));
  connect(m_btnCustomize, &QPushButton::clicked, this,
          &DashRightRail::customizeClicked);
  chromeLay->addWidget(m_btnCustomize, 1);

  m_btnUndo = new QPushButton(QStringLiteral("↶"), m_chrome);
  m_btnUndo->setCursor(Qt::PointingHandCursor);
  m_btnUndo->setFlat(true);
  m_btnUndo->setFixedSize(UiScale::dp(28), UiScale::dp(28));
  m_btnUndo->setVisible(false);
  m_btnUndo->setEnabled(false);
  m_btnUndo->setToolTip(QStringLiteral("Rückgängig"));
  connect(m_btnUndo, &QPushButton::clicked, this, &DashRightRail::undoClicked);
  chromeLay->addWidget(m_btnUndo, 0);

  m_btnMore = new QPushButton(QStringLiteral("⋯"), m_chrome);
  m_btnMore->setCursor(Qt::PointingHandCursor);
  m_btnMore->setFlat(true);
  m_btnMore->setFixedSize(UiScale::dp(28), UiScale::dp(28));
  m_btnMore->setVisible(false);
  m_btnMore->setToolTip(QStringLiteral("Blöcke & Layout"));
  connect(m_btnMore, &QPushButton::clicked, this, &DashRightRail::moreClicked);
  chromeLay->addWidget(m_btnMore, 0);
  root->addWidget(m_chrome, 0);

  m_bodyScroll = new QScrollArea(this);
  m_bodyScroll->setObjectName(QStringLiteral("DashRailBodyScroll"));
  m_bodyScroll->setWidgetResizable(true);
  m_bodyScroll->setFrameShape(QFrame::NoFrame);
  m_bodyScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_bodyScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_bodyScroll->setStyleSheet(QStringLiteral(
      "QScrollArea#DashRailBodyScroll { background: transparent; border: none; }"
      "QScrollArea#DashRailBodyScroll > QWidget > QWidget { background: transparent; }"));
  if (m_bodyScroll->viewport()) {
    m_bodyScroll->viewport()->setAutoFillBackground(false);
    m_bodyScroll->viewport()->setStyleSheet(
        QStringLiteral("background: transparent;"));
  }
  OverlayScrollIndicator::install(m_bodyScroll);
  BlopScroll::enableFingerScroll(m_bodyScroll, BlopScroll::Axes::VerticalOnly);

  m_body = new QWidget;
  m_body->setObjectName(QStringLiteral("DashRailBody"));
  m_body->setStyleSheet(QStringLiteral("background: transparent;"));
  m_bodyLay = new QVBoxLayout(m_body);
  m_bodyLay->setContentsMargins(0, UiScale::dp(4), 0, UiScale::dp(8));
  m_bodyLay->setSpacing(UiScale::dp(6));
  m_bodyLay->addStretch(1);
  m_bodyScroll->setWidget(m_body);
  root->addWidget(m_bodyScroll, 1);

  m_btnAdd = new QPushButton(QStringLiteral("+ Widget"), this);
  m_btnAdd->setCursor(Qt::PointingHandCursor);
  m_btnAdd->setFlat(true);
  m_btnAdd->setMinimumHeight(UiScale::dp(34));
  m_btnAdd->setToolTip(QStringLiteral("Seitenleisten-Widgets ein-/ausblenden"));
  connect(m_btnAdd, &QPushButton::clicked, this, &DashRightRail::showRailMenu);
  root->addWidget(m_btnAdd, 0);

  m_clockTimer = new QTimer(this);
  m_clockTimer->setInterval(30000);
  connect(m_clockTimer, &QTimer::timeout, this, [this]() {
    if (isModuleVisible(QStringLiteral("clock")) ||
        isModuleVisible(QStringLiteral("nextup")))
      rebuildModules();
  });
  m_clockTimer->start();

  loadModulePrefs();

  connect(&WeatherService::instance(), &WeatherService::weatherChanged, this,
          &DashRightRail::rebuildModules);
  connect(&WeatherService::instance(), &WeatherService::searchFailed, this,
          [this](const QString &msg) {
            BlopDialogs::notify(this, QStringLiteral("Wetter"), msg);
          });
  connect(&CalendarService::instance(), &CalendarService::eventsChanged, this,
          &DashRightRail::rebuildModules);
  connect(&BlopTheme::instance(), &BlopTheme::themeChanged, this, [this]() {
    applyChrome();
    rebuildModules();
  });

  applyChrome();
  rebuildModules();
  WeatherService::instance().refresh(false);
}

void DashRightRail::setEditMode(bool on) {
  m_editMode = on;
  applyChrome();
  rebuildModules();
}

void DashRightRail::setUndoAvailable(bool on, int stackDepth) {
  if (!m_btnUndo)
    return;
  m_btnUndo->setVisible(m_editMode);
  m_btnUndo->setEnabled(on && m_editMode);
  m_btnUndo->setToolTip(on ? QStringLiteral("Rückgängig (%1)").arg(stackDepth)
                           : QStringLiteral("Rückgängig"));
}

QWidget *DashRightRail::overflowAnchor() const { return m_btnMore; }

void DashRightRail::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);
  QPainter p(this);
  p.fillRect(rect(), railFill());
  p.fillRect(0, UiScale::dp(12), 1, qMax(0, height() - UiScale::dp(24)),
             QColor(55, 53, 47, BlopTheme::instance().isDark() ? 40 : 28));
}

void DashRightRail::refresh() { rebuildModules(); }

void DashRightRail::setBoardOccupiedIds(const QStringList &ids) {
  if (m_boardOccupied == ids)
    return;
  m_boardOccupied = ids;
  rebuildModules();
}

bool DashRightRail::isSuppressedByBoard(const QString &id) const {
  if (id != QLatin1String("recent") && id != QLatin1String("todos"))
    return false;
  return m_boardOccupied.contains(id);
}

bool DashRightRail::isModuleVisible(const QString &id) const {
  return !m_hiddenModules.contains(id) && !isSuppressedByBoard(id) &&
         m_moduleOrder.contains(id);
}

void DashRightRail::loadModulePrefs() {
  QSettings st(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  m_moduleOrder = st.value(QLatin1String(kOrderKey)).toStringList();
  m_hiddenModules = st.value(QLatin1String(kHiddenKey)).toStringList();
  st.remove(QStringLiteral("dashboard/rightRail.collapsed"));

  const QStringList known = knownModules();
  constexpr const char *kModulesV4 = "dashboard/rightRail.modulesV4";
  if (!st.value(QLatin1String(kModulesV4)).toBool()) {
    st.setValue(QLatin1String(kModulesV4), true);
    // Expand defaults: useful rail tools, keep board-duplicated lists hidden.
    m_moduleOrder = {QStringLiteral("weather"), QStringLiteral("clock"),
                     QStringLiteral("nextup"), QStringLiteral("focus"),
                     QStringLiteral("capture"), QStringLiteral("shortcuts")};
    m_hiddenModules = {QStringLiteral("search"), QStringLiteral("favorites"),
                       QStringLiteral("recent"), QStringLiteral("todos")};
    saveModulePrefs();
  }

  if (m_moduleOrder.isEmpty()) {
    m_moduleOrder = {QStringLiteral("weather"), QStringLiteral("clock"),
                     QStringLiteral("nextup")};
  }
  for (const QString &id : known) {
    if (!m_moduleOrder.contains(id) && !m_hiddenModules.contains(id))
      m_hiddenModules.append(id);
  }
  for (const QString &id : known) {
    if (!m_moduleOrder.contains(id))
      m_moduleOrder.append(id);
  }

  constexpr const char *kDedupeKey = "dashboard/rightRail.dedupeBoardV4";
  if (!st.value(QLatin1String(kDedupeKey)).toBool()) {
    st.setValue(QLatin1String(kDedupeKey), true);
    if (!m_hiddenModules.contains(QStringLiteral("recent")))
      m_hiddenModules.append(QStringLiteral("recent"));
    if (!m_hiddenModules.contains(QStringLiteral("todos")))
      m_hiddenModules.append(QStringLiteral("todos"));
    saveModulePrefs();
  }
}

void DashRightRail::saveModulePrefs() const {
  QSettings st(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  st.setValue(QLatin1String(kOrderKey), m_moduleOrder);
  st.setValue(QLatin1String(kHiddenKey), m_hiddenModules);
}

void DashRightRail::hideModule(const QString &id) {
  if (!m_hiddenModules.contains(id))
    m_hiddenModules.append(id);
  saveModulePrefs();
  rebuildModules();
}

void DashRightRail::showModule(const QString &id) {
  m_hiddenModules.removeAll(id);
  if (!m_moduleOrder.contains(id))
    m_moduleOrder.append(id);
  saveModulePrefs();
  rebuildModules();
}

void DashRightRail::applyChrome() {
  if (!m_btnAdd)
    return;
  setStyleSheet(QStringLiteral(
      "QWidget#DashRightRail { background: %1; border: none; }")
                    .arg(railFill().name(QColor::HexRgb)));

  const QString acc = BlopTheme::accentPrimary().name(QColor::HexRgb);
  const int rad = UiScale::dp(BlopStyle::radiusMdDp());

  if (m_btnCustomize) {
    m_btnCustomize->setText(m_editMode ? QStringLiteral("Fertig")
                                       : QStringLiteral("Anpassen"));
    if (m_editMode) {
      m_btnCustomize->setStyleSheet(
          QStringLiteral("QPushButton {"
                         "  color: %1; font-size: 13px; font-weight: 600;"
                         "  background: rgba(%2,%3,%4,0.16); border: none;"
                         "  border-radius: %5px; padding: 6px 10px;"
                         "  text-align: left;"
                         "}"
                         "QPushButton:hover { background: rgba(%2,%3,%4,0.24); }")
              .arg(acc)
              .arg(BlopTheme::accentPrimary().red())
              .arg(BlopTheme::accentPrimary().green())
              .arg(BlopTheme::accentPrimary().blue())
              .arg(rad));
    } else {
      m_btnCustomize->setStyleSheet(
          QStringLiteral("QPushButton {"
                         "  color: %1; font-size: 13px; font-weight: 500;"
                         "  background: transparent; border: none;"
                         "  border-radius: %2px; padding: 4px 2px;"
                         "  text-align: left;"
                         "}"
                         "QPushButton:hover { color: %3; }")
              .arg(railMuted(), QString::number(rad), acc));
    }
  }

  const QString ghostIcon =
      QStringLiteral("QPushButton {"
                     "  color: %1; font-size: 16px; font-weight: 500;"
                     "  background: transparent; border: none;"
                     "  border-radius: %2px; padding: 0;"
                     "}"
                     "QPushButton:hover { color: %3; background: %4; }"
                     "QPushButton:disabled { color: %5; }")
          .arg(railMuted(), QString::number(rad), acc,
               BlopTheme::instance().isDark()
                   ? QStringLiteral("rgba(255,255,255,0.06)")
                   : QStringLiteral("rgba(15,23,42,0.05)"),
               BlopTheme::instance().isDark()
                   ? QStringLiteral("rgba(255,255,255,0.22)")
                   : QStringLiteral("rgba(55,53,47,0.28)"));
  if (m_btnUndo) {
    m_btnUndo->setStyleSheet(ghostIcon);
    m_btnUndo->setVisible(m_editMode);
  }
  if (m_btnMore) {
    m_btnMore->setStyleSheet(ghostIcon);
    m_btnMore->setVisible(m_editMode);
  }

  m_btnAdd->setVisible(m_editMode);
  m_btnAdd->setEnabled(m_editMode);
  m_btnAdd->setText(QStringLiteral("+ Widget"));
  m_btnAdd->setStyleSheet(
      QStringLiteral("QPushButton {"
                     "  color: %1; font-size: 13px; font-weight: 500;"
                     "  background: transparent; border: none;"
                     "  border-radius: %2px; padding: 10px 2px; text-align: left;"
                     "}"
                     "QPushButton:hover { color: %3; }")
          .arg(railMuted(), QString::number(rad), acc));
}

void DashRightRail::rebuildModules() {
  if (!m_bodyLay)
    return;
  while (QLayoutItem *it = m_bodyLay->takeAt(0)) {
    if (it->widget())
      delete it->widget();
    delete it;
  }

  for (const QString &id : m_moduleOrder) {
    if (m_hiddenModules.contains(id))
      continue;
    if (isSuppressedByBoard(id))
      continue;

    QWidget *content = nullptr;
    if (id == QLatin1String("weather"))
      content = buildWeatherContent();
    else if (id == QLatin1String("clock"))
      content = buildClockContent();
    else if (id == QLatin1String("nextup"))
      content = buildNextUpContent();
    else if (id == QLatin1String("focus"))
      content = buildFocusContent();
    else if (id == QLatin1String("capture"))
      content = buildCaptureContent();
    else if (id == QLatin1String("shortcuts"))
      content = buildShortcutsContent();
    else if (id == QLatin1String("search"))
      content = buildSearchContent();
    else if (id == QLatin1String("favorites"))
      content = buildFavoritesContent();
    else if (id == QLatin1String("recent"))
      content = buildRecentContent();
    else if (id == QLatin1String("todos"))
      content = buildTodosContent();

    if (content)
      m_bodyLay->addWidget(makeSegment(id, moduleTitle(id), content), 0);
  }
  m_bodyLay->addStretch(1);
  applyChrome();
}

QWidget *DashRightRail::makeSegment(const QString &id, const QString &title,
                                    QWidget *content) {
  auto *seg = new QFrame(m_body);
  seg->setObjectName(QStringLiteral("DashRailSegment"));
  seg->setAttribute(Qt::WA_StyledBackground, true);
  const int rad = UiScale::dp(10);
  const SegStyle style = styleForModule(id);

  QString bg = QStringLiteral("transparent");
  QString border = QStringLiteral("none");
  int padX = UiScale::dp(2);
  int padY = UiScale::dp(10);
  if (style == SegStyle::Callout) {
    bg = calloutWash(id);
    border = QStringLiteral("1px solid %1").arg(hairline());
    padX = UiScale::dp(12);
    padY = UiScale::dp(12);
  } else if (style == SegStyle::Soft) {
    bg = softWash();
    padX = UiScale::dp(10);
    padY = UiScale::dp(10);
  }

  seg->setStyleSheet(
      QStringLiteral("QFrame#DashRailSegment {"
                     "  background: %1;"
                     "  border: %2;"
                     "  border-radius: %3px;"
                     "}")
          .arg(bg, border, QString::number(rad)));

  auto *lay = new QVBoxLayout(seg);
  lay->setContentsMargins(padX, padY, padX, padY);
  lay->setSpacing(UiScale::dp(5));

  auto *hdr = new QWidget(seg);
  auto *hdrLay = new QHBoxLayout(hdr);
  hdrLay->setContentsMargins(0, 0, 0, 0);
  hdrLay->setSpacing(UiScale::dp(4));

  auto *cap = new QLabel(title, hdr);
  cap->setStyleSheet(
      QStringLiteral("color: %1; font-size: 11px; font-weight: 500;"
                     "letter-spacing: 0.3px; background: transparent;")
          .arg(railMuted()));
  hdrLay->addWidget(cap, 1);

  auto *dismiss = new QPushButton(QStringLiteral("×"), hdr);
  dismiss->setCursor(Qt::PointingHandCursor);
  dismiss->setFlat(true);
  dismiss->setFixedSize(UiScale::dp(22), UiScale::dp(22));
  dismiss->setToolTip(QStringLiteral("Ausblenden"));
  dismiss->setVisible(m_editMode);
  dismiss->setStyleSheet(
      QStringLiteral("QPushButton {"
                     "  color: %1; background: transparent; border: none;"
                     "  font-size: 14px; padding: 0;"
                     "}"
                     "QPushButton:hover { color: %2; }")
          .arg(railMuted(), railInk()));
  connect(dismiss, &QPushButton::clicked, this,
          [this, id]() { hideModule(id); });
  hdrLay->addWidget(dismiss, 0, Qt::AlignTop);
  lay->addWidget(hdr, 0);

  content->setParent(seg);
  lay->addWidget(content, 0);

  if (style == SegStyle::Plain) {
    auto *rule = new QFrame(seg);
    rule->setFixedHeight(1);
    rule->setStyleSheet(
        QStringLiteral("background: %1; border: none;").arg(hairline()));
    lay->addSpacing(UiScale::dp(6));
    lay->addWidget(rule);
  }
  return seg;
}

QWidget *DashRightRail::buildWeatherContent() {
  auto *box = new QWidget();
  auto *lay = new QVBoxLayout(box);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(UiScale::dp(2));

  const auto snap = WeatherService::instance().current();
  if (!WeatherService::instance().hasLocation()) {
    auto *setOrt = new QPushButton(QStringLiteral("Ort festlegen"), box);
    setOrt->setCursor(Qt::PointingHandCursor);
    setOrt->setFlat(true);
    setOrt->setStyleSheet(
        QStringLiteral("QPushButton { color: %1; font-size: 13px; font-weight: 550;"
                       "  background: transparent; border: none; text-align: left; }"
                       "QPushButton:hover { color: %2; }")
            .arg(BlopTheme::accentPrimary().name(QColor::HexRgb),
                 BlopTheme::accentHover().name(QColor::HexRgb)));
    connect(setOrt, &QPushButton::clicked, this,
            &DashRightRail::promptWeatherPlace);
    lay->addWidget(setOrt, 0, Qt::AlignLeft);
    return box;
  }

  if (snap.valid) {
    auto *temp = new QPushButton(
        QStringLiteral("%1°").arg(QString::number(snap.tempC, 'f', 0)), box);
    temp->setFlat(true);
    temp->setCursor(Qt::PointingHandCursor);
    temp->setToolTip(QStringLiteral("Aktualisieren"));
    temp->setStyleSheet(
        QStringLiteral("QPushButton { color: %1; font-size: 32px; font-weight: 700;"
                       "  letter-spacing: -1.2px; background: transparent;"
                       "  border: none; text-align: left; padding: 0; }"
                       "QPushButton:hover { color: %2; }")
            .arg(railInk(), BlopTheme::accentPrimary().name(QColor::HexRgb)));
    connect(temp, &QPushButton::clicked, this,
            []() { WeatherService::instance().refresh(true); });
    lay->addWidget(temp, 0, Qt::AlignLeft);

    auto *sum = new QLabel(snap.summary, box);
    sum->setWordWrap(true);
    sum->setStyleSheet(
        QStringLiteral("color: %1; font-size: 13px; font-weight: 500;"
                       "background: transparent;")
            .arg(railInk()));
    lay->addWidget(sum);

    auto *place = new QLabel(snap.placeLabel, box);
    place->setWordWrap(true);
    place->setStyleSheet(
        QStringLiteral("color: %1; font-size: 11px; background: transparent;")
            .arg(railMuted()));
    lay->addWidget(place);
  } else {
    auto *err = new QLabel(
        snap.error.isEmpty() ? QStringLiteral("…") : snap.error, box);
    err->setWordWrap(true);
    err->setStyleSheet(
        QStringLiteral("color: %1; font-size: 12px; background: transparent;")
            .arg(railMuted()));
    lay->addWidget(err);
  }

  auto *setOrt = new QPushButton(QStringLiteral("Ort ändern"), box);
  setOrt->setCursor(Qt::PointingHandCursor);
  setOrt->setFlat(true);
  setOrt->setStyleSheet(linkBtnQss());
  connect(setOrt, &QPushButton::clicked, this,
          &DashRightRail::promptWeatherPlace);
  lay->addWidget(setOrt, 0, Qt::AlignLeft);
  return box;
}

QWidget *DashRightRail::buildClockContent() {
  auto *box = new QWidget();
  auto *lay = new QVBoxLayout(box);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(UiScale::dp(1));

  const QTime now = QTime::currentTime();
  auto *time = new QLabel(now.toString(QStringLiteral("HH:mm")), box);
  time->setStyleSheet(
      QStringLiteral("color: %1; font-size: 34px; font-weight: 700;"
                     "letter-spacing: -1.4px; background: transparent;")
          .arg(railInk()));
  lay->addWidget(time);

  auto *date = new QLabel(
      BlopLocale::instance().formatDate(QDate::currentDate(),
                                        QStringLiteral("dddd, d. MMM")),
      box);
  date->setWordWrap(true);
  date->setStyleSheet(
      QStringLiteral("color: %1; font-size: 12px; font-weight: 450;"
                     "background: transparent;")
          .arg(railMuted()));
  lay->addWidget(date);
  return box;
}

QWidget *DashRightRail::buildNextUpContent() {
  auto *box = new QWidget();
  auto *lay = new QVBoxLayout(box);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(UiScale::dp(8));

  const auto upcoming = CalendarService::instance().upcoming(3);
  if (upcoming.isEmpty()) {
    auto *empty = new QLabel(QStringLiteral("Keine Termine"), box);
    empty->setStyleSheet(
        QStringLiteral("color: %1; font-size: 13px; background: transparent;")
            .arg(railMuted()));
    lay->addWidget(empty);
  } else {
    const QString acc = BlopTheme::accentPrimary().name(QColor::HexRgb);
    for (const auto &e : upcoming) {
      auto *row = new QWidget(box);
      auto *rowLay = new QHBoxLayout(row);
      rowLay->setContentsMargins(0, 0, 0, 0);
      rowLay->setSpacing(UiScale::dp(8));

      auto *bar = new QFrame(row);
      bar->setFixedWidth(UiScale::dp(3));
      bar->setMinimumHeight(UiScale::dp(28));
      bar->setStyleSheet(
          QStringLiteral("background: %1; border: none; border-radius: 2px;")
              .arg(acc));
      rowLay->addWidget(bar, 0);

      auto *col = new QWidget(row);
      auto *colLay = new QVBoxLayout(col);
      colLay->setContentsMargins(0, 0, 0, 0);
      colLay->setSpacing(UiScale::dp(1));

      auto *title = new QLabel(
          e.title.isEmpty() ? QStringLiteral("Termin") : e.title, col);
      title->setWordWrap(true);
      title->setStyleSheet(
          QStringLiteral("color: %1; font-size: 13px; font-weight: 550;"
                         "background: transparent;")
              .arg(railInk()));
      colLay->addWidget(title);

      const QString when =
          e.allDay ? BlopLocale::instance().formatDate(e.start.date(),
                                                       QStringLiteral("d. MMM"))
                   : e.start.toString(QStringLiteral("ddd · HH:mm"));
      auto *meta = new QLabel(when, col);
      meta->setStyleSheet(
          QStringLiteral("color: %1; font-size: 11px; background: transparent;")
              .arg(railMuted()));
      colLay->addWidget(meta);
      rowLay->addWidget(col, 1);
      lay->addWidget(row);
    }
  }

  auto *open = new QPushButton(QStringLiteral("Kalender öffnen"), box);
  open->setCursor(Qt::PointingHandCursor);
  open->setFlat(true);
  open->setStyleSheet(linkBtnQss());
  connect(open, &QPushButton::clicked, this,
          &DashRightRail::openCalendarRequested);
  lay->addWidget(open, 0, Qt::AlignLeft);
  return box;
}

QWidget *DashRightRail::buildShortcutsContent() {
  auto *box = new QWidget();
  auto *lay = new QVBoxLayout(box);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(UiScale::dp(2));

  struct Item {
    const char *label;
    std::function<void()> fire;
  };
  const QList<Item> items = {
      {"Neue Notiz", [this]() { emit newNoteRequested(); }},
      {"Bibliothek", [this]() { emit snapToNotesRequested(); }},
      {"Lernen", [this]() { emit studyRequested(); }},
      {"Kalender", [this]() { emit openCalendarRequested(); }},
  };
  for (const auto &it : items) {
    auto *btn = new QPushButton(QString::fromUtf8(it.label), box);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFlat(true);
    btn->setStyleSheet(rowBtnQss());
    connect(btn, &QPushButton::clicked, this, it.fire);
    lay->addWidget(btn);
  }
  return box;
}

QWidget *DashRightRail::buildSearchContent() {
  auto *box = new QWidget();
  auto *lay = new QVBoxLayout(box);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(UiScale::dp(6));

  auto *edit = new QLineEdit(box);
  edit->setPlaceholderText(QStringLiteral("Notizen suchen…"));
  edit->setClearButtonEnabled(true);
  const int rad = UiScale::dp(BlopStyle::radiusMdDp());
  edit->setStyleSheet(
      QStringLiteral("QLineEdit {"
                     "  color: %1; background: %2; border: %3;"
                     "  border-radius: %4px; padding: 7px 9px; font-size: 13px;"
                     "}"
                     "QLineEdit:focus { border: 1px solid %5; }")
          .arg(railInk(),
               BlopTheme::instance().isDark()
                   ? QStringLiteral("rgba(0,0,0,0.22)")
                   : QStringLiteral("rgba(255,255,255,0.72)"),
               fieldBorder(), QString::number(rad),
               BlopTheme::accentPrimary().name(QColor::HexRgb)));
  connect(edit, &QLineEdit::returnPressed, this, [this, edit]() {
    const QString q = edit->text().trimmed();
    if (!q.isEmpty())
      emit searchLibrary(q);
  });
  lay->addWidget(edit);

  auto *go = new QPushButton(QStringLiteral("Suchen"), box);
  go->setCursor(Qt::PointingHandCursor);
  go->setFlat(true);
  go->setStyleSheet(linkBtnQss());
  connect(go, &QPushButton::clicked, this, [this, edit]() {
    const QString q = edit->text().trimmed();
    if (!q.isEmpty())
      emit searchLibrary(q);
    else
      promptQuickSearch();
  });
  lay->addWidget(go, 0, Qt::AlignLeft);
  return box;
}

QWidget *DashRightRail::buildFavoritesContent() {
  auto *box = new QWidget();
  auto *lay = new QVBoxLayout(box);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(UiScale::dp(2));

  const QStringList paths = LibraryOrgStore::favoritePaths();
  if (paths.isEmpty()) {
    auto *empty = new QLabel(QStringLiteral("Keine Favoriten"), box);
    empty->setStyleSheet(
        QStringLiteral("color: %1; font-size: 12px; background: transparent;")
            .arg(railMuted()));
    lay->addWidget(empty);
    return box;
  }
  int n = 0;
  for (const QString &path : paths) {
    if (n >= 5)
      break;
    auto *btn = new QPushButton(QFileInfo(path).completeBaseName(), box);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFlat(true);
    btn->setStyleSheet(rowBtnQss());
    connect(btn, &QPushButton::clicked, this,
            [this, path]() { emit openNotePath(path); });
    lay->addWidget(btn);
    ++n;
  }
  return box;
}

QWidget *DashRightRail::buildRecentContent() {
  auto *box = new QWidget();
  auto *lay = new QVBoxLayout(box);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(UiScale::dp(2));

  const QStringList paths = LibraryOrgStore::recentPaths(5);
  if (paths.isEmpty()) {
    auto *empty = new QLabel(QStringLiteral("Leer"), box);
    empty->setStyleSheet(
        QStringLiteral("color: %1; font-size: 12px; background: transparent;")
            .arg(railMuted()));
    lay->addWidget(empty);
    return box;
  }
  for (const QString &path : paths) {
    auto *btn = new QPushButton(QFileInfo(path).completeBaseName(), box);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFlat(true);
    btn->setStyleSheet(rowBtnQss());
    connect(btn, &QPushButton::clicked, this,
            [this, path]() { emit openNotePath(path); });
    lay->addWidget(btn);
  }
  return box;
}

QWidget *DashRightRail::buildTodosContent() {
  auto *box = new QWidget();
  auto *lay = new QVBoxLayout(box);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(UiScale::dp(4));

  if (QStyle *fusion = QStyleFactory::create(QStringLiteral("Fusion")))
    box->setStyle(fusion);

  int shown = 0;
  for (const auto &t : TodoStore::load()) {
    if (t.done || shown >= 5)
      continue;
    auto *cb = new QCheckBox(t.title, box);
    cb->setStyleSheet(todoCheckQss());
    const QString id = t.id;
    connect(cb, &QCheckBox::toggled, this, [this, id](bool on) {
      if (!on)
        return;
      TodoStore::setDone(id, true);
      emit contentChanged();
      rebuildModules();
    });
    lay->addWidget(cb);
    ++shown;
  }
  if (shown == 0) {
    auto *empty = new QLabel(QStringLiteral("Nichts offen"), box);
    empty->setStyleSheet(
        QStringLiteral("color: %1; font-size: 12px; background: transparent;")
            .arg(railMuted()));
    lay->addWidget(empty);
  }
  return box;
}

void DashRightRail::promptWeatherPlace() {
  const QString q = BlopDialogs::promptText(
      this, QStringLiteral("Wetter-Ort"), QStringLiteral("Stadt oder Ort:"),
      WeatherService::instance().current().placeLabel);
  if (q.trimmed().isEmpty())
    return;
  WeatherService::instance().searchPlace(q.trimmed());
}

void DashRightRail::promptQuickSearch() {
  const QString q = BlopDialogs::promptText(
      this, QStringLiteral("Suche"), QStringLiteral("Notizen durchsuchen:"),
      QString());
  if (!q.trimmed().isEmpty())
    emit searchLibrary(q.trimmed());
}

void DashRightRail::showRailMenu() {
  QList<BlopInWindowMenu::Item> items;
  for (const QString &id : knownModules()) {
    // Board already owns these — offering them would silently add nothing.
    if (isSuppressedByBoard(id))
      continue;
    const bool visible = isModuleVisible(id);
    BlopInWindowMenu::Item it;
    if (visible) {
      it.label = QStringLiteral("✓  %1").arg(moduleTitle(id));
      it.handler = [this, id]() { hideModule(id); };
    } else {
      it.label = QStringLiteral("+  %1").arg(moduleTitle(id));
      it.handler = [this, id]() { showModule(id); };
    }
    items.push_back(it);
  }
  if (items.isEmpty())
    return;
  BlopInWindowMenu::show(
      this, m_btnAdd->mapToGlobal(QPoint(0, m_btnAdd->height())), items);
}

QWidget *DashRightRail::buildFocusContent() {
  auto *box = new QWidget();
  auto *lay = new QVBoxLayout(box);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(UiScale::dp(6));

  const int mins = m_focusSecsLeft / 60;
  const int secs = m_focusSecsLeft % 60;
  auto *lbl = new QLabel(QStringLiteral("%1:%2")
                             .arg(mins, 2, 10, QLatin1Char('0'))
                             .arg(secs, 2, 10, QLatin1Char('0')),
                         box);
  lbl->setObjectName(QStringLiteral("DashFocusLabel"));
  lbl->setStyleSheet(
      QStringLiteral("color: %1; font-size: 30px; font-weight: 700;"
                     "letter-spacing: -1.0px; background: transparent;")
          .arg(railInk()));
  lay->addWidget(lbl);

  auto *hint = new QLabel(QStringLiteral("Fokus · Pomodoro"), box);
  hint->setStyleSheet(
      QStringLiteral("color: %1; font-size: 11px; background: transparent;")
          .arg(railMuted()));
  lay->addWidget(hint);

  auto *row = new QWidget(box);
  auto *rowLay = new QHBoxLayout(row);
  rowLay->setContentsMargins(0, 0, 0, 0);
  rowLay->setSpacing(UiScale::dp(8));

  auto *toggle = new QPushButton(
      m_focusRunning ? QStringLiteral("Pause") : QStringLiteral("Start"), row);
  toggle->setCursor(Qt::PointingHandCursor);
  toggle->setFlat(true);
  toggle->setStyleSheet(linkBtnQss());
  connect(toggle, &QPushButton::clicked, this, [this]() {
    m_focusRunning = !m_focusRunning;
    if (m_focusRunning) {
      if (!m_focusTick) {
        m_focusTick = new QTimer(this);
        m_focusTick->setInterval(1000);
        connect(m_focusTick, &QTimer::timeout, this, &DashRightRail::tickFocus);
      }
      m_focusTick->start();
    } else if (m_focusTick) {
      m_focusTick->stop();
    }
    rebuildModules();
  });
  rowLay->addWidget(toggle, 0);

  auto *reset = new QPushButton(QStringLiteral("25 Min"), row);
  reset->setCursor(Qt::PointingHandCursor);
  reset->setFlat(true);
  reset->setStyleSheet(linkBtnQss());
  connect(reset, &QPushButton::clicked, this, [this]() {
    m_focusRunning = false;
    m_focusSecsLeft = 25 * 60;
    if (m_focusTick)
      m_focusTick->stop();
    rebuildModules();
  });
  rowLay->addWidget(reset, 0);
  rowLay->addStretch(1);
  lay->addWidget(row);
  return box;
}

void DashRightRail::tickFocus() {
  if (!m_focusRunning)
    return;
  if (m_focusSecsLeft <= 0) {
    m_focusRunning = false;
    if (m_focusTick)
      m_focusTick->stop();
    BlopDialogs::notify(this, QStringLiteral("Fokus"),
                        QStringLiteral("25 Minuten geschafft."));
    m_focusSecsLeft = 25 * 60;
    rebuildModules();
    return;
  }
  --m_focusSecsLeft;
  if (auto *lbl = findChild<QLabel *>(QStringLiteral("DashFocusLabel"))) {
    const int mins = m_focusSecsLeft / 60;
    const int secs = m_focusSecsLeft % 60;
    lbl->setText(QStringLiteral("%1:%2")
                     .arg(mins, 2, 10, QLatin1Char('0'))
                     .arg(secs, 2, 10, QLatin1Char('0')));
  }
}

QWidget *DashRightRail::buildCaptureContent() {
  auto *box = new QWidget();
  auto *lay = new QVBoxLayout(box);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(UiScale::dp(6));

  auto *edit = new QLineEdit(box);
  edit->setPlaceholderText(QStringLiteral("Aufgabe notieren…"));
  edit->setClearButtonEnabled(true);
  const int rad = UiScale::dp(BlopStyle::radiusMdDp());
  edit->setStyleSheet(
      QStringLiteral("QLineEdit {"
                     "  color: %1; background: %2; border: %3;"
                     "  border-radius: %4px; padding: 7px 9px; font-size: 13px;"
                     "}"
                     "QLineEdit:focus { border: 1px solid %5; }")
          .arg(railInk(),
               BlopTheme::instance().isDark()
                   ? QStringLiteral("rgba(0,0,0,0.22)")
                   : QStringLiteral("rgba(255,255,255,0.72)"),
               fieldBorder(), QString::number(rad),
               BlopTheme::accentPrimary().name(QColor::HexRgb)));
  auto addTask = [this, edit]() {
    const QString t = edit->text().trimmed();
    if (t.isEmpty())
      return;
    TodoStore::add(t);
    edit->clear();
    emit contentChanged();
  };
  connect(edit, &QLineEdit::returnPressed, this, addTask);
  lay->addWidget(edit);

  auto *go = new QPushButton(QStringLiteral("Hinzufügen"), box);
  go->setCursor(Qt::PointingHandCursor);
  go->setFlat(true);
  go->setStyleSheet(linkBtnQss());
  connect(go, &QPushButton::clicked, this, addTask);
  lay->addWidget(go, 0, Qt::AlignLeft);
  return box;
}
