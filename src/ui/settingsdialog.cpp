#include "settingsdialog.h"
#include "blop_inwindow_menu.h"
#include "blop_modal.h"
#include "blop_theme.h"
#include "blopripple.h"
#include "blopstyle.h"
#include "ui_SettingsDialog.h"

#include <QButtonGroup>
#include <QEasingCurve>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QScroller>
#include <QShowEvent>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariantAnimation>

// v3.16.1: Settings overhaul.
//
// Old layout was a single QFormLayout-like dump of fields inside the tab
// designed in Qt Designer. New layout uses a Hero section (current profile
// + "Profil bearbeiten") on top, a search bar, and four collapsible
// BlopSheet-skinned cards (Konto / Erscheinungsbild / Verhalten / Erweitert).
// Wide-mode (>=720px) lays the four cards in a 2-col grid; narrow-mode
// stacks them. Animations are limited to the section expand/collapse so
// the dialog itself stays responsive and there are no Windows-style
// off-screen-pixmap costs (same lesson learnt during Phase A for MorphTray).

namespace {

// Painted chevron — Unicode ▾/▸ often renders as tofu on Android fonts.
class SettingsChevronLabel : public QLabel {
public:
    explicit SettingsChevronLabel(QWidget *parent = nullptr) : QLabel(parent) {
        setFixedSize(18, 18);
        setAttribute(Qt::WA_TranslucentBackground, true);
    }
    void setExpanded(bool expanded) {
        if (m_expanded == expanded)
            return;
        m_expanded = expanded;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(Qt::NoPen);
        QColor c = BlopTheme::textSecondary();
        c.setAlpha(220);
        p.setBrush(c);
        const QPointF cpt(width() / 2.0, height() / 2.0);
        QPolygonF tri;
        if (m_expanded) {
            tri << QPointF(cpt.x() - 5, cpt.y() - 2)
                << QPointF(cpt.x() + 5, cpt.y() - 2)
                << QPointF(cpt.x(), cpt.y() + 4);
        } else {
            tri << QPointF(cpt.x() - 2, cpt.y() - 5)
                << QPointF(cpt.x() - 2, cpt.y() + 5)
                << QPointF(cpt.x() + 4, cpt.y());
        }
        p.drawPolygon(tri);
    }

private:
    bool m_expanded{true};
};

// Collapsible card with title bar, chevron and animated body. Used for the
// four section cards. The card itself adopts BlopStyle::surfaceStyle so it
// reads as part of the unified design language.
class BlopSettingsCard : public QFrame {
public:
    BlopSettingsCard(const QString &title, const QString &subtitle, QWidget *parent)
        : QFrame(parent), m_title(title), m_subtitle(subtitle) {
        setObjectName(QStringLiteral("BlopSettingsCard"));
        setStyleSheet(BlopStyle::surfaceStyle(QStringLiteral("BlopSettingsCard")));

        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(20, 14, 20, 16);
        root->setSpacing(0);

        auto *header = new QWidget(this);
        header->setCursor(Qt::PointingHandCursor);
        auto *hl = new QHBoxLayout(header);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(12);

        m_titleLbl = new QLabel(m_title, header);
        m_titleLbl->setStyleSheet(BlopTheme::themed(QStringLiteral(
            "color: #ECEEFD; %1 background: transparent;")
            .arg(BlopTheme::typeQss(BlopTheme::TextRole::TitleLarge))));
        m_subtitleLbl = new QLabel(m_subtitle, header);
        m_subtitleLbl->setStyleSheet(BlopTheme::themed(QStringLiteral(
            "color: rgba(180, 188, 215, 0.78); %1 background: transparent;")
            .arg(BlopTheme::typeQss(BlopTheme::TextRole::BodySmall))));

        auto *titleColumn = new QVBoxLayout();
        titleColumn->setContentsMargins(0, 0, 0, 0);
        titleColumn->setSpacing(2);
        titleColumn->addWidget(m_titleLbl);
        if (!m_subtitle.isEmpty())
            titleColumn->addWidget(m_subtitleLbl);
        else
            m_subtitleLbl->hide();
        hl->addLayout(titleColumn, 1);

        m_chevron = new SettingsChevronLabel(header);
        hl->addWidget(m_chevron, 0, Qt::AlignVCenter);
        root->addWidget(header);

        m_body = new QWidget(this);
        m_bodyLay = new QVBoxLayout(m_body);
        m_bodyLay->setContentsMargins(0, 12, 0, 0);
        m_bodyLay->setSpacing(10);
        root->addWidget(m_body);

        header->installEventFilter(new HeaderClickFilter(this));
    }

    void addBodyWidget(QWidget *w) { m_bodyLay->addWidget(w); }
    void addBodyLayout(QLayout *l) { m_bodyLay->addLayout(l); }

    QString title() const { return m_title; }
    QString subtitle() const { return m_subtitle; }

    void setExpanded(bool on) {
        if (on == m_expanded) return;
        m_expanded = on;
        // Animate via maxHeight rather than visible toggle so the layout
        // pushes the cards below this one smoothly. Use a QVariantAnimation
        // calling setMaximumHeight; we cache the body's natural height so
        // we don't measure it during the animation.
        const int target = on ? m_body->sizeHint().height() : 0;
        const int current = m_body->maximumHeight() == QWIDGETSIZE_MAX
                                ? m_body->sizeHint().height()
                                : m_body->maximumHeight();
        if (on) m_body->setVisible(true);
        auto *anim = new QVariantAnimation(this);
        anim->setDuration(BlopMotion::kStandard);
        anim->setEasingCurve(BlopMotion::kEaseStandard);
        anim->setStartValue(current);
        anim->setEndValue(target);
        QObject::connect(anim, &QVariantAnimation::valueChanged, this,
                         [this](const QVariant &v) {
                             m_body->setMaximumHeight(v.toInt());
                         });
        QObject::connect(anim, &QVariantAnimation::finished, this, [this, on]() {
            if (on)
                m_body->setMaximumHeight(QWIDGETSIZE_MAX);
            else
                m_body->setVisible(false);
        });
        anim->start(QAbstractAnimation::DeleteWhenStopped);
        m_chevron->setExpanded(on);
    }

    bool expanded() const { return m_expanded; }

private:
    class HeaderClickFilter : public QObject {
    public:
        explicit HeaderClickFilter(BlopSettingsCard *card)
            : QObject(card), m_card(card) {}
    protected:
        bool eventFilter(QObject *watched, QEvent *event) override {
            if (event->type() == QEvent::MouseButtonRelease)
                m_card->setExpanded(!m_card->expanded());
            return QObject::eventFilter(watched, event);
        }
    private:
        BlopSettingsCard *m_card;
    };

    QString m_title;
    QString m_subtitle;
    QLabel *m_titleLbl{nullptr};
    QLabel *m_subtitleLbl{nullptr};
    SettingsChevronLabel *m_chevron{nullptr};
    QWidget *m_body{nullptr};
    QVBoxLayout *m_bodyLay{nullptr};
    bool m_expanded{true};
};

} // namespace

// v3.18.5: Android-safe text prompt. Replaces QInputDialog::getText which
// spawns a top-level QWindow and trips the Qt 6.10 EGL deadlock when
// another EGL surface is contended. The prompt is built as a plain child
// QDialog and routed through BlopModal::execBlocking (same pattern as
// the main Settings entry point), so no native window is allocated.
static QString blopPromptText(QWidget *parent, const QString &title,
                              const QString &label, const QString &initial,
                              bool *ok) {
    QDialog dlg(parent);
    dlg.setWindowTitle(title);
    auto *lay = new QVBoxLayout(&dlg);
    lay->setContentsMargins(20, 18, 20, 16);
    lay->setSpacing(12);
    auto *lbl = new QLabel(label, &dlg);
    lbl->setStyleSheet(BlopTheme::themed(QStringLiteral(
        "color: %1; background: transparent;")
        .arg(BlopTheme::textPrimary().name())));
    lay->addWidget(lbl);
    auto *edit = new QLineEdit(initial, &dlg);
    edit->setStyleSheet(BlopTheme::inputQss());
    edit->selectAll();
    lay->addWidget(edit);
    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch(1);
    auto *cancel = new QPushButton(QObject::tr("Abbrechen"), &dlg);
    cancel->setStyleSheet(BlopTheme::secondaryButtonQss());
    auto *okBtn = new QPushButton(QObject::tr("OK"), &dlg);
    okBtn->setStyleSheet(BlopTheme::primaryButtonQss());
    okBtn->setDefault(true);
    btnRow->addWidget(cancel);
    btnRow->addWidget(okBtn);
    lay->addLayout(btnRow);
    QObject::connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);
    QObject::connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    QObject::connect(edit, &QLineEdit::returnPressed, &dlg, &QDialog::accept);
    int code = BlopModal::execBlocking(parent ? parent->window() : nullptr, &dlg);
    if (ok) *ok = (code == QDialog::Accepted);
    return code == QDialog::Accepted ? edit->text() : QString();
}

// v3.18.5: Android-safe confirm. Replaces QMessageBox::question to avoid
// the same top-level QWindow / EGL deadlock path.
static bool blopConfirm(QWidget *parent, const QString &title,
                        const QString &message) {
    QDialog dlg(parent);
    dlg.setWindowTitle(title);
    auto *lay = new QVBoxLayout(&dlg);
    lay->setContentsMargins(20, 18, 20, 16);
    lay->setSpacing(12);
    auto *lbl = new QLabel(message, &dlg);
    lbl->setWordWrap(true);
    lbl->setStyleSheet(BlopTheme::themed(QStringLiteral(
        "color: %1; background: transparent;")
        .arg(BlopTheme::textPrimary().name())));
    lay->addWidget(lbl);
    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch(1);
    auto *no = new QPushButton(QObject::tr("Abbrechen"), &dlg);
    no->setStyleSheet(BlopTheme::secondaryButtonQss());
    auto *yes = new QPushButton(QObject::tr("Ja"), &dlg);
    yes->setStyleSheet(BlopTheme::primaryButtonQss());
    yes->setDefault(true);
    btnRow->addWidget(no);
    btnRow->addWidget(yes);
    lay->addLayout(btnRow);
    QObject::connect(no, &QPushButton::clicked, &dlg, &QDialog::reject);
    QObject::connect(yes, &QPushButton::clicked, &dlg, &QDialog::accept);
    return BlopModal::execBlocking(parent ? parent->window() : nullptr, &dlg) ==
           QDialog::Accepted;
}

SettingsDialog::SettingsDialog(UiProfileManager *profileMgr, QWidget *parent)
    : QDialog(parent), ui(new Ui::SettingsDialog), m_profileManager(profileMgr) {
    ui->setupUi(this);
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground);
    setObjectName(QStringLiteral("SettingsDialog"));
    setStyleSheet(BlopStyle::surfaceStyle(QStringLiteral("SettingsDialog")));

    // Replace the Designer-generated tab with our overhauled layout. The
    // old QFormLayout dump is replaced by a Hero card + 4 section cards.
    QWidget *tabDesign = ui->tabWidget->widget(0);
    if (tabDesign) {
        qDeleteAll(tabDesign->children());
        if (tabDesign->layout())
            delete tabDesign->layout();
    }

    auto *root = new QVBoxLayout(tabDesign);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ----- Hero strip ---------------------------------------------------
    auto *hero = new QFrame(tabDesign);
    hero->setObjectName(QStringLiteral("SettingsHero"));
    hero->setStyleSheet(QStringLiteral(
        "#SettingsHero {"
        "  background-color: rgba(124, 92, 252, 0.10);"
        "  border-bottom: 1px solid rgba(124, 92, 252, 0.20);"
        "}"));
    auto *heroLay = new QHBoxLayout(hero);
    heroLay->setContentsMargins(24, 18, 24, 18);
    heroLay->setSpacing(14);

    auto *avatar = new QLabel(hero);
    avatar->setFixedSize(56, 56);
    avatar->setStyleSheet(BlopTheme::themed(QStringLiteral(
        "border-radius: 28px; background-color: rgba(124, 92, 252, 0.35);"
        "color: #ECEEFD; font-size: 22px; font-weight: 700;")));
    avatar->setAlignment(Qt::AlignCenter);
    UiProfile currentP = m_profileManager ? m_profileManager->currentProfile() : UiProfile();
    QString initial = currentP.name.left(1).toUpper();
    if (initial.isEmpty()) initial = QStringLiteral("B");
    avatar->setText(initial);
    heroLay->addWidget(avatar);

    auto *heroText = new QVBoxLayout();
    heroText->setContentsMargins(0, 0, 0, 0);
    heroText->setSpacing(2);
    auto *heroName = new QLabel(currentP.name.isEmpty() ? QStringLiteral("Blop") : currentP.name, hero);
    heroName->setStyleSheet(BlopTheme::themed(QStringLiteral(
        "color: #ECEEFD; %1 background: transparent;")
        .arg(BlopTheme::typeQss(BlopTheme::TextRole::HeadlineSmall))));
    auto *heroSub = new QLabel(QStringLiteral("Aktives UI-Profil"), hero);
    heroSub->setStyleSheet(BlopTheme::themed(QStringLiteral(
        "color: rgba(180, 188, 215, 0.85); %1 background: transparent;")
        .arg(BlopTheme::typeQss(BlopTheme::TextRole::LabelLarge))));
    heroText->addWidget(heroName);
    heroText->addWidget(heroSub);
    heroLay->addLayout(heroText, 1);

    auto *heroEditBtn = new QPushButton(QStringLiteral("Profil bearbeiten"), hero);
    heroEditBtn->setCursor(Qt::PointingHandCursor);
    heroEditBtn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background-color: rgba(124, 92, 252, 0.85);"
        "  color: #FFFFFF;"
        "  border: none; border-radius: 10px;"
        "  padding: 9px 14px; font-weight: 600;"
        "}"
        "QPushButton:hover { background-color: rgba(124, 92, 252, 1.0); }"));
    connect(heroEditBtn, &QPushButton::clicked, this, [this]() {
        openEditor(m_profileManager ? m_profileManager->currentProfile().id : QString());
    });
    BlopRipple::attachPressFeedback(heroEditBtn, 0.92);
    heroLay->addWidget(heroEditBtn);

    root->addWidget(hero);

    // ----- Search bar ---------------------------------------------------
    auto *searchRow = new QFrame(tabDesign);
    auto *searchLay = new QHBoxLayout(searchRow);
    searchLay->setContentsMargins(24, 14, 24, 6);
    auto *search = new QLineEdit(searchRow);
    search->setPlaceholderText(QStringLiteral("Einstellungen durchsuchen..."));
    search->setStyleSheet(BlopTheme::themed(QStringLiteral(
        "QLineEdit {"
        "  background: rgba(22, 24, 36, 0.92);"
        "  color: #ECEEFD;"
        "  border: 1px solid rgba(120, 130, 160, 0.32);"
        "  border-radius: 10px;"
        "  padding: 9px 12px; font-size: 13px;"
        "}"
        "QLineEdit:focus { border: 1px solid rgba(124, 92, 252, 0.75); }")));
    searchLay->addWidget(search);
    root->addWidget(searchRow);

    // ----- Scrollable section area --------------------------------------
    auto *scroll = new QScrollArea(tabDesign);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(QStringLiteral("background: transparent;"));
    QScroller::grabGesture(scroll, QScroller::LeftMouseButtonGesture);

    auto *contentWidget = new QWidget();
    contentWidget->setStyleSheet(QStringLiteral("background: transparent;"));
    auto *contentLay = new QVBoxLayout(contentWidget);
    contentLay->setContentsMargins(20, 12, 20, 24);
    contentLay->setSpacing(14);

    scroll->setWidget(contentWidget);
    root->addWidget(scroll, 1);

    // ----- Card: Konto --------------------------------------------------
    auto *cardKonto = new BlopSettingsCard(
        QStringLiteral("Konto"),
        QStringLiteral("Profil verwalten, abmelden"),
        contentWidget);
    {
        auto *btnEdit = new QPushButton(
            QStringLiteral("Aktuelles Profil bearbeiten"), cardKonto);
        btnEdit->setCursor(Qt::PointingHandCursor);
        btnEdit->setStyleSheet(BlopTheme::themed(QStringLiteral(
            "QPushButton { background-color: rgba(40,42,60,0.92); color: #ECEEFD;"
            "  border: 1px solid rgba(120,130,160,0.32); border-radius: 10px;"
            "  padding: 11px 14px; text-align: left; font-weight: 600; }"
            "QPushButton:hover { border-color: rgba(124,92,252,0.65); }")));
        connect(btnEdit, &QPushButton::clicked, this, [this]() {
            openEditor(m_profileManager ? m_profileManager->currentProfile().id
                                        : QString());
        });
        BlopRipple::attachPressFeedback(btnEdit, 0.92);
        cardKonto->addBodyWidget(btnEdit);
    }
    contentLay->addWidget(cardKonto);

    // ----- Card: Darstellung (Light/Dark Mode) --------------------------
    // v3.17.0: new theme switcher backed by BlopTheme. Lets the user pick
    // a light or dark surface palette while the accent (Blop purple by
    // default) stays the same in both modes.
    auto *cardTheme = new BlopSettingsCard(
        QStringLiteral("Darstellung"),
        QStringLiteral("Helles oder dunkles Design"),
        contentWidget);
    {
        auto *lblMode = new QLabel(QStringLiteral("Modus"), cardTheme);
        lblMode->setStyleSheet(BlopTheme::themed(QStringLiteral(
            "color: rgba(200, 208, 235, 0.92); font-size: 12px; font-weight: 600;"
            "background: transparent;")));
        cardTheme->addBodyWidget(lblMode);

        auto *modeRow = new QWidget(cardTheme);
        auto *modeLay = new QHBoxLayout(modeRow);
        modeLay->setContentsMargins(0, 0, 0, 0);
        modeLay->setSpacing(8);

        auto *btnDark = new QPushButton(QStringLiteral("Dunkel"), modeRow);
        auto *btnLight = new QPushButton(QStringLiteral("Hell"), modeRow);
        btnDark->setCheckable(true);
        btnLight->setCheckable(true);
        btnDark->setCursor(Qt::PointingHandCursor);
        btnLight->setCursor(Qt::PointingHandCursor);
        btnDark->setMinimumHeight(40);
        btnLight->setMinimumHeight(40);
        const QString segStyle = BlopTheme::themed(QStringLiteral(
            "QPushButton {"
            "  background: rgba(40,42,60,0.65);"
            "  color: #ECEEFD;"
            "  border: 1px solid rgba(120,130,160,0.32);"
            "  border-radius: 10px;"
            "  padding: 8px 14px;"
            "  font-weight: 600;"
            "}"
            "QPushButton:checked {"
            "  background: rgba(124,92,252,0.85);"
            "  color: #FFFFFF;"
            "  border: 1px solid rgba(124,92,252,1.0);"
            "}"
            "QPushButton:hover:!checked {"
            "  border-color: rgba(124,92,252,0.65);"
            "}"));
        btnDark->setStyleSheet(segStyle);
        btnLight->setStyleSheet(segStyle);
        BlopRipple::attachPressFeedback(btnDark, 0.92);
        BlopRipple::attachPressFeedback(btnLight, 0.92);
        modeLay->addWidget(btnDark, 1);
        modeLay->addWidget(btnLight, 1);
        cardTheme->addBodyWidget(modeRow);

        auto *bgMode = new QButtonGroup(this);
        bgMode->setExclusive(true);
        bgMode->addButton(btnDark, 0);
        bgMode->addButton(btnLight, 1);
        const bool startLight = BlopTheme::instance().isLight();
        btnDark->setChecked(!startLight);
        btnLight->setChecked(startLight);
        connect(bgMode, &QButtonGroup::idClicked, this, [](int id) {
            BlopTheme::instance().setMode(id == 1 ? BlopTheme::Mode::Light
                                                  : BlopTheme::Mode::Dark);
        });

        auto *hint = new QLabel(
            QStringLiteral("Die Akzentfarbe bleibt in beiden Modi erhalten."),
            cardTheme);
        hint->setWordWrap(true);
        hint->setStyleSheet(BlopTheme::themed(QStringLiteral(
            "color: rgba(180, 188, 215, 0.78); font-size: 12px;"
            "background: transparent; padding-top: 6px;")));
        cardTheme->addBodyWidget(hint);

        // v3.17.1/B4: integrated accent picker. The old free-floating
        // "Akzentfarbe" block inside "Erscheinungsbild" only emitted a
        // local signal that the surrounding system didn't persist (it
        // was rebuilt on every Settings open). The new picker drives
        // BlopTheme::setAccent() directly, which persists to
        // QSettings and refreshes UIStyles + cardQss callers via the
        // themeChanged signal -- no per-call-site rewiring needed.
        auto *lblAccentTheme =
            new QLabel(QStringLiteral("Akzentfarbe"), cardTheme);
        lblAccentTheme->setStyleSheet(BlopTheme::themed(QStringLiteral(
            "color: rgba(200, 208, 235, 0.92); font-size: 12px; "
            "font-weight: 600; background: transparent; padding-top: 8px;")));
        cardTheme->addBodyWidget(lblAccentTheme);

        auto *accentRow = new QWidget(cardTheme);
        auto *accentLay = new QHBoxLayout(accentRow);
        accentLay->setContentsMargins(0, 0, 0, 0);
        accentLay->setSpacing(10);
        struct AccentChoice {
            BlopTheme::Accent value;
            QString hex;
            QString tip;
        };
        const QVector<AccentChoice> choices = {
            {BlopTheme::Accent::Purple, QStringLiteral("#7C5CFC"),
             QStringLiteral("Purple")},
            {BlopTheme::Accent::Blue, QStringLiteral("#6BA3F5"),
             QStringLiteral("Blue")},
            {BlopTheme::Accent::Green, QStringLiteral("#34D399"),
             QStringLiteral("Green")},
            {BlopTheme::Accent::Pink, QStringLiteral("#FF6B9D"),
             QStringLiteral("Pink")}};
        const BlopTheme::Accent activeAccent =
            BlopTheme::instance().accent();
        auto *accentGroup = new QButtonGroup(this);
        accentGroup->setExclusive(true);
        for (int i = 0; i < choices.size(); ++i) {
            const AccentChoice &ch = choices[i];
            auto *b = new QPushButton(accentRow);
            b->setCheckable(true);
            b->setCursor(Qt::PointingHandCursor);
            b->setFixedSize(40, 40);
            b->setToolTip(ch.tip);
            // The button background stays the accent color (Brand) so
            // themed() must NOT touch it. We compose with .arg() so
            // BlopTheme::themed runs over the static frame, then we
            // patch the dynamic bg into the result. Simpler: just set
            // it without themed() since the frame uses rgba() tints
            // that look fine in both modes.
            b->setStyleSheet(
                QStringLiteral(
                    "QPushButton { background-color: %1;"
                    "  border-radius: 20px;"
                    "  border: 2px solid rgba(255,255,255,0.14); }"
                    "QPushButton:hover { border: 2px solid rgba(255,255,255,0.5); }"
                    "QPushButton:checked { border: 3px solid #FFFFFF; }")
                    .arg(ch.hex));
            b->setChecked(ch.value == activeAccent);
            accentGroup->addButton(b, static_cast<int>(ch.value));
            BlopRipple::attachPressFeedback(b, 0.88);
            accentLay->addWidget(b);
        }
        accentLay->addStretch();
        cardTheme->addBodyWidget(accentRow);
        connect(accentGroup, &QButtonGroup::idClicked, this, [this](int id) {
            const auto a = static_cast<BlopTheme::Accent>(id);
            BlopTheme::instance().setAccent(a);
            emit accentColorChanged(BlopTheme::accentPrimary());
        });
    }
    contentLay->addWidget(cardTheme);

    // ----- Card: Erscheinungsbild ---------------------------------------
    // v3.17.1/B4: the standalone "Akzentfarbe" row is removed -- the
    // Darstellung card above now owns the accent picker (persistent +
    // BlopTheme-backed). Toolbar mode stays here.
    auto *cardLook = new BlopSettingsCard(
        QStringLiteral("Erscheinungsbild"),
        QStringLiteral("Werkzeugleiste"),
        contentWidget);
    {
        auto *lblTb = new QLabel(QStringLiteral("Werkzeugleiste"), cardLook);
        lblTb->setStyleSheet(BlopTheme::themed(QStringLiteral(
            "color: rgba(200, 208, 235, 0.92); font-size: 12px; font-weight: 600;"
            "background: transparent;")));
        cardLook->addBodyWidget(lblTb);

        auto *rNorm = new QRadioButton(QStringLiteral("Vertikal / Adaptiv"), cardLook);
        rNorm->setObjectName(QStringLiteral("radioVert"));
        auto *rFull = new QRadioButton(QStringLiteral("Radial"), cardLook);
        rFull->setObjectName(QStringLiteral("radioRadial"));
        const QString radioStyle = BlopTheme::themed(QStringLiteral(
            "QRadioButton { color: #ECEEFD; background: transparent; "
            "padding: 4px 0; font-size: 13px; }"));
        rNorm->setStyleSheet(radioStyle);
        rFull->setStyleSheet(radioStyle);
        cardLook->addBodyWidget(rNorm);
        cardLook->addBodyWidget(rFull);

        auto *bgToolbar = new QButtonGroup(this);
        bgToolbar->addButton(rNorm, 0);
        bgToolbar->addButton(rFull, 1);
        connect(bgToolbar, &QButtonGroup::idClicked, this,
                [this](int id) { emit toolbarStyleChanged(id > 0); });
    }
    contentLay->addWidget(cardLook);

    // ----- Card: Verhalten (Profile list) -------------------------------
    auto *cardBehavior = new BlopSettingsCard(
        QStringLiteral("Verhalten"),
        QStringLiteral("UI-Profile / Modi"),
        contentWidget);
    {
        m_profileList = new QListWidget(cardBehavior);
        m_profileList->setStyleSheet(BlopTheme::themed(QStringLiteral(
            "QListWidget {"
            "  background: rgba(22, 24, 36, 0.78);"
            "  border: 1px solid rgba(120, 130, 160, 0.28);"
            "  border-radius: 10px;"
            "  color: #ECEEFD;"
            "  padding: 4px;"
            "}"
            "QListWidget::item {"
            "  padding: 8px 10px;"
            "  border-radius: 6px;"
            "  margin: 2px;"
            "}"
            "QListWidget::item:selected {"
            "  background: rgba(124, 92, 252, 0.55);"
            "}")));
        m_profileList->setFixedHeight(132);
        m_profileList->setContextMenuPolicy(Qt::CustomContextMenu);
        QScroller::grabGesture(m_profileList, QScroller::LeftMouseButtonGesture);
        connect(m_profileList, &QListWidget::customContextMenuRequested, this,
                &SettingsDialog::onProfileContextMenu);
        connect(m_profileList, &QListWidget::itemClicked, this,
                &SettingsDialog::onProfileClicked);
        cardBehavior->addBodyWidget(m_profileList);

        auto *btnNewProfile = new QPushButton(
            QStringLiteral("Neuen Modus erstellen"), cardBehavior);
        btnNewProfile->setCursor(Qt::PointingHandCursor);
        btnNewProfile->setStyleSheet(BlopTheme::themed(QStringLiteral(
            "QPushButton { background-color: rgba(40,42,60,0.92); color: #ECEEFD;"
            "  border: 1px solid rgba(120,130,160,0.32); border-radius: 10px;"
            "  padding: 10px 14px; font-weight: 600; }"
            "QPushButton:hover { border-color: rgba(124,92,252,0.65); }")));
        connect(btnNewProfile, &QPushButton::clicked, this,
                &SettingsDialog::onCreateProfile);
        cardBehavior->addBodyWidget(btnNewProfile);
    }
    contentLay->addWidget(cardBehavior);

    // ----- Card: Erweitert ----------------------------------------------
    auto *cardAdv = new BlopSettingsCard(
        QStringLiteral("Erweitert"),
        QStringLiteral("Version, Informationen"),
        contentWidget);
    {
        auto *info = new QLabel(QStringLiteral("Blop v3.18.10"), cardAdv);
        info->setStyleSheet(BlopTheme::themed(QStringLiteral(
            "color: rgba(180, 188, 215, 0.78); font-size: 12px;"
            "background: transparent; padding: 4px 0;")));
        cardAdv->addBodyWidget(info);
    }
    contentLay->addWidget(cardAdv);
    cardAdv->setExpanded(false);
    contentLay->addStretch();

    // Search: hide cards whose title doesn't match the filter (simple
    // top-level filter; deeper filtering could match labels inside the
    // card body but the four sections + their subtitles cover the most
    // common use case).
    connect(search, &QLineEdit::textChanged, this, [=](const QString &q) {
        const QString needle = q.trimmed().toLower();
        const QList<BlopSettingsCard *> cards = {cardKonto, cardTheme, cardLook,
                                                 cardBehavior, cardAdv};
        for (BlopSettingsCard *c : cards) {
            if (needle.isEmpty()) {
                c->setVisible(true);
                continue;
            }
            const bool hit = c->title().toLower().contains(needle) ||
                             c->subtitle().toLower().contains(needle);
            c->setVisible(hit);
        }
    });

    refreshProfileList();
}

SettingsDialog::~SettingsDialog() { delete ui; }

void SettingsDialog::showEvent(QShowEvent *event) {
    QDialog::showEvent(event);
    if (!parentWidget()) {
        setWindowOpacity(1.0);
        return;
    }
    if (m_dialogIntroDone)
        return;
    m_dialogIntroDone = true;
#ifdef Q_OS_ANDROID
    // v3.18.5: on Android the dialog is embedded into BlopModal (no
    // top-level QWindow), and BlopModal already animates its own card.
    // Running a windowOpacity/pos animation here during the same
    // show_sys() flush competes with the EGL surface lock and was a
    // contributing factor to crash reports when Settings opened.
    return;
#else
    // v3.16.1: unified slide-in (280ms OutCubic + 220ms opacity).
    const QPoint dest = pos();
    setWindowOpacity(0.0);
    auto *opAnim = new QPropertyAnimation(this, "windowOpacity", this);
    opAnim->setDuration(BlopMotion::kStandard);
    opAnim->setStartValue(0.0);
    opAnim->setEndValue(1.0);
    opAnim->setEasingCurve(BlopMotion::kEaseStandard);
    opAnim->start(QAbstractAnimation::DeleteWhenStopped);
    move(dest.x(), dest.y() + 24);
    auto *posAnim = new QPropertyAnimation(this, "pos", this);
    posAnim->setDuration(BlopMotion::kEmphasis);
    posAnim->setStartValue(QPoint(dest.x(), dest.y() + 24));
    posAnim->setEndValue(dest);
    posAnim->setEasingCurve(BlopMotion::kEaseStandard);
    posAnim->start(QAbstractAnimation::DeleteWhenStopped);
#endif
}

void SettingsDialog::refreshProfileList() {
    if (!m_profileList) return;
    m_profileList->clear();
    QString currentId = m_profileManager->currentProfile().id;
    for (const auto &p : m_profileManager->profiles()) {
        QListWidgetItem *item = new QListWidgetItem(p.name);
        item->setData(Qt::UserRole, p.id);
        m_profileList->addItem(item);
        if (p.id == currentId)
            m_profileList->setCurrentItem(item);
    }
}

void SettingsDialog::onProfileClicked(QListWidgetItem *item) {
    if (!item) return;
    QString id = item->data(Qt::UserRole).toString();
    m_profileManager->setCurrentProfile(id);
}

void SettingsDialog::onCreateProfile() {
    bool ok = false;
    QString text = blopPromptText(this, QStringLiteral("Neuer Modus"),
                                  QStringLiteral("Name:"),
                                  QStringLiteral("Mein Modus"), &ok);
    if (ok && !text.isEmpty()) {
        m_profileManager->createProfile(text);
        refreshProfileList();
    }
}

void SettingsDialog::onProfileContextMenu(const QPoint &pos) {
    QListWidgetItem *item = m_profileList->itemAt(pos);
    if (!item) return;
    const QString itemId = item->data(Qt::UserRole).toString();
    const QString itemText = item->text();
    QList<BlopInWindowMenu::Item> items;
    BlopInWindowMenu::Item edit;
    edit.label = QStringLiteral("Bearbeiten");
    edit.handler = [this, itemId]() { openEditor(itemId); };
    items.append(edit);

    BlopInWindowMenu::Item rename;
    rename.label = QStringLiteral("Umbenennen");
    rename.handler = [this, itemId, itemText]() {
        bool ok = false;
        QString text = blopPromptText(
            this, QStringLiteral("Umbenennen"),
            QStringLiteral("Name:"), itemText, &ok);
        if (ok && !text.isEmpty()) {
            UiProfile p = m_profileManager->profileById(itemId);
            p.name = text;
            m_profileManager->updateProfile(p);
            refreshProfileList();
        }
    };
    items.append(rename);

    BlopInWindowMenu::Item del;
    del.label = QStringLiteral("L\u00F6schen");
    del.destructive = true;
    del.handler = [this, itemId]() {
        if (blopConfirm(this, QStringLiteral("L\u00F6schen"),
                        QStringLiteral("Modus wirklich l\u00F6schen?"))) {
            m_profileManager->deleteProfile(itemId);
            refreshProfileList();
        }
    };
    items.append(del);

    BlopInWindowMenu::show(m_profileList, m_profileList->mapToGlobal(pos), items);
}

void SettingsDialog::openEditor(const QString &profileId) {
    m_editId = profileId;
    done(EditProfileCode);
}

void SettingsDialog::setToolbarConfig(bool isRadial, bool) {
    QRadioButton *rVert = this->findChild<QRadioButton *>("radioVert");
    QRadioButton *rFull = this->findChild<QRadioButton *>("radioRadial");

    if (isRadial) {
        if (rFull) rFull->setChecked(true);
    } else if (rVert) {
        rVert->setChecked(true);
    }
}
