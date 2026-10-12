#include "cloudnotesbrowser.h"

#include "blop_theme.h"
#include "blopstyle.h"
#include "cloudstoragestore.h"
#include "uiscale.h"

#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QShowEvent>
#include <QStandardPaths>
#include <QVBoxLayout>

namespace {

QIcon makeViewGlyph(const QString &kind, const QColor &fg, int px) {
  QPixmap pm(px, px);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing);
  p.setPen(QPen(fg, 1.6));
  p.setBrush(Qt::NoBrush);
  if (kind == QLatin1String("grid")) {
    const qreal g = px * 0.18;
    const qreal s = (px - 3 * g) / 2.0;
    for (int r = 0; r < 2; ++r)
      for (int c = 0; c < 2; ++c)
        p.drawRoundedRect(QRectF(g + c * (s + g), g + r * (s + g), s, s), 2, 2);
  } else {
    const qreal x = px * 0.18;
    const qreal w = px - 2 * x;
    for (int i = 0; i < 3; ++i) {
      const qreal y = px * 0.22 + i * px * 0.22;
      p.drawRoundedRect(QRectF(x, y, w, px * 0.12), 2, 2);
    }
  }
  return QIcon(pm);
}

} // namespace

CloudNotesBrowser::CloudNotesBrowser(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("CloudNotesBrowser"));
  setAttribute(Qt::WA_StyledBackground, true);
  setFocusPolicy(Qt::StrongFocus);
  rebuildChrome();
  if (parent)
    parent->installEventFilter(this);
}

void CloudNotesBrowser::rebuildChrome() {
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(0);

  auto *bar = new QWidget(this);
  bar->setObjectName(QStringLiteral("CloudNotesBar"));
  bar->setFixedHeight(UiScale::dp(48));
  auto *barLay = new QHBoxLayout(bar);
  barLay->setContentsMargins(UiScale::dp(12), UiScale::dp(6), UiScale::dp(12),
                             UiScale::dp(6));
  barLay->setSpacing(UiScale::dp(6));

  m_title = new QLabel(bar);
  m_title->setObjectName(QStringLiteral("CloudNotesTitle"));

  const int viewBtn = UiScale::dp(28);
  m_btnGrid = new QPushButton(bar);
  m_btnGrid->setObjectName(QStringLiteral("CloudNotesViewGrid"));
  m_btnGrid->setFixedSize(viewBtn, viewBtn);
  m_btnGrid->setCheckable(true);
  m_btnGrid->setCursor(Qt::PointingHandCursor);
  m_btnGrid->setToolTip(QStringLiteral("Rasteransicht"));
  m_btnGrid->setStyleSheet(BlopStyle::quietIconButtonQss(6, 28));

  m_btnList = new QPushButton(bar);
  m_btnList->setObjectName(QStringLiteral("CloudNotesViewList"));
  m_btnList->setFixedSize(viewBtn, viewBtn);
  m_btnList->setCheckable(true);
  m_btnList->setCursor(Qt::PointingHandCursor);
  m_btnList->setToolTip(QStringLiteral("Listenansicht"));
  m_btnList->setStyleSheet(BlopStyle::quietIconButtonQss(6, 28));

  m_btnRefresh = new QPushButton(QStringLiteral("Aktualisieren"), bar);
  m_btnRefresh->setCursor(Qt::PointingHandCursor);
  m_btnRefresh->setMinimumHeight(UiScale::dp(32));
  m_btnClose = new QPushButton(QStringLiteral("Fertig"), bar);
  m_btnClose->setObjectName(QStringLiteral("CloudNotesDone"));
  m_btnClose->setCursor(Qt::PointingHandCursor);
  m_btnClose->setMinimumHeight(UiScale::dp(32));

  barLay->addWidget(m_title, 1);
  barLay->addWidget(m_btnGrid);
  barLay->addWidget(m_btnList);
  barLay->addWidget(m_btnRefresh);
  barLay->addWidget(m_btnClose);
  root->addWidget(bar);

  m_status = new QLabel(this);
  m_status->setObjectName(QStringLiteral("CloudNotesStatus"));
  m_status->setWordWrap(true);
  m_status->setContentsMargins(UiScale::dp(16), UiScale::dp(8), UiScale::dp(16),
                               UiScale::dp(4));
  root->addWidget(m_status);

  m_list = new QListWidget(this);
  m_list->setObjectName(QStringLiteral("CloudNotesList"));
  m_list->setFrameShape(QFrame::NoFrame);
  m_list->setUniformItemSizes(true);
  m_list->setMovement(QListView::Static);
  m_list->setResizeMode(QListView::Adjust);
  m_list->setWordWrap(true);
  m_list->setSpacing(UiScale::dp(6));
  root->addWidget(m_list, 1);

  const bool dark = BlopTheme::instance().isDark();
  const QString bg =
      dark ? BlopStyle::libraryMain().name(QColor::HexRgb)
           : BlopStyle::libraryMain().name(QColor::HexRgb);
  const QString ink =
      dark ? QStringLiteral("#E8EAF0")
           : BlopStyle::paperInk().name(QColor::HexRgb);
  const QString muted =
      dark ? QStringLiteral("#9AA3B2")
           : BlopStyle::paperInkMuted().name(QColor::HexRgb);
  const QString barBg =
      dark ? BlopStyle::librarySidebar().name(QColor::HexRgb)
           : BlopStyle::paperSurface().name(QColor::HexRgb);
  const QString hover =
      dark ? QStringLiteral("rgba(255,255,255,0.08)")
           : BlopStyle::paperHover().name(QColor::HexRgb);
  const QString border =
      dark ? QStringLiteral("rgba(255,255,255,0.10)")
           : BlopStyle::paperBorder().name(QColor::HexRgb);
  const QColor acc = BlopTheme::accentPrimary();
  setStyleSheet(QStringLiteral(
                    "QWidget#CloudNotesBrowser { background: %1; }"
                    "QWidget#CloudNotesBar {"
                    "  background: %2; border: none;"
                    "  border-bottom: 1px solid %9;"
                    "}"
                    "QLabel#CloudNotesTitle {"
                    "  color: %3; font-size: 14px; font-weight: 700;"
                    "  background: transparent;"
                    "}"
                    "QLabel#CloudNotesStatus {"
                    "  color: %4; font-size: 12px; background: transparent;"
                    "}"
                    "QListWidget#CloudNotesList {"
                    "  background: transparent; border: none; outline: 0;"
                    "  color: %3; font-size: 13px;"
                    "}"
                    "QListWidget#CloudNotesList::item {"
                    "  padding: 8px; border-radius: 8px;"
                    "}"
                    "QListWidget#CloudNotesList::item:hover { background: %5; }"
                    "QListWidget#CloudNotesList::item:selected {"
                    "  background: rgba(%6,%7,%8,0.18);"
                    "}"
                    "QPushButton#CloudNotesDone, QPushButton {"
                    "  background: transparent; color: %3;"
                    "  border: 1px solid %9;"
                    "  border-radius: 8px; padding: 0 12px; font-weight: 600;"
                    "}"
                    "QPushButton#CloudNotesDone:hover, QPushButton:hover {"
                    "  background: %5;"
                    "}"
                    "QPushButton#CloudNotesViewGrid, QPushButton#CloudNotesViewList {"
                    "  border: none; padding: 0; min-width: 28px; max-width: 28px;"
                    "}")
                    .arg(bg, barBg, ink, muted, hover)
                    .arg(acc.red())
                    .arg(acc.green())
                    .arg(acc.blue())
                    .arg(border));

  connect(m_btnClose, &QPushButton::clicked, this, [this]() {
    hide();
    emit closed();
  });
  connect(m_btnRefresh, &QPushButton::clicked, this,
          &CloudNotesBrowser::refreshList);
  connect(m_btnGrid, &QPushButton::clicked, this, [this]() {
    setListMode(false);
  });
  connect(m_btnList, &QPushButton::clicked, this, [this]() {
    setListMode(true);
  });
  connect(m_list, &QListWidget::itemActivated, this,
          [this](QListWidgetItem *) { openCurrentItem(); });
  connect(m_list, &QListWidget::itemDoubleClicked, this,
          [this](QListWidgetItem *) { openCurrentItem(); });
  syncViewButtons();
  applyViewMode();
}

CloudNotesBrowser *CloudNotesBrowser::showIn(QWidget *host,
                                             const QString &providerType,
                                             const QString &displayName,
                                             bool listMode) {
  if (!host)
    return nullptr;
  auto *ex = host->findChild<CloudNotesBrowser *>(
      QStringLiteral("CloudNotesBrowser"), Qt::FindDirectChildrenOnly);
  if (!ex) {
    ex = new CloudNotesBrowser(host);
  } else if (ex->parentWidget() != host) {
    ex->setParent(host);
    host->installEventFilter(ex);
  }
  ex->setListMode(listMode);
  ex->setGeometry(host->rect());
  ex->openProvider(providerType, displayName);
  ex->show();
  ex->raise();
  ex->setFocus();
  return ex;
}

void CloudNotesBrowser::openProvider(const QString &providerType,
                                     const QString &displayName) {
  m_type = providerType;
  const QString title =
      displayName.isEmpty()
          ? CloudStorageStore::displayNameForType(providerType)
          : displayName;
  if (m_title)
    m_title->setText(QStringLiteral("%1 · BlopNotizen").arg(title));
  refreshList();
}

void CloudNotesBrowser::setListMode(bool on) {
  if (m_listMode == on) {
    syncViewButtons();
    return;
  }
  m_listMode = on;
  syncViewButtons();
  applyViewMode();
  refreshList();
}

void CloudNotesBrowser::syncViewButtons() {
  const QColor on = BlopTheme::accentPrimary();
  const QColor off = BlopTheme::instance().isDark()
                         ? QColor(0x9A, 0xA3, 0xB2)
                         : BlopStyle::paperInkMuted();
  const int px = UiScale::dp(16);
  if (m_btnGrid) {
    m_btnGrid->setChecked(!m_listMode);
    m_btnGrid->setIcon(makeViewGlyph(QStringLiteral("grid"),
                                     m_listMode ? off : on, px));
  }
  if (m_btnList) {
    m_btnList->setChecked(m_listMode);
    m_btnList->setIcon(makeViewGlyph(QStringLiteral("list"),
                                     m_listMode ? on : off, px));
  }
}

void CloudNotesBrowser::applyViewMode() {
  if (!m_list)
    return;
  if (m_listMode) {
    m_list->setViewMode(QListView::ListMode);
    m_list->setFlow(QListView::TopToBottom);
    m_list->setWrapping(false);
    m_list->setIconSize(QSize(UiScale::dp(28), UiScale::dp(28)));
    m_list->setGridSize(QSize());
    m_list->setSpacing(UiScale::dp(2));
  } else {
    m_list->setViewMode(QListView::IconMode);
    m_list->setFlow(QListView::LeftToRight);
    m_list->setWrapping(true);
    const int tile = UiScale::dp(96);
    m_list->setIconSize(QSize(tile, tile));
    m_list->setGridSize(QSize(tile + UiScale::dp(28), tile + UiScale::dp(40)));
    m_list->setSpacing(UiScale::dp(8));
  }
}

QIcon CloudNotesBrowser::fileIconFor(const QString &name) const {
  const int side = m_listMode ? UiScale::dp(28) : UiScale::dp(96);
  QPixmap pm(side, side);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing);
  const bool dark = BlopTheme::instance().isDark();
  const QColor paper =
      dark ? QColor(0x2A, 0x2E, 0x38) : QColor(0xFF, 0xFF, 0xFF);
  const QColor edge =
      dark ? QColor(255, 255, 255, 40) : BlopStyle::paperBorder();
  const QColor ink =
      dark ? QColor(0xE8, 0xEA, 0xF0) : BlopStyle::paperInk();
  const qreal pad = side * 0.12;
  QRectF card(pad, pad, side - 2 * pad, side - 2 * pad);
  QPainterPath path;
  path.addRoundedRect(card, side * 0.12, side * 0.12);
  p.setPen(QPen(edge, 1));
  p.setBrush(paper);
  p.drawPath(path);
  // Folded corner.
  QPainterPath fold;
  const qreal f = side * 0.22;
  fold.moveTo(card.right() - f, card.top());
  fold.lineTo(card.right(), card.top() + f);
  fold.lineTo(card.right() - f, card.top() + f);
  fold.closeSubpath();
  p.setBrush(dark ? QColor(0x3A, 0x40, 0x4C) : QColor(0xEE, 0xF0, 0xF4));
  p.drawPath(fold);
  QString ext = QFileInfo(name).suffix().toUpper();
  if (ext.size() > 5)
    ext = ext.left(5);
  if (ext.isEmpty())
    ext = QStringLiteral("FILE");
  QFont fnt = p.font();
  fnt.setBold(true);
  fnt.setPixelSize(qMax(9, int(side * 0.16)));
  p.setFont(fnt);
  p.setPen(ink);
  p.drawText(card.adjusted(0, side * 0.18, 0, 0), Qt::AlignCenter, ext);
  return QIcon(pm);
}

void CloudNotesBrowser::refreshList() {
  if (!m_list)
    return;
  m_list->clear();
  if (m_status)
    m_status->setText(QStringLiteral("Lade BlopNotizen…"));
  const QVector<CloudRemoteItem> items =
      CloudLinkHub::instance().listNotes(m_type);
  if (items.isEmpty()) {
    const QString err = CloudLinkHub::instance().lastError();
    if (m_status) {
      m_status->setText(err.isEmpty()
                            ? QStringLiteral(
                                  "Noch keine Notizen in BlopNotizen. "
                                  "Doppelklick öffnet eine Datei nach dem Upload.")
                            : err);
    }
    return;
  }
  int count = 0;
  for (const CloudRemoteItem &it : items) {
    if (it.isFolder)
      continue;
    ++count;
    auto *row = new QListWidgetItem(fileIconFor(it.name), it.name, m_list);
    row->setData(Qt::UserRole, it.id);
    row->setData(Qt::UserRole + 1, it.name);
    row->setTextAlignment(m_listMode ? (Qt::AlignVCenter | Qt::AlignLeft)
                                     : Qt::AlignHCenter | Qt::AlignTop);
    QString tip = it.name;
    if (it.modified.isValid())
      tip += QStringLiteral("\n") +
             it.modified.toLocalTime().toString(QStringLiteral("dd.MM.yyyy HH:mm"));
    row->setToolTip(tip);
  }
  if (m_status)
    m_status->setText(
        QStringLiteral("%1 Einträge — Doppelklick öffnet in Blop")
            .arg(count));
  applyViewMode();
}

void CloudNotesBrowser::openCurrentItem() {
  if (!m_list)
    return;
  QListWidgetItem *item = m_list->currentItem();
  if (!item)
    return;
  const QString id = item->data(Qt::UserRole).toString();
  const QString name = item->data(Qt::UserRole + 1).toString();
  if (id.isEmpty() || name.isEmpty())
    return;
  if (m_status)
    m_status->setText(QStringLiteral("Lade %1…").arg(name));

  const QString cacheRoot =
      QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
          .filePath(QStringLiteral("cloud-cache/%1").arg(m_type));
  QDir().mkpath(cacheRoot);
  const QString localPath = QDir(cacheRoot).filePath(name);
  if (!CloudLinkHub::instance().fetchNote(m_type, id, localPath)) {
    if (m_status) {
      const QString err = CloudLinkHub::instance().lastError();
      m_status->setText(err.isEmpty()
                            ? QStringLiteral("Download fehlgeschlagen.")
                            : err);
    }
    return;
  }
  emit noteOpenRequested(localPath);
}

void CloudNotesBrowser::showEvent(QShowEvent *event) {
  QWidget::showEvent(event);
  if (parentWidget())
    setGeometry(parentWidget()->rect());
}

bool CloudNotesBrowser::eventFilter(QObject *watched, QEvent *event) {
  if (watched == parentWidget() && event &&
      (event->type() == QEvent::Resize || event->type() == QEvent::Show) &&
      isVisible() && parentWidget()) {
    setGeometry(parentWidget()->rect());
  }
  return QWidget::eventFilter(watched, event);
}

void CloudNotesBrowser::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape) {
    hide();
    emit closed();
    event->accept();
    return;
  }
  QWidget::keyPressEvent(event);
}
