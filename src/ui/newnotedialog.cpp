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
    // Keep paper fill when embedded in BlopModal (avoids transparent wipe).
    setProperty("blopOwnsBackground", true);
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
    const QString accSubtle = BlopStyle::paperPrimaryLight().name(QColor::HexRgb);

    const QString textHex = BlopStyle::paperInk().name(QColor::HexRgb);
    const QString mutedHex = BlopStyle::paperInkMuted().name(QColor::HexRgb);
    const QString borderHex = BlopStyle::paperBorder().name(QColor::HexRgb);
    const QString surfaceHex = BlopStyle::paperSurface().name(QColor::HexRgb);
    const int radLg = UiScale::dp(BlopStyle::radiusLgDp());

    BlopStyle::paintPaperSurface(container, QStringLiteral("NewNoteCard"));
    container->setStyleSheet(QStringLiteral(
        "#NewNoteCard {"
        "  background: %1;"
        "  border: 1px solid %2;"
        "  border-radius: %3px;"
        "}"
        "QLabel { color: %4; border: none; background: transparent; }")
                                 .arg(surfaceHex, borderHex,
                                      QString::number(radLg), textHex));
    // Shadow comes from BlopModal's card — nested DropShadowEffects on the
    // inner container break hit-testing on Windows software rasterizer.

    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // White header (Light Mode) — not Obsidian strip.
    auto *titleBar = new QWidget(container);
    titleBar->setObjectName(QStringLiteral("NewNoteTitleBar"));
    titleBar->setAttribute(Qt::WA_StyledBackground, true);
    titleBar->setFixedHeight(UiScale::dp(48));
    titleBar->setStyleSheet(QStringLiteral(
        "QWidget#NewNoteTitleBar {"
        "  background: %1;"
        "  border-top-left-radius: %2px;"
        "  border-top-right-radius: %2px;"
        "  border-bottom: 1px solid %3;"
        "}")
                                .arg(surfaceHex, QString::number(radLg),
                                     borderHex));
    auto *titleBarLay = new QHBoxLayout(titleBar);
    titleBarLay->setContentsMargins(UiScale::dp(20), 0, UiScale::dp(20), 0);
    auto *lblTitle = new QLabel(QStringLiteral("Neue Notiz"), titleBar);
    lblTitle->setStyleSheet(QStringLiteral(
        "font-size: 15px; font-weight: 700; color: %1;"
        "letter-spacing: -0.2px; background: transparent;")
                                .arg(textHex));
    titleBarLay->addWidget(lblTitle);
    layout->addWidget(titleBar);

    auto *body = new QWidget(container);
    body->setObjectName(QStringLiteral("NewNoteBody"));
    body->setAttribute(Qt::WA_StyledBackground, true);
    body->setStyleSheet(QStringLiteral(
        "QWidget#NewNoteBody { background: %1; border-bottom-left-radius: %2px;"
        "  border-bottom-right-radius: %2px; }")
                            .arg(BlopStyle::paperBg().name(QColor::HexRgb),
                                 QString::number(radLg)));
    auto *bodyLay = new QVBoxLayout(body);
    bodyLay->setContentsMargins(UiScale::dp(20), UiScale::dp(16),
                                UiScale::dp(20), UiScale::dp(16));
    bodyLay->setSpacing(UiScale::dp(10));

    auto sectionLabel = [mutedHex](const QString &text, QWidget *parent) {
        auto *lbl = new QLabel(text, parent);
        lbl->setStyleSheet(QStringLiteral(
            "font-size: 10px; color: %1; font-weight: 700; letter-spacing: 0.6px;"
            "background: transparent; padding-top: 2px;")
            .arg(mutedHex));
        return lbl;
    };

    auto makeRowGroup = [surfaceHex, borderHex, radLg](QWidget *parent) -> QFrame * {
        auto *g = new QFrame(parent);
        g->setObjectName(QStringLiteral("NewNoteRowGroup"));
        g->setAttribute(Qt::WA_StyledBackground, true);
        g->setStyleSheet(QStringLiteral(
            "QFrame#NewNoteRowGroup {"
            "  background: %1;"
            "  border: 1px solid %2;"
            "  border-radius: %3px;"
            "}")
                             .arg(surfaceHex, borderHex, QString::number(radLg)));
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
    m_btnFormatStruktur = makeSeg(QStringLiteral("Struktur"));
    m_btnFormatInfinite->setChecked(true);
    m_groupFormat = new QButtonGroup(this);
    m_groupFormat->addButton(m_btnFormatInfinite, 0);
    m_groupFormat->addButton(m_btnFormatA4, 1);
    m_groupFormat->addButton(m_btnFormatStruktur, 2);
    m_groupFormat->setExclusive(true);
    formatRow->addWidget(m_btnFormatInfinite);
    formatRow->addWidget(m_btnFormatA4);
    formatRow->addWidget(m_btnFormatStruktur);
    optsLay->addLayout(formatRow);

    auto *layoutCap = sectionLabel(QStringLiteral("LAYOUT"), optsGroup);
    m_layoutSection = new QWidget(optsGroup);
    auto *layoutSectionLay = new QVBoxLayout(m_layoutSection);
    layoutSectionLay->setContentsMargins(0, 0, 0, 0);
    layoutSectionLay->setSpacing(UiScale::dp(8));
    layoutSectionLay->addWidget(layoutCap);
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
        auto *btn = new QPushButton(QString::fromUtf8(opt.name), m_layoutSection);
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
    layoutSectionLay->addLayout(layoutRow);
    optsLay->addWidget(m_layoutSection);
    connect(m_groupFormat, &QButtonGroup::idClicked, this, [this](int id) {
        if (m_layoutSection)
            m_layoutSection->setVisible(id != 2);
    });
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
    const int radMd = UiScale::dp(BlopStyle::radiusMdDp());
    btnAddTag->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; color: white; border: none; "
        "border-radius: %3px; font-weight: 700; font-size: 16px; }"
        "QPushButton:hover { background: %2; }")
            .arg(acc, accHover, QString::number(radMd)));
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
        "QListWidget::item { padding: 6px 10px; border-radius: %4px;"
        "  min-height: %3px; }"
        "QListWidget::item:selected { background: %2; color: %5; }"
        "QListWidget::item:hover:!selected { background: %6; }")
        .arg(textHex, accSubtle,
             QString::number(UiScale::dp(28)),
             QString::number(radMd), acc,
             BlopStyle::paperHover().name(QColor::HexRgb))
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
    return createFormat() == 0;
}

int NewNoteDialog::createFormat() const {
    if (m_btnFormatStruktur && m_btnFormatStruktur->isChecked())
        return 2;
    if (m_btnFormatA4 && m_btnFormatA4->isChecked())
        return 1;
    return 0;
}

bool NewNoteDialog::isStrukturFormat() const {
    return createFormat() == 2;
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
