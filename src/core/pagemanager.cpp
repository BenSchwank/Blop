#include "pagemanager.h"
#include "blop_dialogs.h"
#include "blop_inwindow_menu.h"
#include "blop_theme.h"
#include "blopripple.h"
#include "editoroverlays.h"
#include "uiscale.h"
#include <QAbstractItemModel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QScroller>
#include <QMouseEvent>
#include <QEvent>
#include <QKeyEvent>
#include <QPropertyAnimation>
#include <QShowEvent>
#include <QVariantAnimation>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPointer>
#include <QCheckBox>

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

    // v3.18.5: read live BlopTheme tokens instead of fixed dark hex.
    // The previous palette (kCardIdle = #343640 dark) was unreadable
    // against the Light surface; tokens auto-flip per mode.
    const QColor cardIdle = BlopTheme::surfaceBase();
    const QColor cardSelected = BlopTheme::surfaceElevated();
    const QColor borderSubtle = BlopTheme::borderSubtle();
    const QColor accent = BlopTheme::accentPrimary();
    const QColor textPrimary = BlopTheme::textPrimary();
    const QColor textMuted = BlopTheme::textSecondary();
    const QColor thumbBg = BlopTheme::surfaceMuted();
    const bool isDark = BlopTheme::instance().isDark();

    QRect cardRect = option.rect.adjusted(10, 5, -10, -5);

    QColor bgColor = cardIdle;
    if (option.state & QStyle::State_Selected) {
        bgColor = cardSelected;
        painter->setPen(QPen(accent, 2));
    } else {
        painter->setPen(QPen(borderSubtle, 1));
    }
    painter->setBrush(bgColor);
    painter->drawRoundedRect(cardRect, 14, 14);

    int thumbW = 68;
    int thumbH = 90;
    QRect thumbRect(cardRect.left() + 14, cardRect.center().y() - thumbH / 2, thumbW, thumbH);

    painter->setPen(Qt::NoPen);
    painter->setBrush(thumbBg);
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
    painter->setPen(textPrimary);
    QFont titleFont = painter->font();
    titleFont.setBold(true);
    titleFont.setPointSize(11);
    painter->setFont(titleFont);
    painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter, title);

    QString subTitle = index.data(Qt::DisplayRole).toString();
    QRect subRect(textLeft, titleRect.bottom() + 2, textRight - textLeft, 20);
    painter->setPen(textMuted);
    QFont subFont = painter->font();
    subFont.setBold(false);
    subFont.setPointSize(9);
    painter->setFont(subFont);
    painter->drawText(subRect, Qt::AlignLeft | Qt::AlignVCenter, subTitle);

    QRect menuRect = getMenuButtonRect(cardRect);
    painter->setBrush(isDark ? QColor(255, 255, 255, 38) : QColor(0, 0, 0, 28));
    painter->setPen(Qt::NoPen);
    painter->drawRoundedRect(menuRect, 10, 10);
    const qreal cx = menuRect.center().x();
    const qreal cy = menuRect.center().y();
    const qreal dotR = 3.5;
    const qreal gap = 8.0;
    painter->setBrush(textPrimary);
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
    // v3.17.0: theme-aware scrim + panel. The hard 420 px min-width that
    // used to clip on 360-dp phones is gone -- fillParent() computes the
    // correct width responsively. Scrim/panel colors come from BlopTheme
    // so the Light-mode toggle in Settings reskins the manager too.
    auto rgba = [](const QColor &c) {
      return QStringLiteral("rgba(%1,%2,%3,%4)")
          .arg(c.red()).arg(c.green()).arg(c.blue())
          .arg(QString::number(c.alphaF(), 'f', 3));
    };
    m_scrim = new QPushButton(this);
    m_scrim->setFlat(true);
    m_scrim->setCursor(Qt::PointingHandCursor);
    m_scrim->setFocusPolicy(Qt::NoFocus);
    m_scrim->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    m_scrim->setStyleSheet(
        QStringLiteral(
            "QPushButton { background-color: %1; border: none; }"
            "QPushButton:hover { background-color: %1; }")
            .arg(rgba(BlopTheme::scrimColor())));
    connect(m_scrim, &QPushButton::clicked, this, &PageManager::dismissAnimated);

    m_panel = new QWidget(this);
    m_panel->setObjectName(QStringLiteral("PageManagerPanel"));
    m_panel->setStyleSheet(
        QStringLiteral(
            "#PageManagerPanel {"
            "  background-color: %1;"
            "  border: 1px solid %2;"
            "  border-radius: 18px;"
            "}")
            .arg(BlopTheme::surfaceElevated().name(QColor::HexRgb),
                 rgba(BlopTheme::borderDefault())));

    auto *contentLay = new QVBoxLayout(m_panel);
    contentLay->setContentsMargins(0, 0, 0, 0);
    contentLay->setSpacing(0);

    m_header = new QWidget(m_panel);
    m_header->setFixedHeight(96);
    auto *headLay = new QVBoxLayout(m_header);
    headLay->setContentsMargins(14, 10, 12, 8);
    headLay->setSpacing(6);

    auto *titleRow = new QHBoxLayout();
    titleRow->setContentsMargins(0, 0, 0, 0);
    titleRow->setSpacing(8);

    m_lblTitle = new QLabel(QStringLiteral("Seiten"), m_header);
    m_lblSubtitle = new QLabel(QStringLiteral("Drag & Drop · Mehrfachauswahl"), m_header);
    m_lblSubtitle->setAlignment(Qt::AlignVCenter | Qt::AlignRight);

    m_btnClose = new QPushButton(QStringLiteral("✕"), m_header);
    m_btnClose->setFixedSize(32, 32);
    m_btnClose->setCursor(Qt::PointingHandCursor);
    m_btnClose->setFocusPolicy(Qt::NoFocus);
    connect(m_btnClose, &QPushButton::clicked, this, &PageManager::dismissAnimated);

    titleRow->addWidget(m_lblTitle, 0);
    titleRow->addWidget(m_lblSubtitle, 1);
    titleRow->addWidget(m_btnClose, 0, Qt::AlignVCenter);
    headLay->addLayout(titleRow);

    auto *toolsRow = new QHBoxLayout();
    toolsRow->setContentsMargins(0, 0, 0, 0);
    toolsRow->setSpacing(8);
    m_search = new QLineEdit(m_header);
    m_search->setPlaceholderText(QStringLiteral("Seiten suchen…"));
    connect(m_search, &QLineEdit::textChanged, this, &PageManager::onSearchChanged);
    m_groupFilter = new QComboBox(m_header);
    m_groupFilter->addItem(QStringLiteral("Alle"), QStringLiteral("all"));
    m_groupFilter->addItem(QStringLiteral("Leer"), QStringLiteral("blank"));
    m_groupFilter->addItem(QStringLiteral("Liniert"), QStringLiteral("lined"));
    m_groupFilter->addItem(QStringLiteral("Kariert"), QStringLiteral("grid"));
    m_groupFilter->addItem(QStringLiteral("Punktiert"), QStringLiteral("dotted"));
    m_groupFilter->addItem(QStringLiteral("Legal"), QStringLiteral("legal"));
    m_groupFilter->setFixedWidth(110);
    connect(m_groupFilter, qOverload<int>(&QComboBox::currentIndexChanged), this,
            &PageManager::onGroupFilterChanged);
    m_btnSelectMode = new QPushButton(QStringLiteral("☑"), m_header);
    m_btnSelectMode->setToolTip(QStringLiteral("Mehrfachauswahl umschalten"));
    m_btnSelectMode->setCheckable(true);
    m_btnSelectMode->setFixedSize(32, 32);
    connect(m_btnSelectMode, &QPushButton::clicked, this, &PageManager::onToggleSelectMode);
    toolsRow->addWidget(m_search, 1);
    toolsRow->addWidget(m_groupFilter, 0);
    toolsRow->addWidget(m_btnSelectMode);
    headLay->addLayout(toolsRow);
    contentLay->addWidget(m_header);

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
    BlopRipple::attachPressFeedback(m_btnClose, 0.92);
    BlopRipple::attachPressFeedback(m_btnSelectMode, 0.92);
    for (QPushButton *b :
         {m_btnSelectAll, m_btnClearSelection, m_btnDuplicateSelection,
          m_btnDeleteSelection, m_btnMoveUp, m_btnMoveDown, m_btnApplyLayout}) {
      BlopRipple::attachPressFeedback(b, 0.90);
    }
    footerLay->addLayout(batchRow);

    auto *addRow = new QHBoxLayout();
    addRow->setContentsMargins(0, 0, 0, 0);
    addRow->addStretch();

    m_fabAdd = new QPushButton(QStringLiteral("＋"), m_panel);
    m_fabAdd->setFixedSize(56, 56);
    m_fabAdd->setCursor(Qt::PointingHandCursor);
    m_fabAdd->setFocusPolicy(Qt::NoFocus);
    connect(m_fabAdd, &QPushButton::clicked, this, &PageManager::onAddPage);
    BlopRipple::attachPressFeedback(m_fabAdd, 0.92);

    addRow->addWidget(m_fabAdd);
    footerLay->addLayout(addRow);
    contentLay->addLayout(footerLay);

    m_scrim->raise();
    m_panel->raise();

    applyControlStyles();
}

void PageManager::applyControlStyles() {
    // v3.18.5: token-driven styles for every interactive control. Called
    // from setupUi() once and again from applyThemeRefresh() so a
    // Light/Dark switch reskins the whole modal.
    auto rgba = [](const QColor &c) {
        return QStringLiteral("rgba(%1,%2,%3,%4)")
            .arg(c.red()).arg(c.green()).arg(c.blue())
            .arg(QString::number(c.alphaF(), 'f', 3));
    };
    const QString textPrimary = BlopTheme::textPrimary().name();
    const QString textSecondary = BlopTheme::textSecondary().name();
    const QString border = rgba(BlopTheme::borderSubtle());
    const bool isDark = BlopTheme::instance().isDark();
    const QString hoverBg = rgba(isDark ? QColor(255, 255, 255, 22)
                                        : QColor(0, 0, 0, 18));
    const QString idleBg = rgba(isDark ? QColor(255, 255, 255, 14)
                                       : QColor(0, 0, 0, 12));
    const QString inputBg = BlopTheme::surfaceMuted().name(QColor::HexRgb);
    const QString accent = BlopTheme::accentPrimary().name();
    const QString accentBorder = rgba(BlopTheme::accentBorder());
    const QString accentSubtle = rgba(BlopTheme::accentSubtle());
    const QString surfaceBase = BlopTheme::surfaceBase().name(QColor::HexRgb);
    const QString surfaceElevated = BlopTheme::surfaceElevated().name(QColor::HexRgb);

    if (m_header) {
        m_header->setStyleSheet(QStringLiteral(
            "background: transparent; border-bottom: 1px solid %1;").arg(border));
    }
    if (m_lblTitle) {
        m_lblTitle->setStyleSheet(QStringLiteral(
            "color: %1; font-weight: 700; font-size: 15px; letter-spacing: 0.1px;"
            "background: transparent;").arg(textPrimary));
    }
    if (m_lblSubtitle) {
        m_lblSubtitle->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 11px; background: transparent;"
            "padding-right: 6px;").arg(textSecondary));
    }
    if (m_btnClose) {
        m_btnClose->setStyleSheet(QStringLiteral(
            "QPushButton { color: %1; border: none; font-size: 16px;"
            "  border-radius: 10px; background: %2; }"
            "QPushButton:hover { color: %3; background: %4; }")
            .arg(textSecondary, idleBg, textPrimary, hoverBg));
    }
    if (m_search) {
        m_search->setStyleSheet(QStringLiteral(
            "QLineEdit { background: %1; color: %2; border: 1px solid %3;"
            "  border-radius: 10px; padding: 6px 10px; min-height: 28px; }"
            "QLineEdit:focus { border-color: %4; }")
            .arg(inputBg, textPrimary, border, accentBorder));
    }
    if (m_groupFilter) {
        m_groupFilter->setStyleSheet(QStringLiteral(
            "QComboBox { background: %1; color: %2; border: 1px solid %3;"
            "  border-radius: 9px; padding: 6px 10px; }")
            .arg(inputBg, textPrimary, border));
    }
    if (m_btnSelectMode) {
        m_btnSelectMode->setStyleSheet(QStringLiteral(
            "QPushButton { background: %1; color: %2; border: 1px solid %3;"
            "  border-radius: 9px; padding: 2px 2px; font-size: 16px; font-weight: 700; }"
            "QPushButton:checked { background: %4; border-color: %5; color: %6; }")
            .arg(idleBg, textPrimary, border, accentSubtle, accentBorder, accent));
    }
    const QString footerBtnQss = QStringLiteral(
        "QPushButton { background: %1; color: %2; border: 1px solid %3;"
        "  border-radius: 8px; padding: 2px 2px; font-size: 16px; font-weight: 700; }"
        "QPushButton:hover { background: %4; }")
        .arg(idleBg, textPrimary, border, hoverBg);
    for (QPushButton *b :
         {m_btnSelectAll, m_btnClearSelection, m_btnDuplicateSelection,
          m_btnDeleteSelection, m_btnMoveUp, m_btnMoveDown, m_btnApplyLayout}) {
        if (b)
            b->setStyleSheet(footerBtnQss);
    }
    if (m_fabAdd) {
        m_fabAdd->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background-color: %1;"
            "  border: 1px solid %2;"
            "  border-radius: 28px;"
            "  color: %3;"
            "  font-size: 26px; font-weight: 600;"
            "  padding-bottom: 2px;"
            "}"
            "QPushButton:hover {"
            "  background-color: %4;"
            "  border-color: %5;"
            "}")
            .arg(surfaceBase, border, accent, surfaceElevated, accentBorder));
    }
}

void PageManager::fillParent() {
    if (parentWidget()) {
        const int pw = parentWidget()->width();
        const int ph = parentWidget()->height();
        setGeometry(0, 0, pw, ph);
        m_scrim->setGeometry(rect());
#ifdef Q_OS_ANDROID
        const int sidePad = UiScale::safeHorizontalPaddingPx(parentWidget());
        const int topPad = UiScale::safeTopPx(parentWidget()) + UiScale::dp(8);
        const int bottomPad = UiScale::safeBottomPx(parentWidget()) + UiScale::dp(8);
        const int availW = qMax(UiScale::dp(240), pw - (2 * sidePad));
        const int availH = qMax(UiScale::dp(240), ph - topPad - bottomPad);
        const int w = qBound(UiScale::dp(280), static_cast<int>(availW * 0.92), qMin(UiScale::dp(760), availW));
        const int h = qBound(UiScale::dp(260), static_cast<int>(availH * 0.86), qMin(UiScale::dp(760), availH));
        const int x = (pw - w) / 2;
        const int y = qBound(topPad, ph - h - bottomPad, ph - h);
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

void PageManager::applyScrimProgress(qreal progress) {
    if (!m_scrim)
        return;
    QColor c = BlopTheme::scrimColor();
    c.setAlphaF(c.alphaF() * qBound<qreal>(0.0, progress, 1.0));
    const QString col = QStringLiteral("rgba(%1,%2,%3,%4)")
                            .arg(c.red()).arg(c.green()).arg(c.blue())
                            .arg(QString::number(c.alphaF(), 'f', 3));
    m_scrim->setStyleSheet(
        QStringLiteral(
            "QPushButton { background-color: %1; border: none; }"
            "QPushButton:hover { background-color: %1; }")
            .arg(col));
}

void PageManager::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    m_dismissing = false;
    fillParent();
    raise();
    setFocus(Qt::PopupFocusReason);

    // v3.18.2: scrim fade-in (kFast) + panel slide-up (kStandard) instead of
    // the hard show().
    applyScrimProgress(0.0);
    auto *fade = new QVariantAnimation(this);
    fade->setDuration(BlopMotion::kFast);
    fade->setStartValue(0.0);
    fade->setEndValue(1.0);
    fade->setEasingCurve(BlopMotion::kEaseStandard);
    connect(fade, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &v) { applyScrimProgress(v.toReal()); });
    fade->start(QAbstractAnimation::DeleteWhenStopped);

    if (m_panel) {
        const QPoint target = m_panel->pos();
        const QPoint start = target + QPoint(0, UiScale::dp(16));
        m_panel->move(start);
        auto *slide = new QPropertyAnimation(m_panel, "pos", this);
        slide->setDuration(BlopMotion::kStandard);
        slide->setStartValue(start);
        slide->setEndValue(target);
        slide->setEasingCurve(BlopMotion::kEaseStandard);
        slide->start(QAbstractAnimation::DeleteWhenStopped);
    }
}

void PageManager::dismissAnimated() {
    if (m_dismissing || isHidden()) {
        return;
    }
    m_dismissing = true;

    if (m_panel) {
        auto *slide = new QPropertyAnimation(m_panel, "pos", this);
        slide->setDuration(BlopMotion::kFast);
        slide->setStartValue(m_panel->pos());
        slide->setEndValue(m_panel->pos() + QPoint(0, UiScale::dp(12)));
        slide->setEasingCurve(QEasingCurve::InCubic);
        slide->start(QAbstractAnimation::DeleteWhenStopped);
    }

    auto *fade = new QVariantAnimation(this);
    fade->setDuration(BlopMotion::kFast);
    fade->setStartValue(1.0);
    fade->setEndValue(0.0);
    fade->setEasingCurve(QEasingCurve::InCubic);
    connect(fade, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &v) { applyScrimProgress(v.toReal()); });
    connect(fade, &QVariantAnimation::finished, this, [this]() {
        hide();
        m_dismissing = false;
        applyScrimProgress(1.0);
    });
    fade->start(QAbstractAnimation::DeleteWhenStopped);
}

void PageManager::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        dismissAnimated();
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

void PageManager::applyThemeRefresh() {
    // Helper copied from setupUi(): convert a QColor with alpha to a QSS rgba() string.
    auto rgba = [](const QColor &c) {
        return QStringLiteral("rgba(%1,%2,%3,%4)")
            .arg(c.red())
            .arg(c.green())
            .arg(c.blue())
            .arg(QString::number(c.alphaF(), 'f', 3));
    };
    if (m_scrim) {
        m_scrim->setStyleSheet(
            QStringLiteral(
                "QPushButton { background-color: %1; border: none; }"
                "QPushButton:hover { background-color: %1; }")
                .arg(rgba(BlopTheme::scrimColor())));
    }
    if (m_panel) {
        m_panel->setStyleSheet(
            QStringLiteral(
                "#PageManagerPanel {"
                "  background-color: %1;"
                "  border: 1px solid %2;"
                "  border-radius: 18px;"
                "}")
                .arg(BlopTheme::surfaceElevated().name(QColor::HexRgb),
                     rgba(BlopTheme::borderDefault())));
    }
    // v3.18.5: reapply every header/footer/FAB stylesheet so a theme
    // toggle is fully reflected even after the modal has been opened.
    applyControlStyles();
    // Force the list viewport to repaint with the new card colors and let
    // QStyle re-polish the children so the QSS-tagged inner buttons pick
    // up the refreshed BlopTheme tokens.
    if (m_listWidget && m_listWidget->viewport())
        m_listWidget->viewport()->update();
    if (style()) {
        style()->unpolish(this);
        style()->polish(this);
    }
    update();
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

    QList<BlopInWindowMenu::Item> items;
    items.append({QStringLiteral("Umbenennen"), QIcon(), [this, index]() {
        if (!m_view || !m_view->note() || index < 0 ||
            index >= m_view->note()->pages.size())
            return;
        const QString oldName = m_view->note()->pages[index].title;
        const QString newName = BlopDialogs::promptText(
            this, QStringLiteral("Seite umbenennen"), QStringLiteral("Name:"),
            oldName);
        if (!newName.isEmpty()) {
            m_view->renamePage(index, newName);
            rebuildList();
        }
    }});
    items.append({QStringLiteral("Duplizieren"), QIcon(), [this, index]() {
        if (!m_view)
            return;
        m_view->duplicatePage(index);
        rebuildList(false);
    }});
    items.append({QString(), QIcon(), {}, false, true});
    items.append({QStringLiteral("Löschen"), QIcon(), [this, index]() {
        if (!m_view || m_view->pageCount() <= 1)
            return;
        if (!BlopDialogs::confirm(this, QStringLiteral("Seite löschen"),
                                  QStringLiteral("Diese Seite wirklich löschen?"),
                                  QStringLiteral("Löschen"),
                                  QStringLiteral("Abbrechen")))
            return;
        m_view->deletePage(index);
        rebuildList(false);
    }, true});
    BlopInWindowMenu::show(this, pos, items);
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
      // v3.17.6: kick off thumbnail render asynchronously. A placeholder
      // (plain white tile) is shown while the worker runs; when the
      // worker finishes the QListWidgetItem is updated via QPersistent-
      // ModelIndex (the QListWidgetItem* pointer would otherwise be
      // invalidated if rebuildList() runs again before the worker
      // completes). On hit-cache (most repeat opens) the callback fires
      // synchronously, so the placeholder is never observable.
      QPixmap placeholder(QSize(92, 126));
      placeholder.fill(Qt::white);
      item->setIcon(QIcon(placeholder));
      item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable |
                     Qt::ItemIsDragEnabled | Qt::ItemIsEnabled);
      const bool selected = keepSelection ? oldSel.contains(i) : m_selectedPages.contains(i);
      setCardSelectedStyle(item, selected);
      if (selected)
        m_selectedPages.insert(i);
      m_listWidget->addItem(item);

      QPointer<QListWidget> guardList(m_listWidget);
      const int pageIdx = i;
      m_view->generateThumbnailAsync(pageIdx, QSize(92, 126),
                                     [guardList, pageIdx](QPixmap pm) {
        if (!guardList || pm.isNull())
          return;
        for (int r = 0; r < guardList->count(); ++r) {
          QListWidgetItem *it = guardList->item(r);
          if (it && it->data(Qt::UserRole).toInt() == pageIdx) {
            it->setIcon(QIcon(pm));
            break;
          }
        }
      });
    }
    updateSubtitle();
}
