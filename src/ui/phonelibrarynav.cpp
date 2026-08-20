#include "phonelibrarynav.h"

#include "blop_theme.h"
#include "cloudstoragestore.h"
#include "uiscale.h"

#include <QApplication>
#include <QBrush>
#include <QColor>
#include <QEvent>
#include <QFileInfo>
#include <QFont>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPen>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSizePolicy>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QTouchEvent>
#include <QVBoxLayout>

namespace {
constexpr int kPillW = 168;
constexpr int kPillH = 48;
constexpr int kSectionRole = Qt::UserRole + 1;
constexpr int kSubtitleRole = Qt::UserRole + 2;
constexpr int kChevronRole = Qt::UserRole + 3;

class BurgerRowDelegate : public QStyledItemDelegate {
public:
  using QStyledItemDelegate::QStyledItemDelegate;

  void paint(QPainter *p, const QStyleOptionViewItem &opt,
             const QModelIndex &index) const override {
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    const QRect r = opt.rect.adjusted(2, 1, -2, -1);
    const bool section = index.data(kSectionRole).toBool();
    if (section) {
      QFont f = opt.font;
      f.setPixelSize(UiScale::sp(11));
      f.setWeight(QFont::DemiBold);
      f.setLetterSpacing(QFont::PercentageSpacing, 112);
      p->setFont(f);
      p->setPen(QColor(QStringLiteral("#8B92A8")));
      p->drawText(r.adjusted(UiScale::dp(12), 0, -UiScale::dp(12), 0),
                  Qt::AlignVCenter | Qt::AlignLeft,
                  index.data(Qt::DisplayRole).toString());
      p->restore();
      return;
    }

    QPainterPath path;
    path.addRoundedRect(QRectF(r), 10, 10);
    const bool selected = opt.state & QStyle::State_Selected;
    const bool hover = opt.state & QStyle::State_MouseOver;
    if (selected)
      p->fillPath(path, QColor(124, 92, 252, 120));
    else if (hover)
      p->fillPath(path, QColor(255, 255, 255, 26));

    const QString title = index.data(Qt::DisplayRole).toString();
    const QString sub = index.data(kSubtitleRole).toString();
    const bool chevron = index.data(kChevronRole).toBool();
    const int pad = UiScale::dp(14);
    const int chevronW = chevron ? UiScale::dp(18) : 0;
    QRect textR = r.adjusted(pad, 0, -pad - chevronW, 0);

    QFont titleFont = opt.font;
    titleFont.setPixelSize(UiScale::sp(15));
    titleFont.setWeight(QFont::DemiBold);
    p->setFont(titleFont);
    p->setPen(QColor(QStringLiteral("#F4F2FF")));
    if (sub.isEmpty()) {
      p->drawText(textR, Qt::AlignVCenter | Qt::AlignLeft, title);
    } else {
      QFont subFont = opt.font;
      subFont.setPixelSize(UiScale::sp(12));
      subFont.setWeight(QFont::Normal);
      const int titleH = QFontMetrics(titleFont).height();
      const int subH = QFontMetrics(subFont).height();
      const int blockH = titleH + subH + 2;
      const int y0 = textR.y() + (textR.height() - blockH) / 2;
      p->drawText(QRect(textR.x(), y0, textR.width(), titleH),
                  Qt::AlignVCenter | Qt::AlignLeft, title);
      p->setFont(subFont);
      p->setPen(QColor(QStringLiteral("#9AA3BB")));
      p->drawText(QRect(textR.x(), y0 + titleH + 1, textR.width(), subH),
                  Qt::AlignVCenter | Qt::AlignLeft, sub);
    }
    if (chevron) {
      p->setPen(QPen(QColor(200, 208, 235, 180), 1.8, Qt::SolidLine, Qt::RoundCap,
                     Qt::RoundJoin));
      const int cx = r.right() - UiScale::dp(16);
      const int cy = r.center().y();
      p->drawLine(cx - 4, cy - 5, cx + 2, cy);
      p->drawLine(cx + 2, cy, cx - 4, cy + 5);
    }
    p->restore();
  }

  QSize sizeHint(const QStyleOptionViewItem &opt,
                 const QModelIndex &index) const override {
    Q_UNUSED(opt);
    if (index.data(kSectionRole).toBool())
      return QSize(10, UiScale::dp(index.data(Qt::DisplayRole).toString().isEmpty()
                                       ? 12
                                       : 30));
    const bool hasSub = !index.data(kSubtitleRole).toString().isEmpty();
    return QSize(10, UiScale::dp(hasSub ? 58 : 48));
  }
};
} // namespace

PhoneLibraryNav::PhoneLibraryNav(QWidget *host) : QWidget(host), m_host(host) {
  setObjectName(QStringLiteral("PhoneLibraryNavPill"));
  setAttribute(Qt::WA_StyledBackground, false);
  setAttribute(Qt::WA_TranslucentBackground, true);
  setCursor(Qt::PointingHandCursor);
  setFocusPolicy(Qt::NoFocus);
  setFixedSize(UiScale::dp(kPillW), UiScale::dp(kPillH));
  if (host) {
    host->installEventFilter(this);
    if (QWidget *win = host->window())
      win->installEventFilter(this);
  }
  syncPillGeometry();
  raise();
}

PhoneLibraryNav::~PhoneLibraryNav() {
  if (qApp)
    qApp->removeEventFilter(this);
}

void PhoneLibraryNav::setAccountName(const QString &name) {
  m_accountName = name.trimmed().isEmpty() ? QStringLiteral("Gast") : name.trimmed();
  if (m_account)
    m_account->setText(m_accountName);
}

void PhoneLibraryNav::setQuickNotePaths(const QStringList &recentPaths,
                                        const QStringList &favoritePaths) {
  m_recentNotePaths = recentPaths;
  m_favoriteNotePaths = favoritePaths;
}

void PhoneLibraryNav::setPillVisible(bool on) {
  setVisible(on);
  if (on) {
    syncPillGeometry();
    raise();
  } else {
    closeMenu();
  }
}

bool PhoneLibraryNav::isMenuOpen() const {
  return m_sheet && m_sheet->isVisible();
}

bool PhoneLibraryNav::spaciousMenu() const {
  QWidget *win = window() ? window() : m_host;
  if (!win)
    return false;
  return win->width() >= UiScale::dp(600);
}

bool PhoneLibraryNav::wideMenu() const {
  QWidget *win = window() ? window() : m_host;
  if (!win)
    return false;
  return win->width() >= UiScale::dp(900);
}

void PhoneLibraryNav::emitAndClose(const QString &id) {
  if (id.isEmpty())
    return;
  closeMenu();
  emit menuAction(id);
}

void PhoneLibraryNav::syncPillGeometry() {
  if (!m_host)
    return;
  const int w = UiScale::dp(kPillW);
  const int h = UiScale::dp(kPillH);
  const int x = qMax(0, (m_host->width() - w) / 2);
  const int y = qMax(0, m_host->height() - h - UiScale::safeBottomPx(m_host) -
                            UiScale::dp(10));
  setGeometry(x, y, w, h);
  raise();
}

void PhoneLibraryNav::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);
  QPainterPath path;
  path.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5),
                      height() / 2.0, height() / 2.0);
  p.fillPath(path, QColor(22, 24, 32, 235));
  p.setPen(QPen(QColor(255, 255, 255, 28), 1));
  p.drawPath(path);

  const int mid = width() * 2 / 3;
  p.setPen(QPen(QColor(255, 255, 255, 36), 1));
  p.drawLine(mid, UiScale::dp(12), mid, height() - UiScale::dp(12));

  p.setPen(QColor(244, 242, 255));
  p.setFont(font());
  QFont f = p.font();
  f.setPixelSize(UiScale::sp(13));
  f.setWeight(QFont::DemiBold);
  p.setFont(f);

  // Magnifier
  const int iconY = height() / 2;
  const QPoint c(UiScale::dp(22), iconY);
  p.setBrush(Qt::NoBrush);
  p.setPen(QPen(QColor(232, 228, 255), 1.8));
  p.drawEllipse(c, UiScale::dp(6), UiScale::dp(6));
  p.drawLine(c + QPoint(UiScale::dp(5), UiScale::dp(5)),
             c + QPoint(UiScale::dp(9), UiScale::dp(9)));

  p.setPen(QColor(244, 242, 255));
  p.drawText(QRect(UiScale::dp(36), 0, mid - UiScale::dp(42), height()),
             Qt::AlignVCenter | Qt::AlignLeft, QStringLiteral("Menü"));

  // Burger
  const int hx = (mid + width()) / 2;
  const int hy = height() / 2;
  p.setPen(QPen(QColor(244, 242, 255), 2.0, Qt::SolidLine, Qt::RoundCap));
  p.drawLine(hx - UiScale::dp(9), hy - UiScale::dp(6), hx + UiScale::dp(9),
             hy - UiScale::dp(6));
  p.drawLine(hx - UiScale::dp(9), hy, hx + UiScale::dp(9), hy);
  p.drawLine(hx - UiScale::dp(9), hy + UiScale::dp(6), hx + UiScale::dp(9),
             hy + UiScale::dp(6));
}

void PhoneLibraryNav::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton)
    openMenu();
  QWidget::mousePressEvent(event);
}

void PhoneLibraryNav::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  syncPillGeometry();
}

void PhoneLibraryNav::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Back || event->key() == Qt::Key_Escape) {
    if (isMenuOpen()) {
      closeMenu();
      event->accept();
      return;
    }
  }
  QWidget::keyPressEvent(event);
}

bool PhoneLibraryNav::handleSheetPointer(QObject *watched, QEvent *event) {
  if (!m_sheet || !event)
    return false;
  const QEvent::Type t = event->type();
  if (t != QEvent::MouseButtonPress && t != QEvent::MouseMove &&
      t != QEvent::MouseButtonRelease && t != QEvent::TouchBegin &&
      t != QEvent::TouchUpdate && t != QEvent::TouchEnd)
    return false;

  qreal y = 0;
  bool isPress = false;
  bool isMove = false;
  bool isRelease = false;
  if (t == QEvent::MouseButtonPress || t == QEvent::MouseMove ||
      t == QEvent::MouseButtonRelease) {
    auto *me = static_cast<QMouseEvent *>(event);
    if (t != QEvent::MouseMove && me->button() != Qt::LeftButton)
      return false;
    y = me->globalPosition().y();
    isPress = t == QEvent::MouseButtonPress;
    isMove = t == QEvent::MouseMove;
    isRelease = t == QEvent::MouseButtonRelease;
    if (isPress && watched == m_scrim) {
      closeMenu();
      return true;
    }
    if (isPress && watched == m_sheet && m_card &&
        !m_card->geometry().contains(me->pos())) {
      closeMenu();
      return true;
    }
  } else {
    auto *te = static_cast<QTouchEvent *>(event);
    if (te->points().isEmpty())
      return false;
    y = te->points().first().globalPosition().y();
    isPress = t == QEvent::TouchBegin;
    isMove = t == QEvent::TouchUpdate;
    isRelease = t == QEvent::TouchEnd;
    if (isPress && watched == m_scrim) {
      closeMenu();
      return true;
    }
  }

  const bool onHandleZone = (watched == m_card);
  if (isPress && onHandleZone) {
    m_swiping = true;
    m_swipeStartY = y;
    return false;
  }
  if (isMove && m_swiping && (y - m_swipeStartY) >= UiScale::dp(56)) {
    m_swiping = false;
    closeMenu();
    return true;
  }
  if (isRelease)
    m_swiping = false;
  return false;
}

bool PhoneLibraryNav::eventFilter(QObject *watched, QEvent *event) {
  if (m_sheet && event) {
    if (event->type() == QEvent::KeyPress ||
        event->type() == QEvent::ShortcutOverride) {
      auto *ke = static_cast<QKeyEvent *>(event);
      if (ke->key() == Qt::Key_Back || ke->key() == Qt::Key_Escape) {
        closeMenu();
        return true;
      }
    }
    if (event->type() == QEvent::MouseButtonPress) {
      auto *me = static_cast<QMouseEvent *>(event);
      if (me->button() == Qt::LeftButton && m_card) {
        const QPoint gp = me->globalPosition().toPoint();
        const QRect cardGlobal(m_card->mapToGlobal(QPoint(0, 0)), m_card->size());
        if (!cardGlobal.contains(gp)) {
          closeMenu();
          return true;
        }
        const int localY = m_card->mapFromGlobal(gp).y();
        if (localY >= 0 && localY < UiScale::dp(52)) {
          m_swiping = true;
          m_swipeStartY = gp.y();
        }
      }
    }
    if (event->type() == QEvent::MouseMove && m_swiping) {
      auto *me = static_cast<QMouseEvent *>(event);
      if ((me->globalPosition().y() - m_swipeStartY) >= UiScale::dp(48)) {
        m_swiping = false;
        closeMenu();
        return true;
      }
    }
    if (event->type() == QEvent::MouseButtonRelease)
      m_swiping = false;
    if (event->type() == QEvent::TouchBegin && m_card) {
      auto *te = static_cast<QTouchEvent *>(event);
      if (!te->points().isEmpty()) {
        const QRect cardGlobal(m_card->mapToGlobal(QPoint(0, 0)), m_card->size());
        if (!cardGlobal.contains(te->points().first().globalPosition().toPoint())) {
          closeMenu();
          return true;
        }
      }
    }
  }
  if (event && (event->type() == QEvent::Resize || event->type() == QEvent::Show) &&
      isVisible())
    syncPillGeometry();
  if (m_sheet && watched == m_sheet->parentWidget() && event &&
      event->type() == QEvent::Resize && m_sheet->parentWidget()) {
    m_sheet->setGeometry(m_sheet->parentWidget()->rect());
  }
  if (handleSheetPointer(watched, event))
    return true;
  return QWidget::eventFilter(watched, event);
}

void PhoneLibraryNav::addSpacer() {
  if (!m_list)
    return;
  auto *item = new QListWidgetItem(m_list);
  item->setText(QString());
  item->setFlags(Qt::ItemIsEnabled);
  item->setSizeHint(QSize(item->sizeHint().width(), UiScale::dp(14)));
  item->setData(Qt::UserRole, QString());
  item->setData(kSectionRole, true);
}

void PhoneLibraryNav::addSection(const QString &title) {
  if (!m_list)
    return;
  auto *item = new QListWidgetItem(m_list);
  item->setText(title.toUpper());
  item->setFlags(Qt::ItemIsEnabled);
  item->setData(Qt::UserRole, QString());
  item->setData(kSectionRole, true);
  item->setForeground(QBrush(QColor(QStringLiteral("#8B92A8"))));
  QFont f = m_list->font();
  f.setPixelSize(UiScale::sp(11));
  f.setWeight(QFont::DemiBold);
  f.setLetterSpacing(QFont::PercentageSpacing, 112);
  item->setFont(f);
  item->setSizeHint(QSize(item->sizeHint().width(), UiScale::dp(32)));
}

void PhoneLibraryNav::addRow(const QString &id, const QString &title,
                             const QString &subtitle, bool selected,
                             bool chevron) {
  if (!m_list)
    return;
  auto *item = new QListWidgetItem(m_list);
  item->setText(title);
  item->setData(Qt::UserRole, id);
  item->setData(kSectionRole, false);
  item->setData(kSubtitleRole, subtitle);
  item->setData(kChevronRole, chevron);
  item->setForeground(QBrush(QColor(QStringLiteral("#F4F2FF"))));
  item->setSizeHint(QSize(item->sizeHint().width(),
                          UiScale::dp(subtitle.isEmpty() ? 48 : 58)));
  if (selected)
    item->setSelected(true);
}

void PhoneLibraryNav::rebuildMenu() {
  if (!m_list)
    return;
  m_list->clear();
  const bool roomy = spaciousMenu();
  addSection(QStringLiteral("Bibliothek"));
  addRow(QStringLiteral("notes"), QStringLiteral("Notizen"),
         roomy ? QStringLiteral("Lokale Bibliothek") : QString(), true);
  addRow(QStringLiteral("study"), QStringLiteral("Study"),
         roomy ? QStringLiteral("Kurse, Login und Lernen") : QString());
  addRow(QStringLiteral("device"), QStringLiteral("Gerät"),
         roomy ? QStringLiteral("Dateien auf diesem Gerät") : QString());
  addRow(QStringLiteral("tags"), QStringLiteral("Tags"),
         roomy ? QStringLiteral("Notizen nach Tags filtern") : QString(), false,
         true);
  addSpacer();
  addSection(QStringLiteral("Cloud"));
  const auto clouds = CloudStorageStore::load();
  for (const CloudStorageEntry &e : clouds) {
    const QString name = e.name.isEmpty()
                             ? CloudStorageStore::displayNameForType(e.type)
                             : e.name;
    QString sub;
    if (roomy) {
      sub = e.webConnected ? QStringLiteral("Verbunden · in Blop öffnen")
                           : QStringLiteral("In Blop öffnen");
      if (!e.path.isEmpty())
        sub = QStringLiteral("Sync-Ordner · in Blop öffnen");
    }
    addRow(QStringLiteral("cloud:") + e.id, name, sub);
  }
  addRow(QStringLiteral("cloud_add"), QStringLiteral("Eigene Cloud hinzufügen…"),
         roomy ? QStringLiteral("Adresse oder Anbieter verknüpfen") : QString());
  addSpacer();
  addSection(QStringLiteral("Konto"));
  addRow(QStringLiteral("settings"), QStringLiteral("Einstellungen"),
         roomy ? QStringLiteral("Konto, Design und Speicher") : QString(), false,
         true);
}

namespace {
QString noteDisplayName(const QString &path) {
  const QFileInfo fi(path);
  const QString base = fi.completeBaseName();
  return base.isEmpty() ? fi.fileName() : base;
}

QPushButton *makeQuickButton(QWidget *parent, const QString &title, bool primary) {
  auto *btn = new QPushButton(title, parent);
  btn->setCursor(Qt::PointingHandCursor);
  btn->setMinimumHeight(UiScale::dp(primary ? 50 : 42));
  btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  if (primary) {
    btn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #5B9DFF; color: #0B1220;"
        "  border: none; border-radius: 12px; padding: 10px 14px;"
        "  font-size: 14px; font-weight: 700; text-align: left; }"
        "QPushButton:hover { background: #7CB0FF; }"
        "QPushButton:pressed { background: #3D86F0; }"));
  } else {
    btn->setStyleSheet(QStringLiteral(
        "QPushButton { background: rgba(255,255,255,0.07); color: #F4F2FF;"
        "  border: 1px solid rgba(255,255,255,0.08); border-radius: 12px;"
        "  padding: 9px 14px; font-size: 13px; font-weight: 600; text-align: left; }"
        "QPushButton:hover { background: rgba(255,255,255,0.12); }"));
  }
  return btn;
}

QLabel *makeMutedHint(QWidget *parent, const QString &text) {
  auto *lbl = new QLabel(text, parent);
  lbl->setWordWrap(true);
  lbl->setStyleSheet(QStringLiteral(
      "color: #8B92A8; font-size: 12px; font-weight: 500; background: transparent;"));
  return lbl;
}

QLabel *makeColCaption(QWidget *parent, const QString &text) {
  auto *lbl = new QLabel(text.toUpper(), parent);
  lbl->setStyleSheet(QStringLiteral(
      "color: #8B92A8; font-size: 11px; font-weight: 700; letter-spacing: 1px;"
      "background: transparent; padding: 4px 2px 2px 2px;"));
  return lbl;
}
} // namespace

QWidget *PhoneLibraryNav::buildQuickAccess(QWidget *parent) {
  auto *col = new QWidget(parent);
  col->setObjectName(QStringLiteral("PhoneBurgerQuickCol"));
  col->setAttribute(Qt::WA_TranslucentBackground, true);
  col->setAutoFillBackground(false);
  col->setStyleSheet(QStringLiteral(
      "QWidget#PhoneBurgerQuickCol { background: transparent; }"));
  auto *v = new QVBoxLayout(col);
  v->setContentsMargins(UiScale::dp(6), 0, 0, 0);
  v->setSpacing(UiScale::dp(8));

  auto *head = new QLabel(QStringLiteral("Schnellzugriff"), col);
  head->setStyleSheet(QStringLiteral(
      "color: #F4F2FF; font-size: 16px; font-weight: 700; background: transparent;"));
  v->addWidget(head);
  v->addWidget(makeMutedHint(
      col, QStringLiteral("Konto, neue Notiz und zuletzt geöffnete Dateien.")));

  auto *acc = new QFrame(col);
  acc->setObjectName(QStringLiteral("PhoneBurgerAccountCard"));
  acc->setStyleSheet(QStringLiteral(
      "QFrame#PhoneBurgerAccountCard {"
      "  background: rgba(255,255,255,0.06);"
      "  border: 1px solid rgba(255,255,255,0.08);"
      "  border-radius: 14px;"
      "}"));
  auto *al = new QHBoxLayout(acc);
  al->setContentsMargins(UiScale::dp(12), UiScale::dp(10), UiScale::dp(12),
                         UiScale::dp(10));
  al->setSpacing(UiScale::dp(10));
  auto *avatar = new QLabel(acc);
  const QString initial =
      m_accountName.isEmpty() ? QStringLiteral("G") : m_accountName.left(1).toUpper();
  avatar->setText(initial);
  avatar->setAlignment(Qt::AlignCenter);
  avatar->setFixedSize(UiScale::dp(36), UiScale::dp(36));
  avatar->setStyleSheet(QStringLiteral(
      "background: #5B9DFF; color: #0B1220; border-radius: 18px;"
      "font-size: 15px; font-weight: 800;"));
  al->addWidget(avatar, 0, Qt::AlignVCenter);
  auto *accText = new QWidget(acc);
  auto *accCol = new QVBoxLayout(accText);
  accCol->setContentsMargins(0, 0, 0, 0);
  accCol->setSpacing(0);
  const bool guest =
      m_accountName.isEmpty() || m_accountName == QLatin1String("Gast");
  auto *accName = new QLabel(guest ? QStringLiteral("Gast") : m_accountName, accText);
  accName->setStyleSheet(QStringLiteral(
      "color: #F4F2FF; font-size: 14px; font-weight: 700; background: transparent;"));
  auto *accSub = new QLabel(guest ? QStringLiteral("Nicht angemeldet")
                                  : QStringLiteral("Angemeldet"),
                            accText);
  accSub->setStyleSheet(QStringLiteral(
      "color: #9AA3BB; font-size: 12px; font-weight: 500; background: transparent;"));
  accCol->addWidget(accName);
  accCol->addWidget(accSub);
  al->addWidget(accText, 1);
  v->addWidget(acc);

  auto *btnNew = makeQuickButton(col, QStringLiteral("Neue Notiz"), true);
  connect(btnNew, &QPushButton::clicked, this,
          [this]() { emitAndClose(QStringLiteral("new_note")); });
  v->addWidget(btnNew);

  auto *shortcuts = new QWidget(col);
  auto *sh = new QHBoxLayout(shortcuts);
  sh->setContentsMargins(0, 0, 0, 0);
  sh->setSpacing(UiScale::dp(8));
  auto *btnStudy = makeQuickButton(shortcuts, QStringLiteral("Study"), false);
  auto *btnSettings = makeQuickButton(shortcuts, QStringLiteral("Einstellungen"), false);
  connect(btnStudy, &QPushButton::clicked, this,
          [this]() { emitAndClose(QStringLiteral("study")); });
  connect(btnSettings, &QPushButton::clicked, this,
          [this]() { emitAndClose(QStringLiteral("settings")); });
  sh->addWidget(btnStudy);
  sh->addWidget(btnSettings);
  v->addWidget(shortcuts);

  auto addNoteButtons = [this, col, v](const QString &caption,
                                       const QStringList &paths) {
    if (paths.isEmpty())
      return 0;
    v->addWidget(makeColCaption(col, caption));
    int n = 0;
    for (const QString &path : paths) {
      if (path.trimmed().isEmpty())
        continue;
      auto *btn = makeQuickButton(col, noteDisplayName(path), false);
      btn->setToolTip(path);
      connect(btn, &QPushButton::clicked, this, [this, path]() {
        emitAndClose(QStringLiteral("note:") + path);
      });
      v->addWidget(btn);
      ++n;
      if (n >= 4)
        break;
    }
    return n;
  };

  const int favCount = addNoteButtons(QStringLiteral("Favoriten"), m_favoriteNotePaths);
  const int recentCount =
      addNoteButtons(QStringLiteral("Zuletzt"), m_recentNotePaths);
  if (favCount == 0 && recentCount == 0)
    v->addWidget(makeMutedHint(
        col, QStringLiteral("Öffne eine Notiz, dann erscheint sie hier.")));

  v->addStretch(1);
  return col;
}

void PhoneLibraryNav::closeMenu() {
  m_swiping = false;
  if (qApp)
    qApp->removeEventFilter(this);
  if (!m_sheet)
    return;
  m_sheet->hide();
  m_sheet->deleteLater();
  m_sheet = nullptr;
  m_card = nullptr;
  m_scrim = nullptr;
  m_list = nullptr;
  m_search = nullptr;
  m_account = nullptr;
}

void PhoneLibraryNav::openMenu() {
  QWidget *win = window() ? window() : m_host;
  if (!win)
    return;
  if (m_sheet) {
    closeMenu();
    return;
  }

  emit menuAboutToOpen();

  auto *sheet = new QWidget(win);
  m_sheet = sheet;
  sheet->setObjectName(QStringLiteral("PhoneLibraryMenuSheet"));
  sheet->setAttribute(Qt::WA_StyledBackground, true);
  sheet->setFocusPolicy(Qt::StrongFocus);
  sheet->setStyleSheet(QStringLiteral(
      "QWidget#PhoneLibraryMenuSheet { background: rgba(6,8,14,0.55); }"));
  sheet->setGeometry(win->rect());
  win->installEventFilter(this);
  sheet->installEventFilter(this);
  qApp->installEventFilter(this);

  auto *root = new QVBoxLayout(sheet);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(0);

  auto *scrim = new QWidget(sheet);
  m_scrim = scrim;
  scrim->setObjectName(QStringLiteral("PhoneLibraryMenuScrim"));
  scrim->setAttribute(Qt::WA_StyledBackground, true);
  scrim->setCursor(Qt::ArrowCursor);
  scrim->setStyleSheet(QStringLiteral(
      "QWidget#PhoneLibraryMenuScrim { background: rgba(6,8,14,0.62); }"));
  scrim->installEventFilter(this);
  root->addWidget(scrim, 1);

  const bool roomy = spaciousMenu();
  const bool wide = wideMenu();
  const int side = roomy ? qMax(UiScale::dp(28), win->width() / 18)
                         : UiScale::dp(10);
  const int bottomPad = roomy ? UiScale::dp(36)
                              : (UiScale::dp(10) + UiScale::safeBottomPx(win));
  const int cardCap = wide ? UiScale::dp(920) : UiScale::dp(560);
  const int maxCardW =
      roomy ? qMin(cardCap, qMax(UiScale::dp(320), win->width() - 2 * side))
            : qMax(UiScale::dp(280), win->width() - 2 * side);

  auto *card = new QWidget(sheet);
  m_card = card;
  card->setObjectName(QStringLiteral("PhoneLibraryMenuCard"));
  card->setAttribute(Qt::WA_StyledBackground, true);
  card->setStyleSheet(BlopTheme::themed(
      roomy ? QStringLiteral(
                  "QWidget#PhoneLibraryMenuCard {"
                  "  background: #12141C;"
                  "  border-radius: 22px;"
                  "  border: 1px solid rgba(255,255,255,0.10);"
                  "}")
            : QStringLiteral(
                  "QWidget#PhoneLibraryMenuCard {"
                  "  background: #12141C;"
                  "  border-top-left-radius: 22px;"
                  "  border-top-right-radius: 22px;"
                  "  border: 1px solid rgba(255,255,255,0.08);"
                  "}")));
  card->installEventFilter(this);
  if (roomy) {
    auto *shadow = new QGraphicsDropShadowEffect(card);
    shadow->setBlurRadius(48);
    shadow->setOffset(0, 18);
    shadow->setColor(QColor(0, 0, 0, 90));
    card->setGraphicsEffect(shadow);
  }

  const int cardH =
      roomy ? qBound(UiScale::dp(wide ? 520 : 480),
                     int(win->height() * (wide ? 0.80 : 0.74)),
                     win->height() - UiScale::safeTopPx(win) - UiScale::dp(64))
            : qBound(UiScale::dp(360), int(win->height() * 0.58),
                     qMax(UiScale::dp(320),
                          win->height() - UiScale::safeTopPx(win) -
                              UiScale::dp(96)));
  card->setFixedHeight(cardH);
  if (roomy)
    card->setFixedWidth(maxCardW);

  auto *bottomWrap = new QWidget(sheet);
  bottomWrap->setAttribute(Qt::WA_TranslucentBackground, true);
  auto *bl = new QHBoxLayout(bottomWrap);
  bl->setContentsMargins(side, 0, side, bottomPad);
  bl->setSpacing(0);
  if (roomy)
    bl->addStretch(1);
  bl->addWidget(card, roomy ? 0 : 1);
  if (roomy)
    bl->addStretch(1);
  root->addWidget(bottomWrap, 0);

  auto *lay = new QVBoxLayout(card);
  lay->setContentsMargins(UiScale::dp(roomy ? 22 : 18), UiScale::dp(roomy ? 16 : 14),
                          UiScale::dp(roomy ? 22 : 18),
                          roomy ? UiScale::dp(16)
                                : (UiScale::dp(18) + UiScale::safeBottomPx(win)));
  lay->setSpacing(UiScale::dp(roomy ? 10 : 8));

  auto *handle = new QWidget(card);
  handle->setFixedSize(UiScale::dp(36), UiScale::dp(4));
  handle->setStyleSheet(
      QStringLiteral("background: rgba(255,255,255,0.22); border-radius: 2px;"));
  lay->addWidget(handle, 0, Qt::AlignHCenter);

  auto *hdr = new QLabel(QStringLiteral("Menü"), card);
  hdr->setStyleSheet(BlopTheme::themed(
      QStringLiteral("color: #F4F2FF; font-size: 18px; font-weight: 700; "
                     "background: transparent;")));
  lay->addWidget(hdr);
  if (roomy) {
    auto *hdrSub = new QLabel(
        wide ? QStringLiteral("Navigation links · Schnellzugriff rechts")
             : QStringLiteral("Bibliothek, Clouds und Konto"),
        card);
    hdrSub->setStyleSheet(BlopTheme::themed(
        QStringLiteral("color: #9AA3BB; font-size: 12px; font-weight: 500; "
                       "background: transparent; padding-bottom: 2px;")));
    lay->addWidget(hdrSub);
  }

  m_search = new QLineEdit(card);
  m_search->setPlaceholderText(QStringLiteral("Suchen…"));
  m_search->setClearButtonEnabled(true);
  m_search->setMinimumHeight(UiScale::dp(42));
  m_search->setStyleSheet(QStringLiteral(
      "QLineEdit { background: rgba(255,255,255,0.08); color: #E8E4FF;"
      "  border: 1px solid rgba(255,255,255,0.16); border-radius: 12px;"
      "  padding: 8px 12px; font-size: 14px; selection-background-color: #7C5CFC; }"));
  connect(m_search, &QLineEdit::textChanged, this, &PhoneLibraryNav::searchChanged);
  lay->addWidget(m_search);

  m_list = new QListWidget(card);
  m_list->setFrameShape(QFrame::NoFrame);
  m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_list->setUniformItemSizes(false);
  m_list->setSpacing(2);
  m_list->setItemDelegate(new BurgerRowDelegate(m_list));
  {
    QPalette pal = m_list->palette();
    pal.setColor(QPalette::Base, Qt::transparent);
    pal.setColor(QPalette::Text, QColor(QStringLiteral("#F4F2FF")));
    pal.setColor(QPalette::ButtonText, QColor(QStringLiteral("#F4F2FF")));
    pal.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#FFFFFF")));
    pal.setColor(QPalette::Highlight, QColor(124, 92, 252, 120));
    pal.setColor(QPalette::WindowText, QColor(QStringLiteral("#F4F2FF")));
    m_list->setPalette(pal);
  }
  m_list->setStyleSheet(QStringLiteral(
      "QListWidget { background: transparent; border: none; outline: none; color: #F4F2FF; }"
      "QListWidget::item { color: #F4F2FF; padding: 0; border-radius: 10px; }"
      "QListWidget::item:selected { background: transparent; }"
      "QListWidget::item:hover { background: transparent; }"));
  connect(m_list, &QListWidget::itemClicked, this,
          [this](QListWidgetItem *item) {
            if (!item)
              return;
            if (item->data(kSectionRole).toBool())
              return;
            emitAndClose(item->data(Qt::UserRole).toString());
          });

  if (wide) {
    auto *cols = new QWidget(card);
    auto *hl = new QHBoxLayout(cols);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(UiScale::dp(16));

    auto *left = new QWidget(cols);
    auto *leftLay = new QVBoxLayout(left);
    leftLay->setContentsMargins(0, 0, 0, 0);
    leftLay->setSpacing(0);
    leftLay->addWidget(m_list, 1);

    auto *div = new QFrame(cols);
    div->setFixedWidth(1);
    div->setStyleSheet(
        QStringLiteral("background: rgba(255,255,255,0.10); border: none;"));

    auto *quickHost = new QScrollArea(cols);
    quickHost->setWidgetResizable(true);
    quickHost->setFrameShape(QFrame::NoFrame);
    quickHost->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    quickHost->setAutoFillBackground(false);
    QPalette qp = quickHost->palette();
    qp.setColor(QPalette::Window, Qt::transparent);
    qp.setColor(QPalette::Base, Qt::transparent);
    quickHost->setPalette(qp);
    if (quickHost->viewport()) {
      quickHost->viewport()->setAutoFillBackground(false);
      quickHost->viewport()->setPalette(qp);
    }
    quickHost->setStyleSheet(QStringLiteral(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollArea > QWidget { background: transparent; }"
        "QScrollArea QScrollBar:vertical { background: transparent; width: 8px; }"
        "QScrollArea QScrollBar::handle:vertical {"
        "  background: rgba(255,255,255,0.18); border-radius: 4px; min-height: 24px; }"));
    auto *quick = buildQuickAccess(quickHost);
    quick->setAutoFillBackground(false);
    quick->setAttribute(Qt::WA_TranslucentBackground, true);
    quickHost->setWidget(quick);

    hl->addWidget(left, 11);
    hl->addWidget(div, 0);
    hl->addWidget(quickHost, 9);
    lay->addWidget(cols, 1);
  } else {
    lay->addWidget(m_list, 1);
  }

  rebuildMenu();
  m_list->doItemsLayout();
  if (m_list->count() > 0)
    m_list->scrollToItem(m_list->item(0));
  m_list->viewport()->update();
  QTimer::singleShot(0, m_list, [list = m_list]() {
    if (!list)
      return;
    list->doItemsLayout();
    if (list->count() > 0)
      list->scrollToItem(list->item(0));
    list->viewport()->update();
  });

  auto *accountRow = new QWidget(card);
  auto *accLay = new QHBoxLayout(accountRow);
  accLay->setContentsMargins(4, 4, 4, 4);
  m_account = new QLabel(accountRow);
  if (m_accountName.isEmpty() || m_accountName == QLatin1String("Gast")) {
    m_account->setText(roomy ? QStringLiteral("Gast · nicht angemeldet")
                             : QStringLiteral("Gast"));
  } else {
    m_account->setText(roomy ? QStringLiteral("Angemeldet als %1").arg(m_accountName)
                             : m_accountName);
  }
  m_account->setStyleSheet(BlopTheme::themed(
      QStringLiteral("color: rgba(200,208,235,0.88); font-size: 13px; "
                     "font-weight: 600; background: transparent;")));
  accLay->addWidget(m_account, 1);
  auto *btnClose = new QPushButton(QStringLiteral("Schließen"), accountRow);
  btnClose->setCursor(Qt::PointingHandCursor);
  btnClose->setStyleSheet(BlopTheme::themed(QStringLiteral(
      "QPushButton { background: rgba(255,255,255,0.08); color: #F4F2FF;"
      "  border: none; border-radius: 10px; padding: 8px 14px; font-weight: 700; }")));
  connect(btnClose, &QPushButton::clicked, this, &PhoneLibraryNav::closeMenu);
  accLay->addWidget(btnClose);
  lay->addWidget(accountRow);

  sheet->show();
  sheet->raise();
  sheet->setFocus(Qt::OtherFocusReason);
  raise();
}
