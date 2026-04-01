#include "pagemanager.h"
#include "editoroverlays.h"
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
#include <QCheckBox>

namespace {
constexpr int kPanelMinWidth = 420;
constexpr int kPanelMaxWidth = 860;

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
    const int h = index.data(Qt::UserRole + 2).toInt();
    return QSize(option.rect.width(), h > 0 ? h : 120);
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

    int thumbW = 68;
    int thumbH = 90;
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
    int textRight = cardRect.right() - 50;

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
    m_scrim = new QPushButton(this);
    m_scrim->setFlat(true);
    m_scrim->setCursor(Qt::PointingHandCursor);
    m_scrim->setFocusPolicy(Qt::NoFocus);
    m_scrim->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    m_scrim->setStyleSheet(
        "QPushButton { background-color: rgba(12, 14, 22, 0.52); border: none; }"
        "QPushButton:hover { background-color: rgba(12, 14, 22, 0.58); }");
    connect(m_scrim, &QPushButton::clicked, this, &QWidget::hide);

    m_panel = new QWidget(this);
    m_panel->setMinimumWidth(kPanelMinWidth);
    m_panel->setMaximumWidth(kPanelMaxWidth);
    m_panel->setObjectName(QStringLiteral("PageManagerPanel"));
    m_panel->setStyleSheet(
        "#PageManagerPanel {"
        "  background-color: rgba(30, 32, 44, 0.985);"
        "  border: 1px solid rgba(120, 130, 160, 0.30);"
        "  border-radius: 18px;"
        "}");

    auto *contentLay = new QVBoxLayout(m_panel);
    contentLay->setContentsMargins(0, 0, 0, 0);
    contentLay->setSpacing(0);

    auto *header = new QWidget(m_panel);
    header->setFixedHeight(140);
    header->setStyleSheet("background: transparent; border-bottom: 1px solid rgba(120, 130, 160, 0.18);");
    auto *headLay = new QVBoxLayout(header);
    headLay->setContentsMargins(16, 12, 16, 10);
    headLay->setSpacing(8);

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

    m_lblSubtitle = new QLabel(QStringLiteral("Drag & Drop · Mehrfachauswahl · Vorlagen"), header);
    m_lblSubtitle->setStyleSheet("color: rgba(160, 168, 190, 0.85); font-size: 11px;");

    headLay->addLayout(titleRow);
    headLay->addWidget(m_lblSubtitle);

    m_search = new QLineEdit(header);
    m_search->setPlaceholderText(QStringLiteral("Seiten suchen..."));
    m_search->setStyleSheet(
        "QLineEdit { background: rgba(22,24,34,0.92); color: #E8EAFF; border: 1px solid rgba(120,130,160,0.35);"
        "border-radius: 10px; padding: 8px 10px; }"
        "QLineEdit:focus { border-color: rgba(107,163,245,0.68); }");
    connect(m_search, &QLineEdit::textChanged, this, &PageManager::onSearchChanged);
    headLay->addWidget(m_search);

    auto *filterRow = new QHBoxLayout();
    filterRow->setContentsMargins(0, 0, 0, 0);
    filterRow->setSpacing(8);
    m_groupFilter = new QComboBox(header);
    m_groupFilter->addItem(QStringLiteral("Alle Gruppen"), QStringLiteral("all"));
    m_groupFilter->addItem(QStringLiteral("Leer"), QStringLiteral("blank"));
    m_groupFilter->addItem(QStringLiteral("Liniert"), QStringLiteral("lined"));
    m_groupFilter->addItem(QStringLiteral("Kariert"), QStringLiteral("grid"));
    m_groupFilter->addItem(QStringLiteral("Punktiert"), QStringLiteral("dotted"));
    m_groupFilter->addItem(QStringLiteral("Legal"), QStringLiteral("legal"));
    m_groupFilter->setStyleSheet(
        "QComboBox { background: rgba(22,24,34,0.9); color: #E8EAFF; border: 1px solid rgba(120,130,160,0.35);"
        "border-radius: 9px; padding: 6px 10px; }");
    connect(m_groupFilter, qOverload<int>(&QComboBox::currentIndexChanged), this,
            &PageManager::onGroupFilterChanged);
    m_btnSelectMode = new QPushButton(QStringLiteral("☑"), header);
    m_btnSelectMode->setToolTip(QStringLiteral("Mehrfachauswahl umschalten"));
    m_btnSelectMode->setCheckable(true);
    m_btnSelectMode->setFixedSize(36, 32);
    m_btnSelectMode->setStyleSheet(
        "QPushButton { background: rgba(255,255,255,0.06); color: #D8DEF2; border: 1px solid rgba(120,130,160,0.35);"
        "border-radius: 9px; padding: 2px 2px; font-size: 16px; font-weight: 700; }"
        "QPushButton:checked { background: rgba(107,163,245,0.22); border-color: rgba(107,163,245,0.62); color: #EEF4FF; }");
    connect(m_btnSelectMode, &QPushButton::clicked, this, &PageManager::onToggleSelectMode);
    filterRow->addWidget(m_groupFilter, 1);
    filterRow->addWidget(m_btnSelectMode);
    headLay->addLayout(filterRow);
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

    m_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listWidget->setDragEnabled(true);
    m_listWidget->setAcceptDrops(true);
    m_listWidget->setDragDropMode(QAbstractItemView::InternalMove);
    m_listWidget->setDefaultDropAction(Qt::MoveAction);

    connect(m_listWidget->model(), &QAbstractItemModel::rowsMoved, this, &PageManager::onRowMoved);

    QScroller::grabGesture(m_listWidget, QScroller::LeftMouseButtonGesture);

    contentLay->addWidget(m_listWidget, 1);

    auto *footerLay = new QVBoxLayout();
    footerLay->setContentsMargins(12, 8, 12, 12);
    footerLay->setSpacing(8);

    auto *batchRow = new QHBoxLayout();
    batchRow->setContentsMargins(0, 0, 0, 0);
    batchRow->setSpacing(6);
    auto mkBtn = [this](const QString &txt, const QString &tooltip) {
      auto *b = new QPushButton(txt, m_panel);
      b->setToolTip(tooltip);
      b->setFixedSize(34, 30);
      b->setStyleSheet(
          "QPushButton { background: rgba(255,255,255,0.06); color: #D6DDF4; border: 1px solid rgba(120,130,160,0.3);"
          "border-radius: 8px; padding: 2px 2px; font-size: 16px; font-weight: 700; }"
          "QPushButton:hover { background: rgba(255,255,255,0.10); }");
      return b;
    };
    m_btnSelectAll = mkBtn(QStringLiteral("☑"), QStringLiteral("Alle Seiten auswählen"));
    m_btnClearSelection = mkBtn(QStringLiteral("☐"), QStringLiteral("Auswahl aufheben"));
    m_btnDuplicateSelection = mkBtn(QStringLiteral("⧉"), QStringLiteral("Ausgewählte Seiten duplizieren"));
    m_btnDeleteSelection = mkBtn(QStringLiteral("⌫"), QStringLiteral("Ausgewählte Seiten löschen"));
    m_btnMoveUp = mkBtn(QStringLiteral("↑"), QStringLiteral("Auswahl nach oben verschieben"));
    m_btnMoveDown = mkBtn(QStringLiteral("↓"), QStringLiteral("Auswahl nach unten verschieben"));
    m_btnApplyLayout = mkBtn(QStringLiteral("◫"), QStringLiteral("Vorlage/Farbe auf Auswahl anwenden"));
    batchRow->addWidget(m_btnSelectAll);
    batchRow->addWidget(m_btnClearSelection);
    batchRow->addWidget(m_btnDuplicateSelection);
    batchRow->addWidget(m_btnDeleteSelection);
    batchRow->addWidget(m_btnMoveUp);
    batchRow->addWidget(m_btnMoveDown);
    batchRow->addWidget(m_btnApplyLayout);
    connect(m_btnSelectAll, &QPushButton::clicked, this, &PageManager::onSelectAll);
    connect(m_btnClearSelection, &QPushButton::clicked, this, &PageManager::onClearSelection);
    connect(m_btnDuplicateSelection, &QPushButton::clicked, this, &PageManager::onDuplicateSelected);
    connect(m_btnDeleteSelection, &QPushButton::clicked, this, &PageManager::onDeleteSelected);
    connect(m_btnMoveUp, &QPushButton::clicked, this, &PageManager::onMoveSelectedUp);
    connect(m_btnMoveDown, &QPushButton::clicked, this, &PageManager::onMoveSelectedDown);
    connect(m_btnApplyLayout, &QPushButton::clicked, this, &PageManager::onApplyTemplateColor);
    footerLay->addLayout(batchRow);

    auto *addRow = new QHBoxLayout();
    addRow->setContentsMargins(0, 0, 0, 0);
    addRow->addStretch();

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

    addRow->addWidget(m_fabAdd);
    footerLay->addLayout(addRow);
    contentLay->addLayout(footerLay);

    m_scrim->raise();
    m_panel->raise();
}

void PageManager::fillParent() {
    if (parentWidget()) {
        const int pw = parentWidget()->width();
        const int ph = parentWidget()->height();
        setGeometry(0, 0, pw, ph);
        m_scrim->setGeometry(rect());
#ifdef Q_OS_ANDROID
        const int w = qMin(pw - 16, qMax(360, static_cast<int>(pw * 0.92)));
        const int h = qMin(ph - 16, 760);
        const int x = (pw - w) / 2;
        const int y = qMax(8, ph - h - 8);
#else
        const int w = qMin(760, qMax(460, static_cast<int>(pw * 0.44)));
        const int h = qMin(760, qMax(500, ph - 120));
        const int x = (pw - w) / 2;
        const int y = (ph - h) / 2;
#endif
        m_panel->setGeometry(x, y, w, h);
        m_panel->raise();
    }
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
    const int pageIdx = item->data(Qt::UserRole).toInt();
    if (m_multiSelectMode) {
        if (item->checkState() == Qt::Checked)
            m_selectedPages.insert(pageIdx);
        else
            m_selectedPages.remove(pageIdx);
        updateSubtitle();
        return;
    }
    emit pageSelected(pageIdx);
}

void PageManager::setNoteView(MultiPageNoteView* view) {
    m_view = view;
    rebuildList(false);
}

void PageManager::refreshThumbnails() {
    rebuildList();
}

void PageManager::onAddPage() {
    if (!m_view || !m_view->note()) return;
    int idx = m_view->note()->pages.size();
    m_view->note()->ensurePage(idx);
    m_view->setNote(m_view->note());
    rebuildList(false);
    m_listWidget->scrollToBottom();
}

void PageManager::onRowMoved(const QModelIndex&, int start, int, const QModelIndex&, int row) {
    if (!m_view) return;
    int dest = row;
    if (start < row) dest = row - 1;
    if (start != dest) {
        m_view->movePage(start, dest);
        rebuildList(false);
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
            m_view->renamePage(index, newName);
            rebuildList();
        }
    } else if (res == dup) {
        m_view->duplicatePage(index);
        rebuildList(false);
    } else if (res == del) {
        if (m_view->pageCount() > 1) {
            m_view->deletePage(index);
            rebuildList(false);
        }
    }
}

void PageManager::onSearchChanged(const QString &text) {
    m_searchText = text.trimmed();
    rebuildList(false);
}

void PageManager::onToggleSelectMode() {
    m_multiSelectMode = m_btnSelectMode->isChecked();
    m_selectedPages.clear();
    m_listWidget->setSelectionMode(m_multiSelectMode ? QAbstractItemView::MultiSelection
                                                     : QAbstractItemView::SingleSelection);
    rebuildList(false);
}

void PageManager::onSelectAll() {
    if (!m_view)
        return;
    m_selectedPages.clear();
    for (int i = 0; i < m_view->pageCount(); ++i)
        m_selectedPages.insert(i);
    rebuildList(false);
}

void PageManager::onClearSelection() {
    m_selectedPages.clear();
    rebuildList(false);
}

QList<int> PageManager::selectedPageIndices() const {
    QList<int> out = m_selectedPages.values();
    std::sort(out.begin(), out.end());
    return out;
}

void PageManager::onDeleteSelected() {
    if (!m_view)
        return;
    const QList<int> idxs = selectedPageIndices();
    if (idxs.isEmpty())
        return;
    m_view->deletePages(idxs);
    m_selectedPages.clear();
    rebuildList(false);
}

void PageManager::onDuplicateSelected() {
    if (!m_view)
        return;
    const QList<int> idxs = selectedPageIndices();
    if (idxs.isEmpty())
        return;
    m_view->duplicatePages(idxs);
    rebuildList(false);
}

void PageManager::onMoveSelectedUp() {
    if (!m_view)
        return;
    QList<int> idxs = selectedPageIndices();
    std::sort(idxs.begin(), idxs.end());
    for (int i : idxs) {
      if (i > 0)
        m_view->movePage(i, i - 1);
    }
    rebuildList(false);
}

void PageManager::onMoveSelectedDown() {
    if (!m_view)
        return;
    QList<int> idxs = selectedPageIndices();
    std::sort(idxs.begin(), idxs.end(), std::greater<int>());
    for (int i : idxs) {
      if (i < m_view->pageCount() - 1)
        m_view->movePage(i, i + 1);
    }
    rebuildList(false);
}

QString PageManager::pageGroupKey(int pageIndex) const {
    if (!m_view || !m_view->note() || pageIndex < 0 || pageIndex >= m_view->note()->pages.size())
        return QStringLiteral("all");
    const int t = m_view->note()->pages[pageIndex].backgroundType;
    switch (t) {
      case 0: return QStringLiteral("blank");
      case 1: return QStringLiteral("lined");
      case 2: return QStringLiteral("grid");
      case 3: return QStringLiteral("dotted");
      case 4: return QStringLiteral("legal");
      default: return QStringLiteral("all");
    }
}

void PageManager::onGroupFilterChanged(int) {
    rebuildList(false);
}

void PageManager::onApplyTemplateColor() {
    if (!m_view || !m_view->note())
        return;
    QList<int> idxs = selectedPageIndices();
    if (idxs.isEmpty() && m_listWidget->currentItem())
        idxs << m_listWidget->currentItem()->data(Qt::UserRole).toInt();
    if (idxs.isEmpty())
        return;
    const int seed = idxs.first();
    const NotePage &pg = m_view->note()->pages[seed];
    A4LayoutDialogResult r;
    if (!showA4LayoutOverlay(this, QStringLiteral("Layout auf Seiten anwenden"),
                             QStringLiteral("Ausgewählt: %1 Seite(n)").arg(idxs.size()),
                             pg.backgroundType, pg.paperColor, &r))
        return;
    m_view->applyLayoutToPages(idxs, r.backgroundType, r.paperColor);
    rebuildList(false);
}

void PageManager::setCardSelectedStyle(QListWidgetItem *item, bool selected) {
    if (!item)
        return;
    item->setCheckState(selected ? Qt::Checked : Qt::Unchecked);
}

void PageManager::updateSubtitle() {
    if (!m_view) {
      m_lblSubtitle->setText(QStringLiteral("Keine Notiz"));
      return;
    }
    const int total = m_view->pageCount();
    const int sel = m_selectedPages.size();
    if (m_multiSelectMode)
      m_lblSubtitle->setText(QStringLiteral("%1 Seiten · %2 ausgewählt").arg(total).arg(sel));
    else
      m_lblSubtitle->setText(QStringLiteral("%1 Seiten · Drag & Drop aktiv").arg(total));
}

void PageManager::rebuildList(bool keepSelection) {
    if (!m_view) {
      m_listWidget->clear();
      updateSubtitle();
      return;
    }
    const QString grp = m_groupFilter ? m_groupFilter->currentData().toString() : QStringLiteral("all");
    const QSet<int> oldSel = m_selectedPages;
    m_listWidget->clear();
    for (int i = 0; i < m_view->pageCount(); ++i) {
      const QString title = m_view->pageTitle(i).isEmpty() ? QStringLiteral("Seite %1").arg(i + 1)
                                                            : m_view->pageTitle(i);
      if (!m_searchText.isEmpty() &&
          !title.contains(m_searchText, Qt::CaseInsensitive) &&
          !QString::number(i + 1).contains(m_searchText))
        continue;
      const QString g = pageGroupKey(i);
      if (grp != QStringLiteral("all") && g != grp)
        continue;
      auto *item = new QListWidgetItem;
      item->setData(Qt::UserRole, i);
      item->setData(Qt::DisplayRole, QStringLiteral("Seite %1").arg(i + 1));
      item->setData(Qt::UserRole + 1, title);
      item->setData(Qt::UserRole + 2, m_multiSelectMode ? 136 : 126);
      item->setIcon(QIcon(m_view->generateThumbnail(i, QSize(92, 126))));
      item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable |
                     Qt::ItemIsDragEnabled | Qt::ItemIsEnabled);
      const bool selected = keepSelection ? oldSel.contains(i) : m_selectedPages.contains(i);
      setCardSelectedStyle(item, selected);
      if (selected)
        m_selectedPages.insert(i);
      m_listWidget->addItem(item);
    }
    updateSubtitle();
}
