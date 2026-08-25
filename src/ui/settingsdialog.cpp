#include "settingsdialog.h"
#include "cloudstoragestore.h"
#include "storageprefs.h"
#include "uiprofilemanager.h"
#include "blop_inwindow_menu.h"
#include "blop_modal.h"
#include "blop_dialogs.h"
#include "blop_theme.h"
#include "blop_scroll.h"
#include "blopripple.h"
#include "blopstyle.h"
#include "uiscale.h"
#include "ui_SettingsDialog.h"

#include <QButtonGroup>
#include <QBoxLayout>
#include <QByteArray>
#include <QDir>
#include <QEasingCurve>
#include <QFileDialog>
#include <QFrame>
#include <QGridLayout>
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
#include <QSettings>
#include <QShowEvent>
#include <QSizePolicy>
#include <QStandardPaths>
#include <QTabBar>
#include <QToolButton>
#include <QUrl>
#include <QUuid>
#include <QVBoxLayout>
#include <QVariantAnimation>
#include <functional>
#include <initializer_list>

#ifndef BLOP_VERSION_STR
#define BLOP_VERSION_STR "3.18.12"
#endif

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

constexpr const char *kRawQssProp = "blopRawQss";
constexpr const char *kTokenQssProp = "blopTokenQss";
constexpr const char *kSurfaceNameProp = "blopSurfaceName";

void applyStoredQss(QWidget *w) {
    if (!w)
        return;
    const QString surface = w->property(kSurfaceNameProp).toString();
    if (!surface.isEmpty()) {
        w->setStyleSheet(BlopStyle::surfaceStyle(surface));
        return;
    }
    const QByteArray token = w->property(kTokenQssProp).toByteArray();
    if (token == "input") {
        w->setStyleSheet(BlopTheme::inputQss());
        return;
    }
    if (token == "primary") {
        w->setStyleSheet(BlopTheme::primaryButtonQss());
        return;
    }
    if (token == "secondary") {
        w->setStyleSheet(BlopTheme::secondaryButtonQss());
        return;
    }
    if (token == "tertiary") {
        w->setStyleSheet(BlopTheme::tertiaryButtonQss());
        return;
    }
    const QVariant raw = w->property(kRawQssProp);
    if (raw.isValid())
        w->setStyleSheet(BlopTheme::themed(raw.toString()));
}

void setThemedQss(QWidget *w, const QString &raw) {
    if (!w)
        return;
    w->setProperty(kRawQssProp, raw);
    w->setStyleSheet(BlopTheme::themed(raw));
}

void setTokenQss(QWidget *w, const char *kind) {
    if (!w)
        return;
    w->setProperty(kTokenQssProp, QByteArray(kind));
    applyStoredQss(w);
}

void setSurfaceQss(QWidget *w, const QString &name) {
    if (!w)
        return;
    w->setObjectName(name);
    w->setProperty(kSurfaceNameProp, name);
    w->setStyleSheet(BlopStyle::surfaceStyle(name));
}

void refreshThemedTree(QWidget *root) {
    if (!root)
        return;
    applyStoredQss(root);
    const auto kids = root->findChildren<QWidget *>();
    for (QWidget *w : kids) {
        applyStoredQss(w);
        w->update();
    }
}

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
        setSurfaceQss(this, QStringLiteral("BlopSettingsCard"));
#ifndef Q_OS_ANDROID
        // Desktop workspace: no accent hairline — it read as a white rim
        // against the dark page (and stacked with SettingsWorkspaceCard).
        if (!UiScale::isAndroidPhoneUi(parent)) {
            const QColor bg = BlopStyle::surfaceBg();
            setStyleSheet(QStringLiteral(
                "#BlopSettingsCard {"
                "  background-color: rgba(%1, %2, %3, %4);"
                "  border: none;"
                "  border-radius: %5px;"
                "}")
                              .arg(bg.red())
                              .arg(bg.green())
                              .arg(bg.blue())
                              .arg(QString::number(bg.alphaF(), 'f', 3))
                              .arg(UiScale::dp(BlopStyle::surfaceRadiusDp())));
        }
#endif
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

        auto *root = new QVBoxLayout(this);
        const bool compact = UiScale::isAndroidPhoneUi(parent);
        const int cm = compact ? UiScale::dp(14) : 24;
        const int cv = compact ? UiScale::dp(12) : 20;
        root->setContentsMargins(cm, cv, cm, cv);
        root->setSpacing(0);

        auto *header = new QWidget(this);
        header->setCursor(Qt::PointingHandCursor);
        auto *hl = new QHBoxLayout(header);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(12);

        m_titleLbl = new QLabel(m_title, header);
        setThemedQss(m_titleLbl, QStringLiteral(
            "color: #E0E0E0; %1 background: transparent;")
            .arg(BlopTheme::typeQss(BlopTheme::TextRole::TitleLarge)));
        m_subtitleLbl = new QLabel(m_subtitle, header);
        setThemedQss(m_subtitleLbl, QStringLiteral(
            "color: rgba(180, 188, 215, 0.78); %1 background: transparent;")
            .arg(BlopTheme::typeQss(BlopTheme::TextRole::BodySmall)));

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

    void refreshTheme() {
        setSurfaceQss(this, QStringLiteral("BlopSettingsCard"));
#ifndef Q_OS_ANDROID
        if (!UiScale::isAndroidPhoneUi(parentWidget())) {
            const QColor bg = BlopStyle::surfaceBg();
            setStyleSheet(QStringLiteral(
                "#BlopSettingsCard {"
                "  background-color: rgba(%1, %2, %3, %4);"
                "  border: none;"
                "  border-radius: %5px;"
                "}")
                              .arg(bg.red())
                              .arg(bg.green())
                              .arg(bg.blue())
                              .arg(QString::number(bg.alphaF(), 'f', 3))
                              .arg(UiScale::dp(BlopStyle::surfaceRadiusDp())));
        }
#endif
        if (m_titleLbl)
            applyStoredQss(m_titleLbl);
        if (m_subtitleLbl)
            applyStoredQss(m_subtitleLbl);
        if (m_chevron)
            m_chevron->update();
    }

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

    // Remove any legacy tabs from the .ui skeleton; the redesigned cards
    // live inside tabDesign. Then hide the tab bar since there is only one panel.
    while (ui->tabWidget->count() > 1)
        ui->tabWidget->removeTab(1);
    ui->tabWidget->tabBar()->hide();

    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    // Opaque fill: nested rounded+translucent surfaces punched black
    // L-corners through the BlopModal card on the software rasterizer.
    setAttribute(Qt::WA_TranslucentBackground, false);
    setThemedQss(this, QStringLiteral(
        "QDialog { background-color: #1A1A24; border: none; border-radius: 0px; }"));
    const bool phoneUi = UiScale::isAndroidPhoneUi(parent);
    const int pagePad = phoneUi ? UiScale::dp(14) : 36;
    const int cardGap = phoneUi ? UiScale::dp(12) : 24;
    if (phoneUi)
        setMinimumSize(0, 0);
    else
        setMinimumSize(400, 360);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

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

    // ----- Hero strip (quiet profile row) --------------------------------
    auto *hero = new QFrame(tabDesign);
    hero->setObjectName(QStringLiteral("SettingsHero"));
    setThemedQss(hero, QStringLiteral(
        "#SettingsHero {"
        "  background-color: rgba(255, 255, 255, 0.03);"
        "  border-bottom: 1px solid rgba(120, 130, 160, 0.18);"
        "}"));
    auto *heroLay = new QHBoxLayout(hero);
    heroLay->setContentsMargins(pagePad, phoneUi ? UiScale::dp(14) : 22,
                                pagePad, phoneUi ? UiScale::dp(14) : 22);
    heroLay->setSpacing(phoneUi ? UiScale::dp(10) : 16);

    auto *avatar = new QLabel(hero);
    avatar->setFixedSize(44, 44);
    setThemedQss(avatar, QStringLiteral(
        "border-radius: 14px; background-color: rgba(124, 92, 252, 0.28);"
        "color: #E0E0E0; font-size: 18px; font-weight: 700;"));
    avatar->setAlignment(Qt::AlignCenter);
    UiProfile currentP = m_profileManager ? m_profileManager->currentProfile() : UiProfile();
    QString initial = currentP.name.left(1).toUpper();
    if (initial.isEmpty()) initial = QStringLiteral("B");
    avatar->setText(initial);
    heroLay->addWidget(avatar);

    const QSettings accountSt(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
    const QString studyUser =
        accountSt.value(QStringLiteral("username")).toString().trimmed();
    const QString studySid =
        accountSt.value(QStringLiteral("session_id")).toString().trimmed();
    const bool studyLoggedIn = !studyUser.isEmpty() && !studySid.isEmpty()
        && studyUser.compare(QLatin1String("Gast"), Qt::CaseInsensitive) != 0
        && studyUser.compare(QLatin1String("Guest"), Qt::CaseInsensitive) != 0;

    auto *heroText = new QVBoxLayout();
    heroText->setContentsMargins(0, 0, 0, 0);
    heroText->setSpacing(2);
    auto *heroName = new QLabel(studyLoggedIn ? studyUser
                                              : (currentP.name.isEmpty()
                                                     ? QStringLiteral("Blop")
                                                     : currentP.name),
                                hero);
    setThemedQss(heroName, QStringLiteral(
        "color: #E0E0E0; %1 background: transparent;")
        .arg(BlopTheme::typeQss(BlopTheme::TextRole::TitleLarge)));
    auto *heroSub = new QLabel(
        studyLoggedIn ? QStringLiteral("Angemeldet bei Study")
                      : QStringLiteral("Nicht angemeldet"),
        hero);
    setThemedQss(heroSub, QStringLiteral(
        "color: rgba(180, 188, 215, 0.70); %1 background: transparent;")
        .arg(BlopTheme::typeQss(BlopTheme::TextRole::LabelLarge)));
    heroText->addWidget(heroName);
    heroText->addWidget(heroSub);
    heroLay->addLayout(heroText, 1);

    auto *heroEditBtn = new QPushButton(QStringLiteral("Bearbeiten"), hero);
    heroEditBtn->setCursor(Qt::PointingHandCursor);
    setTokenQss(heroEditBtn, "secondary");
    connect(heroEditBtn, &QPushButton::clicked, this, [this]() {
        openEditor(m_profileManager ? m_profileManager->currentProfile().id : QString());
    });
    BlopRipple::attachPressFeedback(heroEditBtn, 0.92);
    heroLay->addWidget(heroEditBtn);

    root->addWidget(hero);

    // ----- Search bar ---------------------------------------------------
    auto *searchRow = new QFrame(tabDesign);
    auto *searchLay = new QHBoxLayout(searchRow);
    searchLay->setContentsMargins(pagePad, phoneUi ? UiScale::dp(10) : 18,
                                  pagePad, phoneUi ? UiScale::dp(8) : 12);
    auto *search = new QLineEdit(searchRow);
    search->setPlaceholderText(QStringLiteral("Einstellungen durchsuchen..."));
    setThemedQss(search, QStringLiteral(
        "QLineEdit {"
        "  background: rgba(22, 24, 36, 0.92);"
        "  color: #E0E0E0;"
        "  border: 1px solid rgba(120, 130, 160, 0.28);"
        "  border-radius: 12px;"
        "  padding: 12px 16px; font-size: 14px;"
        "}"
        "QLineEdit:focus { border: 1px solid rgba(124, 92, 252, 0.70); }"));
    if (phoneUi) {
        search->setMinimumWidth(0);
        search->setMaximumWidth(QWIDGETSIZE_MAX);
        search->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    } else {
        search->setMinimumWidth(420);
        search->setMaximumWidth(640);
        search->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    }
    search->setMinimumHeight(phoneUi ? UiScale::dp(40) : 44);
    searchLay->addWidget(search, phoneUi ? 1 : 0);
    if (!phoneUi)
        searchLay->addStretch(1);
    root->addWidget(searchRow);

    // ----- Scrollable section area --------------------------------------
    auto *scroll = new QScrollArea(tabDesign);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(QStringLiteral("background: transparent;"));
    BlopScroll::enableFingerScroll(scroll);

    auto *contentWidget = new QWidget();
    contentWidget->setStyleSheet(QStringLiteral("background: transparent;"));
    contentWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *contentLay = new QVBoxLayout(contentWidget);
    contentLay->setContentsMargins(pagePad, phoneUi ? UiScale::dp(12) : 24,
                                   pagePad, phoneUi ? UiScale::dp(20) : 32);
    contentLay->setSpacing(cardGap);

    scroll->setWidget(contentWidget);
    root->addWidget(scroll, 1);

    auto *cardsHost = new QWidget(contentWidget);
    cardsHost->setObjectName(QStringLiteral("SettingsCardsHost"));
    cardsHost->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    cardsHost->setStyleSheet(QStringLiteral("background: transparent;"));
    auto *hostLay = new QVBoxLayout(cardsHost);
    hostLay->setContentsMargins(0, 0, 0, 0);
    hostLay->setSpacing(cardGap);

    // ----- Card: Konto --------------------------------------------------
    auto *cardKonto = new BlopSettingsCard(
        QStringLiteral("Konto"),
        studyLoggedIn ? QStringLiteral("Profil und Anmeldebildschirm")
                      : QStringLiteral("Zum Anmeldebildschirm"),
        contentWidget);
    {
        auto closeAfterAccountAction = [this, phoneUi]() {
            if (phoneUi)
                accept();
        };

        if (studyLoggedIn) {
            auto *who = new QLabel(
                QStringLiteral("Angemeldet als %1").arg(studyUser), cardKonto);
            who->setWordWrap(true);
            setThemedQss(who, QStringLiteral(
                "color: #E0E0E0; font-size: 14px; font-weight: 600;"
                "background: transparent; padding: 2px 0 8px 0;"));
            cardKonto->addBodyWidget(who);
        } else {
            auto *hint = new QLabel(
                QStringLiteral(
                    "Melde dich bei Study an, um Notizen zu teilen. "
                    "Google öffnet den sicheren System-Login — "
                    "E-Mail und Registrierung laufen in Blop."),
                cardKonto);
            hint->setWordWrap(true);
            setThemedQss(hint, QStringLiteral(
                "color: rgba(180, 188, 215, 0.88); font-size: 13px;"
                "background: transparent; padding: 2px 0 8px 0;"));
            cardKonto->addBodyWidget(hint);
        }

        auto *btnAuthScreen = new QPushButton(
            QStringLiteral("Zum Anmeldebildschirm"), cardKonto);
        btnAuthScreen->setCursor(Qt::PointingHandCursor);
        btnAuthScreen->setMinimumHeight(phoneUi ? UiScale::dp(46) : 42);
        setTokenQss(btnAuthScreen, studyLoggedIn ? "secondary" : "primary");
        connect(btnAuthScreen, &QPushButton::clicked, this,
                [this, studyLoggedIn, closeAfterAccountAction]() {
                  if (studyLoggedIn)
                    emit logoutRequested();
                  emit studyLoginRequested();
                  closeAfterAccountAction();
                });
        BlopRipple::attachPressFeedback(btnAuthScreen, 0.92);
        cardKonto->addBodyWidget(btnAuthScreen);

        if (!studyLoggedIn) {
            auto *btnGoogle = new QPushButton(QStringLiteral("Mit Google anmelden"),
                                              cardKonto);
            btnGoogle->setCursor(Qt::PointingHandCursor);
            btnGoogle->setMinimumHeight(phoneUi ? UiScale::dp(46) : 42);
            setThemedQss(btnGoogle, QStringLiteral(
                "QPushButton { background-color: #4285F4; color: white;"
                "  border: none; border-radius: 10px;"
                "  padding: 11px 14px; text-align: center; font-weight: 700; }"
                "QPushButton:hover { background-color: #3367D6; }"));
            connect(btnGoogle, &QPushButton::clicked, this,
                    [this, closeAfterAccountAction]() {
                      emit googleLoginRequested();
                      closeAfterAccountAction();
                    });
            BlopRipple::attachPressFeedback(btnGoogle, 0.92);
            cardKonto->addBodyWidget(btnGoogle);
        }

        auto *btnEdit = new QPushButton(
            QStringLiteral("Aktuelles Profil bearbeiten"), cardKonto);
        btnEdit->setCursor(Qt::PointingHandCursor);
        setThemedQss(btnEdit, QStringLiteral(
            "QPushButton { background-color: #252526; color: #E0E0E0;"
            "  border: 1px solid rgba(120,130,160,0.32); border-radius: 10px;"
            "  padding: 11px 14px; text-align: left; font-weight: 600; }"
            "QPushButton:hover { border-color: rgba(124,92,252,0.65); }"));
        connect(btnEdit, &QPushButton::clicked, this, [this]() {
            openEditor(m_profileManager ? m_profileManager->currentProfile().id
                                        : QString());
        });
        BlopRipple::attachPressFeedback(btnEdit, 0.92);
        cardKonto->addBodyWidget(btnEdit);

        if (studyLoggedIn) {
            auto *btnLogout = new QPushButton(
                QStringLiteral("Abmelden"), cardKonto);
            btnLogout->setCursor(Qt::PointingHandCursor);
            setThemedQss(btnLogout, QStringLiteral(
                "QPushButton { background-color: rgba(180,40,40,0.18); color: #FF6B6B;"
                "  border: 1px solid rgba(200,60,60,0.45); border-radius: 10px;"
                "  padding: 11px 14px; text-align: left; font-weight: 600; }"
                "QPushButton:hover { background-color: rgba(200,50,50,0.32);"
                "  border-color: rgba(220,80,80,0.75); }"));
            connect(btnLogout, &QPushButton::clicked, this, [this]() {
                emit logoutRequested();
                accept();
            });
            BlopRipple::attachPressFeedback(btnLogout, 0.92);
            cardKonto->addBodyWidget(btnLogout);
        }
    }

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
        setThemedQss(lblMode, QStringLiteral(
            "color: rgba(200, 208, 235, 0.92); font-size: 12px; font-weight: 600;"
            "background: transparent;"));
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
        const QString segStyle = QStringLiteral(
            "QPushButton {"
            "  background: #252526;"
            "  color: #E0E0E0;"
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
            "}");
        setThemedQss(btnDark, segStyle);
        setThemedQss(btnLight, segStyle);
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
        setThemedQss(hint, QStringLiteral(
            "color: rgba(180, 188, 215, 0.78); font-size: 12px;"
            "background: transparent; padding-top: 6px;"));
        cardTheme->addBodyWidget(hint);

        // v3.17.1/B4: integrated accent picker. The old free-floating
        // "Akzentfarbe" block inside "Erscheinungsbild" only emitted a
        // local signal that the surrounding system didn't persist (it
        // was rebuilt on every Settings open). The new picker drives
        // BlopTheme::setAccent() directly, which persists to
        // QSettings and refreshes UIStyles + cardQss callers via the
        // themeChanged signal. SettingsDialog::refreshTheme() re-skins
        // this dialog live so Hell/Dunkel does not leave dark glass islands.
        auto *lblAccentTheme =
            new QLabel(QStringLiteral("Akzentfarbe"), cardTheme);
        setThemedQss(lblAccentTheme, QStringLiteral(
            "color: rgba(200, 208, 235, 0.92); font-size: 12px; "
            "font-weight: 600; background: transparent; padding-top: 8px;"));
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

        auto *btnBurger = new QPushButton(
            QStringLiteral("Burger-Menü auch auf Tablet/Laptop"), cardTheme);
        btnBurger->setCheckable(true);
        btnBurger->setCursor(Qt::PointingHandCursor);
        btnBurger->setMinimumHeight(40);
        btnBurger->setChecked(UiScale::forceBurgerMenu());
        setThemedQss(btnBurger, QStringLiteral(
            "QPushButton { background: #252526; color: #E0E0E0;"
            "  border: 1px solid rgba(120,130,160,0.32); border-radius: 10px;"
            "  padding: 10px 14px; text-align: left; font-weight: 600; }"
            "QPushButton:checked { background: rgba(91,157,255,0.28);"
            "  border-color: #5B9DFF; }"));
        connect(btnBurger, &QPushButton::toggled, this, [this](bool on) {
            UiScale::setForceBurgerMenu(on);
            emit uiLayoutPrefsChanged();
        });
        BlopRipple::attachPressFeedback(btnBurger, 0.96);
        cardTheme->addBodyWidget(btnBurger);
        auto *burgerHint = new QLabel(
            QStringLiteral(
                "Auf dem Handy ist das Burger-Menü immer an und die "
                "Seitenleiste ausgeblendet. Auf Tablet oder Laptop kannst du "
                "es hier zusätzlich einschalten."),
            cardTheme);
        burgerHint->setWordWrap(true);
        setThemedQss(burgerHint, QStringLiteral(
            "color: rgba(180, 188, 210, 0.78); font-size: 11px;"
            "background: transparent; padding: 2px 0 4px 0;"));
        cardTheme->addBodyWidget(burgerHint);
    }

    // ----- Card: Erscheinungsbild ---------------------------------------
    // v3.17.1/B4: the standalone "Akzentfarbe" row is removed -- the
    // Darstellung card above now owns the accent picker (persistent +
    // BlopTheme-backed). Toolbar mode stays here.
    // Desktop Drawboard: Favorites rail is locked; Radial stays Android-only.
    auto *cardLook = new BlopSettingsCard(
        QStringLiteral("Erscheinungsbild"),
        QStringLiteral("Werkzeugleiste"),
        contentWidget);
    {
        auto *rNorm = new QRadioButton(
#ifdef Q_OS_ANDROID
            QStringLiteral("Vertikal / Adaptiv"),
#else
            QStringLiteral("Favorites-Leiste (Drawboard)"),
#endif
            cardLook);
        rNorm->setObjectName(QStringLiteral("radioVert"));
        auto *rFull = new QRadioButton(QStringLiteral("Radial"), cardLook);
        rFull->setObjectName(QStringLiteral("radioRadial"));
        const QString radioStyle = QStringLiteral(
            "QRadioButton { color: #E0E0E0; background: transparent; "
            "padding: 4px 0; font-size: 13px; }");
        setThemedQss(rNorm, radioStyle);
        setThemedQss(rFull, radioStyle);
        cardLook->addBodyWidget(rNorm);
#ifdef Q_OS_ANDROID
        cardLook->addBodyWidget(rFull);
#else
        rFull->hide();
        rFull->setEnabled(false);
        auto *hint = new QLabel(
            QStringLiteral("Radial-Toolbar und FAB bleiben auf Desktop "
                           "deaktiviert. Stift-Presets erscheinen unten rechts "
                           "bei Stift/Bleistift/Textmarker."),
            cardLook);
        hint->setWordWrap(true);
        setThemedQss(hint, QStringLiteral(
            "color: rgba(180, 188, 210, 0.78); font-size: 11px;"
            "background: transparent; padding: 2px 0 4px 0;"));
        cardLook->addBodyWidget(hint);
#endif

        auto *bgToolbar = new QButtonGroup(this);
        bgToolbar->addButton(rNorm, 0);
        bgToolbar->addButton(rFull, 1);
        if (m_profileManager &&
            m_profileManager->currentProfile().toolbarStyle == 1)
            rFull->setChecked(true);
        else
            rNorm->setChecked(true);
        connect(bgToolbar, &QButtonGroup::idClicked, this,
                [this](int id) {
                    auto profile = m_profileManager->currentProfile();
                    profile.toolbarStyle = (id > 0) ? 1 : 0;
                    m_profileManager->updateProfile(profile, true);
                    emit toolbarStyleChanged(id > 0);
                });
    }

    // ----- Card: Verhalten (Profile list) -------------------------------
    auto *cardBehavior = new BlopSettingsCard(
        QStringLiteral("Verhalten"),
        QStringLiteral("UI-Profile / Modi"),
        contentWidget);
    {
        m_profileList = new QListWidget(cardBehavior);
        setThemedQss(m_profileList, QStringLiteral(
            "QListWidget {"
            "  background: rgba(22, 24, 36, 0.78);"
            "  border: 1px solid rgba(120, 130, 160, 0.28);"
            "  border-radius: 10px;"
            "  color: #E0E0E0;"
            "  padding: 4px;"
            "}"
            "QListWidget::item {"
            "  padding: 8px 10px;"
            "  border-radius: 6px;"
            "  margin: 2px;"
            "}"
            "QListWidget::item:selected {"
            "  background: rgba(124, 92, 252, 0.55);"
            "}"));
        m_profileList->setMinimumHeight(132);
        m_profileList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        m_profileList->setContextMenuPolicy(Qt::CustomContextMenu);
        BlopScroll::enableFingerScroll(m_profileList);
        connect(m_profileList, &QListWidget::customContextMenuRequested, this,
                &SettingsDialog::onProfileContextMenu);
        connect(m_profileList, &QListWidget::itemClicked, this,
                &SettingsDialog::onProfileClicked);
        cardBehavior->addBodyWidget(m_profileList);

        auto *btnNewProfile = new QPushButton(
            QStringLiteral("Neuen Modus erstellen"), cardBehavior);
        btnNewProfile->setCursor(Qt::PointingHandCursor);
        setThemedQss(btnNewProfile, QStringLiteral(
            "QPushButton { background-color: #252526; color: #E0E0E0;"
            "  border: 1px solid rgba(120,130,160,0.32); border-radius: 10px;"
            "  padding: 10px 14px; font-weight: 600; }"
            "QPushButton:hover { border-color: rgba(124,92,252,0.65); }"));
        connect(btnNewProfile, &QPushButton::clicked, this,
                &SettingsDialog::onCreateProfile);
        cardBehavior->addBodyWidget(btnNewProfile);
    }

    // ----- Card: Speicher (local / cloud / both) -----------------------
    // Notes stay on the filesystem. Supabase is never the note store.
    auto *cardStorage = new BlopSettingsCard(
        QStringLiteral("Speicher"),
        QStringLiteral("Notizen lokal — Clouds öffnen in Blop, nicht in Chrome"),
        contentWidget);
    {
        StoragePrefs::ensureLocalLibraryRoot();

        auto requestCloud = [this, phoneUi](CloudStorageEntry e) {
            emit cloudExplorerRequested(e.id, e.type, e.name, e.webUrl);
            if (phoneUi)
                accept();
        };

        auto *hint = new QLabel(StoragePrefs::modeHint(StoragePrefs::mode()),
                                cardStorage);
        hint->setObjectName(QStringLiteral("StorageModeHint"));
        hint->setWordWrap(true);
        setThemedQss(hint, QStringLiteral(
            "color: rgba(180, 188, 215, 0.78); font-size: 12px;"
            "background: transparent; padding: 2px 0 8px 0;"));
        cardStorage->addBodyWidget(hint);

        auto *lblMode = new QLabel(QStringLiteral("Speicherort für Notizen"),
                                   cardStorage);
        setThemedQss(lblMode, QStringLiteral(
            "color: rgba(200, 208, 235, 0.92); font-size: 12px; font-weight: 600;"
            "background: transparent;"));
        cardStorage->addBodyWidget(lblMode);

        auto *modeRow = new QWidget(cardStorage);
        auto *modeLay = new QHBoxLayout(modeRow);
        modeLay->setContentsMargins(0, 0, 0, 0);
        modeLay->setSpacing(8);

        const QString segStyle = QStringLiteral(
            "QPushButton {"
            "  background: #252526;"
            "  color: #E0E0E0;"
            "  border: 1px solid rgba(120,130,160,0.32);"
            "  border-radius: 10px;"
            "  padding: 8px 10px;"
            "  font-weight: 600;"
            "}"
            "QPushButton:checked {"
            "  background: rgba(124,92,252,0.85);"
            "  color: #FFFFFF;"
            "  border: 1px solid rgba(124,92,252,1.0);"
            "}"
            "QPushButton:hover:!checked {"
            "  border-color: rgba(124,92,252,0.65);"
            "}");

        auto *btnLocal = new QPushButton(QStringLiteral("Nur lokal"), modeRow);
        auto *btnCloud = new QPushButton(QStringLiteral("Nur Cloud"), modeRow);
        auto *btnBoth = new QPushButton(QStringLiteral("Lokal + Cloud"), modeRow);
        for (QPushButton *b : {btnLocal, btnCloud, btnBoth}) {
            b->setCheckable(true);
            b->setCursor(Qt::PointingHandCursor);
            b->setMinimumHeight(40);
            setThemedQss(b, segStyle);
            BlopRipple::attachPressFeedback(b, 0.92);
            modeLay->addWidget(b, 1);
        }
        const auto curMode = StoragePrefs::mode();
        btnLocal->setChecked(curMode == StoragePrefs::Mode::LocalOnly);
        btnCloud->setChecked(curMode == StoragePrefs::Mode::CloudOnly);
        btnBoth->setChecked(curMode == StoragePrefs::Mode::LocalAndCloud);
        cardStorage->addBodyWidget(modeRow);

        auto *localPathLbl = new QLabel(
            QStringLiteral("Lokal: %1").arg(StoragePrefs::ensureLocalLibraryRoot()),
            cardStorage);
        localPathLbl->setWordWrap(true);
        setThemedQss(localPathLbl, QStringLiteral(
            "color: rgba(160, 168, 195, 0.85); font-size: 11px;"
            "background: transparent; padding: 4px 0;"));
        cardStorage->addBodyWidget(localPathLbl);

        auto *arch = new QLabel(StoragePrefs::architectureHint(), cardStorage);
        arch->setWordWrap(true);
        setThemedQss(arch, QStringLiteral(
            "color: rgba(160, 168, 195, 0.90); font-size: 11px;"
            "background: transparent; padding: 2px 0 8px 0;"));
        cardStorage->addBodyWidget(arch);

        auto *btnConnectDrive = new QPushButton(cardStorage);
        btnConnectDrive->setCursor(Qt::PointingHandCursor);
        btnConnectDrive->setMinimumHeight(46);
        const bool driveLinked = StoragePrefs::isGoogleDriveLinked();
        btnConnectDrive->setText(
            driveLinked ? QStringLiteral("Google Drive öffnen")
                        : QStringLiteral("Bei Google Drive anmelden"));
        setTokenQss(btnConnectDrive, "primary");
        BlopRipple::attachPressFeedback(btnConnectDrive, 0.92);

        auto *btnConnectDriveManual = new QPushButton(
            QStringLiteral("Lokaler Ordner…"), cardStorage);
        btnConnectDriveManual->setCursor(Qt::PointingHandCursor);
        setTokenQss(btnConnectDriveManual, "secondary");
        BlopRipple::attachPressFeedback(btnConnectDriveManual, 0.92);

        auto *driveBtnRow = new QWidget(cardStorage);
        auto *driveBtnLay = new QBoxLayout(
            phoneUi ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight,
            driveBtnRow);
        driveBtnLay->setContentsMargins(0, 0, 0, 0);
        driveBtnLay->setSpacing(phoneUi ? UiScale::dp(8) : 12);
        driveBtnLay->addWidget(btnConnectDrive, 2);
        driveBtnLay->addWidget(btnConnectDriveManual, 1);
        cardStorage->addBodyWidget(driveBtnRow);

        auto *cloudHeader = new QLabel(QStringLiteral("Weitere Cloud-Anbieter"),
                                       cardStorage);
        setThemedQss(cloudHeader, QStringLiteral(
            "color: rgba(200, 208, 235, 0.92); font-size: 12px; font-weight: 600;"
            "background: transparent; padding-top: 8px;"));
        cardStorage->addBodyWidget(cloudHeader);

        auto *cloudList = new QWidget(cardStorage);
        auto *cloudLay = new QGridLayout(cloudList);
        cloudLay->setContentsMargins(0, 0, 0, 0);
        cloudLay->setHorizontalSpacing(phoneUi ? 0 : 16);
        cloudLay->setVerticalSpacing(phoneUi ? UiScale::dp(10) : 8);
        cloudLay->setColumnStretch(0, 1);
        cloudLay->setColumnStretch(1, 0);
        int cloudIdx = 0;

        // Helpers used by every cloud-provider row and the Google Drive hero button.
        auto refreshCloudRow = [](const QString &providerId,
                                  const QString &displayName, QLabel *name,
                                  QPushButton *autoBtn, QPushButton *manualBtn,
                                  QPushButton *primaryBtn) {
            const bool linked = StoragePrefs::isProviderLinked(providerId);
            bool webOk = false;
            QVector<CloudStorageEntry> rows = CloudStorageStore::load();
            if (CloudStorageEntry *cur =
                    CloudStorageStore::findMutable(rows, providerId))
                webOk = cur->webConnected;
            name->setText(QStringLiteral("%1%2").arg(
                displayName,
                (linked || webOk) ? QStringLiteral(" · verbunden")
                                  : QStringLiteral(" · in Blop anmelden")));
            autoBtn->setText(QStringLiteral("In Blop öffnen"));
            if (manualBtn)
                manualBtn->setVisible(true);
            if (manualBtn)
                manualBtn->setText(QStringLiteral("Lokaler Ordner…"));
            primaryBtn->setEnabled(linked);
            primaryBtn->setText(
                (StoragePrefs::primaryCloudId() == providerId)
                    ? QStringLiteral("Primär")
                    : QStringLiteral("Als Primär"));
        };

        auto connectCloudProvider =
            [this, btnLocal, btnCloud, btnBoth, hint](
                const QString &providerId, const QString &displayName,
                bool forceManual) -> QString {
            QString folder;
            if (!forceManual) {
                folder =
                    StoragePrefs::bestSuggestedRootForProvider(providerId);
            }
            if (folder.isEmpty()) {
#ifdef Q_OS_ANDROID
                // QFileDialog is a top-level QWindow on Android and aborts via
                // the Qt 6.10 EGL deadlock protector. Notes stay on-device;
                // Drive/Nextcloud are opened through the in-app/browser explorer.
                BlopDialogs::notify(
                    this, displayName,
                    QStringLiteral(
                        "Auf dem Handy speichert Blop Notizen lokal auf dem "
                        "Gerät.\n\nGoogle Drive und andere Clouds öffnest du "
                        "über „Anmelden“ / „Öffnen“ — nicht über einen "
                        "Android-Dateiordner."));
                return QString();
#else
                QString start =
                    StoragePrefs::bestSuggestedRootForProvider(providerId);
                if (start.isEmpty())
                    start = QStandardPaths::writableLocation(
                        QStandardPaths::HomeLocation);
                QVector<CloudStorageEntry> entries =
                    CloudStorageStore::load();
                if (CloudStorageEntry *cur =
                        CloudStorageStore::findMutable(entries, providerId)) {
                    if (!cur->path.isEmpty() &&
                        !StoragePrefs::isNonFilesystemPath(cur->path))
                        start = cur->path;
                }
                folder = QFileDialog::getExistingDirectory(
                    this,
                    QStringLiteral("%1 — Sync-Ordner wählen")
                        .arg(displayName),
                    start);
#endif
            }
            if (folder.isEmpty())
                return QString();
            if (!StoragePrefs::connectProviderForNotes(providerId, folder))
                return QString();
            const auto m = StoragePrefs::mode();
            btnLocal->setChecked(m == StoragePrefs::Mode::LocalOnly);
            btnCloud->setChecked(m == StoragePrefs::Mode::CloudOnly);
            btnBoth->setChecked(m == StoragePrefs::Mode::LocalAndCloud);
            hint->setText(StoragePrefs::modeHint(m));
            return folder;
        };

        auto syncPrimaryLabels = [cloudList](QPushButton *activeBtn) {
            const auto buttons = cloudList->findChildren<QPushButton *>();
            for (QPushButton *b : buttons) {
                if (b->text() == QStringLiteral("Primär") ||
                    b->text() == QStringLiteral("Als Primär"))
                    b->setText(QStringLiteral("Als Primär"));
            }
            if (activeBtn)
                activeBtn->setText(QStringLiteral("Primär"));
        };

        // Keep references to the Google Drive row so the big "Connect Drive"
        // button can update the list in-place.
        QLabel *driveListName{nullptr};
        QPushButton *driveListPrimaryBtn{nullptr};
        QPushButton *driveListAutoBtn{nullptr};
        QPushButton *driveListManualBtn{nullptr};

        const QString primaryId = StoragePrefs::primaryCloudId();
        for (const CloudStorageEntry &e : CloudStorageStore::load()) {
            const bool isDrive =
                e.id.compare(QLatin1String("googledrive"), Qt::CaseInsensitive) == 0 ||
                e.type.compare(QLatin1String("googledrive"), Qt::CaseInsensitive) == 0 ||
                e.name.compare(QLatin1String("Google Drive"), Qt::CaseInsensitive) == 0;
            if (isDrive)
                continue;
            auto *row = new QWidget(cloudList);
            row->setObjectName(QStringLiteral("CloudProviderRow"));
            row->setStyleSheet(QStringLiteral(
                "QWidget#CloudProviderRow {"
                "  background: rgba(255,255,255,0.04);"
                "  border-radius: 12px;"
                "}"));
            auto *outer = new QVBoxLayout(row);
            outer->setContentsMargins(phoneUi ? 0 : 12, phoneUi ? 0 : 10,
                                      phoneUi ? 0 : 12, phoneUi ? 0 : 10);
            outer->setSpacing(phoneUi ? UiScale::dp(6) : 8);
            auto *hl = new QHBoxLayout();
            hl->setContentsMargins(0, 0, 0, 0);
            hl->setSpacing(8);
            const bool linked = !e.path.isEmpty() && QDir(e.path).exists();
            auto *name = new QLabel(row);
            name->setObjectName(QStringLiteral("CloudProviderLabel"));
            name->setText(QStringLiteral("%1%2").arg(
                e.name, linked ? QStringLiteral(" · verknüpft")
                               : QStringLiteral(" · nicht verknüpft")));
            setThemedQss(name, QStringLiteral(
                "color: #E0E0E0; font-size: 13px; font-weight: 600;"
                "background: transparent;"));

            auto *btnPrimary = new QPushButton(
                (primaryId == e.id) ? QStringLiteral("Primär")
                                    : QStringLiteral("Als Primär"),
                row);
            btnPrimary->setEnabled(linked);
            btnPrimary->setCursor(Qt::PointingHandCursor);
            setTokenQss(btnPrimary, "secondary");
            const QString id = e.id;
            const QString displayName = e.name;
            QObject::connect(btnPrimary, &QPushButton::clicked, this,
                             [this, id, cloudList]() {
                               StoragePrefs::setPrimaryCloudId(id);
                               // Refresh primary button labels in this card.
                               const auto buttons =
                                   cloudList->findChildren<QPushButton *>();
                               for (QPushButton *b : buttons) {
                                 if (b->text() == QStringLiteral("Primär") ||
                                     b->text() == QStringLiteral("Als Primär"))
                                   b->setText(QStringLiteral("Als Primär"));
                               }
                               if (auto *senderBtn =
                                       qobject_cast<QPushButton *>(sender()))
                                 senderBtn->setText(QStringLiteral("Primär"));
                               emit storagePrefsChanged();
                             });
            hl->addWidget(btnPrimary);

            auto *btnAuto = new QPushButton(row);
            btnAuto->setCursor(Qt::PointingHandCursor);
            setTokenQss(btnAuto, "primary");

            auto *btnManual = new QPushButton(QStringLiteral("Manuell…"), row);
            btnManual->setCursor(Qt::PointingHandCursor);
            setTokenQss(btnManual, "secondary");

            refreshCloudRow(id, displayName, name, btnAuto, btnManual,
                            btnPrimary);

            if (id == QLatin1String("googledrive")) {
                driveListName = name;
                driveListPrimaryBtn = btnPrimary;
                driveListAutoBtn = btnAuto;
                driveListManualBtn = btnManual;
            }

            QObject::connect(btnAuto, &QPushButton::clicked, this,
                             [this, id, displayName, name, btnAuto, btnManual,
                              btnPrimary, refreshCloudRow, requestCloud,
                              syncPrimaryLabels]() {
                                 QVector<CloudStorageEntry> entries =
                                     CloudStorageStore::load();
                                 CloudStorageEntry e;
                                 if (CloudStorageEntry *found =
                                         CloudStorageStore::findMutable(
                                             entries, id))
                                   e = *found;
                                 else {
                                   e.id = id;
                                   e.name = displayName;
                                   e.type = id;
                                 }
                                 requestCloud(e);
                                 refreshCloudRow(id, displayName, name, btnAuto,
                                                 btnManual, btnPrimary);
                                 syncPrimaryLabels(btnPrimary);
                                 emit storagePrefsChanged();
                             });
            QObject::connect(btnManual, &QPushButton::clicked, this,
                             [this, id, displayName, name, btnAuto, btnManual,
                              btnPrimary, connectCloudProvider, refreshCloudRow,
                              syncPrimaryLabels]() {
                                 const QString folder = connectCloudProvider(
                                     id, displayName, /*forceManual=*/true);
                                 if (folder.isEmpty())
                                     return;
                                 refreshCloudRow(id, displayName, name, btnAuto,
                                                 btnManual, btnPrimary);
                                 syncPrimaryLabels(btnPrimary);
                                 emit storagePrefsChanged();
                             });

            outer->addWidget(name);
            outer->addWidget(btnAuto);
            hl->addWidget(btnPrimary, 1);
            hl->addWidget(btnManual, 1);
            outer->addLayout(hl);
            cloudLay->addWidget(row, cloudIdx, 0);
            ++cloudIdx;
        }
        cardStorage->addBodyWidget(cloudList);

        auto applyMode = [this, btnLocal, btnCloud, btnBoth, hint](StoragePrefs::Mode m) {
            btnLocal->setChecked(m == StoragePrefs::Mode::LocalOnly);
            btnCloud->setChecked(m == StoragePrefs::Mode::CloudOnly);
            btnBoth->setChecked(m == StoragePrefs::Mode::LocalAndCloud);
            StoragePrefs::setMode(m);
            QString text = StoragePrefs::modeHint(m);
            if (m != StoragePrefs::Mode::LocalOnly &&
                StoragePrefs::primaryLinkedCloudPath().isEmpty()) {
                text += QStringLiteral(
                    "\nTipp: Verknüpfe unten Google Drive, Nextcloud oder "
                    "einen anderen Sync-Ordner.");
            }
            hint->setText(text);
            emit storagePrefsChanged();
        };
        QObject::connect(btnLocal, &QPushButton::clicked, this,
                         [applyMode]() { applyMode(StoragePrefs::Mode::LocalOnly); });
        QObject::connect(btnCloud, &QPushButton::clicked, this,
                         [applyMode]() { applyMode(StoragePrefs::Mode::CloudOnly); });
        QObject::connect(btnBoth, &QPushButton::clicked, this,
                         [applyMode]() { applyMode(StoragePrefs::Mode::LocalAndCloud); });

        // Hero Google Drive button: auto-detect the sync root, BlopNotizen is
        // created automatically. Falls back to a folder picker if nothing is found.
        QObject::connect(
            btnConnectDrive, &QPushButton::clicked, this,
            [this, btnConnectDrive, driveListName, driveListAutoBtn,
             driveListManualBtn, driveListPrimaryBtn, refreshCloudRow,
             requestCloud, syncPrimaryLabels]() {
                QVector<CloudStorageEntry> entries = CloudStorageStore::load();
                CloudStorageEntry e;
                if (CloudStorageEntry *found = CloudStorageStore::findMutable(
                        entries, QStringLiteral("googledrive")))
                  e = *found;
                else {
                  e.id = QStringLiteral("googledrive");
                  e.type = e.id;
                  e.name = QStringLiteral("Google Drive");
                }
                requestCloud(e);
                btnConnectDrive->setText(QStringLiteral("Google Drive öffnen"));
                if (driveListName)
                    refreshCloudRow(QStringLiteral("googledrive"),
                                    QStringLiteral("Google Drive"),
                                    driveListName, driveListAutoBtn,
                                    driveListManualBtn, driveListPrimaryBtn);
                if (driveListPrimaryBtn)
                    syncPrimaryLabels(driveListPrimaryBtn);
                emit storagePrefsChanged();
            });

        // Secondary manual selection for Google Drive.
        QObject::connect(
            btnConnectDriveManual, &QPushButton::clicked, this,
            [this, btnConnectDrive, driveListName, driveListAutoBtn,
             driveListManualBtn, driveListPrimaryBtn, connectCloudProvider,
             refreshCloudRow, syncPrimaryLabels]() {
                const QString folder = connectCloudProvider(
                    QStringLiteral("googledrive"),
                    QStringLiteral("Google Drive"), /*forceManual=*/true);
                if (folder.isEmpty())
                    return;
                btnConnectDrive->setText(QStringLiteral(
                    "Google Drive verbunden · Ordner ändern…"));
                if (driveListName)
                    refreshCloudRow(QStringLiteral("googledrive"),
                                    QStringLiteral("Google Drive"),
                                    driveListName, driveListAutoBtn,
                                    driveListManualBtn, driveListPrimaryBtn);
                if (driveListPrimaryBtn)
                    syncPrimaryLabels(driveListPrimaryBtn);
                emit storagePrefsChanged();
            });

        auto *customHdr = new QLabel(QStringLiteral("Eigene Cloud einbetten"),
                                     cardStorage);
        setThemedQss(customHdr, QStringLiteral(
            "color: rgba(200, 208, 235, 0.92); font-size: 12px; font-weight: 600;"
            "background: transparent; padding-top: 10px;"));
        cardStorage->addBodyWidget(customHdr);
        auto *customHint = new QLabel(
            QStringLiteral(
                "Nextcloud, Owncloud oder eine andere Web-Adresse — "
                "wird in Blop geöffnet, wie Study."),
            cardStorage);
        customHint->setWordWrap(true);
        setThemedQss(customHint, QStringLiteral(
            "color: rgba(160, 168, 195, 0.90); font-size: 12px;"
            "background: transparent;"));
        cardStorage->addBodyWidget(customHint);
        auto *customUrl = new QLineEdit(cardStorage);
        customUrl->setPlaceholderText(QStringLiteral("https://cloud.example.com"));
        customUrl->setMinimumHeight(phoneUi ? UiScale::dp(42) : 40);
        setTokenQss(customUrl, "input");
        cardStorage->addBodyWidget(customUrl);
        auto *customName = new QLineEdit(cardStorage);
        customName->setPlaceholderText(QStringLiteral("Name (optional)"));
        customName->setMinimumHeight(phoneUi ? UiScale::dp(42) : 40);
        setTokenQss(customName, "input");
        cardStorage->addBodyWidget(customName);
        auto *btnEmbed = new QPushButton(QStringLiteral("In Blop öffnen"),
                                         cardStorage);
        btnEmbed->setCursor(Qt::PointingHandCursor);
        btnEmbed->setMinimumHeight(phoneUi ? UiScale::dp(44) : 40);
        setTokenQss(btnEmbed, "primary");
        BlopRipple::attachPressFeedback(btnEmbed, 0.92);
        connect(btnEmbed, &QPushButton::clicked, this,
                [this, customUrl, customName, requestCloud]() {
                  const QString typed = customUrl->text().trimmed();
                  if (typed.isEmpty())
                    return;
                  CloudStorageEntry e;
                  e.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
                  e.type = QStringLiteral("custom");
                  e.name = customName->text().trimmed().isEmpty()
                               ? QStringLiteral("Eigene Cloud")
                               : customName->text().trimmed();
                  e.webUrl = QUrl::fromUserInput(typed).toString();
                  CloudStorageStore::upsert(e);
                  requestCloud(e);
                });
        cardStorage->addBodyWidget(btnEmbed);
    }

    // ----- Card: Erweitert ----------------------------------------------
    auto *cardAdv = new BlopSettingsCard(
        QStringLiteral("Erweitert"),
        QStringLiteral("Version, Informationen"),
        contentWidget);
    {
        const QString version = QString(BLOP_VERSION_STR);
        const QString versionLabel =
            (version.startsWith('v') ? QStringLiteral("Blop ")
                                     : QStringLiteral("Blop v")) +
            version;
        auto *info = new QLabel(versionLabel, cardAdv);
        setThemedQss(info, QStringLiteral(
            "color: rgba(180, 188, 215, 0.78); font-size: 12px;"
            "background: transparent; padding: 4px 0;"));
        cardAdv->addBodyWidget(info);
    }
    cardAdv->setExpanded(true);

    // Three equal columns so cards keep their natural height instead of
    // stretching into empty slabs; Speicher stays a full-width panel.
    const auto policyCard = [](BlopSettingsCard *c) {
        c->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    };
    policyCard(cardKonto);
    policyCard(cardTheme);
    policyCard(cardLook);
    policyCard(cardBehavior);
    policyCard(cardAdv);
    policyCard(cardStorage);

    auto makeCol = [cardsHost, cardGap](std::initializer_list<QWidget *> cards) {
        auto *col = new QWidget(cardsHost);
        auto *v = new QVBoxLayout(col);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(cardGap);
        for (QWidget *c : cards)
            v->addWidget(c);
        return col;
    };
    if (phoneUi) {
        auto *stack = new QVBoxLayout();
        stack->setContentsMargins(0, 0, 0, 0);
        stack->setSpacing(cardGap);
        for (BlopSettingsCard *c : {cardKonto, cardStorage, cardTheme, cardLook,
                                    cardBehavior, cardAdv})
            stack->addWidget(c);
        hostLay->addLayout(stack, 0);
    } else {
        auto *topCols = new QHBoxLayout();
        topCols->setContentsMargins(0, 0, 0, 0);
        topCols->setSpacing(cardGap);
        topCols->addWidget(makeCol({cardKonto, cardBehavior, cardAdv}), 1);
        topCols->addWidget(makeCol({cardTheme, cardLook}), 1);
        hostLay->addLayout(topCols, 0);
        hostLay->addWidget(cardStorage, 0);
    }
    hostLay->addStretch(1);
    contentLay->addWidget(cardsHost, 1);

    // Search: hide cards whose title doesn't match the filter (simple
    // top-level filter; deeper filtering could match labels inside the
    // card body but the four sections + their subtitles cover the most
    // common use case).
    connect(search, &QLineEdit::textChanged, this, [=](const QString &q) {
        const QString needle = q.trimmed().toLower();
        const QList<BlopSettingsCard *> cards = {cardKonto, cardStorage, cardTheme, cardLook,
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

    connect(&BlopTheme::instance(), &BlopTheme::themeChanged, this,
            &SettingsDialog::refreshTheme);
}

SettingsDialog::~SettingsDialog() { delete ui; }

void SettingsDialog::refreshTheme() {
    refreshThemedTree(this);
}

void SettingsDialog::showEvent(QShowEvent *event) {
    QDialog::showEvent(event);
    setWindowOpacity(1.0);
    // Embedded in BlopModal: never animate pos/opacity here. pos() still
    // reports leftover top-level coordinates and would shove the panel
    // sideways inside the card (Windows: settings appeared shifted right).
    if (!isWindow())
        return;
    if (!parentWidget() || m_dialogIntroDone)
        return;
    m_dialogIntroDone = true;
#ifdef Q_OS_ANDROID
    return;
#else
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
    if (!isWindow()) {
        emit profileEditRequested(profileId);
        return;
    }
    done(EditProfileCode);
}

void SettingsDialog::embedInWorkspace() {
    setWindowFlags(Qt::Widget);
    setModal(false);
    setAttribute(Qt::WA_QuitOnClose, false);
    setSizeGripEnabled(false);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(0, 0);
    setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    if (auto *lay = layout())
        lay->setSizeConstraint(QLayout::SetNoConstraint);
    if (ui && ui->tabWidget) {
        ui->tabWidget->setSizePolicy(QSizePolicy::Expanding,
                                     QSizePolicy::Expanding);
        ui->tabWidget->setMinimumSize(0, 0);
        if (QWidget *panel = ui->tabWidget->widget(0)) {
            panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            panel->setMinimumSize(0, 0);
        }
    }
    setThemedQss(this, QStringLiteral(
        "QDialog { background: transparent; border: none; border-radius: 0px; }"));
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
