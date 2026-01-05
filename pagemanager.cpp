#include "pagemanager.h"
#include "UIStyles.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QScroller>
#include <QGraphicsDropShadowEffect>
#include <QMenu>
#include <QMouseEvent>
#include <QInputDialog>
#include <QMessageBox>
#include <QApplication>
#include <QEvent>

// ============================================================================
// PageListDelegate
// ============================================================================

PageListDelegate::PageListDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

QSize PageListDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
    return QSize(option.rect.width(), 120);
}

static QRect getMenuButtonRect(const QRect& itemRect) {
    return QRect(itemRect.right() - 35, itemRect.top() + 10, 30, 30);
}

void PageListDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    QRect cardRect = option.rect.adjusted(10, 5, -10, -5);

    QColor bgColor("#252526");
    if (option.state & QStyle::State_Selected) {
        bgColor = QColor("#2D2D30");
        painter->setPen(QPen(QColor("#5E5CE6"), 2));
    } else {
        painter->setPen(Qt::NoPen);
    }
    painter->setBrush(bgColor);
    painter->drawRoundedRect(cardRect, 12, 12);

    // Thumbnail
    int thumbW = 60;
    int thumbH = 80;
    QRect thumbRect(cardRect.left() + 15, cardRect.center().y() - thumbH/2, thumbW, thumbH);

    painter->setPen(Qt::NoPen);
    painter->setBrush(Qt::white);
    painter->drawRoundedRect(thumbRect, 4, 4);

    QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
    QPixmap thumb = icon.pixmap(120, 160);
    if (!thumb.isNull()) {
        painter->drawPixmap(thumbRect, thumb.scaled(thumbRect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    // Text
    int textLeft = thumbRect.right() + 15;
    int textRight = cardRect.right() - 40;

    QString title = index.data(Qt::UserRole + 1).toString();
    if (title.isEmpty()) title = index.data(Qt::DisplayRole).toString();

    QRect titleRect(textLeft, thumbRect.top() + 5, textRight - textLeft, 25);
    painter->setPen(Qt::white);
    QFont titleFont = painter->font();
    titleFont.setBold(true);
    titleFont.setPointSize(11);
    painter->setFont(titleFont);
    painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter, title);

    QString subTitle = index.data(Qt::DisplayRole).toString();
    QRect subRect(textLeft, titleRect.bottom() + 2, textRight - textLeft, 20);
    painter->setPen(QColor("#888"));
    QFont subFont = painter->font();
    subFont.setBold(false);
    subFont.setPointSize(9);
    painter->setFont(subFont);
    painter->drawText(subRect, Qt::AlignLeft | Qt::AlignVCenter, subTitle);

    // Menü Punkte
    QRect menuRect = getMenuButtonRect(cardRect);
    painter->setBrush(Qt::white);
    painter->setPen(Qt::NoPen);
    int dotSize = 4;
    int cx = menuRect.center().x();
    int cy = menuRect.center().y();
    painter->drawEllipse(QPoint(cx - 6, cy), dotSize/2, dotSize/2);
    painter->drawEllipse(QPoint(cx, cy), dotSize/2, dotSize/2);
    painter->drawEllipse(QPoint(cx + 6, cy), dotSize/2, dotSize/2);

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
// PageManager Dialog
// ============================================================================

PageManager::PageManager(QWidget* parent) : QDialog(parent) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground);

    setFixedWidth(320);

    QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(40);
    shadow->setOffset(-5, 0);
    shadow->setColor(QColor(0, 0, 0, 150));
    setGraphicsEffect(shadow);

    setupUi();

    // Parent Resize überwachen, um Position zu halten
    if (parent) parent->installEventFilter(this);
}

void PageManager::setupUi() {
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    QWidget* container = new QWidget(this);
    container->setStyleSheet("background-color: #1E1E1E; border-left: 1px solid #333;");
    layout->addWidget(container);

    QVBoxLayout* contentLay = new QVBoxLayout(container);
    contentLay->setContentsMargins(0, 0, 0, 0);
    contentLay->setSpacing(0);

    // Header
    QWidget* header = new QWidget(container);
    header->setFixedHeight(60);
    header->setStyleSheet("border-bottom: 1px solid #333;");
    QHBoxLayout* headLay = new QHBoxLayout(header);
    headLay->setContentsMargins(20, 0, 15, 0);

    m_lblTitle = new QLabel("Page Manager", header);
    m_lblTitle->setStyleSheet("color: white; font-weight: bold; font-size: 18px;");

    m_btnClose = new QPushButton("✕", header);
    m_btnClose->setFixedSize(30, 30);
    m_btnClose->setCursor(Qt::PointingHandCursor);
    m_btnClose->setStyleSheet("QPushButton { color: #AAA; border: none; font-size: 16px; } QPushButton:hover { color: white; background: #333; border-radius: 15px; }");
    connect(m_btnClose, &QPushButton::clicked, this, &QDialog::accept);

    headLay->addWidget(m_lblTitle);
    headLay->addStretch();
    headLay->addWidget(m_btnClose);
    contentLay->addWidget(header);

    // Liste
    m_listWidget = new QListWidget(container);
    m_listWidget->setFrameShape(QFrame::NoFrame);
    m_listWidget->setStyleSheet("QListWidget { background: transparent; border: none; outline: 0; }");
    m_listWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_listWidget->setSpacing(0);

    PageListDelegate* delegate = new PageListDelegate(this);
    m_listWidget->setItemDelegate(delegate);
    connect(delegate, &PageListDelegate::menuRequested, this, &PageManager::showContextMenu);

    m_listWidget->setDragEnabled(true);
    m_listWidget->setAcceptDrops(true);
    m_listWidget->setDragDropMode(QAbstractItemView::InternalMove);
    m_listWidget->setDefaultDropAction(Qt::MoveAction);

    QScroller::grabGesture(m_listWidget, QScroller::LeftMouseButtonGesture);

    contentLay->addWidget(m_listWidget);

    // Footer Layout für FAB (Wiederhergestellt, damit Button sichtbar ist!)
    QHBoxLayout* footerLay = new QHBoxLayout();
    footerLay->setContentsMargins(0, 10, 20, 20); // Abstand rechts/unten
    footerLay->addStretch();

    m_fabAdd = new QPushButton("+", container);
    m_fabAdd->setFixedSize(56, 56);
    m_fabAdd->setCursor(Qt::PointingHandCursor);
    m_fabAdd->setStyleSheet(
        "QPushButton { "
        "background-color: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #5E5CE6, stop:1 #7c7af0); "
        "color: white; border-radius: 28px; font-size: 30px; border: none; padding-bottom: 4px;"
        "}"
        "QPushButton:hover { background-color: #4b49c9; }"
        );
    QGraphicsDropShadowEffect* fabShadow = new QGraphicsDropShadowEffect(this);
    fabShadow->setBlurRadius(20);
    fabShadow->setColor(QColor(0,0,0,100));
    fabShadow->setOffset(0, 4);
    m_fabAdd->setGraphicsEffect(fabShadow);

    connect(m_fabAdd, &QPushButton::clicked, this, &PageManager::onAddPage);

    footerLay->addWidget(m_fabAdd);
    contentLay->addLayout(footerLay);
}

// --- NEU: Automatische Positionierung am rechten Rand ---
void PageManager::alignToRight() {
    if (parentWidget()) {
        // Globale Koordinate der rechten oberen Ecke des Parents
        QPoint parentTopRight = parentWidget()->mapToGlobal(QPoint(parentWidget()->width(), 0));

        int x = parentTopRight.x() - this->width();
        int y = parentTopRight.y();
        int h = parentWidget()->height();

        // Positionieren und Größe anpassen
        this->setGeometry(x, y, this->width(), h);
    }
}

void PageManager::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    alignToRight(); // Beim Anzeigen positionieren
}

bool PageManager::eventFilter(QObject* watched, QEvent* event) {
    if (watched == parentWidget() && event->type() == QEvent::Resize) {
        alignToRight(); // Mitbewegen wenn Parent Größe ändert
    }
    return QDialog::eventFilter(watched, event);
}
// ---------------------------------------------------------

void PageManager::setNoteView(MultiPageNoteView* view) {
    m_view = view;
    refreshThumbnails();
}

void PageManager::refreshThumbnails() {
    if (!m_view) return;
    m_listWidget->clear();
    Note* note = m_view->note();
    if (!note) return;

    for (int i = 0; i < note->pages.size(); ++i) {
        QPixmap thumb = m_view->generateThumbnail(i, QSize(60, 80));
        QListWidgetItem* item = new QListWidgetItem();

        QString displayTitle = QString("Seite %1").arg(i + 1);
        item->setText(displayTitle);

        QString realTitle = note->pages[i].title;
        if(realTitle.isEmpty()) realTitle = displayTitle;
        item->setData(Qt::UserRole + 1, realTitle);

        item->setIcon(QIcon(thumb));
        m_listWidget->addItem(item);
    }
}

void PageManager::onAddPage() {
    if(!m_view || !m_view->note()) return;
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
    if(!m_view) return;
    QMenu menu(this);
    menu.setStyleSheet("QMenu { background-color: #252526; color: white; border: 1px solid #444; border-radius: 8px; padding: 5px; } QMenu::item { padding: 8px 20px; border-radius: 4px; } QMenu::item:selected { background-color: #5E5CE6; }");

    QAction* ren = menu.addAction("Umbenennen");
    QAction* dup = menu.addAction("Duplizieren");
    menu.addSeparator();
    QAction* del = menu.addAction("Löschen");

    QAction* res = menu.exec(pos);
    if(res == ren) {
        QString oldName = m_view->note()->pages[index].title;
        bool ok;
        QString newName = QInputDialog::getText(this, "Seite umbenennen", "Name:", QLineEdit::Normal, oldName, &ok);
        if(ok && !newName.isEmpty()) {
            m_view->note()->pages[index].title = newName;
            refreshThumbnails();
        }
    } else if(res == dup) {
        m_view->duplicatePage(index);
        refreshThumbnails();
    } else if(res == del) {
        if(m_view->note()->pages.size() > 1) {
            m_view->deletePage(index);
            refreshThumbnails();
        }
    }
}
