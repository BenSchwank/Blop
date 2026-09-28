#include "dashrightrail.h"

#include "blop_dialogs.h"
#include "blop_inwindow_menu.h"
#include "blop_theme.h"
#include "blopstyle.h"
#include "libraryorgstore.h"
#include "todostore.h"
#include "uiscale.h"
#include "weatherservice.h"

#include <QFileInfo>
#include <QFrame>
#include <QHash>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

namespace {
constexpr const char *kOrderKey = "dashboard/rightRail.moduleOrder";
constexpr const char *kHiddenKey = "dashboard/rightRail.hiddenModules";
constexpr int kRailWidthDp = 168;
/// Same charcoal as LibraryIconRail / desktop shell chrome.
QString chromeBg() {
  return BlopTheme::instance().isDark() ? QStringLiteral("#16181E")
                                        : QStringLiteral("#F7F7F5");
}

QString railInk() {
  return BlopTheme::instance().isDark()
             ? QColor(255, 255, 255, 220).name(QColor::HexArgb)
             : BlopStyle::paperInk().name(QColor::HexRgb);
}
QString railMuted() {
  return BlopTheme::instance().isDark()
             ? QColor(255, 255, 255, 130).name(QColor::HexArgb)
             : QColor(55, 53, 47, 160).name(QColor::HexArgb);
}
/// Segments sit on the chrome rail — no second card plate (was reading as a
/// lighter island next to the charcoal left rail / title bar).
QString segmentBg() { return QStringLiteral("transparent"); }
QString segmentBorder() { return QStringLiteral("none"); }
} // namespace

DashRightRail::DashRightRail(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("DashRightRail"));
  setAttribute(Qt::WA_StyledBackground, true);
  setFixedWidth(UiScale::dp(kRailWidthDp));
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(UiScale::dp(12), UiScale::dp(14), UiScale::dp(10),
                           UiScale::dp(12));
  root->setSpacing(UiScale::dp(6));

  m_body = new QWidget(this);
  m_body->setStyleSheet(QStringLiteral("background: transparent;"));
  m_bodyLay = new QVBoxLayout(m_body);
  m_bodyLay->setContentsMargins(0, 0, 0, 0);
  m_bodyLay->setSpacing(UiScale::dp(10));
  root->addWidget(m_body, 1);

  m_btnAdd = new QPushButton(QStringLiteral("+ Widget"), this);
  m_btnAdd->setCursor(Qt::PointingHandCursor);
  m_btnAdd->setFlat(true);
  m_btnAdd->setVisible(false);
  m_btnAdd->setMinimumHeight(UiScale::dp(30));
  connect(m_btnAdd, &QPushButton::clicked, this, &DashRightRail::showAddMenu);
  root->addWidget(m_btnAdd, 0);

  loadModulePrefs();

  connect(&WeatherService::instance(), &WeatherService::weatherChanged, this,
          &DashRightRail::rebuildModules);
  connect(&WeatherService::instance(), &WeatherService::searchFailed, this,
          [this](const QString &msg) {
            BlopDialogs::notify(this, QStringLiteral("Wetter"), msg);
          });
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

void DashRightRail::refresh() { rebuildModules(); }

void DashRightRail::setBoardOccupiedIds(const QStringList &ids) {
  if (m_boardOccupied == ids)
    return;
  m_boardOccupied = ids;
  rebuildModules();
}

bool DashRightRail::isSuppressedByBoard(const QString &id) const {
  // Never duplicate Zuletzt/Aufgaben that already live on the board.
  if (id != QLatin1String("recent") && id != QLatin1String("todos"))
    return false;
  return m_boardOccupied.contains(id);
}

void DashRightRail::loadModulePrefs() {
  QSettings st(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  m_moduleOrder = st.value(QLatin1String(kOrderKey)).toStringList();
  m_hiddenModules = st.value(QLatin1String(kHiddenKey)).toStringList();
  // Drop legacy collapsed preference — rail is always a slim column now.
  st.remove(QStringLiteral("dashboard/rightRail.collapsed"));

  const QStringList known = {QStringLiteral("weather"), QStringLiteral("recent"),
                             QStringLiteral("todos")};
  // Fresh install: weather only — recent/todos live on the board.
  if (!st.contains(QLatin1String(kOrderKey)) &&
      !st.contains(QLatin1String(kHiddenKey))) {
    m_moduleOrder = {QStringLiteral("weather")};
    m_hiddenModules = {QStringLiteral("recent"), QStringLiteral("todos")};
    saveModulePrefs();
    return;
  }
  if (m_moduleOrder.isEmpty())
    m_moduleOrder = {QStringLiteral("weather")};
  for (const QString &id : known) {
    if (!m_moduleOrder.contains(id))
      m_moduleOrder.append(id);
  }

  // Force-hide board duplicates (users who still had both after earlier prefs).
  constexpr const char *kDedupeKey = "dashboard/rightRail.dedupeBoardV3";
  if (!st.value(QLatin1String(kDedupeKey)).toBool()) {
    st.setValue(QLatin1String(kDedupeKey), true);
    st.setValue(QStringLiteral("dashboard/rightRail.dedupeBoardV1"), true);
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
      "QWidget#DashRightRail {"
      "  background: %1;"
      "  border: none;"
      "  border-left: 1px solid %2;"
      "}")
                    .arg(chromeBg(),
                         BlopTheme::instance().isDark()
                             ? QStringLiteral("rgba(255,255,255,0.06)")
                             : QStringLiteral("rgba(15,23,42,0.08)")));
  const QString acc = BlopTheme::accentPrimary().name(QColor::HexRgb);
  m_btnAdd->setStyleSheet(
      QStringLiteral("QPushButton {"
                     "  color: %1; font-size: 11px; font-weight: 550;"
                     "  background: transparent; border: none;"
                     "  border-radius: %2px; padding: 6px 4px; text-align: left;"
                     "}"
                     "QPushButton:hover { color: %3; }")
          .arg(railMuted(), QString::number(UiScale::dp(8)), acc));
  const bool anyHidden = !m_hiddenModules.isEmpty();
  m_btnAdd->setVisible(m_editMode && anyHidden);
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
    // Prefs win: if the user re-enabled a module in Anpassen, keep it after Fertig
    // — unless the board already shows the same block (see isSuppressedByBoard).
    QWidget *content = nullptr;
    QString title;
    if (id == QLatin1String("weather")) {
      title = QStringLiteral("Wetter");
      content = buildWeatherContent();
    } else if (id == QLatin1String("recent")) {
      title = QStringLiteral("Zuletzt");
      content = buildRecentContent();
    } else if (id == QLatin1String("todos")) {
      title = QStringLiteral("Aufgaben");
      content = buildTodosContent();
    }
    if (content)
      m_bodyLay->addWidget(makeSegment(id, title, content), 0);
  }
  m_bodyLay->addStretch(1);
  applyChrome();
}

QWidget *DashRightRail::makeSegment(const QString &id, const QString &title,
                                    QWidget *content) {
  auto *seg = new QFrame(m_body);
  seg->setObjectName(QStringLiteral("DashRailSegment"));
  seg->setAttribute(Qt::WA_StyledBackground, true);
  const int rad = UiScale::dp(BlopStyle::radiusMdDp());
  seg->setStyleSheet(
      QStringLiteral("QFrame#DashRailSegment {"
                     "  background: %1;"
                     "  border: %2;"
                     "  border-radius: %3px;"
                     "}")
          .arg(segmentBg(), segmentBorder(), QString::number(rad)));

  auto *lay = new QVBoxLayout(seg);
  lay->setContentsMargins(UiScale::dp(10), UiScale::dp(8), UiScale::dp(6),
                          UiScale::dp(10));
  lay->setSpacing(UiScale::dp(5));

  auto *hdr = new QWidget(seg);
  auto *hdrLay = new QHBoxLayout(hdr);
  hdrLay->setContentsMargins(0, 0, 0, 0);
  hdrLay->setSpacing(UiScale::dp(4));

  auto *cap = new QLabel(title, hdr);
  cap->setStyleSheet(
      QStringLiteral("color: %1; font-size: 12px; font-weight: 550;"
                     "letter-spacing: -0.2px; background: transparent;")
          .arg(railMuted()));
  hdrLay->addWidget(cap, 1);

  auto *dismiss = new QPushButton(QStringLiteral("×"), hdr);
  dismiss->setCursor(Qt::PointingHandCursor);
  dismiss->setFlat(true);
  dismiss->setFixedSize(UiScale::dp(20), UiScale::dp(20));
  dismiss->setToolTip(QStringLiteral("Segment ausblenden"));
  dismiss->setVisible(m_editMode);
  dismiss->setStyleSheet(
      QStringLiteral("QPushButton {"
                     "  color: %1; background: transparent; border: none;"
                     "  font-size: 13px; font-weight: 400; padding: 0;"
                     "}"
                     "QPushButton:hover { color: %2; }")
          .arg(railMuted(), railInk()));
  connect(dismiss, &QPushButton::clicked, this, [this, id]() {
    hideModule(id);
  });
  hdrLay->addWidget(dismiss, 0, Qt::AlignTop);
  lay->addWidget(hdr, 0);

  content->setParent(seg);
  lay->addWidget(content, 0);
  return seg;
}

QWidget *DashRightRail::buildWeatherContent() {
  auto *box = new QWidget();
  auto *lay = new QVBoxLayout(box);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(UiScale::dp(2));

  const auto snap = WeatherService::instance().current();
  if (!WeatherService::instance().hasLocation()) {
    auto *empty = new QLabel(QStringLiteral("Ort wählen"), box);
    empty->setStyleSheet(
        QStringLiteral("color: %1; font-size: 12px; background: transparent;")
            .arg(railMuted()));
    lay->addWidget(empty);
  } else if (snap.valid) {
    auto *temp = new QLabel(
        QStringLiteral("%1°").arg(QString::number(snap.tempC, 'f', 0)), box);
    temp->setStyleSheet(
        QStringLiteral("color: %1; font-size: 22px; font-weight: 600;"
                       "letter-spacing: -0.6px; background: transparent;")
            .arg(railInk()));
    lay->addWidget(temp);
    auto *sum = new QLabel(snap.summary, box);
    sum->setStyleSheet(
        QStringLiteral("color: %1; font-size: 12px; background: transparent;")
            .arg(railMuted()));
    lay->addWidget(sum);
    auto *place = new QLabel(snap.placeLabel, box);
    place->setWordWrap(true);
    place->setStyleSheet(
        QStringLiteral("color: %1; font-size: 11px; background: transparent;")
            .arg(railMuted()));
    lay->addWidget(place);
  } else {
    auto *err = new QLabel(snap.error.isEmpty() ? QStringLiteral("…")
                                                : snap.error,
                           box);
    err->setWordWrap(true);
    err->setStyleSheet(
        QStringLiteral("color: %1; font-size: 12px; background: transparent;")
            .arg(railMuted()));
    lay->addWidget(err);
  }

  auto *setOrt = new QPushButton(
      WeatherService::instance().hasLocation() ? QStringLiteral("Ort")
                                               : QStringLiteral("Festlegen"),
      box);
  setOrt->setCursor(Qt::PointingHandCursor);
  setOrt->setFlat(true);
  setOrt->setStyleSheet(
      QStringLiteral("QPushButton { color: %1; font-size: 11px; font-weight: 600;"
                     "  background: transparent; border: none; text-align: left;"
                     "  padding: 4px 0 0 0; }"
                     "QPushButton:hover { color: %2; }")
          .arg(BlopTheme::accentPrimary().name(QColor::HexRgb),
               BlopTheme::accentHover().name(QColor::HexRgb)));
  connect(setOrt, &QPushButton::clicked, this,
          &DashRightRail::promptWeatherPlace);
  lay->addWidget(setOrt, 0, Qt::AlignLeft);
  return box;
}

QWidget *DashRightRail::buildRecentContent() {
  auto *box = new QWidget();
  auto *lay = new QVBoxLayout(box);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(0);

  const QStringList paths = LibraryOrgStore::recentPaths(4);
  if (paths.isEmpty()) {
    auto *empty = new QLabel(QStringLiteral("Leer"), box);
    empty->setStyleSheet(
        QStringLiteral("color: %1; font-size: 12px; background: transparent;")
            .arg(railMuted()));
    lay->addWidget(empty);
    return box;
  }
  const QString hover = BlopTheme::instance().isDark()
                            ? QStringLiteral("rgba(255,255,255,0.05)")
                            : BlopStyle::paperHover().name(QColor::HexRgb);
  for (const QString &path : paths) {
    auto *btn = new QPushButton(QFileInfo(path).completeBaseName(), box);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFlat(true);
    btn->setStyleSheet(
        QStringLiteral("QPushButton {"
                       "  text-align: left; padding: 5px 2px;"
                       "  color: %1; font-size: 12px; font-weight: 500;"
                       "  background: transparent; border: none;"
                       "  border-radius: 4px;"
                       "}"
                       "QPushButton:hover { background: %2; }")
            .arg(railInk(), hover));
    connect(btn, &QPushButton::clicked, this, [this, path]() {
      emit openNotePath(path);
    });
    lay->addWidget(btn);
  }
  return box;
}

QWidget *DashRightRail::buildTodosContent() {
  auto *box = new QWidget();
  auto *lay = new QVBoxLayout(box);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(UiScale::dp(3));

  int shown = 0;
  for (const auto &t : TodoStore::load()) {
    if (t.done || shown >= 4)
      continue;
    auto *row = new QLabel(QStringLiteral("·  %1").arg(t.title), box);
    row->setWordWrap(true);
    row->setStyleSheet(
        QStringLiteral("color: %1; font-size: 12px; background: transparent;")
            .arg(railInk()));
    lay->addWidget(row);
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
      this, QStringLiteral("Wetter-Ort"),
      QStringLiteral("Stadt oder Ort:"),
      WeatherService::instance().current().placeLabel);
  if (q.trimmed().isEmpty())
    return;
  WeatherService::instance().searchPlace(q.trimmed());
}

void DashRightRail::showAddMenu() {
  QList<BlopInWindowMenu::Item> items;
  const QHash<QString, QString> names = {
      {QStringLiteral("weather"), QStringLiteral("Wetter")},
      {QStringLiteral("recent"), QStringLiteral("Zuletzt")},
      {QStringLiteral("todos"), QStringLiteral("Aufgaben")},
  };
  for (const QString &id : m_hiddenModules) {
    BlopInWindowMenu::Item it;
    it.label = QStringLiteral("+ %1").arg(names.value(id, id));
    it.handler = [this, id]() { showModule(id); };
    items.push_back(it);
  }
  if (items.isEmpty())
    return;
  const QPoint g =
      m_btnAdd ? m_btnAdd->mapToGlobal(QPoint(0, m_btnAdd->height()))
               : mapToGlobal(QPoint(0, height()));
  BlopInWindowMenu::show(this, g, items);
}
