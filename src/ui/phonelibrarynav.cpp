#include "phonelibrarynav.h"

#include "blop_theme.h"
#include "cloudstoragestore.h"
#include "uiscale.h"

#include <QEvent>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QResizeEvent>
#include <QVBoxLayout>

namespace {
constexpr int kPillW = 168;
constexpr int kPillH = 48;
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

  // Hamburger
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

bool PhoneLibraryNav::eventFilter(QObject *watched, QEvent *event) {
  if (event && (event->type() == QEvent::Resize || event->type() == QEvent::Show) &&
      isVisible())
    syncPillGeometry();
  if (m_sheet && watched == m_sheet->parentWidget() && event &&
      event->type() == QEvent::Resize && m_sheet->parentWidget()) {
    m_sheet->setGeometry(m_sheet->parentWidget()->rect());
  }
  Q_UNUSED(watched);
  return QWidget::eventFilter(watched, event);
}

void PhoneLibraryNav::addRow(const QString &id, const QString &title,
                             bool selected, bool chevron) {
  if (!m_list)
    return;
  auto *item = new QListWidgetItem(m_list);
  item->setText(title + (chevron ? QStringLiteral("  ›") : QString()));
  item->setData(Qt::UserRole, id);
  item->setSizeHint(QSize(item->sizeHint().width(), UiScale::dp(48)));
  if (selected)
    item->setSelected(true);
}

void PhoneLibraryNav::rebuildMenu() {
  if (!m_list)
    return;
  m_list->clear();
  addRow(QStringLiteral("notes"), QStringLiteral("Notizen"), true);
  addRow(QStringLiteral("study"), QStringLiteral("Study"));
  addRow(QStringLiteral("device"), QStringLiteral("Gerät"));
  addRow(QStringLiteral("tags"), QStringLiteral("Tags"), false, true);
  for (const CloudStorageEntry &e : CloudStorageStore::load()) {
    addRow(QStringLiteral("cloud:") + e.id, e.name.isEmpty()
                                                ? CloudStorageStore::displayNameForType(e.type)
                                                : e.name);
  }
  addRow(QStringLiteral("cloud_add"), QStringLiteral("Eigene Cloud hinzufügen…"));
  addRow(QStringLiteral("settings"), QStringLiteral("Einstellungen"), false, true);
}

void PhoneLibraryNav::closeMenu() {
  if (!m_sheet)
    return;
  m_sheet->hide();
  m_sheet->deleteLater();
  m_sheet = nullptr;
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
  sheet->setStyleSheet(QStringLiteral(
      "QWidget#PhoneLibraryMenuSheet { background: rgba(6,8,14,0.72); }"));
  sheet->setGeometry(win->rect());
  win->installEventFilter(this);

  auto *card = new QWidget(sheet);
  card->setObjectName(QStringLiteral("PhoneLibraryMenuCard"));
  card->setAttribute(Qt::WA_StyledBackground, true);
  card->setStyleSheet(BlopTheme::themed(QStringLiteral(
      "QWidget#PhoneLibraryMenuCard {"
      "  background: #12141C;"
      "  border-top-left-radius: 22px;"
      "  border-top-right-radius: 22px;"
      "  border: 1px solid rgba(255,255,255,0.08);"
      "}")));

  auto *root = new QVBoxLayout(sheet);
  root->setContentsMargins(0, 0, 0, 0);
  root->addStretch(1);

  const int cardH = qBound(UiScale::dp(420), int(win->height() * 0.82),
                           win->height() - UiScale::safeTopPx(win));
  card->setFixedHeight(cardH);
  root->addWidget(card, 0);

  auto *lay = new QVBoxLayout(card);
  lay->setContentsMargins(UiScale::dp(18), UiScale::dp(14), UiScale::dp(18),
                          UiScale::dp(18) + UiScale::safeBottomPx(win));
  lay->setSpacing(UiScale::dp(10));

  auto *handle = new QWidget(card);
  handle->setFixedSize(UiScale::dp(36), UiScale::dp(4));
  handle->setStyleSheet(
      QStringLiteral("background: rgba(255,255,255,0.22); border-radius: 2px;"));
  lay->addWidget(handle, 0, Qt::AlignHCenter);

  auto *hdr = new QLabel(QStringLiteral("Blop"), card);
  hdr->setStyleSheet(BlopTheme::themed(
      QStringLiteral("color: #F4F2FF; font-size: 18px; font-weight: 700; "
                     "background: transparent;")));
  lay->addWidget(hdr);

  m_search = new QLineEdit(card);
  m_search->setPlaceholderText(QStringLiteral("Suchen…"));
  m_search->setClearButtonEnabled(true);
  m_search->setMinimumHeight(UiScale::dp(42));
  m_search->setStyleSheet(BlopTheme::themed(QStringLiteral(
      "QLineEdit { background: rgba(255,255,255,0.06); color: #E8E4FF;"
      "  border: 1px solid rgba(120,130,160,0.28); border-radius: 12px;"
      "  padding: 8px 12px; font-size: 14px; }")));
  connect(m_search, &QLineEdit::textChanged, this, &PhoneLibraryNav::searchChanged);
  lay->addWidget(m_search);

  m_list = new QListWidget(card);
  m_list->setFrameShape(QFrame::NoFrame);
  m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_list->setStyleSheet(BlopTheme::themed(QStringLiteral(
      "QListWidget { background: transparent; border: none; outline: none; }"
      "QListWidget::item { color: #ECEEFD; padding: 10px 12px; border-radius: 10px;"
      "  font-size: 15px; font-weight: 600; }"
      "QListWidget::item:selected { background: rgba(124,92,252,0.28); color: #FFFFFF; }"
      "QListWidget::item:hover { background: rgba(255,255,255,0.06); }")));
  connect(m_list, &QListWidget::itemClicked, this,
          [this](QListWidgetItem *item) {
            if (!item)
              return;
            const QString id = item->data(Qt::UserRole).toString();
            closeMenu();
            emit menuAction(id);
          });
  lay->addWidget(m_list, 1);
  rebuildMenu();

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
  m_search->setFocus(Qt::OtherFocusReason);
  raise();
}
