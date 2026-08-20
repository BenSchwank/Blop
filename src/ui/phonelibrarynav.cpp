#include "phonelibrarynav.h"

#include "blop_theme.h"
#include "cloudstoragestore.h"
#include "uiscale.h"

#include <QBrush>
#include <QColor>
#include <QEvent>
#include <QFont>
#include <QFrame>
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
#include <QPushButton>
#include <QResizeEvent>
#include <QTimer>
#include <QTouchEvent>
#include <QVBoxLayout>

namespace {
constexpr int kPillW = 168;
constexpr int kPillH = 48;
constexpr int kSectionRole = Qt::UserRole + 1;
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

void PhoneLibraryNav::setAccountName(const QString &name) {
  m_accountName = name.trimmed().isEmpty() ? QStringLiteral("Gast") : name.trimmed();
  if (m_account)
    m_account->setText(m_accountName);
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
  if (event && (event->type() == QEvent::Resize || event->type() == QEvent::Show) &&
      isVisible())
    syncPillGeometry();
  if (m_sheet && watched == m_sheet->parentWidget() && event &&
      event->type() == QEvent::Resize && m_sheet->parentWidget()) {
    m_sheet->setGeometry(m_sheet->parentWidget()->rect());
  }
  if (event && (event->type() == QEvent::KeyPress ||
                event->type() == QEvent::ShortcutOverride)) {
    auto *ke = static_cast<QKeyEvent *>(event);
    if ((ke->key() == Qt::Key_Back || ke->key() == Qt::Key_Escape) &&
        isMenuOpen()) {
      closeMenu();
      return true;
    }
  }
  if (handleSheetPointer(watched, event))
    return true;
  return QWidget::eventFilter(watched, event);
}

void PhoneLibraryNav::addSpacer() {
  if (!m_list)
    return;
  auto *item = new QListWidgetItem(m_list);
  item->setFlags(Qt::NoItemFlags);
  item->setSizeHint(QSize(item->sizeHint().width(), UiScale::dp(12)));
  item->setData(kSectionRole, true);
}

void PhoneLibraryNav::addSection(const QString &title) {
  if (!m_list)
    return;
  auto *item = new QListWidgetItem(m_list);
  item->setText(title.toUpper());
  item->setFlags(Qt::NoItemFlags);
  item->setData(Qt::UserRole, QString());
  item->setData(kSectionRole, true);
  item->setForeground(QBrush(QColor(QStringLiteral("#8B92A8"))));
  QFont f = m_list->font();
  f.setPixelSize(UiScale::sp(11));
  f.setWeight(QFont::DemiBold);
  f.setLetterSpacing(QFont::PercentageSpacing, 112);
  item->setFont(f);
  item->setSizeHint(QSize(item->sizeHint().width(), UiScale::dp(28)));
}

void PhoneLibraryNav::addRow(const QString &id, const QString &title,
                             bool selected, bool chevron) {
  if (!m_list)
    return;
  auto *item = new QListWidgetItem(m_list);
  item->setText(title + (chevron ? QStringLiteral("  ›") : QString()));
  item->setData(Qt::UserRole, id);
  item->setData(kSectionRole, false);
  item->setForeground(QBrush(QColor(QStringLiteral("#F4F2FF"))));
  item->setSizeHint(QSize(item->sizeHint().width(), UiScale::dp(48)));
  if (selected)
    item->setSelected(true);
}

void PhoneLibraryNav::rebuildMenu() {
  if (!m_list)
    return;
  m_list->clear();
  addSection(QStringLiteral("Bibliothek"));
  addRow(QStringLiteral("notes"), QStringLiteral("Notizen"), true);
  addRow(QStringLiteral("study"), QStringLiteral("Study"));
  addRow(QStringLiteral("device"), QStringLiteral("Gerät"));
  addRow(QStringLiteral("tags"), QStringLiteral("Tags"), false, true);
  addSpacer();
  addSection(QStringLiteral("Cloud"));
  const auto clouds = CloudStorageStore::load();
  for (const CloudStorageEntry &e : clouds) {
    addRow(QStringLiteral("cloud:") + e.id, e.name.isEmpty()
                                                ? CloudStorageStore::displayNameForType(e.type)
                                                : e.name);
  }
  addRow(QStringLiteral("cloud_add"), QStringLiteral("Eigene Cloud hinzufügen…"));
  addSpacer();
  addSection(QStringLiteral("Konto"));
  addRow(QStringLiteral("settings"), QStringLiteral("Einstellungen"), false, true);
}

void PhoneLibraryNav::closeMenu() {
  m_swiping = false;
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

  auto *sheet = new QWidget(win);
  m_sheet = sheet;
  sheet->setObjectName(QStringLiteral("PhoneLibraryMenuSheet"));
  sheet->setAttribute(Qt::WA_StyledBackground, true);
  sheet->setFocusPolicy(Qt::StrongFocus);
  sheet->setStyleSheet(QStringLiteral(
      "QWidget#PhoneLibraryMenuSheet { background: transparent; }"));
  sheet->setGeometry(win->rect());
  win->installEventFilter(this);
  sheet->installEventFilter(this);

  auto *root = new QVBoxLayout(sheet);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(0);

  auto *scrim = new QWidget(sheet);
  m_scrim = scrim;
  scrim->setObjectName(QStringLiteral("PhoneLibraryMenuScrim"));
  scrim->setAttribute(Qt::WA_StyledBackground, true);
  scrim->setCursor(Qt::ArrowCursor);
  scrim->setStyleSheet(QStringLiteral(
      "QWidget#PhoneLibraryMenuScrim { background: rgba(6,8,14,0.72); }"));
  scrim->installEventFilter(this);
  root->addWidget(scrim, 1);

  auto *card = new QWidget(sheet);
  m_card = card;
  card->setObjectName(QStringLiteral("PhoneLibraryMenuCard"));
  card->setAttribute(Qt::WA_StyledBackground, true);
  card->setStyleSheet(BlopTheme::themed(QStringLiteral(
      "QWidget#PhoneLibraryMenuCard {"
      "  background: #12141C;"
      "  border-top-left-radius: 22px;"
      "  border-top-right-radius: 22px;"
      "  border: 1px solid rgba(255,255,255,0.08);"
      "}")));
  card->installEventFilter(this);

  const int cardH = qBound(UiScale::dp(420), int(win->height() * 0.82),
                           win->height() - UiScale::safeTopPx(win));
  card->setFixedHeight(cardH);
  root->addWidget(card, 0);

  auto *lay = new QVBoxLayout(card);
  lay->setContentsMargins(UiScale::dp(18), UiScale::dp(14), UiScale::dp(18),
                          UiScale::dp(18) + UiScale::safeBottomPx(win));
  lay->setSpacing(UiScale::dp(8));

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
      "QListWidget::item { color: #F4F2FF; padding: 10px 14px; border-radius: 10px;"
      "  font-size: 15px; font-weight: 600; }"
      "QListWidget::item:selected { background: rgba(124,92,252,0.45); color: #FFFFFF; }"
      "QListWidget::item:hover { background: rgba(255,255,255,0.10); color: #FFFFFF; }"
      "QListWidget::item:disabled { background: transparent; color: #8B92A8;"
      "  font-size: 11px; font-weight: 700; padding: 6px 14px; }"));
  connect(m_list, &QListWidget::itemClicked, this,
          [this](QListWidgetItem *item) {
            if (!item)
              return;
            if (item->data(kSectionRole).toBool())
              return;
            const QString id = item->data(Qt::UserRole).toString();
            if (id.isEmpty())
              return;
            closeMenu();
            emit menuAction(id);
          });
  lay->addWidget(m_list, 1);
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
  m_account = new QLabel(m_accountName.isEmpty() ? QStringLiteral("Gast")
                                                 : m_accountName,
                         accountRow);
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
