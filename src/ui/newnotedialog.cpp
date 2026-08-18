#include "newnotedialog.h"
#include "blop_theme.h"
#include "blopripple.h"
#include "librarytagstore.h"
#include "notepreviewicon.h"
#include "uiscale.h"

#include <QAbstractItemView>
#include <QAbstractButton>
#include <QButtonGroup>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMouseEvent>
#include <QPropertyAnimation>
#include <QShowEvent>
#include <QSizePolicy>
#include <QToolButton>
#include <QVBoxLayout>

NewNoteDialog::NewNoteDialog(QWidget *parent) : QDialog(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setupUi();
}

void NewNoteDialog::setupUi()
{
    QWidget *container = new QWidget(this);
    container->setObjectName(QStringLiteral("NewNoteCard"));
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->addWidget(container, 1);
    container->setStyleSheet(
        QStringLiteral("QWidget#NewNoteCard { background-color: %1; border: none; }")
            .arg(BlopTheme::surfaceElevated().name(QColor::HexRgb)) +
        BlopTheme::themed(QStringLiteral(
            "QLabel { color: #DDD; font-family: 'Segoe UI'; border: none; background: transparent; }"
            "QLineEdit { background: rgba(22, 24, 36, 0.95); color: #F4F5FB; border: 1px solid rgba(120,130,160,0.32); border-radius: 14px; padding: 12px 16px; font-size: 15px; selection-background-color: #7C5CFC; }"
            "QLineEdit:focus { border: 1px solid #7C5CFC; }")));

    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(UiScale::dp(48), UiScale::dp(36),
                               UiScale::dp(48), UiScale::dp(28));
    layout->setSpacing(UiScale::dp(22));

    auto *lblTitle = new QLabel(QStringLiteral("Neue Notiz"), container);
    lblTitle->setStyleSheet(BlopTheme::themed(QStringLiteral(
        "font-size: 26px; font-weight: 800; color: #F4F5FB; letter-spacing: -0.4px;")));
    layout->addWidget(lblTitle);

    auto sectionLabel = [](const QString &text, QWidget *parent) {
        auto *lbl = new QLabel(text, parent);
        lbl->setStyleSheet(BlopTheme::themed(QStringLiteral(
            "font-size: 13px; color: #BBB; font-weight: 700; letter-spacing: 0.4px;")));
        return lbl;
    };

    auto *body = new QWidget(container);
    auto *cols = new QHBoxLayout(body);
    cols->setContentsMargins(0, 0, 0, 0);
    cols->setSpacing(UiScale::dp(48));

    // Left column stays a readable width; layout tiles take the remaining
    // overlay so Unendlich/A4 are not stretched into full-window slabs.
    auto *left = new QWidget(body);
    left->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    left->setMinimumWidth(UiScale::dp(360));
    left->setMaximumWidth(UiScale::dp(480));
    auto *leftLay = new QVBoxLayout(left);
    leftLay->setContentsMargins(0, 0, UiScale::dp(8), 0);
    leftLay->setSpacing(UiScale::dp(14));

    leftLay->addWidget(sectionLabel(QStringLiteral("Format"), left));

    auto createFormatBtn = [this, left](const QString &text,
                                        const QString &subtext) {
        auto *btn = new QPushButton(left);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setMinimumHeight(UiScale::dp(100));
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        btn->setIconSize(QSize(UiScale::dp(56), UiScale::dp(56)));
        btn->setText(text + QStringLiteral("\n") + subtext);
        btn->setStyleSheet(BlopTheme::themed(QStringLiteral(
            "QPushButton { background: #252526; color: #AAA; border: 1px solid #444; "
            "border-radius: 18px; text-align: left; padding: 16px 18px; "
            "line-height: 1.3; font-size: 15px; font-weight: 600; }"
            "QPushButton:checked { background: #7C5CFC; color: white; border: 1px solid #7C5CFC; }"
            "QPushButton:hover:!checked { background: #333; border-color: #555; }")));
        BlopRipple::attachPressFeedback(btn, 0.92);
        return btn;
    };

    m_btnFormatInfinite = createFormatBtn(
        QStringLiteral("Unendlich"), QStringLiteral("Freie Leinwand"));
    m_btnFormatA4 = createFormatBtn(
        QStringLiteral("DIN A4"), QStringLiteral("Seitenbasiert"));
    m_btnFormatInfinite->setChecked(true);

    m_groupFormat = new QButtonGroup(this);
    m_groupFormat->addButton(m_btnFormatInfinite, 0);
    m_groupFormat->addButton(m_btnFormatA4, 1);
    m_groupFormat->setExclusive(true);

    auto *formatCol = new QVBoxLayout();
    formatCol->setSpacing(UiScale::dp(12));
    formatCol->addWidget(m_btnFormatInfinite);
    formatCol->addWidget(m_btnFormatA4);
    leftLay->addLayout(formatCol);

    leftLay->addWidget(sectionLabel(QStringLiteral("Name"), left));
    m_nameInput = new QLineEdit(left);
    m_nameInput->setPlaceholderText(QStringLiteral("Meine Notiz"));
    m_nameInput->setMinimumHeight(UiScale::dp(48));
    m_nameInput->setFocus();
    leftLay->addWidget(m_nameInput);

    leftLay->addWidget(sectionLabel(QStringLiteral("Tags"), left));

    auto *tagRow = new QHBoxLayout();
    tagRow->setSpacing(UiScale::dp(10));
    m_tagInput = new QLineEdit(left);
    m_tagInput->setPlaceholderText(QStringLiteral("Tag hinzufügen…"));
    m_tagInput->setMinimumHeight(UiScale::dp(48));
    auto *btnAddTag = new QPushButton(QStringLiteral("+"), left);
    btnAddTag->setFixedSize(UiScale::dp(48), UiScale::dp(48));
    btnAddTag->setCursor(Qt::PointingHandCursor);
    btnAddTag->setStyleSheet(BlopTheme::themed(QStringLiteral(
        "QPushButton { background: #7C5CFC; color: white; border: none; "
        "border-radius: 14px; font-weight: 800; font-size: 20px; }"
        "QPushButton:hover { background: #6A4BE8; }")));
    tagRow->addWidget(m_tagInput, 1);
    tagRow->addWidget(btnAddTag);
    leftLay->addLayout(tagRow);

    m_tagList = new QListWidget(left);
    m_tagList->setSelectionMode(QAbstractItemView::MultiSelection);
    m_tagList->setFrameShape(QFrame::NoFrame);
    m_tagList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_tagList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_tagList->setStyleSheet(BlopTheme::themed(QStringLiteral(
        "QListWidget { background: transparent; color: #E8E4FF; border: none; "
        "font-size: 14px; outline: none; }"
        "QListWidget::item { padding: 10px 8px; border-radius: 10px; }"
        "QListWidget::item:selected { background: rgba(124,92,252,0.28); }")));
    leftLay->addWidget(m_tagList, 1);

    auto addTagFromInput = [this]() {
        const QString n = LibraryTagStore::normalize(m_tagInput->text());
        if (n.isEmpty())
            return;
        LibraryTagStore::addTagToCatalog(m_tagInput->text());
        m_tagInput->clear();
        rebuildTagList();
        for (int i = 0; i < m_tagList->count(); ++i) {
            if (m_tagList->item(i)->text() == n)
                m_tagList->item(i)->setSelected(true);
        }
    };
    connect(btnAddTag, &QPushButton::clicked, this, addTagFromInput);
    connect(m_tagInput, &QLineEdit::returnPressed, this, addTagFromInput);
    rebuildTagList();

    cols->addWidget(left, 0);

    auto *right = new QWidget(body);
    right->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *rightLay = new QVBoxLayout(right);
    rightLay->setContentsMargins(0, 0, 0, 0);
    rightLay->setSpacing(UiScale::dp(12));
    rightLay->addWidget(sectionLabel(QStringLiteral("Layout"), right));

    auto *gridHost = new QWidget(right);
    gridHost->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *grid = new QGridLayout(gridHost);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(UiScale::dp(20));
    grid->setVerticalSpacing(UiScale::dp(20));
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(2, 1);
    grid->setRowStretch(0, 1);
    grid->setRowStretch(1, 1);

    m_groupLayout = new QButtonGroup(this);
    m_groupLayout->setExclusive(true);

    struct LayoutOpt {
        int type;
        QString name;
        int row;
        int col;
        int colSpan;
    };
    const LayoutOpt opts[] = {
        {0, QStringLiteral("Leer"), 0, 0, 1},
        {1, QStringLiteral("Liniert"), 0, 1, 1},
        {2, QStringLiteral("Kariert"), 0, 2, 1},
        {3, QStringLiteral("Punktiert"), 1, 0, 1},
        {4, QStringLiteral("Legal"), 1, 1, 2},
    };

    const QString btnQss = BlopTheme::themed(QStringLiteral(
        "QToolButton { background: #252526; color: #DDD; border: 1px solid #444; "
        "border-radius: 18px; padding: 14px 10px 16px 10px; font-size: 13px; "
        "font-weight: 600; }"
        "QToolButton:checked { background: rgba(124,92,252,0.22); color: white; "
        "border: 2px solid #7C5CFC; }"
        "QToolButton:hover:!checked { border-color: #666; background: #2E2E38; }"));

    for (const auto &opt : opts) {
        auto *tb = new QToolButton(gridHost);
        tb->setText(opt.name);
        tb->setCheckable(true);
        tb->setCursor(Qt::PointingHandCursor);
        tb->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        tb->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        tb->setMinimumSize(UiScale::dp(160), UiScale::dp(180));
        tb->setIconSize(QSize(UiScale::dp(96), UiScale::dp(96)));
        tb->setStyleSheet(btnQss);
        tb->setProperty("blopBgType", opt.type);
        m_groupLayout->addButton(tb, opt.type);
        grid->addWidget(tb, opt.row, opt.col, 1, opt.colSpan);
        if (opt.type == m_backgroundType)
            tb->setChecked(true);
        BlopRipple::attachPressFeedback(tb, 0.94);
    }
    connect(m_groupLayout, &QButtonGroup::idClicked, this, [this](int id) {
        m_backgroundType = id;
        refreshLayoutIcons();
    });
    connect(m_groupFormat, &QButtonGroup::idClicked, this,
            [this](int) { refreshLayoutIcons(); });

    rightLay->addWidget(gridHost, 1);
    cols->addWidget(right, 3);
    layout->addWidget(body, 1);

    auto *actionLay = new QHBoxLayout();
    actionLay->setSpacing(UiScale::dp(14));
    actionLay->addStretch();

    m_btnCancel = new QPushButton(QStringLiteral("Abbrechen"), container);
    m_btnCancel->setCursor(Qt::PointingHandCursor);
    m_btnCancel->setMinimumHeight(UiScale::dp(48));
    m_btnCancel->setStyleSheet(BlopTheme::themed(QStringLiteral(
        "QPushButton { background: transparent; color: #AAA; border: none; "
        "font-weight: 700; font-size: 15px; padding: 10px 18px; }"
        "QPushButton:hover { color: #F4F5FB; }")));
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    m_btnCreate = new QPushButton(QStringLiteral("Erstellen"), container);
    m_btnCreate->setCursor(Qt::PointingHandCursor);
    m_btnCreate->setMinimumHeight(UiScale::dp(48));
    m_btnCreate->setMinimumWidth(UiScale::dp(148));
    m_btnCreate->setStyleSheet(BlopTheme::themed(QStringLiteral(
        "QPushButton { background: #7C5CFC; color: white; border: none; "
        "border-radius: 20px; font-weight: 700; font-size: 15px; padding: 12px 26px; }"
        "QPushButton:hover { background: #6A4BE8; }")));
    connect(m_btnCreate, &QPushButton::clicked, this, &QDialog::accept);
    BlopRipple::attachPressFeedback(m_btnCancel, 0.92);
    BlopRipple::attachPressFeedback(m_btnCreate, 0.92);

    actionLay->addWidget(m_btnCancel);
    actionLay->addWidget(m_btnCreate);
    layout->addLayout(actionLay);

    refreshLayoutIcons();
}

void NewNoteDialog::refreshLayoutIcons()
{
    if (!m_groupLayout)
        return;
    const bool infinite = isInfiniteFormat();
    for (auto *btn : m_groupLayout->buttons()) {
        auto *tb = qobject_cast<QToolButton *>(btn);
        if (!tb)
            continue;
        NotePreviewIcon::Spec spec;
        spec.kind = infinite ? NotePreviewIcon::Kind::Infinite
                             : NotePreviewIcon::Kind::A4;
        spec.backgroundType = tb->property("blopBgType").toInt();
        spec.paper = m_paperColor;
        tb->setIcon(QIcon(NotePreviewIcon::pixmap(spec, UiScale::dp(80))));
    }
    {
        NotePreviewIcon::Spec inf;
        inf.kind = NotePreviewIcon::Kind::Infinite;
        inf.backgroundType = m_backgroundType;
        inf.paper = m_paperColor;
        m_btnFormatInfinite->setIcon(QIcon(NotePreviewIcon::pixmap(inf, UiScale::dp(52))));
        NotePreviewIcon::Spec a4;
        a4.kind = NotePreviewIcon::Kind::A4;
        a4.backgroundType = m_backgroundType;
        a4.paper = m_paperColor;
        m_btnFormatA4->setIcon(QIcon(NotePreviewIcon::pixmap(a4, UiScale::dp(52))));
    }
}

void NewNoteDialog::rebuildTagList()
{
    if (!m_tagList)
        return;
    const QStringList selected = selectedTags();
    m_tagList->clear();
    QStringList catalog = LibraryTagStore::catalog();
    if (catalog.isEmpty()) {
        LibraryTagStore::addTagToCatalog(QStringLiteral("Projekt"));
        LibraryTagStore::addTagToCatalog(QStringLiteral("Entwurf"));
        catalog = LibraryTagStore::catalog();
    }
    for (const QString &tag : catalog) {
        auto *item = new QListWidgetItem(tag, m_tagList);
        item->setSelected(selected.contains(tag));
    }
}

QString NewNoteDialog::getNoteName() const {
    QString t = m_nameInput->text().trimmed();
    return t.isEmpty() ? QStringLiteral("Neue Notiz") : t;
}

bool NewNoteDialog::isInfiniteFormat() const {
    return m_btnFormatInfinite && m_btnFormatInfinite->isChecked();
}

QStringList NewNoteDialog::selectedTags() const {
    QStringList out;
    if (!m_tagList)
        return out;
    const auto items = m_tagList->selectedItems();
    for (QListWidgetItem *it : items) {
        if (it)
            out.append(it->text());
    }
    return out;
}

void NewNoteDialog::showEvent(QShowEvent *event) {
    QDialog::showEvent(event);
    setWindowOpacity(1.0);
    if (!isWindow())
        return;
    if (!parentWidget())
        return;
    if (m_dialogIntroDone)
        return;
    m_dialogIntroDone = true;
    const QPoint dest = pos();
#ifndef Q_OS_ANDROID
    setWindowOpacity(0.0);
    auto *opAnim = new QPropertyAnimation(this, "windowOpacity", this);
    opAnim->setDuration(BlopMotion::kStandard);
    opAnim->setStartValue(0.0);
    opAnim->setEndValue(1.0);
    opAnim->setEasingCurve(BlopMotion::kEaseStandard);
    opAnim->start(QAbstractAnimation::DeleteWhenStopped);
#endif
    move(dest.x(), dest.y() + 24);
    auto *posAnim = new QPropertyAnimation(this, "pos", this);
    posAnim->setDuration(BlopMotion::kEmphasis);
    posAnim->setStartValue(QPoint(dest.x(), dest.y() + 24));
    posAnim->setEndValue(dest);
    posAnim->setEasingCurve(BlopMotion::kEaseStandard);
    posAnim->start(QAbstractAnimation::DeleteWhenStopped);
}

void NewNoteDialog::mousePressEvent(QMouseEvent *event) {
    if (!isWindow()) {
        QDialog::mousePressEvent(event);
        return;
    }
    if (event->button() == Qt::LeftButton) {
        m_dragPos = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void NewNoteDialog::mouseMoveEvent(QMouseEvent *event) {
    if (!isWindow()) {
        QDialog::mouseMoveEvent(event);
        return;
    }
    if (event->buttons() & Qt::LeftButton) {
        move(event->globalPosition().toPoint() - m_dragPos);
        event->accept();
    }
}
