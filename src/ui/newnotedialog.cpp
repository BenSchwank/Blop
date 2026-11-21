#include "newnotedialog.h"
#include "blop_modal.h"
#include "blop_theme.h"
#include "blopripple.h"
#include "blopstyle.h"
#include "editoroverlays.h"
#include "librarytagstore.h"
#include "notepreviewicon.h"
#include "uiscale.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSettings>
#include <QShowEvent>
#include <QSizePolicy>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

constexpr int kSheetWidthDp = 520;
constexpr qreal kSheetHeightFrac = 0.72;

QIcon pageTemplateIcon(int backgroundType, int w, int h, const QColor &paper)
{
    NotePreviewIcon::Spec spec;
    spec.kind = NotePreviewIcon::Kind::A4;
    spec.backgroundType = backgroundType;
    spec.paper = paper.isValid() ? paper : QColor(252, 250, 245);
    const int px = qMax(w, h);
    const QPixmap src = NotePreviewIcon::pixmap(spec, px);
    if (src.isNull())
        return {};
    if (src.size() == QSize(w, h))
        return QIcon(src);
    return QIcon(src.scaled(w, h, Qt::KeepAspectRatio, Qt::FastTransformation));
}

QPixmap paintSwatch(const QColor &color, bool selected, bool dark)
{
    const int s = UiScale::dp(28);
    QPixmap pm(s, s);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    const qreal rad = UiScale::dp(6);
    if (selected) {
        p.setBrush(QColor(0x25, 0x63, 0xEB));
        p.drawRoundedRect(QRectF(0.5, 0.5, s - 1, s - 1), rad, rad);
        p.setBrush(dark ? QColor(0x1C, 0x20, 0x28) : QColor(0xFF, 0xFF, 0xFF));
        p.drawRoundedRect(QRectF(2.5, 2.5, s - 5, s - 5), qMax<qreal>(1, rad - 1),
                          qMax<qreal>(1, rad - 1));
        p.setBrush(color);
        p.drawRoundedRect(QRectF(4.5, 4.5, s - 9, s - 9), qMax<qreal>(1, rad - 2),
                          qMax<qreal>(1, rad - 2));
    } else {
        p.setPen(QPen(dark ? QColor(255, 255, 255, 40) : QColor(0xD1, 0xD5, 0xDB), 1));
        p.setBrush(color);
        p.drawRoundedRect(QRectF(1, 1, s - 2, s - 2), rad, rad);
    }
    return pm;
}

QString rgbaOf(const QColor &c)
{
    return QStringLiteral("rgba(%1,%2,%3,%4)")
        .arg(c.red())
        .arg(c.green())
        .arg(c.blue())
        .arg(QString::number(c.alphaF(), 'f', 3));
}

} // namespace

NewNoteDialog::SheetTokens NewNoteDialog::tokens() const
{
    SheetTokens t;
    t.dark = BlopTheme::instance().isDark();
    if (t.dark) {
        t.surface = BlopTheme::surfaceElevated();
        t.surfaceAlt = BlopTheme::surfaceMuted();
        t.ink = BlopTheme::textPrimary();
        t.muted = BlopTheme::textSecondary();
        t.border = BlopTheme::borderDefault();
        t.hover = BlopTheme::surfaceMuted();
        t.accentSoft = BlopTheme::accentSubtle();
        t.inputQss = BlopTheme::inputQss();
        t.primaryBtnQss = BlopTheme::primaryButtonQss();
        t.secondaryBtnQss = BlopTheme::secondaryButtonQss();
    } else {
        t.surface = BlopStyle::paperSurface();
        t.surfaceAlt = BlopStyle::paperBg();
        t.ink = BlopStyle::paperInk();
        t.muted = BlopStyle::paperInkMuted();
        t.border = BlopStyle::paperBorder();
        t.hover = BlopStyle::paperHover();
        t.accentSoft = BlopStyle::paperPrimaryLight();
        t.inputQss = BlopStyle::paperInputQss();
        t.primaryBtnQss = BlopStyle::paperPrimaryButtonQss();
        t.secondaryBtnQss = BlopStyle::paperSecondaryButtonQss();
    }
    return t;
}

NewNoteDialog::NewNoteDialog(QWidget *parent) : QDialog(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setProperty("blopOwnsBackground", true);
    setProperty("blopForcePaper", true);
    setProperty("blopSoftCardShadow", true);
    setProperty("blopPreventDismiss", true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setStyleSheet(QStringLiteral("QDialog { background: transparent; border: none; }"));
    {
        QSettings st(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
        if (st.value(QStringLiteral("ui/defaultPageColor"),
                     QStringLiteral("light"))
                .toString() == QLatin1String("dark"))
            m_paperColor = QColor(0x16, 0x18, 0x1E);
        else
            m_paperColor = QColor(252, 250, 245);
        m_backgroundType =
            qBound(0, st.value(QStringLiteral("ui/defaultPaperPattern"), 2).toInt(),
                   4);
        m_selectedFormat =
            qBound(0, st.value(QStringLiteral("ui/defaultNoteFormat"), 1).toInt(),
                   2);
    }
    setupUi();
}

BlopModal *NewNoteDialog::hostModal() const
{
    return BlopModal::hostOf(const_cast<NewNoteDialog *>(this));
}

void NewNoteDialog::setupUi()
{
    const SheetTokens tok = tokens();

    m_root = new QWidget(this);
    m_root->setObjectName(QStringLiteral("NewNoteRoot"));
    auto *rootLay = new QVBoxLayout(this);
    rootLay->setContentsMargins(0, 0, 0, 0);
    rootLay->setSpacing(0);
    rootLay->addWidget(m_root, 1);

    auto *stack = new QVBoxLayout(m_root);
    stack->setContentsMargins(0, 0, 0, 0);
    stack->setSpacing(0);

    m_composer = new QFrame(m_root);
    m_composer->setObjectName(QStringLiteral("NewNoteComposer"));
    m_composer->setAttribute(Qt::WA_StyledBackground, true);
    auto *composerLay = new QVBoxLayout(m_composer);
    composerLay->setContentsMargins(0, 0, 0, 0);
    composerLay->setSpacing(0);

    // ── Title bar ──────────────────────────────────────────────────────────
    auto *titleBar = new QWidget(m_composer);
    titleBar->setObjectName(QStringLiteral("NewNoteTitleBar"));
    titleBar->setAttribute(Qt::WA_StyledBackground, true);
    auto *titleCol = new QVBoxLayout(titleBar);
    titleCol->setContentsMargins(UiScale::dp(24), UiScale::dp(20),
                                 UiScale::dp(24), UiScale::dp(16));
    titleCol->setSpacing(UiScale::dp(12));

    auto *titleRow = new QHBoxLayout();
    titleRow->setSpacing(UiScale::dp(8));
    m_composerTitle = new QLabel(QStringLiteral("Neue Notiz erstellen"), titleBar);
    m_composerTitle->setObjectName(QStringLiteral("NewNoteComposerTitle"));
    titleRow->addWidget(m_composerTitle, 1);
    m_livePreview = new QLabel(titleBar);
    m_livePreview->setObjectName(QStringLiteral("NewNoteLivePreview"));
    m_livePreview->setAlignment(Qt::AlignCenter);
    m_livePreview->setFixedSize(UiScale::dp(56), UiScale::dp(72));
    titleRow->addWidget(m_livePreview, 0, Qt::AlignTop);
    auto *btnClose = new QPushButton(QStringLiteral("\u2715"), titleBar);
    btnClose->setObjectName(QStringLiteral("NewNoteClose"));
    btnClose->setFixedSize(UiScale::dp(28), UiScale::dp(28));
    btnClose->setCursor(Qt::PointingHandCursor);
    btnClose->setFlat(true);
    btnClose->setAutoDefault(false);
    btnClose->setFocusPolicy(Qt::NoFocus);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::reject);
    titleRow->addWidget(btnClose, 0, Qt::AlignTop);
    titleCol->addLayout(titleRow);

    m_formatChipBar = new QWidget(titleBar);
    m_formatChipBar->setObjectName(QStringLiteral("NewNoteFormatChips"));
    m_formatChipLay = new QHBoxLayout(m_formatChipBar);
    m_formatChipLay->setContentsMargins(UiScale::dp(4), UiScale::dp(4),
                                        UiScale::dp(4), UiScale::dp(4));
    m_formatChipLay->setSpacing(UiScale::dp(4));
    m_formatChipGroup = new QButtonGroup(this);
    m_formatChipGroup->setExclusive(true);
    const char *chipTitles[] = {"Unendlich", "DIN A4", "Struktur"};
    for (int i = 0; i < 3; ++i) {
        auto *chip = new QPushButton(m_formatChipBar);
        chip->setText(QString::fromUtf8(chipTitles[i]));
        chip->setCheckable(true);
        chip->setCursor(Qt::PointingHandCursor);
        chip->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        chip->setFixedHeight(UiScale::dp(36));
        chip->setProperty("blopFormatId", i);
        m_formatChipGroup->addButton(chip, i);
        m_formatChipLay->addWidget(chip);
    }
    connect(m_formatChipGroup, &QButtonGroup::idClicked, this,
            [this](int id) {
                if (id == m_selectedFormat)
                    return;
                m_selectedFormat = id;
                setFormat(id);
                updateFormatChips();
                refreshLivePreview();
            });
    titleCol->addWidget(m_formatChipBar);
    titleBar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    composerLay->addWidget(titleBar);

    // ── Title field ────────────────────────────────────────────────────────
    auto *titleBlock = new QWidget(m_composer);
    titleBlock->setObjectName(QStringLiteral("NewNoteTitleBlock"));
    auto *titleLay = new QVBoxLayout(titleBlock);
    titleLay->setContentsMargins(UiScale::dp(24), UiScale::dp(16),
                                 UiScale::dp(24), 0);
    titleLay->setSpacing(UiScale::dp(6));
    auto *titleLbl = new QLabel(QStringLiteral("TITEL"), titleBlock);
    titleLbl->setObjectName(QStringLiteral("NewNoteNameLabel"));
    titleLbl->setProperty("blopSection", true);
    titleLay->addWidget(titleLbl);
    m_nameInput = new QLineEdit(titleBlock);
    m_nameInput->setPlaceholderText(QStringLiteral("Neue Notiz"));
    m_nameInput->setFixedHeight(UiScale::dp(40));
    titleLay->addWidget(m_nameInput);
    connect(m_nameInput, &QLineEdit::returnPressed, this, &QDialog::accept);
    connect(m_nameInput, &QLineEdit::textChanged, this,
            [this](const QString &) { refreshLivePreview(); });
    titleBlock->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    composerLay->addWidget(titleBlock);

    // ── Scrollable body ────────────────────────────────────────────────────
    auto *scroll = new QScrollArea(m_composer);
    scroll->setObjectName(QStringLiteral("NewNoteScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto *body = new QWidget(scroll);
    body->setObjectName(QStringLiteral("NewNoteBody"));
    body->setAttribute(Qt::WA_StyledBackground, true);
    auto *bodyLay = new QVBoxLayout(body);
    bodyLay->setContentsMargins(UiScale::dp(24), UiScale::dp(16),
                                UiScale::dp(24), UiScale::dp(8));
    bodyLay->setSpacing(UiScale::dp(16));

    auto sectionLabel = [](const QString &text, QWidget *parent) {
        auto *lbl = new QLabel(text, parent);
        lbl->setProperty("blopSection", true);
        return lbl;
    };

    m_layoutSection = new QWidget(body);
    m_layoutSection->setObjectName(QStringLiteral("NewNoteLayoutSection"));
    auto *layoutSectionLay = new QVBoxLayout(m_layoutSection);
    layoutSectionLay->setContentsMargins(0, 0, 0, 0);
    layoutSectionLay->setSpacing(UiScale::dp(8));
    layoutSectionLay->addWidget(sectionLabel(QStringLiteral("VORLAGE"), m_layoutSection));

    m_groupLayout = new QButtonGroup(this);
    m_groupLayout->setExclusive(true);
    auto *tplRow = new QHBoxLayout();
    tplRow->setSpacing(UiScale::dp(8));

    struct LayoutOpt {
        int type;
        const char *name;
    };
    const LayoutOpt opts[] = {
        {0, "Leer"}, {1, "Liniert"}, {2, "Kariert"},
        {3, "Punktiert"}, {4, "Legal"},
    };
    constexpr int kIcon = 24;
    for (const auto &opt : opts) {
        auto *tb = new QToolButton(m_layoutSection);
        tb->setText(QString::fromUtf8(opt.name));
        tb->setCheckable(true);
        tb->setAutoRaise(true);
        tb->setCursor(Qt::PointingHandCursor);
        tb->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        tb->setIconSize(QSize(UiScale::dp(kIcon), UiScale::dp(kIcon)));
        tb->setMinimumHeight(UiScale::dp(64));
        tb->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        tb->setProperty("blopBgType", opt.type);
        m_groupLayout->addButton(tb, opt.type);
        if (opt.type == m_backgroundType)
            tb->setChecked(true);
        tplRow->addWidget(tb, 1);
        BlopRipple::attachPressFeedback(tb, 0.96);
    }
    connect(m_groupLayout, &QButtonGroup::idClicked, this, [this](int id) {
        m_backgroundType = id;
        refreshLivePreview();
    });
    layoutSectionLay->addLayout(tplRow);

    m_paperSection = new QWidget(m_layoutSection);
    auto *paperCol = new QVBoxLayout(m_paperSection);
    paperCol->setContentsMargins(0, UiScale::dp(4), 0, 0);
    paperCol->setSpacing(UiScale::dp(6));
    paperCol->addWidget(sectionLabel(QStringLiteral("SEITENFARBE"), m_paperSection));
    auto *paperLay = new QHBoxLayout();
    paperLay->setSpacing(UiScale::dp(10));
    m_paperSwatch = new QPushButton(m_paperSection);
    m_paperSwatch->setObjectName(QStringLiteral("NewNotePaperPreset"));
    m_paperSwatch->setProperty("blopPaperCustom", true);
    m_paperSwatch->setFixedSize(UiScale::dp(28), UiScale::dp(28));
    m_paperSwatch->setCursor(Qt::PointingHandCursor);
    m_paperSwatch->setIconSize(QSize(UiScale::dp(28), UiScale::dp(28)));
    paperLay->addWidget(m_paperSwatch);

    const QColor presets[] = {
        QColor(252, 250, 245), QColor(255, 255, 255), QColor(245, 248, 255),
        QColor(255, 248, 240), QColor(240, 252, 245), QColor(0x16, 0x18, 0x1E),
    };
    for (const QColor &c : presets) {
        auto *sw = new QPushButton(m_paperSection);
        sw->setObjectName(QStringLiteral("NewNotePaperPreset"));
        sw->setProperty("blopPaperColor", c);
        sw->setFixedSize(UiScale::dp(28), UiScale::dp(28));
        sw->setIconSize(QSize(UiScale::dp(28), UiScale::dp(28)));
        sw->setCursor(Qt::PointingHandCursor);
        connect(sw, &QPushButton::clicked, this, [this, c]() {
            m_paperColor = c;
            refreshPaperSwatch();
            refreshTemplateIcons();
            refreshLivePreview();
        });
        paperLay->addWidget(sw);
    }
    paperLay->addStretch(1);
    paperCol->addLayout(paperLay);
    layoutSectionLay->addWidget(m_paperSection);
    bodyLay->addWidget(m_layoutSection);

    m_strukturHint = new QLabel(
        QStringLiteral(
            "Struktur nutzt Text-Blöcke und Embeds — keine Papier-Vorlage."),
        body);
    m_strukturHint->setObjectName(QStringLiteral("NewNoteStrukturHint"));
    m_strukturHint->setWordWrap(true);
    m_strukturHint->hide();
    bodyLay->addWidget(m_strukturHint);

    connect(m_paperSwatch, &QPushButton::clicked, this, [this]() {
        QWidget *host = window() ? window() : this;
        QColor c = m_paperColor;
        if (showColorPickerOverlay(host, &c, QStringLiteral("Seitenfarbe wählen"))) {
            m_paperColor = c;
            refreshPaperSwatch();
            refreshTemplateIcons();
            refreshLivePreview();
        }
    });

    auto *tagBlock = new QWidget(body);
    auto *tagBlockLay = new QVBoxLayout(tagBlock);
    tagBlockLay->setContentsMargins(0, 0, 0, 0);
    tagBlockLay->setSpacing(UiScale::dp(6));
    tagBlockLay->addWidget(sectionLabel(QStringLiteral("TAGS"), tagBlock));
    m_tagChipHost = new QWidget(tagBlock);
    m_tagChipHost->setObjectName(QStringLiteral("NewNoteTagChips"));
    m_tagChipLay = new QHBoxLayout(m_tagChipHost);
    m_tagChipLay->setContentsMargins(0, 0, 0, 0);
    m_tagChipLay->setSpacing(UiScale::dp(6));
    m_tagGroup = new QButtonGroup(this);
    m_tagGroup->setExclusive(false);
    tagBlockLay->addWidget(m_tagChipHost);
    bodyLay->addWidget(tagBlock);

    auto *tagRow = new QHBoxLayout();
    tagRow->setSpacing(UiScale::dp(8));
    m_tagInput = new QLineEdit(body);
    m_tagInput->setPlaceholderText(QStringLiteral("Neuen Tag erstellen..."));
    m_tagInput->setFixedHeight(UiScale::dp(38));
    auto *btnAddTag = new QPushButton(QStringLiteral("+"), body);
    btnAddTag->setObjectName(QStringLiteral("NewNoteAddTag"));
    btnAddTag->setAutoDefault(false);
    btnAddTag->setFixedSize(UiScale::dp(38), UiScale::dp(38));
    btnAddTag->setCursor(Qt::PointingHandCursor);
    tagRow->addWidget(m_tagInput, 1);
    tagRow->addWidget(btnAddTag);
    bodyLay->addLayout(tagRow);
    bodyLay->addStretch(1);

    auto addTagFromInput = [this]() {
        const QString n = LibraryTagStore::normalize(m_tagInput->text());
        if (n.isEmpty())
            return;
        LibraryTagStore::addTagToCatalog(m_tagInput->text());
        m_tagInput->clear();
        rebuildTagList();
        if (!m_tagGroup)
            return;
        for (QAbstractButton *b : m_tagGroup->buttons()) {
            if (!b)
                continue;
            QString name = b->text();
            if (name.isEmpty()) {
                if (auto *lbl = b->findChild<QLabel *>(
                        QStringLiteral("NewNoteTagChipText")))
                    name = lbl->text();
            }
            if (name == n)
                b->setChecked(true);
        }
    };
    connect(btnAddTag, &QPushButton::clicked, this, addTagFromInput);
    connect(m_tagInput, &QLineEdit::returnPressed, this, addTagFromInput);

    scroll->setWidget(body);
    composerLay->addWidget(scroll, 1);

    // ── Footer (always visible) ────────────────────────────────────────────
    auto *footer = new QWidget(m_composer);
    footer->setObjectName(QStringLiteral("NewNoteFooter"));
    footer->setAttribute(Qt::WA_StyledBackground, true);
    auto *actionLay = new QHBoxLayout(footer);
    actionLay->setContentsMargins(UiScale::dp(24), UiScale::dp(14),
                                  UiScale::dp(24), UiScale::dp(16));
    actionLay->setSpacing(UiScale::dp(10));
    actionLay->addStretch();
    m_btnCancel = new QPushButton(QStringLiteral("Abbrechen"), footer);
    m_btnCancel->setCursor(Qt::PointingHandCursor);
    m_btnCancel->setAutoDefault(false);
    m_btnCancel->setMinimumHeight(UiScale::dp(BlopStyle::touchTargetMinDp() - 4));
    m_btnCancel->setMinimumWidth(UiScale::dp(110));
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    m_btnCreate = new QPushButton(QStringLiteral("Erstellen"), footer);
    m_btnCreate->setCursor(Qt::PointingHandCursor);
    m_btnCreate->setAutoDefault(true);
    m_btnCreate->setDefault(true);
    m_btnCreate->setMinimumHeight(UiScale::dp(BlopStyle::touchTargetMinDp() - 4));
    m_btnCreate->setMinimumWidth(UiScale::dp(120));
    connect(m_btnCreate, &QPushButton::clicked, this, &QDialog::accept);
    BlopRipple::attachPressFeedback(m_btnCancel, 0.96);
    BlopRipple::attachPressFeedback(m_btnCreate, 0.96);
    actionLay->addWidget(m_btnCancel);
    actionLay->addWidget(m_btnCreate);
    footer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    composerLay->addWidget(footer, 0);

    stack->addWidget(m_composer, 1);

    setFormat(m_selectedFormat);
    updateFormatChips();
    applyChrome();
    rebuildTagList();
    refreshTemplateIcons();
    refreshLivePreview();

    connect(&BlopTheme::instance(), &BlopTheme::themeChanged, this,
            [this]() { applyChrome(); });

    Q_UNUSED(tok);
}

void NewNoteDialog::applyHostModalSize(bool animate)
{
    setProperty("blopPreventDismiss", true);
    BlopModal *modal = hostModal();
    if (!modal)
        return;
    const int w = UiScale::dp(kSheetWidthDp);
    const qreal h = kSheetHeightFrac;
    if (animate) {
        modal->preparePreferredSize(w, h);
        modal->animateCardToPreferred(140);
    } else {
        modal->setPreferredCardWidth(w);
        modal->setPreferredCardHeightFrac(h);
    }
}

void NewNoteDialog::applyChrome()
{
    const SheetTokens tok = tokens();
    const QString acc = BlopTheme::accentPrimary().name(QColor::HexRgb);
    const QString surface = tok.surface.name(QColor::HexRgb);
    const QString surfaceAlt = tok.surfaceAlt.name(QColor::HexRgb);
    const QString ink = tok.ink.name(QColor::HexRgb);
    const QString muted = tok.muted.name(QColor::HexRgb);
    const QString border = rgbaOf(tok.border);
    const QString hover = tok.hover.name(QColor::HexRgb);
    const QString soft = rgbaOf(tok.accentSoft);
    const int radLg = UiScale::dp(BlopStyle::radiusLgDp() + 4);
    const int radMd = UiScale::dp(BlopStyle::radiusMdDp());

    if (m_root) {
        m_root->setStyleSheet(QStringLiteral(
            "QWidget#NewNoteRoot {"
            "  background: %1; border: 1px solid %2; border-radius: %3px;"
            "}")
                                  .arg(surface, border, QString::number(radLg)));
    }
    if (m_composer) {
        m_composer->setStyleSheet(QStringLiteral(
            "QFrame#NewNoteComposer { background: %1; border: none; }")
                                      .arg(surface));
    }
    if (auto *titleBar =
            findChild<QWidget *>(QStringLiteral("NewNoteTitleBar"))) {
        titleBar->setStyleSheet(QStringLiteral(
            "QWidget#NewNoteTitleBar {"
            "  background: transparent; border: none;"
            "  border-bottom: 1px solid %1;"
            "}")
                                    .arg(border));
    }
    if (m_composerTitle) {
        m_composerTitle->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 18px; font-weight: 700; background: transparent;")
                                           .arg(ink));
    }
    if (auto *closeBtn = findChild<QPushButton *>(QStringLiteral("NewNoteClose"))) {
        closeBtn->setStyleSheet(QStringLiteral(
            "QPushButton#NewNoteClose {"
            "  background: transparent; border: none; border-radius: 6px;"
            "  color: %1; font-size: 14px;"
            "}"
            "QPushButton#NewNoteClose:hover { background: %2; }")
                                    .arg(muted, hover));
    }
    if (m_formatChipBar) {
        m_formatChipBar->setStyleSheet(BlopStyle::segmentQss());
        for (QAbstractButton *b : m_formatChipGroup
                                      ? m_formatChipGroup->buttons()
                                      : QList<QAbstractButton *>()) {
            if (auto *pb = qobject_cast<QPushButton *>(b))
                pb->setStyleSheet(QString());
        }
    }

    for (QLabel *lbl : findChildren<QLabel *>()) {
        if (lbl->property("blopSection").toBool()) {
            lbl->setStyleSheet(QStringLiteral(
                "font-size: 11px; color: %1; font-weight: 600;"
                "letter-spacing: 0.6px; background: transparent;")
                                   .arg(muted));
        }
    }
    if (auto *nameLbl = findChild<QLabel *>(QStringLiteral("NewNoteNameLabel"))) {
        nameLbl->setStyleSheet(QStringLiteral(
            "font-size: 11px; font-weight: 600; color: %1;"
            "letter-spacing: 0.6px; background: transparent;")
                                   .arg(muted));
    }

    if (m_nameInput)
        m_nameInput->setStyleSheet(tok.inputQss);
    if (m_tagInput)
        m_tagInput->setStyleSheet(tok.inputQss);

    if (auto *scroll = findChild<QScrollArea *>(QStringLiteral("NewNoteScroll"))) {
        scroll->setStyleSheet(QStringLiteral(
            "QScrollArea#NewNoteScroll { background: transparent; border: none; }"
            "QScrollBar:vertical {"
            "  background: transparent; width: 8px; margin: 4px 2px; border: none;"
            "}"
            "QScrollBar::handle:vertical {"
            "  background: %1; border-radius: 4px; min-height: 24px;"
            "}"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
            "  height: 0; width: 0;"
            "}")
                                  .arg(tok.dark ? QStringLiteral("rgba(255,255,255,0.22)")
                                                : QStringLiteral("rgba(15,23,42,0.18)")));
        if (QWidget *w = scroll->widget())
            w->setStyleSheet(QStringLiteral("background: transparent;"));
    }

    if (m_groupLayout) {
        for (QAbstractButton *b : m_groupLayout->buttons()) {
            auto *tb = qobject_cast<QToolButton *>(b);
            if (!tb)
                continue;
            tb->setStyleSheet(QStringLiteral(
                "QToolButton {"
                "  background: %1; color: %2; border: 1px solid %3;"
                "  border-radius: %4px; padding: 8px 6px;"
                "  font-size: 11px; font-weight: 600;"
                "}"
                "QToolButton:checked {"
                "  background: %5; color: %6; border: 1px solid %6;"
                "}"
                "QToolButton:hover:!checked { background: %7; }")
                                  .arg(surface, ink, border,
                                       QString::number(radMd), soft, acc,
                                       hover));
        }
    }

    if (m_strukturHint) {
        m_strukturHint->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 12px; background: transparent; padding: 4px 0;")
                                          .arg(muted));
    }

    refreshPaperSwatch();

    if (auto *btnAdd =
            findChild<QPushButton *>(QStringLiteral("NewNoteAddTag"))) {
        btnAdd->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background: transparent; color: %1;"
            "  border: 1px solid %2; border-radius: 8px;"
            "  font-weight: 600; font-size: 16px;"
            "}"
            "QPushButton:hover { background: %3; }")
                                  .arg(ink, border, hover));
    }
    if (m_tagGroup) {
        for (QAbstractButton *b : m_tagGroup->buttons()) {
            if (!b)
                continue;
            b->setStyleSheet(QStringLiteral(
                "QPushButton#NewNoteTagChip {"
                "  background: %1; border: 1px solid %2;"
                "  border-radius: 14px; padding: 0 12px;"
                "  min-height: %3px; max-height: %3px;"
                "  color: transparent; font-size: 1px;"
                "}"
                "QPushButton#NewNoteTagChip QLabel#NewNoteTagChipText {"
                "  color: %4; font-size: 12px; font-weight: 500;"
                "  background: transparent; padding: 0; margin: 0;"
                "}"
                "QPushButton#NewNoteTagChip:checked {"
                "  background: %5; border: 1.5px solid %6;"
                "}"
                "QPushButton#NewNoteTagChip:checked QLabel#NewNoteTagChipText {"
                "  color: %6;"
                "}"
                "QPushButton#NewNoteTagChip:hover:!checked { background: %7; }")
                                 .arg(tok.dark ? QStringLiteral("transparent")
                                               : surface,
                                      border,
                                      QString::number(UiScale::dp(28)),
                                      tok.dark ? ink : QStringLiteral("#334155"),
                                      soft, acc, hover));
        }
    }

    if (auto *footer =
            findChild<QWidget *>(QStringLiteral("NewNoteFooter"))) {
        footer->setStyleSheet(QStringLiteral(
            "QWidget#NewNoteFooter {"
            "  background: %1; border-top: 1px solid %2;"
            "  border-bottom-left-radius: %3px; border-bottom-right-radius: %3px;"
            "}")
                                  .arg(surfaceAlt, border, QString::number(radLg)));
    }
    if (m_btnCancel)
        m_btnCancel->setStyleSheet(tok.secondaryBtnQss);
    if (m_btnCreate)
        m_btnCreate->setStyleSheet(tok.primaryBtnQss);

    refreshTemplateIcons();
    refreshLivePreview();
}

void NewNoteDialog::refreshLivePreview()
{
    if (!m_livePreview)
        return;
    const int px = UiScale::dp(56);
    NotePreviewIcon::Spec spec;
    const int fmt = createFormat();
    if (fmt == 0) {
        spec.kind = NotePreviewIcon::Kind::Infinite;
        spec.backgroundType = 3;
    } else if (fmt == 2) {
        spec.kind = NotePreviewIcon::Kind::Struktur;
        spec.backgroundType = 0;
    } else {
        spec.kind = NotePreviewIcon::Kind::A4;
        spec.backgroundType = m_backgroundType;
    }
    spec.paper = m_paperColor.isValid() ? m_paperColor : QColor(252, 250, 245);
    const QPixmap pm = NotePreviewIcon::pixmap(spec, px);
    m_livePreview->setPixmap(pm);
    if (!pm.isNull())
        m_livePreview->setFixedSize(pm.size());
}

void NewNoteDialog::updateFormatChips()
{
    if (!m_formatChipGroup)
        return;
    if (QAbstractButton *b = m_formatChipGroup->button(m_selectedFormat))
        b->setChecked(true);
}

void NewNoteDialog::setFormat(int formatId)
{
    const bool showPaper = formatId != 2;
    setLayoutSectionVisible(showPaper);
    if (m_strukturHint)
        m_strukturHint->setVisible(!showPaper);
}

void NewNoteDialog::setLayoutSectionVisible(bool visible)
{
    if (!m_layoutSection)
        return;
    m_layoutSection->setVisible(visible);
    m_layoutSection->setMaximumHeight(visible ? QWIDGETSIZE_MAX : 0);
}

void NewNoteDialog::refreshPaperSwatch()
{
    const bool dark = BlopTheme::instance().isDark();
    bool presetMatch = false;
    const auto presets =
        findChildren<QPushButton *>(QStringLiteral("NewNotePaperPreset"));
    for (QPushButton *sw : presets) {
        if (!sw || sw->property("blopPaperCustom").toBool())
            continue;
        const QColor c = sw->property("blopPaperColor").value<QColor>();
        if (c.isValid() && c == m_paperColor)
            presetMatch = true;
    }
    auto paintBtn = [&](QPushButton *sw, const QColor &c, bool selected) {
        if (!sw || !c.isValid())
            return;
        sw->setIcon(QIcon(paintSwatch(c, selected, dark)));
        sw->setIconSize(QSize(UiScale::dp(28), UiScale::dp(28)));
        sw->setStyleSheet(QStringLiteral(
            "QPushButton { background: transparent; border: none; padding: 0; }"));
    };
    for (QPushButton *sw : presets) {
        if (!sw || sw->property("blopPaperCustom").toBool())
            continue;
        const QColor c = sw->property("blopPaperColor").value<QColor>();
        paintBtn(sw, c, c.isValid() && c == m_paperColor);
    }
    if (m_paperSwatch)
        paintBtn(m_paperSwatch, m_paperColor, !presetMatch);
}

void NewNoteDialog::refreshTemplateIcons()
{
    if (!m_groupLayout)
        return;
    constexpr int kIcon = 24;
    for (QAbstractButton *b : m_groupLayout->buttons()) {
        auto *tb = qobject_cast<QToolButton *>(b);
        if (!tb)
            continue;
        const int type = tb->property("blopBgType").toInt();
        tb->setIcon(pageTemplateIcon(type, UiScale::dp(kIcon),
                                     UiScale::dp(kIcon), m_paperColor));
    }
}

void NewNoteDialog::rebuildTagList()
{
    if (!m_tagChipLay || !m_tagGroup)
        return;
    const QStringList selected = selectedTags();
    while (QLayoutItem *it = m_tagChipLay->takeAt(0)) {
        if (QWidget *w = it->widget()) {
            if (auto *b = qobject_cast<QAbstractButton *>(w))
                m_tagGroup->removeButton(b);
            delete w;
        }
        delete it;
    }
    QStringList catalog = LibraryTagStore::catalog();
    if (catalog.isEmpty()) {
        LibraryTagStore::addTagToCatalog(QStringLiteral("Projekt"));
        LibraryTagStore::addTagToCatalog(QStringLiteral("Entwurf"));
        catalog = LibraryTagStore::catalog();
    }
    QFont chipFont;
    chipFont.setPixelSize(12);
    chipFont.setWeight(QFont::Medium);
    const QFontMetrics fm(chipFont);
    for (const QString &tag : catalog) {
        auto *chip = new QPushButton(tag, m_tagChipHost);
        chip->setObjectName(QStringLiteral("NewNoteTagChip"));
        chip->setCheckable(true);
        chip->setChecked(selected.contains(tag));
        chip->setCursor(Qt::PointingHandCursor);
        chip->setFocusPolicy(Qt::NoFocus);
        chip->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        chip->setFixedHeight(UiScale::dp(28));
        chip->setMinimumWidth(fm.horizontalAdvance(tag) + UiScale::dp(40));
        auto *chipLay = new QHBoxLayout(chip);
        chipLay->setContentsMargins(UiScale::dp(12), 0, UiScale::dp(12), 0);
        chipLay->setSpacing(UiScale::dp(6));
        const QColor dot = LibraryTagStore::tagColor(tag);
        auto *dotMark = new QWidget(chip);
        const int dotS = UiScale::dp(8);
        dotMark->setFixedSize(dotS, dotS);
        dotMark->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        dotMark->setStyleSheet(
            QStringLiteral("background: %1; border-radius: %2px; border: none;")
                .arg(dot.name(QColor::HexRgb))
                .arg(dotS / 2));
        auto *chipText = new QLabel(tag, chip);
        chipText->setObjectName(QStringLiteral("NewNoteTagChipText"));
        chipText->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        chipText->setAlignment(Qt::AlignVCenter);
        chipText->setFont(chipFont);
        chipLay->addWidget(dotMark, 0, Qt::AlignVCenter);
        chipLay->addWidget(chipText, 0, Qt::AlignVCenter);
        m_tagGroup->addButton(chip);
        m_tagChipLay->addWidget(chip);
    }
    m_tagChipLay->addStretch(1);
    applyChrome();
}

QString NewNoteDialog::getNoteName() const
{
    QString t = m_nameInput ? m_nameInput->text().trimmed() : QString();
    return t.isEmpty() ? QStringLiteral("Neue Notiz") : t;
}

bool NewNoteDialog::isInfiniteFormat() const
{
    return createFormat() == 0;
}

int NewNoteDialog::createFormat() const
{
    if (m_selectedFormat >= 0)
        return m_selectedFormat;
    return 1;
}

bool NewNoteDialog::isStrukturFormat() const
{
    return createFormat() == 2;
}

QStringList NewNoteDialog::selectedTags() const
{
    QStringList out;
    if (!m_tagGroup)
        return out;
    for (QAbstractButton *b : m_tagGroup->buttons()) {
        if (!b || !b->isChecked())
            continue;
        QString name = b->text();
        if (name.isEmpty()) {
            if (auto *lbl = b->findChild<QLabel *>(
                    QStringLiteral("NewNoteTagChipText")))
                name = lbl->text();
        }
        if (!name.isEmpty())
            out.append(name);
    }
    return out;
}

void NewNoteDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
}

void NewNoteDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    setWindowOpacity(1.0);
    QTimer::singleShot(0, this, [this]() {
        applyHostModalSize(/*animate=*/false);
        if (m_nameInput)
            m_nameInput->setFocus();
        refreshLivePreview();
    });
    if (!isWindow() || !parentWidget() || m_dialogIntroDone)
        return;
    m_dialogIntroDone = true;
}

void NewNoteDialog::mousePressEvent(QMouseEvent *event)
{
    if (!isWindow()) {
        QDialog::mousePressEvent(event);
        return;
    }
    if (event->button() == Qt::LeftButton) {
        m_dragPos = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
        return;
    }
    QDialog::mousePressEvent(event);
}

void NewNoteDialog::mouseMoveEvent(QMouseEvent *event)
{
    if (!isWindow()) {
        QDialog::mouseMoveEvent(event);
        return;
    }
    if (event->buttons() & Qt::LeftButton) {
        move(event->globalPosition().toPoint() - m_dragPos);
        event->accept();
        return;
    }
    QDialog::mouseMoveEvent(event);
}
