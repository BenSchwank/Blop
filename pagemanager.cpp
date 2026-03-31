#include "pagemanager.h"
#include <QAbstractItemModel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QScroller>
#include <QMenu>
#include <QMouseEvent>
#include <QInputDialog>
#include <QEvent>
#include <QKeyEvent>
#include <QShowEvent>
#include <QListWidgetItem>

namespace {
constexpr int kPanelWidth = 320;

// Aligned with MultiPageNoteView #PagesBarStrip — modern neutral + accent
static const QColor kCardIdle(52, 54, 64, 249);
static const QColor kCardSelected(62, 64, 78, 252);
static const QColor kBorderSubtle(110, 115, 140, 102);
static const QColor kAccent(107, 163, 245);
static const QColor kTextPrimary(220, 222, 232, 242);
static const QColor kTextMuted(160, 165, 185);
} // namespace

// ============================================================================
// PageListDelegate
// ============================================================================

PageListDelegate::PageListDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

QSize PageListDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
    Q_UNUSED(index);
    return QSize(option.rect.width(), 120);
}

static QRect getMenuButtonRect(const QRect& itemRect) {
    const int side = 44;
    return QRect(itemRect.right() - side - 6, itemRect.top() + 6, side, side);
}

void PageListDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    QRect cardRect = option.rect.adjusted(10, 5, -10, -5);

    QColor bgColor = kCardIdle;
    if (option.state & QStyle::State_Selected) {
        bgColor = kCardSelected;
        painter->setPen(QPen(kAccent, 2));
    } else {
        painter->setPen(QPen(QColor(kBorderSubtle.red(), kBorderSubtle.green(), kBorderSubtle.blue(), 90), 1));
    }
    painter->setBrush(bgColor);
    painter->drawRoundedRect(cardRect, 14, 14);

    int thumbW = 60;
    int thumbH = 80;
    QRect thumbRect(cardRect.left() + 14, cardRect.center().y() - thumbH / 2, thumbW, thumbH);

    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(28, 30, 40));
    painter->drawRoundedRect(thumbRect, 6, 6);

    QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
    QPixmap thumb = icon.pixmap(120, 160);
    if (!thumb.isNull()) {
        painter->drawPixmap(thumbRect, thumb.scaled(thumbRect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    int textLeft = thumbRect.right() + 14;
    int textRight = cardRect.right() - 44;

    QString title = index.data(Qt::UserRole + 1).toString();
    if (title.isEmpty()) title = index.data(Qt::DisplayRole).toString();

    QRect titleRect(textLeft, thumbRect.top() + 5, textRight - textLeft, 25);
    painter->setPen(QColor(kTextPrimary.red(), kTextPrimary.green(), kTextPrimary.blue()));
    QFont titleFont = painter->font();
    titleFont.setBold(true);
    titleFont.setPointSize(11);
    painter->setFont(titleFont);
    painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter, title);

    QString subTitle = index.data(Qt::DisplayRole).toString();
    QRect subRect(textLeft, titleRect.bottom() + 2, textRight - textLeft, 20);
    painter->setPen(kTextMuted);
    QFont subFont = painter->font();
    subFont.setBold(false);
    subFont.setPointSize(9);
    painter->setFont(subFont);
    painter->drawText(subRect, Qt::AlignLeft | Qt::AlignVCenter, subTitle);

    QRect menuRect = getMenuButtonRect(cardRect);
    painter->setBrush(QColor(255, 255, 255, 38));
    painter->setPen(Qt::NoPen);
    painter->drawRoundedRect(menuRect, 10, 10);
    const qreal cx = menuRect.center().x();
    const qreal cy = menuRect.center().y();
    const qreal dotR = 3.5;
    const qreal gap = 8.0;
    painter->setBrush(QColor(kTextPrimary.red(), kTextPrimary.green(), kTextPrimary.blue()));
    painter->drawEllipse(QPointF(cx - gap, cy), dotR, dotR);
    painter->drawEllipse(QPointF(cx, cy), dotR, dotR);
    painter->drawEllipse(QPointF(cx + gap, cy), dotR, dotR);

    painter->restore();
}

bool PageListDelegate::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index) {
    if (event->type() == QEvent::MouseButtonRelease) {
        QMouseEvent *me = static_cast<QMouseEvent*>(event);
        QRect cardRect = option.rect.adjusted(10, 5, -10, -5);
        if (getMenuButtonRect(cardRect).contains(me->pos())) {
            emit menuRequested(me->globalPosition().toPoint(), index.row());
            return true;
        }
    }
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

// ============================================================================
// PageManager overlay (in-editor, not a separate window)
// ============================================================================

PageManager::PageManager(QWidget* parent) : QWidget(parent) {
    setWindowFlags(Qt::Widget);
    setAttribute(Qt::WA_TranslucentBackground);
    setFocusPolicy(Qt::StrongFocus);
    hide();

    setupUi();

    if (parent)
        parent->installEventFilter(this);
}

void PageManager::setupUi() {
    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_scrim = new QPushButton(this);
    m_scrim->setFlat(true);
    m_scrim->setCursor(Qt::PointingHandCursor);
    m_scrim->setFocusPolicy(Qt::NoFocus);
    m_scrim->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_scrim->setStyleSheet(
        "QPushButton { background-color: rgba(12, 14, 22, 0.52); border: none; }"
        "QPushButton:hover { background-color: rgba(12, 14, 22, 0.58); }");
    connect(m_scrim, &QPushButton::clicked, this, &QWidget::hide);

    m_panel = new QWidget(this);
    m_panel->setFixedWidth(kPanelWidth);
    m_panel->setObjectName(QStringLiteral("PageManagerPanel"));
    m_panel->setStyleSheet(
        "#PageManagerPanel {"
        "  background-color: rgba(30, 32, 44, 0.98);"
        "  border-left: 1px solid rgba(120, 130, 160, 0.22);"
        "  border-top-left-radius: 16px;"
        "  border-bottom-left-radius: 16px;"
        "}");

    auto *contentLay = new QVBoxLayout(m_panel);
    contentLay->setContentsMargins(0, 0, 0, 0);
    contentLay->setSpacing(0);

    auto *header = new QWidget(m_panel);
    header->setFixedHeight(72);
    header->setStyleSheet("background: transparent; border-bottom: 1px solid rgba(120, 130, 160, 0.18);");
    auto *headLay = new QVBoxLayout(header);
    headLay->setContentsMargins(20, 14, 16, 12);
    headLay->setSpacing(2);

    auto *titleRow = new QHBoxLayout();
    titleRow->setContentsMargins(0, 0, 0, 0);
    titleRow->setSpacing(8);

    m_lblTitle = new QLabel(QStringLiteral("Seiten"), header);
    m_lblTitle->setStyleSheet(
        "color: rgba(235, 237, 245, 0.98); font-weight: 700; font-size: 17px; letter-spacing: 0.2px;");

    m_btnClose = new QPushButton(QStringLiteral("✕"), header);
    m_btnClose->setFixedSize(36, 36);
    m_btnClose->setCursor(Qt::PointingHandCursor);
    m_btnClose->setFocusPolicy(Qt::NoFocus);
    m_btnClose->setStyleSheet(
        "QPushButton { color: rgba(200, 205, 220, 0.9); border: none; font-size: 16px; "
        "border-radius: 10px; background: rgba(255,255,255,0.04); }"
        "QPushButton:hover { color: #fff; background: rgba(255,255,255,0.10); }");
    connect(m_btnClose, &QPushButton::clicked, this, &QWidget::hide);

    titleRow->addWidget(m_lblTitle, 1);
    titleRow->addWidget(m_btnClose, 0, Qt::AlignTop);

    m_lblSubtitle = new QLabel(QStringLiteral("Reihenfolge per Drag & Drop"), header);
    m_lblSubtitle->setStyleSheet("color: rgba(160, 168, 190, 0.85); font-size: 11px;");

    headLay->addLayout(titleRow);
    headLay->addWidget(m_lblSubtitle);
    contentLay->addWidget(header);

    m_listWidget = new QListWidget(m_panel);
    m_listWidget->setFrameShape(QFrame::NoFrame);
    m_listWidget->setStyleSheet(
        "QListWidget { background: transparent; border: none; outline: 0; }"
        "QListWidget::item { background: transparent; }");
    m_listWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_listWidget->setSpacing(0);

    auto *delegate = new PageListDelegate(this);
    m_listWidget->setItemDelegate(delegate);
    connect(delegate, &PageListDelegate::menuRequested, this, &PageManager::showContextMenu);
    connect(m_listWidget, &QListWidget::itemClicked, this, &PageManager::onListItemClicked);

    m_listWidget->setDragEnabled(true);
    m_listWidget->setAcceptDrops(true);
    m_listWidget->setDragDropMode(QAbstractItemView::InternalMove);
    m_listWidget->setDefaultDropAction(Qt::MoveAction);

    connect(m_listWidget->model(), &QAbstractItemModel::rowsMoved, this, &PageManager::onRowMoved);

    QScroller::grabGesture(m_listWidget, QScroller::LeftMouseButtonGesture);

    contentLay->addWidget(m_listWidget, 1);

    auto *footerLay = new QHBoxLayout();
    footerLay->setContentsMargins(0, 8, 20, 20);
    footerLay->addStretch();

    m_fabAdd = new QPushButton(QStringLiteral("＋"), m_panel);
    m_fabAdd->setFixedSize(56, 56);
    m_fabAdd->setCursor(Qt::PointingHandCursor);
    m_fabAdd->setFocusPolicy(Qt::NoFocus);
    m_fabAdd->setStyleSheet(
        "QPushButton {"
        "  background-color: rgba(58, 60, 72, 0.98);"
        "  border: 1px solid rgba(120, 130, 160, 0.35);"
        "  border-radius: 28px;"
        "  color: #6BA3F5;"
        "  font-size: 26px; font-weight: 600;"
        "  padding-bottom: 2px;"
        "}"
        "QPushButton:hover {"
        "  background-color: rgba(68, 70, 86, 0.98);"
        "  border-color: rgba(140, 150, 185, 0.5);"
        "}");
    connect(m_fabAdd, &QPushButton::clicked, this, &PageManager::onAddPage);

    footerLay->addWidget(m_fabAdd);
    contentLay->addLayout(footerLay);

    root->addWidget(m_scrim, 1);
    root->addWidget(m_panel, 0);
}

void PageManager::fillParent() {
    if (parentWidget())
        setGeometry(0, 0, parentWidget()->width(), parentWidget()->height());
}

void PageManager::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    fillParent();
    raise();
    setFocus(Qt::PopupFocusReason);
}

void PageManager::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        hide();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

bool PageManager::eventFilter(QObject* watched, QEvent* event) {
    if (watched == parentWidget() && event->type() == QEvent::Resize)
        fillParent();
    return QWidget::eventFilter(watched, event);
}

void PageManager::onListItemClicked(QListWidgetItem* item) {
    if (!item || !m_listWidget)
        return;
    emit pageSelected(m_listWidget->row(item));
}

void PageManager::setNoteView(MultiPageNoteView* view) {
    m_view = view;
    refreshThumbnails();
}

void PageManager::refreshThumbnails() {
    if (!m_view)
        return;
    m_listWidget->clear();
    Note* note = m_view->note();
    if (!note)
        return;

    for (int i = 0; i < note->pages.size(); ++i) {
        QPixmap thumb = m_view->generateThumbnail(i, QSize(60, 80));
        auto *item = new QListWidgetItem();

        QString displayTitle = QStringLiteral("Seite %1").arg(i + 1);
        item->setText(displayTitle);

        QString realTitle = note->pages[i].title;
        if (realTitle.isEmpty()) realTitle = displayTitle;
        item->setData(Qt::UserRole + 1, realTitle);

        item->setIcon(QIcon(thumb));
        m_listWidget->addItem(item);
    }
}

void PageManager::onAddPage() {
    if (!m_view || !m_view->note()) return;
    int idx = m_view->note()->pages.size();
    m_view->note()->ensurePage(idx);
    m_view->setNote(m_view->note());
    refreshThumbnails();
    m_listWidget->scrollToBottom();
}

void PageManager::onRowMoved(const QModelIndex&, int start, int, const QModelIndex&, int row) {
    if (!m_view) return;
    int dest = row;
    if (start < row) dest = row - 1;
    if (start != dest) {
        m_view->movePage(start, dest);
        refreshThumbnails();
        emit pageOrderChanged();
    }
}

void PageManager::showContextMenu(const QPoint& pos, int index) {
    if (!m_view) return;
    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background-color: rgba(38, 40, 52, 0.98); color: rgba(235, 237, 245, 0.95); "
        "border: 1px solid rgba(120, 130, 160, 0.35); border-radius: 12px; padding: 6px; font-size: 14px; } "
        "QMenu::item { padding: 12px 28px; min-height: 22px; border-radius: 8px; } "
        "QMenu::item:selected { background-color: rgba(107, 163, 245, 0.35); }");

    QAction* ren = menu.addAction(QStringLiteral("Umbenennen"));
    QAction* dup = menu.addAction(QStringLiteral("Duplizieren"));
    menu.addSeparator();
    QAction* del = menu.addAction(QStringLiteral("Löschen"));

    QAction* res = menu.exec(pos);
    if (res == ren) {
        QString oldName = m_view->note()->pages[index].title;
        bool ok;
        QString newName = QInputDialog::getText(this, QStringLiteral("Seite umbenennen"), QStringLiteral("Name:"),
                                                   QLineEdit::Normal, oldName, &ok);
        if (ok && !newName.isEmpty()) {
            m_view->note()->pages[index].title = newName;
            refreshThumbnails();
        }
    } else if (res == dup) {
        m_view->duplicatePage(index);
        refreshThumbnails();
    } else if (res == del) {
        if (m_view->note()->pages.size() > 1) {
            m_view->deletePage(index);
            refreshThumbnails();
        }
    }
}
