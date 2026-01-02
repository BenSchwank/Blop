#include "pagemanager.h"
#include "UIStyles.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QScroller>
#include <QGraphicsDropShadowEffect>

// ============================================================================
// PageListDelegate
// ============================================================================

PageListDelegate::PageListDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

QSize PageListDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
    // Breite ist durch ListWidget gegeben, Höhe definieren wir fix
    // Thumbnail ca 140 hoch + Text + Padding
    return QSize(option.rect.width(), 180);
}

void PageListDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    QRect rect = option.rect;
    rect.adjust(10, 10, -10, -10); // Margin

    // Hintergrund bei Auswahl oder Hover
    if (option.state & QStyle::State_Selected) {
        painter->setBrush(QColor(0x5E5CE6)); // Accent Color
        painter->setPen(Qt::NoPen);
        painter->drawRoundedRect(option.rect.adjusted(4, 4, -4, -4), 8, 8);
    } else if (option.state & QStyle::State_MouseOver) {
        painter->setBrush(QColor(255, 255, 255, 20));
        painter->setPen(Qt::NoPen);
        painter->drawRoundedRect(option.rect.adjusted(4, 4, -4, -4), 8, 8);
    }

    // Thumbnail holen (gespeichert in UserRole)
    QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
    QPixmap thumb = icon.pixmap(150, 200); // Größe anfordern

    // Thumbnail zentriert zeichnen
    int thumbW = 120;
    int thumbH = 140; // A4 Verhältnis ca.
    QRect thumbRect(rect.center().x() - thumbW/2, rect.top(), thumbW, thumbH);

    // Schatten hinter dem Papier
    painter->setBrush(QColor(0,0,0,50));
    painter->setPen(Qt::NoPen);
    painter->drawRoundedRect(thumbRect.translated(2, 2), 4, 4);

    // Weißes Papier zeichnen (falls Thumbnail transparent ist)
    painter->setBrush(Qt::white);
    painter->drawRoundedRect(thumbRect, 4, 4);

    // Thumbnail draufmalen
    if (!thumb.isNull()) {
        painter->drawPixmap(thumbRect, thumb);
    }

    // Seiten-Nummer Text
    QString text = index.data(Qt::DisplayRole).toString();
    QRect textRect(rect.left(), thumbRect.bottom() + 5, rect.width(), 20);

    painter->setPen(option.state & QStyle::State_Selected ? Qt::white : QColor(200, 200, 200));
    QFont font = painter->font();
    font.setBold(true);
    font.setPointSize(10);
    painter->setFont(font);
    painter->drawText(textRect, Qt::AlignCenter, text);

    painter->restore();
}

// ============================================================================
// PageManager Widget
// ============================================================================

PageManager::PageManager(QWidget* parent) : QWidget(parent) {
    setFixedWidth(240); // Breite wie angefordert (~220px + Padding)

    // Schlagschatten nach links für das Overlay
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

    // Header Area
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
    m_listWidget->setItemDelegate(new PageListDelegate(this));

    // Drag & Drop Einstellungen
    m_listWidget->setDragEnabled(true);
    m_listWidget->setAcceptDrops(true);
    m_listWidget->setDragDropMode(QAbstractItemView::InternalMove);
    m_listWidget->setDefaultDropAction(Qt::MoveAction);
    m_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);

    // Touch Scrolling
    QScroller::grabGesture(m_listWidget, QScroller::LeftMouseButtonGesture);

    layout->addWidget(m_listWidget);

    // Klick auf Seite -> Scrollen
    connect(m_listWidget, &QListWidget::itemClicked, [this](QListWidgetItem* item){
        int idx = m_listWidget->row(item);
        emit pageSelected(idx);
    });

    // Reordering abfangen über das Model
    connect(m_listWidget->model(), &QAbstractItemModel::rowsMoved, this, &PageManager::onRowMoved);
}

void PageManager::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), QColor("#1E1E1E")); // Dunkler Hintergrund

    // Feine Linie links
    p.setPen(QColor("#333"));
    p.drawLine(0, 0, 0, height());
}

void PageManager::setNoteView(MultiPageNoteView* view) {
    m_view = view;
    refreshThumbnails();
}

void PageManager::refreshThumbnails() {
    if (!m_view) return;

    // Signale blockieren, um Flackern zu vermeiden
    bool wasBlocked = m_listWidget->blockSignals(true);
    m_listWidget->clear();

    Note* note = m_view->note();
    if (note) {
        int pageCount = note->pages.size();
        for (int i = 0; i < pageCount; ++i) {
            // Thumbnail vom View generieren lassen
            QPixmap thumb = m_view->generateThumbnail(i, QSize(120, 160));

            QListWidgetItem* item = new QListWidgetItem();
            item->setText(QString("Seite %1").arg(i + 1));
            item->setIcon(QIcon(thumb)); // Im Icon speichern

            m_listWidget->addItem(item);
        }
    }

    m_listWidget->blockSignals(wasBlocked);
}

void PageManager::onRowMoved(const QModelIndex&, int start, int, const QModelIndex&, int row) {
    if (!m_view) return;

    // Korrektur des Ziel-Index, da InternalMove etwas unintuitiv ist
    // Wenn man nach unten zieht, ist der row-Index um 1 höher als der visuelle Index
    int dest = row;
    if (start < row) {
        dest = row - 1;
    }

    if (start != dest) {
        // View mitteilen, dass Seiten getauscht wurden
        m_view->movePage(start, dest);

        // Seitennummern im Text aktualisieren ("Seite 1", "Seite 2"...)
        for(int i=0; i < m_listWidget->count(); ++i) {
            m_listWidget->item(i)->setText(QString("Seite %1").arg(i+1));
        }

        emit pageOrderChanged();
    }
}
