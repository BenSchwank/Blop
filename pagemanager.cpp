#include "pagemanager.h"
#include "UIStyles.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QScroller> // Wichtig
#include <QGraphicsDropShadowEffect>
#include <QMenu>
#include <QMouseEvent>
#include <QApplication>

// ============================================================================
// PageListDelegate
// ============================================================================

PageListDelegate::PageListDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

QSize PageListDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
    return QSize(option.rect.width(), 180);
}

// Helper: Wo liegen die 3 Punkte?
static QRect getMenuButtonRect(const QRect& itemRect) {
    return QRect(itemRect.right() - 35, itemRect.top() + 5, 30, 30);
}

void PageListDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    QRect rect = option.rect;
    rect.adjust(10, 10, -10, -10);

    // Auswahl / Hover Hintergrund
    if (option.state & QStyle::State_Selected) {
        painter->setBrush(QColor(0x5E5CE6));
        painter->setPen(Qt::NoPen);
        painter->drawRoundedRect(option.rect.adjusted(4, 4, -4, -4), 8, 8);
    } else if (option.state & QStyle::State_MouseOver) {
        painter->setBrush(QColor(255, 255, 255, 20));
        painter->setPen(Qt::NoPen);
        painter->drawRoundedRect(option.rect.adjusted(4, 4, -4, -4), 8, 8);
    }

    // Thumbnail holen
    QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
    QPixmap thumb = icon.pixmap(150, 200);

    int thumbW = 120;
    int thumbH = 140;
    QRect thumbRect(rect.center().x() - thumbW/2, rect.top(), thumbW, thumbH);

    // Papier-Look
    painter->setBrush(QColor(0,0,0,50));
    painter->setPen(Qt::NoPen);
    painter->drawRoundedRect(thumbRect.translated(2, 2), 4, 4);

    painter->setBrush(Qt::white);
    painter->drawRoundedRect(thumbRect, 4, 4);

    if (!thumb.isNull()) {
        painter->drawPixmap(thumbRect, thumb);
    }

    // Seitennummer Text
    QString text = index.data(Qt::DisplayRole).toString();
    QRect textRect(rect.left(), thumbRect.bottom() + 5, rect.width(), 20);

    painter->setPen(option.state & QStyle::State_Selected ? Qt::white : QColor(200, 200, 200));
    QFont font = painter->font();
    font.setBold(true);
    font.setPointSize(10);
    painter->setFont(font);
    painter->drawText(textRect, Qt::AlignCenter, text);

    // --- 3 PUNKTE MENÜ ZEICHNEN ---
    QRect menuRect = getMenuButtonRect(option.rect);
    painter->setBrush(QColor(0,0,0,40));
    painter->setPen(Qt::NoPen);
    painter->drawEllipse(menuRect);

    painter->setBrush(Qt::white);
    int dotSize = 3;
    int spacing = 5;
    int cx = menuRect.center().x();
    int cy = menuRect.center().y();

    painter->drawEllipse(QPoint(cx, cy), dotSize, dotSize);
    painter->drawEllipse(QPoint(cx - spacing - dotSize, cy), dotSize, dotSize);
    painter->drawEllipse(QPoint(cx + spacing + dotSize, cy), dotSize, dotSize);

    painter->restore();
}

bool PageListDelegate::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index) {
    if (event->type() == QEvent::MouseButtonRelease) {
        QMouseEvent *me = static_cast<QMouseEvent*>(event);
        QRect menuRect = getMenuButtonRect(option.rect);

        if (menuRect.contains(me->pos())) {
            emit menuRequested(me->globalPosition().toPoint(), index.row());
            return true;
        }
    }
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

// ============================================================================
// PageManager Widget
// ============================================================================

PageManager::PageManager(QWidget* parent) : QWidget(parent) {
    setFixedWidth(240);

    QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(20);
    shadow->setOffset(-5, 0);
    shadow->setColor(QColor(0, 0, 0, 100));
    setGraphicsEffect(shadow);

    setupUi();
}

void PageManager::setupUi() {
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Header
    QWidget* header = new QWidget(this);
    header->setFixedHeight(50);
    header->setStyleSheet("background-color: #252526; border-bottom: 1px solid #333;");
    QHBoxLayout* headLay = new QHBoxLayout(header);
    headLay->setContentsMargins(15, 0, 10, 0);

    m_lblTitle = new QLabel("Seiten", header);
    m_lblTitle->setStyleSheet("color: white; font-weight: bold; font-size: 14px;");

    m_btnClose = new QPushButton("✕", header);
    m_btnClose->setFixedSize(30, 30);
    m_btnClose->setCursor(Qt::PointingHandCursor);
    m_btnClose->setStyleSheet("QPushButton { color: #AAA; border: none; background: transparent; font-size: 14px; } QPushButton:hover { color: white; background: #333; border-radius: 15px; }");
    connect(m_btnClose, &QPushButton::clicked, this, &QWidget::hide);

    headLay->addWidget(m_lblTitle);
    headLay->addStretch();
    headLay->addWidget(m_btnClose);
    layout->addWidget(header);

    // List Widget
    m_listWidget = new QListWidget(this);
    m_listWidget->setFrameShape(QFrame::NoFrame);
    m_listWidget->setStyleSheet("QListWidget { background-color: #1E1E1E; border: none; outline: 0; }");
    m_listWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    PageListDelegate* delegate = new PageListDelegate(this);
    m_listWidget->setItemDelegate(delegate);

    connect(delegate, &PageListDelegate::menuRequested, this, &PageManager::showContextMenu);

    // Drag & Drop
    m_listWidget->setDragEnabled(true);
    m_listWidget->setAcceptDrops(true);
    m_listWidget->setDragDropMode(QAbstractItemView::InternalMove);
    m_listWidget->setDefaultDropAction(Qt::MoveAction);
    m_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);

    // WICHTIG: Scroller aktivieren für Touch/Maus-Drag
    QScroller::grabGesture(m_listWidget, QScroller::LeftMouseButtonGesture);

    layout->addWidget(m_listWidget);

    // Klick -> Scrollen
    connect(m_listWidget, &QListWidget::itemClicked, [this](QListWidgetItem* item){
        int idx = m_listWidget->row(item);
        emit pageSelected(idx);
    });

    // Verschieben
    connect(m_listWidget->model(), &QAbstractItemModel::rowsMoved, this, &PageManager::onRowMoved);
}

void PageManager::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), QColor("#1E1E1E"));
    p.setPen(QColor("#333"));
    p.drawLine(0, 0, 0, height());
}

void PageManager::setNoteView(MultiPageNoteView* view) {
    m_view = view;
    refreshThumbnails();
}

void PageManager::refreshThumbnails() {
    if (!m_view) return;

    bool wasBlocked = m_listWidget->blockSignals(true);
    m_listWidget->clear();

    Note* note = m_view->note();
    if (note) {
        int pageCount = note->pages.size();
        for (int i = 0; i < pageCount; ++i) {
            QPixmap thumb = m_view->generateThumbnail(i, QSize(120, 160));
            QListWidgetItem* item = new QListWidgetItem();
            item->setText(QString("Seite %1").arg(i + 1));
            item->setIcon(QIcon(thumb));
            m_listWidget->addItem(item);
        }
    }
    m_listWidget->blockSignals(wasBlocked);
}

void PageManager::onRowMoved(const QModelIndex&, int start, int, const QModelIndex&, int row) {
    if (!m_view) return;

    int dest = row;
    if (start < row) dest = row - 1;

    if (start != dest) {
        m_view->movePage(start, dest);

        for(int i=0; i < m_listWidget->count(); ++i) {
            m_listWidget->item(i)->setText(QString("Seite %1").arg(i+1));
        }

        emit pageOrderChanged();
    }
}

void PageManager::showContextMenu(const QPoint& pos, int index) {
    if (!m_view) return;

    QMenu menu(this);
    menu.setStyleSheet("QMenu { background-color: #2D2D30; color: white; border: 1px solid #444; border-radius: 8px; padding: 5px; } QMenu::item { padding: 8px 20px; border-radius: 4px; } QMenu::item:selected { background-color: #5E5CE6; }");

    QAction* actCopy = menu.addAction("Kopieren");
    QAction* actDup = menu.addAction("Duplizieren");
    menu.addSeparator();
    QAction* actDel = menu.addAction("Löschen");

    QAction* selected = menu.exec(pos);

    if (selected == actDel) {
        if (m_view->note()->pages.size() <= 1) return;
        m_view->deletePage(index);
        refreshThumbnails();
        emit pageOrderChanged();
    }
    else if (selected == actDup) {
        m_view->duplicatePage(index);
        refreshThumbnails();
        emit pageOrderChanged();
    }
    else if (selected == actCopy) {
        if (index >= 0 && index < m_view->note()->pages.size()) {
            m_copiedPage = m_view->note()->pages[index];
            m_hasCopiedPage = true;
        }
    }
}
