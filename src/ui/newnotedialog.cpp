#include "newnotedialog.h"
#include "blop_theme.h"
#include "blopstyle.h"
#include "blopripple.h"
#include "librarytagstore.h"
#include "uiscale.h"

#include <QAbstractItemView>
#include <QAbstractButton>
#include <QButtonGroup>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMouseEvent>
#include <QPropertyAnimation>
#include <QShowEvent>
#include <QSizePolicy>
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

    const QColor accC = BlopTheme::accentPrimary();
    const QString acc = accC.name(QColor::HexRgb);
    const QString accHover = BlopTheme::accentHover().name(QColor::HexRgb);
    const QString accSubtle = QStringLiteral("rgba(%1,%2,%3,%4)")
                                  .arg(accC.red())
                                  .arg(accC.green())
                                  .arg(accC.blue())
                                  .arg(QString::number(0.16, 'f', 3));

    const QString textHex = BlopStyle::paperInk().name(QColor::HexRgb);
    const QString mutedHex = BlopStyle::paperInkMuted().name(QColor::HexRgb);
    BlopStyle::paintPaperSurface(container, QStringLiteral("NewNoteCard"));
    container->setStyleSheet(QStringLiteral(
        "#NewNoteCard {"
        "  background: %1;"
        "  border: 1px solid rgba(20,24,40,0.10);"
        "  border-radius: 12px;"
        "}"
        "QLabel { color: %2; border: none; background: transparent; }")
                                 .arg(BlopStyle::paperBg().name(QColor::HexRgb),
                                      textHex));

    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *titleBar = new QWidget(container);
    titleBar->setObjectName(QStringLiteral("NewNoteTitleBar"));
    titleBar->setAttribute(Qt::WA_StyledBackground, true);
    titleBar->setFixedHeight(UiScale::dp(44));
    titleBar->setStyleSheet(QStringLiteral(
        "QWidget#NewNoteTitleBar {"
        "  background: %1;"
        "  border-top-left-radius: 12px;"
        "  border-top-right-radius: 12px;"
        "  border-bottom: 1px solid rgba(255,255,255,0.06);"
        "}")
                                .arg(BlopStyle::obsidianBg().name(QColor::HexRgb)));
    auto *titleBarLay = new QHBoxLayout(titleBar);
    titleBarLay->setContentsMargins(UiScale::dp(16), 0, UiScale::dp(16), 0);
    auto *lblTitle = new QLabel(QStringLiteral("Neue Notiz"), titleBar);
    lblTitle->setStyleSheet(QStringLiteral(
        "font-size: 14px; font-weight: 600; color: %1;"
        "letter-spacing: -0.1px; background: transparent;")
                                .arg(BlopStyle::obsidianText().name(
                                    QColor::HexRgb)));
    titleBarLay->addWidget(lblTitle);
    layout->addWidget(titleBar);

    auto *body = new QWidget(container);
    BlopStyle::paintPaperSurface(body, QStringLiteral("NewNoteBody"));
    auto *bodyLay = new QVBoxLayout(body);
    bodyLay->setContentsMargins(UiScale::dp(20), UiScale::dp(14),
                                UiScale::dp(20), UiScale::dp(14));
    bodyLay->setSpacing(UiScale::dp(10));

    auto sectionLabel = [mutedHex](const QString &text, QWidget *parent) {
        auto *lbl = new QLabel(text, parent);
        lbl->setStyleSheet(QStringLiteral(
            "font-size: 10px; color: %1; font-weight: 700; letter-spacing: 0.6px;"
            "background: transparent; padding-top: 2px;")
            .arg(mutedHex));
        return lbl;
    };

    auto makeRowGroup = [](QWidget *parent) -> QFrame * {
        auto *g = new QFrame(parent);
        g->setObjectName(QStringLiteral("NewNoteRowGroup"));
        g->setAttribute(Qt::WA_StyledBackground, true);
        g->setStyleSheet(QStringLiteral(
            "QFrame#NewNoteRowGroup {"
            "  background: %1;"
            "  border: 1px solid rgba(20,24,40,0.08);"
            "  border-radius: 12px;"
            "}")
                             .arg(BlopStyle::paperRowBg().name(QColor::HexRgb)));
        auto *lay = new QVBoxLayout(g);
        lay->setContentsMargins(UiScale::dp(12), UiScale::dp(12),
                                UiScale::dp(12), UiScale::dp(12));
        lay->setSpacing(UiScale::dp(8));
        return g;
    };

    bodyLay->addWidget(sectionLabel(QStringLiteral("TITEL"), body));
    m_nameInput = new QLineEdit(body);
    m_nameInput->setPlaceholderText(QStringLiteral("Unbenannte Notiz"));
    m_nameInput->setMinimumHeight(UiScale::dp(BlopStyle::touchTargetMinDp() - 4));
    m_nameInput->setStyleSheet(BlopStyle::paperInputQss());
    m_nameInput->setFocus();
    bodyLay->addWidget(m_nameInput);
    connect(m_nameInput, &QLineEdit::returnPressed, this, &QDialog::accept);

    const QString segQss = BlopStyle::paperSegmentQss();
    const int chipH = UiScale::dp(BlopStyle::touchTargetMinDp());

    bodyLay->addWidget(sectionLabel(QStringLiteral("FORMAT"), body));
    auto *optsGroup = makeRowGroup(body);
    auto *optsLay = qobject_cast<QVBoxLayout *>(optsGroup->layout());

    auto *formatRow = new QHBoxLayout();
    formatRow->setSpacing(UiScale::dp(8));
    auto makeSeg = [this, optsGroup, &segQss, chipH](const QString &text) {
        auto *btn = new QPushButton(text, optsGroup);
        btn->setCheckable(true);
        btn->setAutoDefault(false);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setMinimumHeight(chipH);
        btn->setMaximumHeight(chipH + 4);
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        btn->setStyleSheet(segQss);
        BlopRipple::attachPressFeedback(btn, 0.96);
        return btn;
    };
    m_btnFormatInfinite = makeSeg(QStringLiteral("Unendlich"));
    m_btnFormatA4 = makeSeg(QStringLiteral("DIN A4"));
    m_btnFormatInfinite->setChecked(true);
    m_groupFormat = new QButtonGroup(this);
    m_groupFormat->addButton(m_btnFormatInfinite, 0);
    m_groupFormat->addButton(m_btnFormatA4, 1);
    m_groupFormat->setExclusive(true);
    formatRow->addWidget(m_btnFormatInfinite);
    formatRow->addWidget(m_btnFormatA4);
    optsLay->addLayout(formatRow);

    auto *layoutCap = sectionLabel(QStringLiteral("LAYOUT"), optsGroup);
    optsLay->addWidget(layoutCap);
    m_groupLayout = new QButtonGroup(this);
    m_groupLayout->setExclusive(true);
    auto *layoutRow = new QHBoxLayout();
    layoutRow->setSpacing(UiScale::dp(6));
    struct LayoutOpt { int type; const char *name; };
    const LayoutOpt opts[] = {
        {0, "Leer"}, {1, "Liniert"}, {2, "Kariert"},
        {3, "Punktiert"}, {4, "Legal"},
    };
    for (const auto &opt : opts) {
        auto *btn = new QPushButton(QString::fromUtf8(opt.name), optsGroup);
        btn->setCheckable(true);
        btn->setAutoDefault(false);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setMinimumHeight(chipH);
        btn->setMaximumHeight(chipH + 4);
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        btn->setStyleSheet(segQss);
        btn->setProperty("blopBgType", opt.type);
        m_groupLayout->addButton(btn, opt.type);
        if (opt.type == m_backgroundType)
            btn->setChecked(true);
        layoutRow->addWidget(btn);
        BlopRipple::attachPressFeedback(btn, 0.96);
    }
    connect(m_groupLayout, &QButtonGroup::idClicked, this,
            [this](int id) { m_backgroundType = id; });
    optsLay->addLayout(layoutRow);
    bodyLay->addWidget(optsGroup);

    bodyLay->addWidget(sectionLabel(QStringLiteral("TAGS"), body));
    auto *tagsGroup = makeRowGroup(body);
    auto *tagsLay = qobject_cast<QVBoxLayout *>(tagsGroup->layout());
    auto *tagRow = new QHBoxLayout();
    tagRow->setSpacing(UiScale::dp(8));
    m_tagInput = new QLineEdit(tagsGroup);
    m_tagInput->setPlaceholderText(QStringLiteral("Tag hinzufügen…"));
    m_tagInput->setMinimumHeight(UiScale::dp(BlopStyle::touchTargetMinDp() - 4));
    m_tagInput->setStyleSheet(BlopStyle::paperInputQss());
    auto *btnAddTag = new QPushButton(QStringLiteral("+"), tagsGroup);
    btnAddTag->setAutoDefault(false);
    btnAddTag->setFixedSize(UiScale::dp(36), UiScale::dp(36));
    btnAddTag->setCursor(Qt::PointingHandCursor);
    btnAddTag->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; color: white; border: none; "
        "border-radius: 10px; font-weight: 700; font-size: 16px; }"
        "QPushButton:hover { background: %2; }")
            .arg(acc, accHover));
    tagRow->addWidget(m_tagInput, 1);
    tagRow->addWidget(btnAddTag);
    tagsLay->addLayout(tagRow);

    m_tagList = new QListWidget(tagsGroup);
    m_tagList->setSelectionMode(QAbstractItemView::MultiSelection);
    m_tagList->setFrameShape(QFrame::NoFrame);
    m_tagList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_tagList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_tagList->setFixedHeight(UiScale::dp(96));
    m_tagList->setStyleSheet(QStringLiteral(
        "QListWidget { background: transparent; color: %1; border: none; "
        "font-size: 13px; outline: none; }"
        "QListWidget::item { padding: 6px 10px; border-radius: 8px;"
        "  min-height: %3px; }"
        "QListWidget::item:selected { background: %2; color: %1; }"
        "QListWidget::item:hover:!selected { background: rgba(20,24,40,0.04); }")
        .arg(textHex, accSubtle, QString::number(UiScale::dp(28)))
        + BlopStyle::paperScrollbarQss()
    );
    tagsLay->addWidget(m_tagList);
    bodyLay->addWidget(tagsGroup);

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

    bodyLay->addStretch(1);

    auto *actionLay = new QHBoxLayout();
    actionLay->setContentsMargins(0, UiScale::dp(4), 0, 0);
    actionLay->setSpacing(UiScale::dp(10));
    actionLay->addStretch();
    m_btnCancel = new QPushButton(QStringLiteral("Abbrechen"), body);
    m_btnCancel->setCursor(Qt::PointingHandCursor);
    m_btnCancel->setAutoDefault(false);
    m_btnCancel->setMinimumHeight(UiScale::dp(BlopStyle::touchTargetMinDp() - 4));
    m_btnCancel->setStyleSheet(BlopStyle::paperSecondaryButtonQss() +
                               QStringLiteral(
                                   "QPushButton { text-align: center; }"));
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    m_btnCreate = new QPushButton(QStringLiteral("Erstellen"), body);
    m_btnCreate->setCursor(Qt::PointingHandCursor);
    m_btnCreate->setAutoDefault(false);
    m_btnCreate->setMinimumHeight(UiScale::dp(BlopStyle::touchTargetMinDp() - 4));
    m_btnCreate->setMinimumWidth(UiScale::dp(112));
    m_btnCreate->setStyleSheet(BlopStyle::paperPrimaryButtonQss());
    connect(m_btnCreate, &QPushButton::clicked, this, &QDialog::accept);
    BlopRipple::attachPressFeedback(m_btnCancel, 0.96);
    BlopRipple::attachPressFeedback(m_btnCreate, 0.96);
    actionLay->addWidget(m_btnCancel);
    actionLay->addWidget(m_btnCreate);
    bodyLay->addLayout(actionLay);

    layout->addWidget(body, 1);
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
    opAnim->setDuration(BlopMotion::kFast);
    opAnim->setStartValue(0.0);
    opAnim->setEndValue(1.0);
    opAnim->setEasingCurve(BlopMotion::kEaseStandard);
    opAnim->start(QAbstractAnimation::DeleteWhenStopped);
#endif
    move(dest.x(), dest.y() + 12);
    auto *posAnim = new QPropertyAnimation(this, "pos", this);
    posAnim->setDuration(BlopMotion::kStandard);
    posAnim->setStartValue(QPoint(dest.x(), dest.y() + 12));
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
