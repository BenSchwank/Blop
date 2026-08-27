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
                                  .arg(QString::number(0.20, 'f', 3));

    // Theme-aware QSS: avoid hard-coded dark-only colors so Light/Dark match
    // the "mix" mockups. We keep token-based accent + neutral surfaces.
    const bool dark = BlopTheme::instance().isDark();
    const int leR = dark ? 22 : 245;
    const int leG = dark ? 24 : 244;
    const int leB = dark ? 36 : 248;
    const double leA = dark ? 0.95 : 0.80;
    // NOTE: % placeholders must match exactly the QString::arg(...) list.
    // Avoid QString::arg placeholder confusion by assembling this block
    // explicitly (no % placeholders here).
    const QString lineEditQss = QStringLiteral(
        "QLabel {"
        "  color: %1;"
        "  font-family: 'Segoe UI';"
        "  border: none;"
        "  background: transparent;"
        "}"
        "QLineEdit {"
        "  background: rgba(%2,%3,%4, %5);"
        "  color: %1;"
        "  border: 1px solid rgba(120,130,160,0.28);"
        "  border-radius: 10px;"
        "  padding: 10px 14px;"
        "  font-size: 15px;"
        "  selection-background-color: %6;"
        "}"
        "QLineEdit:focus { border: 1px solid %7; }");
    // Replace placeholders explicitly (no QString::arg overload quirks).
    QString lineEditQssFinal = lineEditQss;
    const QString textHex = BlopTheme::textPrimary().name(QColor::HexRgb);
    lineEditQssFinal.replace(QStringLiteral("%1"), textHex);
    lineEditQssFinal.replace(QStringLiteral("%2"),
                              QString::number(leR));
    lineEditQssFinal.replace(QStringLiteral("%3"),
                              QString::number(leG));
    lineEditQssFinal.replace(QStringLiteral("%4"),
                              QString::number(leB));
    lineEditQssFinal.replace(QStringLiteral("%5"),
                              QString::number(leA, 'f', 3));
    lineEditQssFinal.replace(QStringLiteral("%6"), accSubtle);
    lineEditQssFinal.replace(QStringLiteral("%7"), acc);

    // Keep this block free of BlopTheme::themed() placeholder processing to
    // avoid `%1` clashes with internal QSS theming replacements.
    container->setStyleSheet(BlopStyle::surfaceStyle(
                                  QStringLiteral("NewNoteCard")) +
                              lineEditQssFinal);

    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(UiScale::dp(22), UiScale::dp(18),
                               UiScale::dp(22), UiScale::dp(16));
    layout->setSpacing(UiScale::dp(12));

    auto *lblTitle = new QLabel(QStringLiteral("Neue Notiz"), container);
    lblTitle->setStyleSheet(BlopTheme::themed(QStringLiteral(
        "font-size: 20px; font-weight: 700; color: #E0E0E0; letter-spacing: -0.2px;")));
    layout->addWidget(lblTitle);

    auto sectionLabel = [](const QString &text, QWidget *parent) {
        auto *lbl = new QLabel(text, parent);
        lbl->setStyleSheet(BlopTheme::themed(QStringLiteral(
            "font-size: 12px; color: #A0A0C8; font-weight: 600; letter-spacing: 0.3px;")));
        return lbl;
    };

    layout->addWidget(sectionLabel(QStringLiteral("Titel"), container));
    m_nameInput = new QLineEdit(container);
    m_nameInput->setPlaceholderText(QStringLiteral("Unbenannte Notiz"));
    m_nameInput->setMinimumHeight(UiScale::dp(40));
    m_nameInput->setFocus();
    layout->addWidget(m_nameInput);
    connect(m_nameInput, &QLineEdit::returnPressed, this, &QDialog::accept);

    layout->addWidget(sectionLabel(QStringLiteral("Format"), container));

    // Segmented controls: use theme text + neutral borders, keep accent fill.
    const QString segQss = BlopTheme::themed(QStringLiteral(
        "QPushButton { background: transparent; color: %1; border: 1px solid rgba(120,130,160,0.28); "
        "border-radius: 8px; padding: 8px 12px; font-size: 13px; font-weight: 600; }"
        "QPushButton:checked { background: %2; color: %3; border: 1px solid %4; }"
        "QPushButton:hover:!checked { background: rgba(255,255,255,0.06); }"))
                            .arg(BlopTheme::textSecondary().name(QColor::HexRgb),
                                 accSubtle,
                                 BlopTheme::textPrimary().name(QColor::HexRgb),
                                 acc);

    auto *formatRow = new QHBoxLayout();
    formatRow->setSpacing(UiScale::dp(8));
    auto makeSeg = [this, container, &segQss](const QString &text) {
        auto *btn = new QPushButton(text, container);
        btn->setCheckable(true);
        btn->setAutoDefault(false);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setMinimumHeight(UiScale::dp(36));
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
    layout->addLayout(formatRow);

    layout->addWidget(sectionLabel(QStringLiteral("Layout"), container));

    const QString chipQss = BlopTheme::themed(QStringLiteral(
        "QPushButton { background: transparent; color: %1; border: 1px solid rgba(120,130,160,0.28); "
        "border-radius: 8px; padding: 6px 10px; font-size: 12px; font-weight: 600; }"
        "QPushButton:checked { background: %2; color: %3; border: 1px solid %4; }"
        "QPushButton:hover:!checked { background: rgba(255,255,255,0.06); }"))
                             .arg(BlopTheme::textSecondary().name(QColor::HexRgb),
                                  accSubtle,
                                  BlopTheme::textPrimary().name(QColor::HexRgb),
                                  acc);

    m_groupLayout = new QButtonGroup(this);
    m_groupLayout->setExclusive(true);
    auto *layoutRow = new QHBoxLayout();
    layoutRow->setSpacing(UiScale::dp(6));
    struct LayoutOpt { int type; const char *name; };
    const LayoutOpt opts[] = {
        {0, "Leer"},
        {1, "Liniert"},
        {2, "Kariert"},
        {3, "Punktiert"},
        {4, "Legal"},
    };
    for (const auto &opt : opts) {
        auto *btn = new QPushButton(QString::fromUtf8(opt.name), container);
        btn->setCheckable(true);
        btn->setAutoDefault(false);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setMinimumHeight(UiScale::dp(32));
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        btn->setStyleSheet(chipQss);
        btn->setProperty("blopBgType", opt.type);
        m_groupLayout->addButton(btn, opt.type);
        if (opt.type == m_backgroundType)
            btn->setChecked(true);
        layoutRow->addWidget(btn);
        BlopRipple::attachPressFeedback(btn, 0.96);
    }
    connect(m_groupLayout, &QButtonGroup::idClicked, this,
            [this](int id) { m_backgroundType = id; });
    layout->addLayout(layoutRow);

    layout->addWidget(sectionLabel(QStringLiteral("Tags"), container));

    auto *tagRow = new QHBoxLayout();
    tagRow->setSpacing(UiScale::dp(8));
    m_tagInput = new QLineEdit(container);
    m_tagInput->setPlaceholderText(QStringLiteral("Tag hinzufügen…"));
    m_tagInput->setMinimumHeight(UiScale::dp(36));
    auto *btnAddTag = new QPushButton(QStringLiteral("+"), container);
    btnAddTag->setAutoDefault(false);
    btnAddTag->setFixedSize(UiScale::dp(36), UiScale::dp(36));
    btnAddTag->setCursor(Qt::PointingHandCursor);
    btnAddTag->setStyleSheet(BlopTheme::themed(QStringLiteral(
        "QPushButton { background: %1; color: white; border: none; "
        "border-radius: 8px; font-weight: 700; font-size: 18px; }"
        "QPushButton:hover { background: %2; }"))
            .arg(acc, accHover));
    tagRow->addWidget(m_tagInput, 1);
    tagRow->addWidget(btnAddTag);
    layout->addLayout(tagRow);

    m_tagList = new QListWidget(container);
    m_tagList->setSelectionMode(QAbstractItemView::MultiSelection);
    m_tagList->setFrameShape(QFrame::NoFrame);
    m_tagList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_tagList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_tagList->setFixedHeight(UiScale::dp(72));
    m_tagList->setStyleSheet(BlopTheme::themed(QStringLiteral(
        "QListWidget { background: transparent; color: #E0E0E0; border: none; "
        "font-size: 13px; outline: none; }"
        "QListWidget::item { padding: 6px 8px; border-radius: 8px; }"
        "QListWidget::item:selected { background: rgba(124,92,252,0.22); }")));
    layout->addWidget(m_tagList);

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

    layout->addStretch(1);

    auto *actionLay = new QHBoxLayout();
    actionLay->setSpacing(UiScale::dp(10));
    actionLay->addStretch();

    m_btnCancel = new QPushButton(QStringLiteral("Abbrechen"), container);
    m_btnCancel->setCursor(Qt::PointingHandCursor);
    m_btnCancel->setAutoDefault(false);
    m_btnCancel->setMinimumHeight(UiScale::dp(40));
    m_btnCancel->setStyleSheet(BlopTheme::tertiaryButtonQss());
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    m_btnCreate = new QPushButton(QStringLiteral("Erstellen"), container);
    m_btnCreate->setCursor(Qt::PointingHandCursor);
    m_btnCreate->setAutoDefault(false);
    m_btnCreate->setMinimumHeight(UiScale::dp(40));
    m_btnCreate->setMinimumWidth(UiScale::dp(120));
    m_btnCreate->setStyleSheet(BlopTheme::primaryButtonQss());
    connect(m_btnCreate, &QPushButton::clicked, this, &QDialog::accept);
    BlopRipple::attachPressFeedback(m_btnCancel, 0.96);
    BlopRipple::attachPressFeedback(m_btnCreate, 0.96);

    actionLay->addWidget(m_btnCancel);
    actionLay->addWidget(m_btnCreate);
    layout->addLayout(actionLay);
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
