#include "settingsdialog.h"
#include "blop_version.h"
#include "settings_ui_helpers.h"
#include "calendarservice.h"
#include "cloudstoragestore.h"
#include "cloudlink.h"
#include "googleauthmanager.h"
#include "storageprefs.h"
#include "uiprofilemanager.h"
#include "blop_inwindow_menu.h"
#include "blop_modal.h"
#include "blop_dialogs.h"
#include "blop_theme.h"
#include "bloplocale.h"
#include "blop_scroll.h"
#include "blopripple.h"
#include "blopstyle.h"
#include "overlayscrollindicator.h"
#include "uiscale.h"
#include "toolhotkeys.h"
#include "notetoolbars.h"
#include "ui_SettingsDialog.h"
#include "blop_diag.h"

#include <QButtonGroup>
#include <QColorDialog>
#include <QSpinBox>
#include <QBoxLayout>
#include <QByteArray>
#include <QDir>
#include <QEasingCurve>
#include <QEvent>
#include <QFileDialog>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QAbstractItemView>
#include <QStackedWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPalette>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QPixmap>
#include <QSettings>
#include <QVector>
#include <QShowEvent>
#include <QSize>
#include <QSizePolicy>
#include <QStandardPaths>
#include <QIcon>
#include <QTabBar>
#include <QToolButton>
#include <QUrl>
#include <QUuid>
#include <QVBoxLayout>
#include <QVariantAnimation>
#include <functional>
#include <initializer_list>
#include <memory>

// v3.16.1: Settings overhaul.
//
// Old layout was a single QFormLayout-like dump of fields inside the tab
// designed in Qt Designer. New layout uses a Hero section (current profile
// + "Profil bearbeiten") on top, a search bar, and four collapsible
// BlopSheet-skinned cards (Konto / Darstellung / Werkzeuge / Profile / Speicher / Mehr).
// Desktop: Notion left-nav + stacked pages. Phone: collapsible cards + search.

namespace {

using namespace SettingsUi;

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
        setSurfaceQss(this, QStringLiteral("BlopSettingsCard"));
#ifndef Q_OS_ANDROID
        if (!UiScale::isAndroidPhoneUi(parent)) {
            setStyleSheet(QStringLiteral(
                "#BlopSettingsCard {"
                "  background-color: transparent;"
                "  border: none;"
                "  border-radius: 0px;"
                "}"));
        }
#endif
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

        auto *root = new QVBoxLayout(this);
        const bool compact = UiScale::isAndroidPhoneUi(parent);
        const int cm = compact ? UiScale::dp(14) : 28;
        const int cv = compact ? UiScale::dp(12) : 24;
        root->setContentsMargins(cm, cv, cm, cv);
        root->setSpacing(0);

        m_header = new QWidget(this);
        m_header->setCursor(Qt::PointingHandCursor);
        auto *hl = new QHBoxLayout(m_header);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(12);

#ifndef Q_OS_ANDROID
        const QString titleCol = QStringLiteral("#1C1E24");
        const QString subCol = QStringLiteral("#6B6F76");
#else
        const QString titleCol = settingsInk();
        const QString subCol = settingsInkMuted();
#endif
        m_titleLbl = new QLabel(m_title, m_header);
        setThemedQss(m_titleLbl, QStringLiteral(
            "color: %1; font-size: 22px; font-weight: 700; letter-spacing: -0.3px;"
            "background: transparent;")
            .arg(titleCol));
        m_subtitleLbl = new QLabel(m_subtitle, m_header);
        setThemedQss(m_subtitleLbl, QStringLiteral(
            "color: %1; font-size: 13px; font-weight: 500;"
            "background: transparent;")
            .arg(subCol));

        auto *titleColumn = new QVBoxLayout();
        titleColumn->setContentsMargins(0, 0, 0, 0);
        titleColumn->setSpacing(4);
        titleColumn->addWidget(m_titleLbl);
        if (!m_subtitle.isEmpty())
            titleColumn->addWidget(m_subtitleLbl);
        else
            m_subtitleLbl->hide();
        hl->addLayout(titleColumn, 1);

        m_chevron = new SettingsChevronLabel(m_header);
        hl->addWidget(m_chevron, 0, Qt::AlignVCenter);
        root->addWidget(m_header);

        m_body = new QWidget(this);
        m_bodyLay = new QVBoxLayout(m_body);
        m_bodyLay->setContentsMargins(0, UiScale::dp(BlopStyle::sectionGapDp()), 0, 0);
        m_bodyLay->setSpacing(UiScale::dp(BlopStyle::sectionGapDp()));
        root->addWidget(m_body);

        m_header->installEventFilter(new HeaderClickFilter(this));
    }

    void addBodyWidget(QWidget *w) { m_bodyLay->addWidget(w); }
    void addBodyLayout(QLayout *l) { m_bodyLay->addLayout(l); }

    QString title() const { return m_title; }
    QString subtitle() const { return m_subtitle; }
    void setSectionKeywords(const QString &keys) { m_sectionKeywords = keys; }
    QString sectionKeywords() const { return m_sectionKeywords; }

    /// Concept B nav-panel: Notion page on paper — no dark surface, no black header.
    void setNavPanelMode(bool on) {
        m_navPanel = on;
        if (m_chevron)
            m_chevron->setVisible(!on);
        if (m_header)
            m_header->setCursor(on ? Qt::ArrowCursor : Qt::PointingHandCursor);
        if (on) {
            m_expanded = true;
            m_body->setMaximumHeight(QWIDGETSIZE_MAX);
            m_body->show();
            setProperty("blopSurfaceName", QVariant());
            setProperty("blopNavPaper", true);
            setAttribute(Qt::WA_StyledBackground, true);
            setAutoFillBackground(true);
            {
                const QColor bg = settingsContentBg();
                const QColor ink = QColor(settingsInk());
                QPalette pal = palette();
                pal.setColor(QPalette::Window, bg);
                pal.setColor(QPalette::Base, bg);
                pal.setColor(QPalette::WindowText, ink);
                setPalette(pal);
            }
            setStyleSheet(QStringLiteral(
                "#BlopSettingsCard {"
                "  background-color: %1;"
                "  border: none;"
                "}")
                              .arg(settingsContentBg().name(QColor::HexRgb)));
            setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
            if (auto *rootLay = qobject_cast<QVBoxLayout *>(layout())) {
                rootLay->setContentsMargins(0, 0, 0, 0);
                rootLay->setSpacing(UiScale::dp(10));
            }
            if (m_header) {
                m_header->setAttribute(Qt::WA_StyledBackground, true);
                m_header->setAutoFillBackground(true);
                QPalette hp = m_header->palette();
                hp.setColor(QPalette::Window, settingsContentBg());
                hp.setColor(QPalette::WindowText, QColor(settingsInk()));
                m_header->setPalette(hp);
                m_header->setStyleSheet(QStringLiteral(
                    "background: %1; border: none;")
                                            .arg(settingsContentBg().name(
                                                QColor::HexRgb)));
            }
            // Seamless list — no boxed card, soft inset only.
            if (m_body) {
                m_body->setObjectName(QStringLiteral("SettingsCardBody"));
                m_body->setAttribute(Qt::WA_StyledBackground, true);
                m_body->setAutoFillBackground(true);
                QPalette pal = m_body->palette();
                pal.setColor(QPalette::Window, settingsContentRowBg());
                pal.setColor(QPalette::Base, settingsContentRowBg());
                pal.setColor(QPalette::WindowText, QColor(settingsInk()));
                m_body->setPalette(pal);
                m_body->setStyleSheet(QStringLiteral(
                    "QWidget#SettingsCardBody {"
                    "  background: transparent;"
                    "  border: none;"
                    "}"));
                if (m_bodyLay) {
                    m_bodyLay->setContentsMargins(0, UiScale::dp(2), 0, 0);
                    m_bodyLay->setSpacing(0);
                }
            }
            if (m_titleLbl) {
                m_titleLbl->setProperty("blopRawQss", QVariant());
                m_titleLbl->setStyleSheet(QStringLiteral(
                    "color: %1; font-size: 15px; font-weight: 650;"
                    "letter-spacing: -0.25px; background: transparent;")
                                              .arg(settingsInk()));
            }
            if (m_subtitleLbl) {
                m_subtitleLbl->setProperty("blopRawQss", QVariant());
                m_subtitleLbl->setStyleSheet(QStringLiteral(
                    "color: %1; font-size: 12px; font-weight: 400;"
                    "background: transparent; padding-top: 1px;")
                                                 .arg(settingsInkMuted()));
            }
        } else {
            setProperty("blopNavPaper", false);
        }
    }

    void reapplyNavPaper() {
        if (m_navPanel)
            setNavPanelMode(true);
    }

    bool navPanelMode() const { return m_navPanel; }

    void setExpanded(bool on) {
        if (m_navPanel) {
            m_expanded = true;
            m_body->setMaximumHeight(QWIDGETSIZE_MAX);
            m_body->show();
            if (m_chevron)
                m_chevron->setExpanded(true);
            return;
        }
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

    QWidget *bodyWidget() const { return m_body; }

    void refreshTheme() {
        if (m_navPanel) {
            setStyleSheet(QStringLiteral(
                "#BlopSettingsCard { background: transparent; border: none; }"));
        } else {
            setSurfaceQss(this, QStringLiteral("BlopSettingsCard"));
#ifndef Q_OS_ANDROID
            if (!UiScale::isAndroidPhoneUi(parentWidget())) {
                setStyleSheet(QStringLiteral(
                    "#BlopSettingsCard {"
                    "  background-color: transparent;"
                    "  border: none;"
                    "}"));
            }
#endif
        }
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
            if (event->type() == QEvent::MouseButtonRelease &&
                m_card && !m_card->navPanelMode())
                m_card->setExpanded(!m_card->expanded());
            return QObject::eventFilter(watched, event);
        }
    private:
        BlopSettingsCard *m_card;
    };

    QString m_title;
    QString m_subtitle;
    QString m_sectionKeywords;
    QLabel *m_titleLbl{nullptr};
    QLabel *m_subtitleLbl{nullptr};
    SettingsChevronLabel *m_chevron{nullptr};
    QWidget *m_header{nullptr};
    QWidget *m_body{nullptr};
    QVBoxLayout *m_bodyLay{nullptr};
    bool m_expanded{true};
    bool m_navPanel{false};
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
    lay->setContentsMargins(UiScale::dp(BlopStyle::edgeInsetDp()),
                            UiScale::dp(BlopStyle::edgeInsetDp()),
                            UiScale::dp(BlopStyle::edgeInsetDp()),
                            UiScale::dp(BlopStyle::edgeInsetDp()));
    lay->setSpacing(UiScale::dp(BlopStyle::sectionGapDp()));
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
    lay->setContentsMargins(UiScale::dp(BlopStyle::edgeInsetDp()),
                            UiScale::dp(BlopStyle::edgeInsetDp()),
                            UiScale::dp(BlopStyle::edgeInsetDp()),
                            UiScale::dp(BlopStyle::edgeInsetDp()));
    lay->setSpacing(UiScale::dp(BlopStyle::sectionGapDp()));
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
#ifndef Q_OS_ANDROID
    // Dialog owns its fill (BlopModal must not replace — blopOwnsBackground).
    // Follow Modus: paper in Light, Obsidian in Dark.
    setProperty("blopOwnsBackground", true);
    setProperty("blopForcePaper", useSettingsPaper());
    setLiteralQss(this, QStringLiteral(
        "QDialog { background-color: %1; border: none; border-radius: 12px; }")
        .arg(settingsContentBg().name(QColor::HexRgb)));
#else
    setProperty("blopOwnsBackground", true);
    setProperty("blopForcePaper", useSettingsPaper());
    if (useSettingsPaper()) {
        setLiteralQss(this, QStringLiteral(
            "QDialog { background-color: %1; border: none; border-radius: 0px; }")
            .arg(BlopStyle::paperBg().name(QColor::HexRgb)));
    } else {
        setLiteralQss(this, QStringLiteral(
            "QDialog { background-color: %1; border: none; border-radius: 0px; }")
            .arg(BlopStyle::obsidianSheet().name(QColor::HexRgb)));
    }
#endif
    const bool phoneUi = UiScale::isAndroidPhoneUi(parent);
    const int pagePad = phoneUi ? UiScale::dp(BlopStyle::edgeInsetDp()) : 0;
    const int cardGap = phoneUi ? UiScale::dp(BlopStyle::sectionGapDp()) : 0;
    if (phoneUi)
        setMinimumSize(0, 0);
    else
        setMinimumSize(0, 0); // SideSheet host sizes the dialog; don't fight BlopModal.
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

#ifndef Q_OS_ANDROID
    // Notion-style single sheet: no dark title chrome. Title lives in the
    // nav column; Fertig sits in the content toolbar (wired below).
    if (ui->headerWidget)
        ui->headerWidget->hide();
    if (ui->lblIcon)
        ui->lblIcon->hide();
#endif

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
#ifndef Q_OS_ANDROID
    setThemedQss(hero, QStringLiteral(
        "#SettingsHero {"
        "  background-color: #FFFFFF;"
        "  border-bottom: 1px solid rgba(20,24,40,0.10);"
        "}"));
#else
    if (useSettingsPaper()) {
        setLiteralQss(hero, QStringLiteral(
            "#SettingsHero {"
            "  background-color: %1;"
            "  border-bottom: 1px solid rgba(20,24,40,0.10);"
            "}")
            .arg(BlopStyle::paperBg().name(QColor::HexRgb)));
    } else {
        setLiteralQss(hero, QStringLiteral(
            "#SettingsHero {"
            "  background-color: rgba(255, 255, 255, 0.03);"
            "  border-bottom: 1px solid rgba(255,255,255,0.08);"
            "}"));
    }
#endif
    auto *heroLay = new QHBoxLayout(hero);
    heroLay->setContentsMargins(pagePad, UiScale::dp(BlopStyle::edgeInsetDp()),
                                pagePad, UiScale::dp(BlopStyle::edgeInsetDp()));
    heroLay->setSpacing(UiScale::dp(BlopStyle::controlGapDp()));

    auto *avatar = new QLabel(hero);
    avatar->setObjectName(QStringLiteral("SettingsHeroAvatar"));
    avatar->setFixedSize(phoneUi ? 44 : 32, phoneUi ? 44 : 32);
    setThemedQss(avatar, QStringLiteral(
        "border-radius: %1px; background-color: %2;"
        "color: %3; font-size: %4px; font-weight: 700;")
        .arg(phoneUi ? 14 : 16)
        .arg(accentRgba(72), BlopTheme::textPrimary().name(QColor::HexRgb))
        .arg(phoneUi ? 18 : 13));
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
    heroText->setSpacing(1);
    auto *heroName = new QLabel(studyLoggedIn ? studyUser
                                              : (currentP.name.isEmpty()
                                                     ? QStringLiteral("Blop")
                                                     : currentP.name),
                                hero);
    heroName->setObjectName(QStringLiteral("SettingsHeroName"));
    heroName->setWordWrap(false);
#ifndef Q_OS_ANDROID
    if (!phoneUi) {
        setLiteralQss(heroName, QStringLiteral(
            "color: %1; font-size: 12px; font-weight: 600;"
            "background: transparent;")
            .arg(BlopStyle::paperInk().name(QColor::HexRgb)));
    } else
#endif
    {
        setThemedQss(heroName, QStringLiteral(
            "color: %1; %2 background: transparent;")
            .arg(settingsInk(),
                 BlopTheme::typeQss(BlopTheme::TextRole::TitleLarge)));
    }
    auto *heroSub = new QLabel(
        studyLoggedIn ? QStringLiteral("Angemeldet")
                      : QStringLiteral("Gast"),
        hero);
    heroSub->setObjectName(QStringLiteral("SettingsHeroSub"));
    heroSub->setWordWrap(false);
#ifndef Q_OS_ANDROID
    if (!phoneUi) {
        setLiteralQss(heroSub, QStringLiteral(
            "color: %1; font-size: 11px; background: transparent;")
            .arg(BlopStyle::paperInkMuted().name(QColor::HexRgb)));
    } else
#endif
    {
        setThemedQss(heroSub, QStringLiteral(
            "color: %1; %2 background: transparent;")
            .arg(settingsInkMuted(),
                 BlopTheme::typeQss(BlopTheme::TextRole::LabelLarge)));
        if (studyLoggedIn)
            heroSub->setText(QStringLiteral("Angemeldet bei Study"));
        else
            heroSub->setText(QStringLiteral("Nicht angemeldet"));
    }
    heroText->addWidget(heroName);
    heroText->addWidget(heroSub);
    heroLay->addLayout(heroText, 1);

    auto *heroEditBtn = new QPushButton(
#ifndef Q_OS_ANDROID
        phoneUi ? QStringLiteral("Bearbeiten") : QStringLiteral("›"),
#else
        QStringLiteral("Bearbeiten"),
#endif
        hero);
    heroEditBtn->setObjectName(QStringLiteral("SettingsHeroEdit"));
    heroEditBtn->setCursor(Qt::PointingHandCursor);
#ifndef Q_OS_ANDROID
    if (!phoneUi) {
        heroEditBtn->setFixedSize(UiScale::dp(22), UiScale::dp(22));
        setLiteralQss(heroEditBtn, QStringLiteral(
            "QPushButton {"
            "  background: transparent; color: %1; border: none;"
            "  font-size: 16px; font-weight: 500; padding: 0;"
            "}"
            "QPushButton:hover { color: %2; }")
            .arg(BlopStyle::paperInkMuted().name(QColor::HexRgb),
                 BlopTheme::accentPrimary().name(QColor::HexRgb)));
    } else
#endif
    {
        setTokenQss(heroEditBtn, "secondary");
    }
    connect(heroEditBtn, &QPushButton::clicked, this, [this]() {
        openEditor(m_profileManager ? m_profileManager->currentProfile().id : QString());
    });
    BlopRipple::attachPressFeedback(heroEditBtn, 0.92);
    heroLay->addWidget(heroEditBtn, 0, Qt::AlignVCenter);

    root->addWidget(hero);

    // ----- Search bar ---------------------------------------------------
    auto *searchRow = new QFrame(tabDesign);
    searchRow->setObjectName(QStringLiteral("SettingsSearchRow"));
    auto *searchLay = new QHBoxLayout(searchRow);
    searchLay->setContentsMargins(pagePad, UiScale::dp(BlopStyle::edgeInsetDp()),
                                  pagePad, UiScale::dp(BlopStyle::sectionGapDp()));
    auto *search = new QLineEdit(searchRow);
    search->setObjectName(QStringLiteral("SettingsSearch"));
    search->setPlaceholderText(QStringLiteral("Suchen…"));
    search->setClearButtonEnabled(true);
    {
        const int s = UiScale::dp(16);
        QPixmap pm(s, s);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(QPen(QColor(0x94, 0xA3, 0xB8), 1.6, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(Qt::NoBrush);
        const qreal r = s * 0.28;
        p.drawEllipse(QPointF(s * 0.42, s * 0.42), r, r);
        p.drawLine(QPointF(s * 0.62, s * 0.62), QPointF(s * 0.82, s * 0.82));
        p.end();
        search->addAction(QIcon(pm), QLineEdit::LeadingPosition);
    }
    if (useSettingsPaper()) {
        setLiteralQss(search, BlopStyle::paperInputQss());
    } else {
        setThemedQss(search, QStringLiteral(
            "QLineEdit {"
            "  background: %1;"
            "  color: %2;"
            "  border: 1px solid rgba(255,255,255,0.12);"
            "  border-radius: 10px;"
            "  padding: 12px 16px; font-size: 14px;"
            "}"
            "QLineEdit:focus { border: 1px solid %3; }")
            .arg(BlopTheme::surfaceMuted().name(QColor::HexRgb),
                 BlopTheme::textPrimary().name(QColor::HexRgb),
                 BlopTheme::accentPrimary().name(QColor::HexRgb)));
    }
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
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(QStringLiteral("background: transparent;"));
    BlopScroll::enableFingerScroll(scroll, BlopScroll::Axes::VerticalOnly);
    OverlayScrollIndicator::install(scroll);

    auto *contentWidget = new QWidget();
    contentWidget->setStyleSheet(QStringLiteral("background: transparent;"));
    contentWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    auto *contentLay = new QVBoxLayout(contentWidget);
    contentLay->setContentsMargins(pagePad, UiScale::dp(BlopStyle::edgeInsetDp()),
                                   pagePad,
                                   UiScale::dp(BlopStyle::edgeInsetDp() * 3));
    contentLay->setSpacing(cardGap);

    scroll->setWidget(contentWidget);
    root->addWidget(scroll, 1);

    // Keep content width ≤ viewport so BlopScroll cannot pan horizontally.
    if (phoneUi) {
        class WidthClampFilter final : public QObject {
        public:
            explicit WidthClampFilter(QScrollArea *sa, QWidget *content)
                : QObject(sa), m_sa(sa), m_content(content) {}
            bool eventFilter(QObject *watched, QEvent *event) override {
                if ((watched == m_sa || watched == m_sa->viewport()) &&
                    (event->type() == QEvent::Resize ||
                     event->type() == QEvent::Show) &&
                    m_sa->viewport() && m_content) {
                    const int w = m_sa->viewport()->width();
                    if (w > 0) {
                        m_content->setMaximumWidth(w);
                        m_content->setMinimumWidth(w);
                    }
                }
                return false;
            }
        private:
            QScrollArea *m_sa;
            QWidget *m_content;
        };
        auto *clamp = new WidthClampFilter(scroll, contentWidget);
        scroll->installEventFilter(clamp);
        if (scroll->viewport())
            scroll->viewport()->installEventFilter(clamp);
    }

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
        studyLoggedIn ? QStringLiteral("Study-Anmeldung und Sitzung")
                      : QStringLiteral("Anmelden bei Study"),
        contentWidget);
    cardKonto->setSectionKeywords(
        QStringLiteral("login anmelden google study account abmelden sitzung session"));
    {
        auto closeAfterAccountAction = [this, phoneUi]() {
            if (phoneUi)
                accept();
        };

        const QString statusText =
            studyLoggedIn
                ? QStringLiteral("Angemeldet als %1").arg(studyUser)
                : QStringLiteral("Nicht angemeldet — Notizen teilen braucht Study");
        auto *statusWrap = new QWidget(cardKonto);
        auto *statusLay = new QHBoxLayout(statusWrap);
        statusLay->setContentsMargins(0, 0, 0, 0);
        statusLay->setSpacing(UiScale::dp(8));
        auto *statusDot = new QLabel(statusWrap);
        statusDot->setFixedSize(UiScale::dp(8), UiScale::dp(8));
        statusDot->setStyleSheet(
            QStringLiteral("background: %1; border-radius: %2px; border: none;")
                .arg(studyLoggedIn ? QStringLiteral("#10B981")
                                   : QStringLiteral("#94A3B8"))
                .arg(UiScale::dp(4)));
        auto *statusLbl = new QLabel(statusText, statusWrap);
        statusLbl->setWordWrap(true);
        setLiteralQss(statusLbl, QStringLiteral(
            "color: %1; font-size: 12px; background: transparent;")
            .arg(settingsInkMuted()));
        statusLay->addWidget(statusDot, 0, Qt::AlignVCenter);
        statusLay->addWidget(statusLbl, 1);
        cardKonto->addBodyWidget(makePropertyRow(
            cardKonto, QStringLiteral("Status"), statusWrap, false,
            QStringLiteral("status sitzung account")));

        auto *btnAuthScreen = makeQuietAction(
            cardKonto,
            studyLoggedIn ? QStringLiteral("Öffnen →")
                          : QStringLiteral("Anmelden →"));
        connect(btnAuthScreen, &QPushButton::clicked, this,
                [this, studyLoggedIn, closeAfterAccountAction]() {
                  if (studyLoggedIn) {
                    emit logoutRequested();
                  } else {
                    emit studyLoginRequested();
                  }
                  closeAfterAccountAction();
                });
        cardKonto->addBodyWidget(makePropertyRow(
            cardKonto, QStringLiteral("Anmeldebildschirm"), btnAuthScreen,
            false, QStringLiteral("login auth anmelden study")));

        if (!studyLoggedIn) {
            auto *btnGoogle = makeQuietAction(cardKonto,
                                              QStringLiteral("Google →"));
            connect(btnGoogle, &QPushButton::clicked, this,
                    [this, closeAfterAccountAction]() {
                      emit googleLoginRequested();
                      closeAfterAccountAction();
                    });
            cardKonto->addBodyWidget(makePropertyRow(
                cardKonto, QStringLiteral("Mit Google anmelden"), btnGoogle,
                false, QStringLiteral("google oauth gmail")));
        }

        if (studyLoggedIn) {
            auto *btnLogout = makeQuietAction(cardKonto,
                                              QStringLiteral("Abmelden"), true);
            connect(btnLogout, &QPushButton::clicked, this, [this]() {
                emit logoutRequested();
                accept();
            });
            cardKonto->addBodyWidget(makePropertyRow(
                cardKonto, QStringLiteral("Sitzung"), btnLogout, false,
                QStringLiteral("logout abmelden session")));
        }
    }

    // ----- Card: Darstellung (Light/Dark Mode) --------------------------
    auto *cardTheme = new BlopSettingsCard(
        QStringLiteral("Darstellung"),
        QStringLiteral("Hell, Dunkel und Akzentfarbe"),
        contentWidget);
        cardTheme->setSectionKeywords(
        QStringLiteral("thema theme dunkelmodus dark light hell akzent farbe "
                       "burger tablet layout sprache language locale deutsch "
                       "english sidebar seitenleiste bewegung motion logo "
                       "banner bild marke"));
    {
        const int segH = settingsSegmentMinHeight();
        const QString segStyle = segmentedControlQss();

        auto *modeSeg = new QWidget(cardTheme);
        styleSegmentTrack(modeSeg);
        auto *modeLay = new QHBoxLayout(modeSeg);
        modeLay->setContentsMargins(UiScale::dp(3), UiScale::dp(3),
                                    UiScale::dp(3), UiScale::dp(3));
        modeLay->setSpacing(UiScale::dp(2));
        auto *btnLight = new QPushButton(QStringLiteral("Hell"), modeSeg);
        auto *btnDark = new QPushButton(QStringLiteral("Dunkel"), modeSeg);
        auto *btnSystem = new QPushButton(QStringLiteral("System"), modeSeg);
        for (QPushButton *b : {btnLight, btnDark, btnSystem}) {
            b->setCheckable(true);
            b->setCursor(Qt::PointingHandCursor);
            b->setFixedHeight(segH);
            b->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
            setThemedQss(b, segStyle);
            BlopRipple::attachPressFeedback(b, 0.92);
            modeLay->addWidget(b, 0);
        }
        auto *bgMode = new QButtonGroup(this);
        bgMode->setExclusive(true);
        bgMode->addButton(btnLight, 1);
        bgMode->addButton(btnDark, 0);
        bgMode->addButton(btnSystem, 2);
        if (BlopTheme::instance().followSystem())
            btnSystem->setChecked(true);
        else if (BlopTheme::instance().isLight())
            btnLight->setChecked(true);
        else
            btnDark->setChecked(true);
        connect(bgMode, &QButtonGroup::idClicked, this, [](int id) {
            if (id == 2) {
                BlopTheme::instance().setFollowSystem(true);
            } else {
                BlopTheme::instance().setFollowSystem(false);
                BlopTheme::instance().setMode(id == 1 ? BlopTheme::Mode::Light
                                                      : BlopTheme::Mode::Dark);
            }
        });
        cardTheme->addBodyWidget(makePropertyRow(
            cardTheme, QStringLiteral("Farbschema"), modeSeg, false,
            QStringLiteral("dark light hell dunkel thema farbschema system")));

        auto *langSeg = new QWidget(cardTheme);
        styleSegmentTrack(langSeg);
        auto *langLay = new QHBoxLayout(langSeg);
        langLay->setContentsMargins(UiScale::dp(3), UiScale::dp(3),
                                    UiScale::dp(3), UiScale::dp(3));
        langLay->setSpacing(UiScale::dp(2));
        auto *btnSys = new QPushButton(QStringLiteral("System"), langSeg);
        auto *btnDe = new QPushButton(QStringLiteral("Deutsch"), langSeg);
        auto *btnEn = new QPushButton(QStringLiteral("English"), langSeg);
        for (QPushButton *b : {btnSys, btnDe, btnEn}) {
            b->setCheckable(true);
            b->setCursor(Qt::PointingHandCursor);
            b->setFixedHeight(segH);
            b->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
            setThemedQss(b, segStyle);
            BlopRipple::attachPressFeedback(b, 0.92);
            langLay->addWidget(b, 0);
        }
        auto *langGroup = new QButtonGroup(this);
        langGroup->setExclusive(true);
        langGroup->addButton(btnSys, static_cast<int>(BlopLocale::Pref::System));
        langGroup->addButton(btnDe, static_cast<int>(BlopLocale::Pref::German));
        langGroup->addButton(btnEn, static_cast<int>(BlopLocale::Pref::English));
        const auto langPref = BlopLocale::instance().preference();
        btnSys->setChecked(langPref == BlopLocale::Pref::System);
        btnDe->setChecked(langPref == BlopLocale::Pref::German);
        btnEn->setChecked(langPref == BlopLocale::Pref::English);
        connect(langGroup, &QButtonGroup::idClicked, this, [](int id) {
            BlopLocale::instance().setPreference(
                static_cast<BlopLocale::Pref>(id));
        });
        cardTheme->addBodyWidget(makePropertyRow(
            // The UI texts are German only (no translation catalogs yet); this
            // setting only drives dates and the greeting, so it says so.
            cardTheme, QStringLiteral("Datum & Begrüßung"), langSeg, false,
            QStringLiteral("datum begrüßung locale region deutsch english")));

        auto *accentRow = new QWidget(cardTheme);
        auto *accentLay = new QHBoxLayout(accentRow);
        accentLay->setContentsMargins(0, 0, 0, 0);
        accentLay->setSpacing(UiScale::dp(8));
        const BlopTheme::Accent activeAccent = BlopTheme::instance().accent();
        auto *accentGroup = new QButtonGroup(this);
        accentGroup->setExclusive(true);
        const int swatch = UiScale::dp(24);
        const auto choices = BlopTheme::accentColors();
        for (int i = 0; i < choices.size(); ++i) {
            const BlopTheme::AccentSwatch &ch = choices[i];
            auto *b = new QPushButton(accentRow);
            b->setCheckable(true);
            b->setCursor(Qt::PointingHandCursor);
            b->setFixedSize(swatch, swatch);
            b->setToolTip(ch.label);
            b->setStyleSheet(
                QStringLiteral(
                    "QPushButton { background-color: %1;"
                    "  border-radius: %2px;"
                    "  border: 2px solid rgba(55,53,47,0.12); }"
                    "QPushButton:hover { border: 2px solid rgba(55,53,47,0.35); }"
                    "QPushButton:checked {"
                    "  border: 2.5px solid #1D4ED8;"
                    "  outline: 2px solid #BFDBFE;"
                    "}")
                    .arg(ch.color.name(QColor::HexRgb),
                         QString::number(swatch / 2)));
            b->setChecked(ch.accent == activeAccent);
            accentGroup->addButton(b, static_cast<int>(ch.accent));
            BlopRipple::attachPressFeedback(b, 0.88);
            accentLay->addWidget(b);
        }
        accentLay->addStretch();
        connect(accentGroup, &QButtonGroup::idClicked, this, [this](int id) {
            const auto a = static_cast<BlopTheme::Accent>(id);
            BlopTheme::instance().setAccent(a);
            emit accentColorChanged(BlopTheme::accentPrimary());
        });
        cardTheme->addBodyWidget(makePropertyRow(
            cardTheme, QStringLiteral("Akzentfarbe"), accentRow, false,
            QStringLiteral("akzent farbe accent blue green pink orange slate")));

        auto *densSeg = new QWidget(cardTheme);
        styleSegmentTrack(densSeg);
        auto *densLay = new QHBoxLayout(densSeg);
        densLay->setContentsMargins(UiScale::dp(3), UiScale::dp(3),
                                    UiScale::dp(3), UiScale::dp(3));
        densLay->setSpacing(UiScale::dp(2));
        auto *btnDensS = new QPushButton(QStringLiteral("Klein"), densSeg);
        auto *btnDensM = new QPushButton(QStringLiteral("Mittel"), densSeg);
        auto *btnDensL = new QPushButton(QStringLiteral("Groß"), densSeg);
        for (QPushButton *b : {btnDensS, btnDensM, btnDensL}) {
            b->setCheckable(true);
            b->setCursor(Qt::PointingHandCursor);
            b->setFixedHeight(segH);
            b->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
            setThemedQss(b, segStyle);
            BlopRipple::attachPressFeedback(b, 0.92);
            densLay->addWidget(b, 0);
        }
        auto *densGroup = new QButtonGroup(this);
        densGroup->setExclusive(true);
        densGroup->addButton(btnDensS, 0);
        densGroup->addButton(btnDensM, 1);
        densGroup->addButton(btnDensL, 2);
        {
            QSettings st(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
            const int d =
                qBound(0, st.value(QStringLiteral("ui/libraryDensity"), 1).toInt(),
                       2);
            if (d == 0)
                btnDensS->setChecked(true);
            else if (d == 2)
                btnDensL->setChecked(true);
            else
                btnDensM->setChecked(true);
        }
        connect(densGroup, &QButtonGroup::idClicked, this, [this](int id) {
            QSettings s(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
            s.setValue(QStringLiteral("ui/libraryDensity"), id);
            emit appPrefsChanged();
        });
        cardTheme->addBodyWidget(makePropertyRow(
            cardTheme, QStringLiteral("Ansichtsdichte (Bibliothek)"), densSeg,
            false,
            QStringLiteral("dichte density klein mittel groß bibliothek")));

        auto *brandBox = new QWidget(cardTheme);
        auto *brandLay = new QHBoxLayout(brandBox);
        brandLay->setContentsMargins(0, 0, 0, 0);
        brandLay->setSpacing(UiScale::dp(8));
        auto *brandPreview = new QLabel(brandBox);
        brandPreview->setAlignment(Qt::AlignCenter);
        brandPreview->setScaledContents(false);
        auto refreshBrandPreview = [brandPreview]() {
          QSettings st(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
          const QString path =
              st.value(QStringLiteral("ui/brandMarkPath")).toString().trimmed();
          QPixmap pix;
          if (!path.isEmpty())
            pix.load(path);
          if (pix.isNull())
            pix = QPixmap(QStringLiteral(":/assets/logo.jpg"));
          const bool banner =
              !pix.isNull() && pix.width() > pix.height() * 2;
          const int h = UiScale::dp(32);
          const int w =
              banner ? qBound(h * 2,
                              int(qreal(h) * pix.width() / qMax(1, pix.height())),
                              UiScale::dp(140))
                     : h;
          brandPreview->setFixedSize(w, h);
          if (pix.isNull()) {
            brandPreview->setPixmap(QPixmap());
            brandPreview->setText(QStringLiteral("B"));
            return;
          }
          brandPreview->setText(QString());
          brandPreview->setPixmap(pix.scaled(w, h, Qt::KeepAspectRatio,
                                             Qt::SmoothTransformation));
        };
        refreshBrandPreview();
        brandLay->addWidget(brandPreview, 0, Qt::AlignVCenter);
        auto *btnBrandPick =
            makeQuietAction(cardTheme, QStringLiteral("Wählen"));
        btnBrandPick->setToolTip(QStringLiteral(
            "Eigenes Logo oder Banner. Ein breites Bild ersetzt das Wort "
            "Blop in der Titelleiste."));
        auto *btnBrandReset =
            makeQuietAction(cardTheme, QStringLiteral("Standard"));
        connect(btnBrandPick, &QPushButton::clicked, this,
                [this, refreshBrandPreview]() {
                  const QString path = QFileDialog::getOpenFileName(
                      this, QStringLiteral("Logo oder Banner"), QString(),
                      QStringLiteral(
                          "Bilder (*.png *.jpg *.jpeg *.webp *.bmp)"));
                  if (path.isEmpty())
                    return;
                  QSettings st(QStringLiteral("Blop"),
                               QStringLiteral("BlopApp"));
                  st.setValue(QStringLiteral("ui/brandMarkPath"), path);
                  refreshBrandPreview();
                  emit appPrefsChanged();
                });
        connect(btnBrandReset, &QPushButton::clicked, this,
                [refreshBrandPreview, this]() {
                  QSettings st(QStringLiteral("Blop"),
                               QStringLiteral("BlopApp"));
                  st.remove(QStringLiteral("ui/brandMarkPath"));
                  refreshBrandPreview();
                  emit appPrefsChanged();
                });
        brandLay->addWidget(btnBrandPick, 0, Qt::AlignVCenter);
        brandLay->addWidget(btnBrandReset, 0, Qt::AlignVCenter);
        cardTheme->addBodyWidget(makePropertyRow(
            cardTheme, QStringLiteral("Logo"), brandBox, false,
            QStringLiteral("logo banner bild marke eigenes")));

        auto *btnBurger = makeQuietAction(
            cardTheme, QStringLiteral("Tablet/Laptop"));
        btnBurger->setCheckable(true);
        btnBurger->setChecked(UiScale::forceBurgerMenu());
        btnBurger->setToolTip(QStringLiteral(
            "Burger-Menü auch auf Tablet/Laptop (Handy: immer an)"));
        connect(btnBurger, &QPushButton::toggled, this, [this](bool on) {
            UiScale::setForceBurgerMenu(on);
            emit uiLayoutPrefsChanged();
        });
        cardTheme->addBodyWidget(makePropertyRow(
            cardTheme, QStringLiteral("Burger-Menü"), btnBurger, false,
            QStringLiteral("burger tablet laptop layout")));

        auto *sidebarSeg = new QWidget(cardTheme);
        styleSegmentTrack(sidebarSeg);
        auto *sidebarLay = new QHBoxLayout(sidebarSeg);
        sidebarLay->setContentsMargins(UiScale::dp(3), UiScale::dp(3),
                                       UiScale::dp(3), UiScale::dp(3));
        sidebarLay->setSpacing(UiScale::dp(2));
        auto *btnSideOpen = new QPushButton(QStringLiteral("Offen"), sidebarSeg);
        auto *btnSideClosed = new QPushButton(QStringLiteral("Zu"), sidebarSeg);
        for (QPushButton *b : {btnSideOpen, btnSideClosed}) {
            b->setCheckable(true);
            b->setCursor(Qt::PointingHandCursor);
            b->setFixedHeight(segH);
            b->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
            setThemedQss(b, segStyle);
            BlopRipple::attachPressFeedback(b, 0.92);
            sidebarLay->addWidget(b, 0);
        }
        auto *sidebarGroup = new QButtonGroup(this);
        sidebarGroup->setExclusive(true);
        sidebarGroup->addButton(btnSideOpen, 1);
        sidebarGroup->addButton(btnSideClosed, 0);
        {
            QSettings st(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
            const bool open =
                st.value(QStringLiteral("ui/sidebarStartOpen"), true).toBool();
            btnSideOpen->setChecked(open);
            btnSideClosed->setChecked(!open);
        }
        connect(sidebarGroup, &QButtonGroup::idClicked, this, [this](int id) {
            QSettings s(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
            s.setValue(QStringLiteral("ui/sidebarStartOpen"), id == 1);
            emit appPrefsChanged();
        });
        cardTheme->addBodyWidget(makePropertyRow(
            cardTheme, QStringLiteral("Sidebar beim Start"), sidebarSeg, false,
            QStringLiteral("sidebar seitenleiste start offen zu")));

        {
            auto *startSeg = new QWidget(cardTheme);
            styleSegmentTrack(startSeg);
            auto *startLay = new QHBoxLayout(startSeg);
            startLay->setContentsMargins(UiScale::dp(3), UiScale::dp(3),
                                         UiScale::dp(3), UiScale::dp(3));
            startLay->setSpacing(UiScale::dp(2));
            auto *btnLib = new QPushButton(QStringLiteral("Bibliothek"), startSeg);
            auto *btnLast = new QPushButton(QStringLiteral("Letzte Notiz"), startSeg);
            for (QPushButton *b : {btnLib, btnLast}) {
                b->setCheckable(true);
                b->setCursor(Qt::PointingHandCursor);
                b->setFixedHeight(segH);
                b->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
                setThemedQss(b, segStyle);
                BlopRipple::attachPressFeedback(b, 0.92);
                startLay->addWidget(b, 0);
            }
            auto *startGroup = new QButtonGroup(this);
            startGroup->setExclusive(true);
            startGroup->addButton(btnLib, 0);
            startGroup->addButton(btnLast, 1);
            QSettings st(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
            const bool lastNote =
                st.value(QStringLiteral("ui/startView"), QStringLiteral("library"))
                    .toString() == QLatin1String("lastNote");
            btnLib->setChecked(!lastNote);
            btnLast->setChecked(lastNote);
            connect(startGroup, &QButtonGroup::idClicked, this, [this](int id) {
                QSettings s(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
                s.setValue(QStringLiteral("ui/startView"),
                           id == 1 ? QStringLiteral("lastNote")
                                   : QStringLiteral("library"));
                emit appPrefsChanged();
            });
            cardTheme->addBodyWidget(makePropertyRow(
                cardTheme, QStringLiteral("Startansicht"), startSeg, false,
                QStringLiteral("start startup bibliothek letzte notiz")));
        }

        auto *btnMotion = makeQuietAction(cardTheme, QStringLiteral("Aus"));
        btnMotion->setCheckable(true);
        {
            QSettings st(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
            btnMotion->setChecked(
                st.value(QStringLiteral("ui/reduceMotion"), false).toBool());
        }
        btnMotion->setText(btnMotion->isChecked() ? QStringLiteral("An")
                                                  : QStringLiteral("Aus"));
        btnMotion->setToolTip(
            QStringLiteral("Weniger Animationen in Einstellungen und UI"));
        connect(btnMotion, &QPushButton::toggled, this, [this, btnMotion](bool on) {
            QSettings s(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
            s.setValue(QStringLiteral("ui/reduceMotion"), on);
            btnMotion->setText(on ? QStringLiteral("An") : QStringLiteral("Aus"));
            emit appPrefsChanged();
        });
        cardTheme->addBodyWidget(makePropertyRow(
            cardTheme, QStringLiteral("Bewegung reduzieren"), btnMotion, true,
            QStringLiteral("motion animation bewegung reduce")));
    }

    // ----- Card: Werkzeuge ----------------------------------------------
    auto *cardLook = new BlopSettingsCard(
        QStringLiteral("Werkzeuge"),
#ifdef Q_OS_ANDROID
        QStringLiteral("Toolbar und Favorites"),
#else
        QStringLiteral("Werkzeugleiste, Speichern und neue Notizen"),
#endif
        contentWidget);
    cardLook->setSectionKeywords(
        QStringLiteral("toolbar radial studio layout favoriten werkzeugleiste "
                       "klassisch vertikal autosave speichern seitenfarbe "
                       "löschen delete"));
    {
#ifdef Q_OS_ANDROID
        auto *rNorm = new QRadioButton(QStringLiteral("Vertikal / Adaptiv"),
                                       cardLook);
        rNorm->setObjectName(QStringLiteral("radioVert"));
        auto *rFull = new QRadioButton(QStringLiteral("Radial"), cardLook);
        rFull->setObjectName(QStringLiteral("radioRadial"));
        const QString radioStyle = QStringLiteral(
            "QRadioButton { color: %1; background: transparent; "
            "padding: 6px 0; font-size: 13px; min-height: 36px; }")
            .arg(settingsInk());
        setThemedQss(rNorm, radioStyle);
        setThemedQss(rFull, radioStyle);
        cardLook->addBodyWidget(rNorm);
        cardLook->addBodyWidget(rFull);

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
#else
        // Keep radio ids for setToolbarConfig / profiles; hide from UI.
        auto *rNorm = new QRadioButton(cardLook);
        rNorm->setObjectName(QStringLiteral("radioVert"));
        rNorm->hide();
        auto *rFull = new QRadioButton(cardLook);
        rFull->setObjectName(QStringLiteral("radioRadial"));
        rFull->hide();
        rFull->setEnabled(false);
        rNorm->setChecked(true);

        auto *variantSeg = new QWidget(cardLook);
        styleSegmentTrack(variantSeg);
        auto *variantLay = new QHBoxLayout(variantSeg);
        variantLay->setContentsMargins(UiScale::dp(3), UiScale::dp(3),
                                       UiScale::dp(3), UiScale::dp(3));
        variantLay->setSpacing(UiScale::dp(2));
        const QString segStyle = segmentedControlQss();
        const int segH = settingsSegmentMinHeight();
        auto *bgVariant = new QButtonGroup(this);
        bgVariant->setExclusive(true);
        struct VariantOpt {
          int id;
          const char *label;
          const char *tip;
        };
        // Desktop note chrome renders NoteToolbarHost (Bar / Floating /
        // Radiant / Science). The old Klassisch/A-D list only drove the hidden
        // ModernToolbar, so the segment now edits the toolbar that is
        // actually on screen.
        // Settings UI shows Dock vs Radial; Floating/Science stay available
        // via profiles / runtime enums.
        const VariantOpt opts[] = {
            {0, "Obere Leiste (Dock)", "Feste Werkzeugleiste oben"},
            {2, "Radialmenü", "Radiale Werkzeugauswahl"},
        };
        int saved = static_cast<int>(noteToolbarStyle());
        if (saved != 0 && saved != 2)
          saved = 0;
        for (const VariantOpt &o : opts) {
          auto *b = new QPushButton(QString::fromUtf8(o.label), variantSeg);
          b->setCheckable(true);
          b->setCursor(Qt::PointingHandCursor);
          b->setToolTip(QString::fromUtf8(o.tip));
          b->setFixedHeight(segH);
          b->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
          setThemedQss(b, segStyle);
          bgVariant->addButton(b, o.id);
          if (o.id == saved)
            b->setChecked(true);
          BlopRipple::attachPressFeedback(b, 0.94);
          variantLay->addWidget(b, 0);
        }
        connect(bgVariant, &QButtonGroup::idClicked, this, [this](int id) {
          setNoteToolbarStyle(static_cast<NoteToolbarStyle>(id));
          emit noteToolbarStyleChanged();
        });
        cardLook->addBodyWidget(makePropertyRow(
            cardLook, QStringLiteral("Standard-Werkzeugleiste im Editor"),
            variantSeg, false,
            QStringLiteral("toolbar werkzeugleiste layout dock radial balken")));
#endif

        const int autoSegH = settingsSegmentMinHeight();
        const QString autoSegStyle = segmentedControlQss();

        auto *fmtSeg = new QWidget(cardLook);
        styleSegmentTrack(fmtSeg);
        auto *fmtLay = new QHBoxLayout(fmtSeg);
        fmtLay->setContentsMargins(UiScale::dp(3), UiScale::dp(3),
                                   UiScale::dp(3), UiScale::dp(3));
        fmtLay->setSpacing(UiScale::dp(2));
        auto *btnFmtA4 = new QPushButton(QStringLiteral("DIN A4"), fmtSeg);
        auto *btnFmtInf = new QPushButton(QStringLiteral("Unendlich"), fmtSeg);
        auto *btnFmtStruct =
            new QPushButton(QStringLiteral("Struktur"), fmtSeg);
        for (QPushButton *b : {btnFmtA4, btnFmtInf, btnFmtStruct}) {
            b->setCheckable(true);
            b->setCursor(Qt::PointingHandCursor);
            b->setFixedHeight(autoSegH);
            b->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
            setThemedQss(b, autoSegStyle);
            BlopRipple::attachPressFeedback(b, 0.92);
            fmtLay->addWidget(b, 0);
        }
        auto *fmtGroup = new QButtonGroup(this);
        fmtGroup->setExclusive(true);
        fmtGroup->addButton(btnFmtA4, 1);
        fmtGroup->addButton(btnFmtInf, 0);
        fmtGroup->addButton(btnFmtStruct, 2);
        {
            QSettings st(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
            const int f =
                st.value(QStringLiteral("ui/defaultNoteFormat"), 1).toInt();
            if (f == 0)
                btnFmtInf->setChecked(true);
            else if (f == 2)
                btnFmtStruct->setChecked(true);
            else
                btnFmtA4->setChecked(true);
        }
        connect(fmtGroup, &QButtonGroup::idClicked, this, [this](int id) {
            QSettings s(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
            s.setValue(QStringLiteral("ui/defaultNoteFormat"), id);
            emit appPrefsChanged();
        });
        cardLook->addBodyWidget(makePropertyRow(
            cardLook, QStringLiteral("Standardformat"), fmtSeg, false,
            QStringLiteral("format a4 unendlich struktur vorlage")));

        auto *tplSeg = new QWidget(cardLook);
        styleSegmentTrack(tplSeg);
        auto *tplLay = new QHBoxLayout(tplSeg);
        tplLay->setContentsMargins(UiScale::dp(3), UiScale::dp(3),
                                   UiScale::dp(3), UiScale::dp(3));
        tplLay->setSpacing(UiScale::dp(2));
        // IDs match NewNoteDialog backgroundType: 0 Leer, 1 Liniert, 2 Kariert, 3 Punktiert.
        auto *btnTplBlank = new QPushButton(QStringLiteral("Leer"), tplSeg);
        auto *btnTplLine = new QPushButton(QStringLiteral("Liniert"), tplSeg);
        auto *btnTplGrid = new QPushButton(QStringLiteral("Kariert"), tplSeg);
        auto *btnTplDot = new QPushButton(QStringLiteral("Punktiert"), tplSeg);
        for (QPushButton *b :
             {btnTplBlank, btnTplLine, btnTplGrid, btnTplDot}) {
            b->setCheckable(true);
            b->setCursor(Qt::PointingHandCursor);
            b->setFixedHeight(autoSegH);
            b->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
            setThemedQss(b, autoSegStyle);
            BlopRipple::attachPressFeedback(b, 0.92);
            tplLay->addWidget(b, 0);
        }
        auto *tplGroup = new QButtonGroup(this);
        tplGroup->setExclusive(true);
        tplGroup->addButton(btnTplBlank, 0);
        tplGroup->addButton(btnTplLine, 1);
        tplGroup->addButton(btnTplGrid, 2);
        tplGroup->addButton(btnTplDot, 3);
        {
            QSettings st(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
            const int p =
                qBound(0,
                       st.value(QStringLiteral("ui/defaultPaperPattern"), 2)
                           .toInt(),
                       3);
            QAbstractButton *btn = tplGroup->button(p);
            if (btn)
                btn->setChecked(true);
            else
                btnTplGrid->setChecked(true);
        }
        connect(tplGroup, &QButtonGroup::idClicked, this, [this](int id) {
            QSettings s(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
            s.setValue(QStringLiteral("ui/defaultPaperPattern"), id);
            emit appPrefsChanged();
        });
        cardLook->addBodyWidget(makePropertyRow(
            cardLook, QStringLiteral("Standard-Vorlage"), tplSeg, false,
            QStringLiteral("vorlage kariert punktiert liniert leer paper")));

        auto *autoToggle = new QPushButton(cardLook);
        autoToggle->setObjectName(QStringLiteral("SettingsAutoSaveToggle"));
        autoToggle->setCheckable(true);
        autoToggle->setCursor(Qt::PointingHandCursor);
        autoToggle->setFixedHeight(UiScale::dp(32));
        autoToggle->setMinimumWidth(UiScale::dp(52));
        {
            QSettings st(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
            const int ms =
                st.value(QStringLiteral("ui/autoSaveMs"), 1500).toInt();
            autoToggle->setChecked(ms >= 0);
        }
        auto restyleAuto = [autoToggle]() {
            const bool on = autoToggle->isChecked();
            autoToggle->setText(on ? QStringLiteral("An")
                                   : QStringLiteral("Aus"));
            autoToggle->setStyleSheet(QStringLiteral(
                "QPushButton#SettingsAutoSaveToggle {"
                "  border: none; border-radius: 16px; padding: 0 14px;"
                "  font-size: 12px; font-weight: 600;"
                "  color: %1; background: %2;"
                "}")
                                          .arg(on ? QStringLiteral("#FFFFFF")
                                                  : QStringLiteral("#475569"),
                                               on ? QStringLiteral("#2563EB")
                                                  : QStringLiteral("#E2E8F0")));
        };
        restyleAuto();
        connect(autoToggle, &QPushButton::toggled, this,
                [this, restyleAuto](bool on) {
                    QSettings s(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
                    s.setValue(QStringLiteral("ui/autoSaveMs"), on ? 1500 : -1);
                    restyleAuto();
                    emit appPrefsChanged();
                });
        cardLook->addBodyWidget(makePropertyRow(
            cardLook, QStringLiteral("Automatisch speichern"), autoToggle, false,
            QStringLiteral("autosave speichern debounce")));

        auto *pageSeg = new QWidget(cardLook);
        styleSegmentTrack(pageSeg);
        auto *pageLay = new QHBoxLayout(pageSeg);
        pageLay->setContentsMargins(UiScale::dp(3), UiScale::dp(3),
                                    UiScale::dp(3), UiScale::dp(3));
        pageLay->setSpacing(UiScale::dp(2));
        auto *btnPageLight = new QPushButton(QStringLiteral("Hell"), pageSeg);
        auto *btnPageDark = new QPushButton(QStringLiteral("Dunkel"), pageSeg);
        for (QPushButton *b : {btnPageLight, btnPageDark}) {
            b->setCheckable(true);
            b->setCursor(Qt::PointingHandCursor);
            b->setFixedHeight(autoSegH);
            b->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
            setThemedQss(b, autoSegStyle);
            BlopRipple::attachPressFeedback(b, 0.92);
            pageLay->addWidget(b, 0);
        }
        auto *pageGroup = new QButtonGroup(this);
        pageGroup->setExclusive(true);
        pageGroup->addButton(btnPageLight, 0);
        pageGroup->addButton(btnPageDark, 1);
        {
            QSettings st(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
            const bool dark =
                st.value(QStringLiteral("ui/defaultPageColor"),
                         QStringLiteral("light"))
                    .toString() == QLatin1String("dark");
            btnPageLight->setChecked(!dark);
            btnPageDark->setChecked(dark);
        }
        connect(pageGroup, &QButtonGroup::idClicked, this, [this](int id) {
            QSettings s(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
            s.setValue(QStringLiteral("ui/defaultPageColor"),
                       id == 1 ? QStringLiteral("dark")
                               : QStringLiteral("light"));
            emit appPrefsChanged();
        });
        cardLook->addBodyWidget(makePropertyRow(
            cardLook, QStringLiteral("Neue Notizen: Seitenfarbe"), pageSeg,
            false,
            QStringLiteral("seitenfarbe paper page color hell dunkel")));

        auto *btnConfirm = makeQuietAction(cardLook, QStringLiteral("An"));
        btnConfirm->setCheckable(true);
        {
            QSettings st(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
            btnConfirm->setChecked(
                st.value(QStringLiteral("ui/confirmDelete"), true).toBool());
        }
        btnConfirm->setText(btnConfirm->isChecked() ? QStringLiteral("An")
                                                    : QStringLiteral("Aus"));
        btnConfirm->setToolTip(
            QStringLiteral("Vor dem Löschen einer Notiz nachfragen"));
        connect(btnConfirm, &QPushButton::toggled, this,
                [this, btnConfirm](bool on) {
                    QSettings s(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
                    s.setValue(QStringLiteral("ui/confirmDelete"), on);
                    btnConfirm->setText(on ? QStringLiteral("An")
                                           : QStringLiteral("Aus"));
                    emit appPrefsChanged();
                });
        cardLook->addBodyWidget(makePropertyRow(
            cardLook, QStringLiteral("Löschen bestätigen"), btnConfirm, true,
            QStringLiteral("löschen delete confirm trash")));
    }

#ifndef Q_OS_ANDROID
    // ----- Card: Keyboard shortcuts (discoverability) -------------------
    auto *cardShortcuts = new BlopSettingsCard(
        QStringLiteral("Tastatur"),
        QStringLiteral("Werkzeug-Kürzel und Kurzbefehle"),
        contentWidget);
    cardShortcuts->setSectionKeywords(
        QStringLiteral("tastatur shortcuts hotkeys tastenkürzel kurzbefehl "
                       "entf ctrl löschen suchen werkzeug stift radierer marker text"));
    {
      struct HotkeyRow {
        QString id;
        bool pen = false;
        QKeySequenceEdit *keys = nullptr;
        QPushButton *color = nullptr;
        QSpinBox *width = nullptr;
      };
      QVector<HotkeyRow> hotkeyRows;
      const QVector<ResolvedHotkey> defs = resolvedToolHotkeys();
      auto publishKeys = [this]() {
        publishAssistantSettings();
        emit appPrefsChanged();
      };
      for (const ResolvedHotkey &def : defs) {
        HotkeyRow row;
        row.id = def.id;
        row.pen = def.penPreset;
        auto *wrap = new QWidget(cardShortcuts);
        auto *box = new QHBoxLayout(wrap);
        box->setContentsMargins(0, 0, 0, 0);
        box->setSpacing(6);
        row.keys = new QKeySequenceEdit(def.sequence, wrap);
        row.keys->setMaximumSequenceLength(1);
        row.keys->setClearButtonEnabled(true);
        row.keys->setFixedWidth(UiScale::dp(140));
        row.keys->setFixedHeight(UiScale::dp(32));
        box->addWidget(row.keys);
        if (def.penPreset) {
          row.color = new QPushButton(wrap);
          row.color->setFixedSize(UiScale::dp(32), UiScale::dp(32));
          row.color->setCursor(Qt::PointingHandCursor);
          row.color->setStyleSheet(
              QStringLiteral("background: %1; border: 1px solid #888; border-radius: 6px;")
                  .arg(def.color.name(QColor::HexRgb)));
          row.width = new QSpinBox(wrap);
          row.width->setRange(1, 40);
          row.width->setValue(def.width);
          row.width->setFixedWidth(UiScale::dp(64));
          box->addWidget(row.color);
          box->addWidget(row.width);
        }
        hotkeyRows.append(row);
        const int index = hotkeyRows.size() - 1;
        auto saveRow = [this, hotkeyRows, index, publishKeys]() {
          const HotkeyRow &item = hotkeyRows.at(index);
          const QColor color = item.color
                                   ? QColor(item.color->property("penColor").toString())
                                   : QColor();
          storeToolHotkey(item.id,
                          item.keys->keySequence().toString(QKeySequence::PortableText),
                          color.isValid() ? color : QColor(Qt::black),
                          item.width ? item.width->value() : 3);
          publishKeys();
        };
        if (row.color) {
          row.color->setProperty("penColor", def.color.name(QColor::HexRgb));
          connect(row.color, &QPushButton::clicked, this, [this, row, saveRow]() {
            const QColor next = QColorDialog::getColor(
                QColor(row.color->property("penColor").toString()), this,
                QStringLiteral("Stiftfarbe"));
            if (!next.isValid())
              return;
            row.color->setProperty("penColor", next.name(QColor::HexRgb));
            row.color->setStyleSheet(
                QStringLiteral("background: %1; border: 1px solid #888; border-radius: 6px;")
                    .arg(next.name(QColor::HexRgb)));
            saveRow();
          });
        }
        connect(row.keys, &QKeySequenceEdit::editingFinished, this, saveRow);
        if (row.width)
          connect(row.width, &QSpinBox::valueChanged, this, [saveRow](int) { saveRow(); });
        cardShortcuts->addBodyWidget(makePropertyRow(
            cardShortcuts, def.label, wrap, false,
            QStringLiteral("tastatur werkzeug %1").arg(def.label.toLower())));
      }
      auto *btnHotkeyReset =
          makeQuietAction(cardShortcuts, QStringLiteral("Standard"));
      connect(btnHotkeyReset, &QPushButton::clicked, this,
              [this, hotkeyRows, publishKeys]() {
                resetToolHotkeys();
                const QVector<ResolvedHotkey> fresh = resolvedToolHotkeys();
                for (int i = 0; i < fresh.size() && i < hotkeyRows.size(); ++i) {
                  hotkeyRows.at(i).keys->blockSignals(true);
                  hotkeyRows.at(i).keys->setKeySequence(fresh.at(i).sequence);
                  hotkeyRows.at(i).keys->blockSignals(false);
                  if (hotkeyRows.at(i).width) {
                    hotkeyRows.at(i).width->blockSignals(true);
                    hotkeyRows.at(i).width->setValue(fresh.at(i).width);
                    hotkeyRows.at(i).width->blockSignals(false);
                  }
                  if (hotkeyRows.at(i).color) {
                    hotkeyRows.at(i).color->setProperty(
                        "penColor", fresh.at(i).color.name(QColor::HexRgb));
                    hotkeyRows.at(i).color->setStyleSheet(
                        QStringLiteral(
                            "background: %1; border: 1px solid #888; border-radius: 6px;")
                            .arg(fresh.at(i).color.name(QColor::HexRgb)));
                  }
                }
                publishKeys();
              });
      cardShortcuts->addBodyWidget(makePropertyRow(
          cardShortcuts, QStringLiteral("Werkzeug-Kürzel"), btnHotkeyReset, false,
          QStringLiteral("standard zurücksetzen hotkey")));

      auto *modelEdit = new QLineEdit(openRouterModel(), cardShortcuts);
      modelEdit->setPlaceholderText(QStringLiteral("openai/gpt-4o-mini"));
      auto *keyEdit = new QLineEdit(openRouterKey(), cardShortcuts);
      keyEdit->setEchoMode(QLineEdit::Password);
      keyEdit->setPlaceholderText(QStringLiteral("OpenRouter-Schlüssel"));
      auto saveRouter = [modelEdit, keyEdit, publishKeys]() {
        storeOpenRouter(modelEdit->text(), keyEdit->text());
        publishKeys();
      };
      connect(modelEdit, &QLineEdit::editingFinished, this, saveRouter);
      connect(keyEdit, &QLineEdit::editingFinished, this, saveRouter);
      cardShortcuts->addBodyWidget(makePropertyRow(
          cardShortcuts, QStringLiteral("OpenRouter-Modell"), modelEdit, false,
          QStringLiteral("openrouter modell ki")));
      cardShortcuts->addBodyWidget(makePropertyRow(
          cardShortcuts, QStringLiteral("OpenRouter-Schlüssel"), keyEdit, false,
          QStringLiteral("openrouter schlüssel key api")));

      struct Row {
        const char *keys;
        const char *action;
      };
      const Row rows[] = {
          {"Ctrl+K", "Bibliothek / Sidebar-Suche fokussieren"},
          {"Entf / Backspace", "Auswahl in den Papierkorb (Bibliothek)"},
          {"Ctrl+A", "Alle Notizen in der Bibliothek auswählen"},
          {"Ctrl+Z", "Im Papierkorb: Auswahl wiederherstellen"},
          {"Ctrl+0 / Ctrl+9", "An Inhalt / an Breite anpassen"},
          {"Ctrl+Shift+O", "Werkzeug-Eigenschaften öffnen"},
          {"Ctrl+Shift+J", "Werkzeugleiste K ↔ J umschalten"},
      };
      const int n = int(sizeof(rows) / sizeof(rows[0]));
      for (int i = 0; i < n; ++i) {
        auto *actLbl =
            new QLabel(QString::fromUtf8(rows[i].action), cardShortcuts);
        actLbl->setWordWrap(true);
        setLiteralQss(actLbl,
                      QStringLiteral(
                          "color: %1; font-size: 12px; background: transparent;")
                          .arg(settingsInkMuted()));
        cardShortcuts->addBodyWidget(makePropertyRow(
            cardShortcuts, QString::fromUtf8(rows[i].keys), actLbl, i + 1 == n,
            QStringLiteral("tastatur shortcut %1")
                .arg(QString::fromUtf8(rows[i].keys).toLower())));
      }
      auto *foot = new QLabel(
          QStringLiteral(
              "Standard: Strg+1, Strg+2 und Strg+3 sind drei Stifte "
              "(Farbe und Stärke daneben), Strg+4 der Textmarker. "
              "P, E, V, T, H und M bleiben. Strg+9 passt an die Breite an. "
              "Das gilt in der offenen Notiz, nicht beim Tippen. "
              "Ein leeres Feld schaltet das Kürzel aus. "
              "Die Datei assistent-einstellungen.json geht mit dem Google-Konto mit."),
          cardShortcuts);
      foot->setWordWrap(true);
      setLiteralQss(foot,
                    QStringLiteral(
                        "color: %1; font-size: 11px; background: transparent;"
                        "padding: 4px 0 0 0;")
                        .arg(settingsInkMuted()));
      cardShortcuts->addBodyWidget(foot);
    }
#endif

    // ----- Card: Profile (UI modes) ------------------------------------
    auto *cardBehavior = new BlopSettingsCard(
        QStringLiteral("Profile"),
        QStringLiteral("UI-Profile und Modi"),
        contentWidget);
    cardBehavior->setSectionKeywords(
        QStringLiteral("verhalten profil mode modus ui-profil bearbeiten"));
    {
        m_profileList = new QListWidget(cardBehavior);
        setLiteralQss(m_profileList, QStringLiteral(
            "QListWidget {"
            "  background: transparent;"
            "  border: none;"
            "  color: #334155;"
            "  padding: 0;"
            "  outline: none;"
            "}"
            "QListWidget::item {"
            "  background: #FFFFFF;"
            "  border: 1px solid #E2E8F0;"
            "  border-radius: 8px;"
            "  margin: 0 0 8px 0;"
            "  padding: 0 14px;"
            "  min-height: 44px;"
            "  color: #334155;"
            "}"
            "QListWidget::item:selected {"
            "  background: #EFF6FF;"
            "  border: 1.5px solid #2563EB;"
            "  color: #1E3A8A;"
            "  font-weight: 600;"
            "}"
            "QListWidget::item:hover:!selected {"
            "  background: #F8FAFC;"
            "  border: 1px solid #CBD5E1;"
            "}"));
        m_profileList->setMinimumHeight(UiScale::dp(100));
        m_profileList->setMaximumHeight(UiScale::dp(120));
        m_profileList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        m_profileList->setSpacing(0);
        m_profileList->setFrameShape(QFrame::NoFrame);
        m_profileList->setContextMenuPolicy(Qt::CustomContextMenu);
        BlopScroll::enableFingerScroll(m_profileList);
        if (phoneUi)
            BlopScroll::makeListFitContents(m_profileList);
        connect(m_profileList, &QListWidget::customContextMenuRequested, this,
                &SettingsDialog::onProfileContextMenu);
        connect(m_profileList, &QListWidget::itemClicked, this,
                &SettingsDialog::onProfileClicked);
        cardBehavior->addBodyWidget(m_profileList);

        auto *btnNewProfile =
            makeQuietAction(cardBehavior, QStringLiteral("Neu →"));
        connect(btnNewProfile, &QPushButton::clicked, this,
                &SettingsDialog::onCreateProfile);
        cardBehavior->addBodyWidget(makePropertyRow(
            cardBehavior, QStringLiteral("Modus erstellen"), btnNewProfile,
            false, QStringLiteral("profil neu erstellen mode")));

        auto *btnEditProfile =
            makeQuietAction(cardBehavior, QStringLiteral("Bearbeiten →"));
        connect(btnEditProfile, &QPushButton::clicked, this, [this]() {
            openEditor(m_profileManager ? m_profileManager->currentProfile().id
                                        : QString());
        });
        cardBehavior->addBodyWidget(makePropertyRow(
            cardBehavior, QStringLiteral("Aktuelles Profil"), btnEditProfile,
            true, QStringLiteral("profil bearbeiten edit mode")));
    }

    // ----- Card: Speicher — Notion property rows for clouds ---------------
    auto *cardStorage = new BlopSettingsCard(
        QStringLiteral("Cloud"),
        QStringLiteral("Speicherort und verbundene Clouds"),
        contentWidget);
    cardStorage->setSectionKeywords(
        QStringLiteral("drive nextcloud cloud lokal sync ordner speicher "
                       "google library bibliothek"));
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
        hint->hide(); // kept for applyMode text updates; UI uses short path row

        auto *modeSeg = new QWidget(cardStorage);
        styleSegmentTrack(modeSeg);
        tagSearchKeys(modeSeg,
                      QStringLiteral("lokal cloud speicher modus sync"));
        auto *modeLay = new QHBoxLayout(modeSeg);
        modeLay->setContentsMargins(UiScale::dp(3), UiScale::dp(3),
                                    UiScale::dp(3), UiScale::dp(3));
        modeLay->setSpacing(UiScale::dp(2));

        const QString segStyle = segmentedControlQss();
        const int segH = settingsSegmentMinHeight();
        auto *btnLocal = new QPushButton(QStringLiteral("Lokal"), modeSeg);
        auto *btnCloud = new QPushButton(QStringLiteral("Cloud"), modeSeg);
        auto *btnBoth = new QPushButton(QStringLiteral("Beides"), modeSeg);
        for (QPushButton *b : {btnLocal, btnCloud, btnBoth}) {
            b->setCheckable(true);
            b->setCursor(Qt::PointingHandCursor);
            b->setFixedHeight(segH);
            b->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
            setThemedQss(b, segStyle);
            BlopRipple::attachPressFeedback(b, 0.92);
            modeLay->addWidget(b, 0);
        }
        const auto curMode = StoragePrefs::mode();
        btnLocal->setChecked(curMode == StoragePrefs::Mode::LocalOnly);
        btnCloud->setChecked(curMode == StoragePrefs::Mode::CloudOnly);
        btnBoth->setChecked(curMode == StoragePrefs::Mode::LocalAndCloud);
        cardStorage->addBodyWidget(makePropertyRow(
            cardStorage, QStringLiteral("Modus"), modeSeg, false,
            QStringLiteral("lokal cloud speicher modus sync")));

        const QString localPath = StoragePrefs::ensureLocalLibraryRoot();
        auto *localPathLbl = new QLabel(localPath, cardStorage);
        localPathLbl->setWordWrap(false);
        localPathLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
        localPathLbl->setToolTip(localPath);
        setLiteralQss(localPathLbl, QStringLiteral(
            "color: %1; font-size: 11px;"
            "background: transparent;")
            .arg(settingsInkMuted()));
        {
          QFontMetrics fm(localPathLbl->font());
          localPathLbl->setText(
              fm.elidedText(localPath, Qt::ElideMiddle, UiScale::dp(280)));
        }
        cardStorage->addBodyWidget(makePropertyRow(
            cardStorage, QStringLiteral("Lokal"), localPathLbl, false,
            QStringLiteral("lokal pfad library")));

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
                BlopDialogs::notify(
                    this, displayName,
                    QStringLiteral(
                        "Auf dem Handy speichert Blop Notizen lokal auf dem "
                        "Gerät.\n\nGoogle Drive und andere Clouds öffnest du "
                        "über „Öffnen“ im System-Browser "
                        "(Cookies & Passwörter) — nicht über einen "
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

        // Ordered Notion list: one row per cloud (Drive first).
        struct CloudRowRef {
            QString id;
            QString name;
            QLabel *statusLbl{nullptr};
            QPushButton *connectBtn{nullptr};
            QPushButton *primaryBtn{nullptr};
            QPushButton *folderBtn{nullptr};
            QPushButton *disconnectBtn{nullptr};
        };
        auto cloudRefs = std::make_shared<QVector<CloudRowRef>>();

        auto refreshCloudUi = [cloudRefs]() {
            const QString primary = StoragePrefs::primaryCloudId();
            for (CloudRowRef &r : *cloudRefs) {
                bool webOk = false;
                bool api = false;
                bool folder = false;
                QVector<CloudStorageEntry> rows = CloudStorageStore::load();
                if (CloudStorageEntry *cur =
                        CloudStorageStore::findMutable(rows, r.id)) {
                    webOk = cur->webConnected;
                    api = cur->apiConnected;
                    folder = StoragePrefs::isUsableFilesystemDir(cur->path);
                }
                const bool linked = api || folder || webOk ||
                                    StoragePrefs::isProviderLinked(r.id);
                const QString st = api ? QStringLiteral("Per API verbunden")
                                    : folder ? QStringLiteral("Ordner verknüpft")
                                    : webOk ? QStringLiteral("Im Web angemeldet")
                                            : QStringLiteral("Nicht verbunden");
                if (r.statusLbl)
                    r.statusLbl->setText(st);
                if (r.connectBtn)
                    r.connectBtn->setVisible(!linked);
                if (r.primaryBtn) {
                    r.primaryBtn->setVisible(linked);
                    r.primaryBtn->setEnabled(api || folder);
                    r.primaryBtn->setText(primary == r.id
                                             ? QStringLiteral("Als Primär ✓")
                                             : QStringLiteral("Als Primär"));
                }
                if (r.folderBtn)
                    r.folderBtn->setVisible(linked);
                if (r.disconnectBtn)
                    r.disconnectBtn->setVisible(linked);
            }
        };

        auto addCloudRow = [&](const CloudStorageEntry &e, bool last) {
            auto *actions = new QWidget(cardStorage);
            auto *btnConnect =
                makeQuietAction(actions, QStringLiteral("Verbinden →"));
            btnConnect->setFixedHeight(UiScale::dp(32));
            btnConnect->setMinimumWidth(UiScale::dp(110));
            setLiteralQss(btnConnect, QStringLiteral(
                "QPushButton {"
                "  color: #334155; background: #FFFFFF;"
                "  border: 1px solid #CBD5E1; border-radius: 6px;"
                "  padding: 0 14px; font-size: 12px; font-weight: 600;"
                "}"
                "QPushButton:hover { border-color: #94A3B8; background: #F8FAFC; }"));
            auto *btnPrimary =
                makeQuietAction(actions, QStringLiteral("Als Primär"));
            auto *btnFolder =
                makeQuietAction(actions, QStringLiteral("Ordner"));
            auto *btnDisconnect =
                makeQuietAction(actions, QStringLiteral("Trennen"), true);

            if (phoneUi) {
                auto *al = new QVBoxLayout(actions);
                al->setContentsMargins(0, 0, 0, 0);
                al->setSpacing(UiScale::dp(4));
                for (QPushButton *b :
                     {btnConnect, btnPrimary, btnFolder, btnDisconnect}) {
                    b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
                    b->setMinimumHeight(
                        UiScale::dp(BlopStyle::touchTargetMinDp()));
                    al->addWidget(b);
                }
            } else {
                auto *al = new QHBoxLayout(actions);
                al->setContentsMargins(0, 0, 0, 0);
                al->setSpacing(UiScale::dp(4));
                al->addWidget(btnConnect);
                al->addWidget(btnPrimary);
                al->addWidget(btnFolder);
                al->addWidget(btnDisconnect);
            }

            const bool linked = StoragePrefs::isProviderLinked(e.id) ||
                                e.apiConnected || e.webConnected;
            auto *row = makeNamedPropertyRow(
                cardStorage, e.name,
                linked ? QStringLiteral("Verbunden")
                       : QStringLiteral("Nicht verbunden"),
                actions, last);
            QLabel *statusLbl = row->findChild<QLabel *>(
                QStringLiteral("PropStatus"));

            CloudRowRef ref;
            ref.id = e.id;
            ref.name = e.name;
            ref.statusLbl = statusLbl;
            ref.connectBtn = btnConnect;
            ref.primaryBtn = btnPrimary;
            ref.folderBtn = btnFolder;
            ref.disconnectBtn = btnDisconnect;
            cloudRefs->append(ref);

            const QString id = e.id;
            const QString displayName = e.name;
            QObject::connect(btnConnect, &QPushButton::clicked, this,
                             [this, id]() {
                                 CloudLinkHub::instance().connectProvider(id,
                                                                          this);
                             });
            QObject::connect(btnFolder, &QPushButton::clicked, this,
                             [this, id, displayName, connectCloudProvider,
                              refreshCloudUi]() {
                                 if (connectCloudProvider(id, displayName,
                                                          /*forceManual=*/true)
                                         .isEmpty())
                                     return;
                                 refreshCloudUi();
                                 emit storagePrefsChanged();
                             });
            QObject::connect(btnPrimary, &QPushButton::clicked, this,
                             [this, id, refreshCloudUi]() {
                                 StoragePrefs::setPrimaryCloudId(id);
                                 refreshCloudUi();
                                 emit storagePrefsChanged();
                             });
            QObject::connect(btnDisconnect, &QPushButton::clicked, this,
                             [this, id, refreshCloudUi]() {
                                 QVector<CloudStorageEntry> entries =
                                     CloudStorageStore::load();
                                 if (CloudStorageEntry *cur =
                                         CloudStorageStore::findMutable(entries,
                                                                        id)) {
                                     cur->path.clear();
                                     cur->apiConnected = false;
                                     cur->webConnected = false;
                                     CloudStorageStore::save(entries);
                                 }
                                 if (StoragePrefs::primaryCloudId() == id)
                                     StoragePrefs::setPrimaryCloudId(QString());
                                 refreshCloudUi();
                                 emit storagePrefsChanged();
                             });

            cardStorage->addBodyWidget(row);
        };

        // Canonical provider order matching the library sidebar.
        {
            QVector<CloudStorageEntry> entries = CloudStorageStore::load();
            const char *wantIds[] = {"googledrive", "dropbox", "onedrive",
                                     "nextcloud", "icloud"};
            QVector<CloudStorageEntry> ordered;
            auto takeOrMake = [&](const QString &id, const QString &name) {
                for (const CloudStorageEntry &e : entries) {
                    if (e.id.compare(id, Qt::CaseInsensitive) == 0 ||
                        e.type.compare(id, Qt::CaseInsensitive) == 0) {
                        CloudStorageEntry copy = e;
                        copy.id = id;
                        copy.type = id;
                        copy.name = name;
                        ordered.append(copy);
                        return;
                    }
                }
                CloudStorageEntry e;
                e.id = id;
                e.type = id;
                e.name = name;
                e.webUrl = CloudStorageStore::defaultWebUrl(id);
                ordered.append(e);
            };
            takeOrMake(QStringLiteral("googledrive"),
                       QStringLiteral("Google Drive"));
            takeOrMake(QStringLiteral("dropbox"), QStringLiteral("Dropbox"));
            takeOrMake(QStringLiteral("onedrive"), QStringLiteral("OneDrive"));
            takeOrMake(QStringLiteral("nextcloud"), QStringLiteral("Nextcloud"));
            takeOrMake(QStringLiteral("icloud"), QStringLiteral("iCloud"));
            Q_UNUSED(wantIds);

            for (int i = 0; i < ordered.size(); ++i)
                addCloudRow(ordered[i], /*last=*/false);
            refreshCloudUi();
            connect(&CloudLinkHub::instance(), &CloudLinkHub::connectFinished,
                    cardStorage,
                    [this, refreshCloudUi](const QString &, bool ok,
                                           const QString &detail) {
                        refreshCloudUi();
                        if (!ok) {
                            if (!detail.isEmpty())
                                BlopDialogs::notify(this, QStringLiteral("Cloud"),
                                                    detail);
                            return;
                        }
                        emit storagePrefsChanged();
                    });
        }

        auto applyMode = [this, btnLocal, btnCloud, btnBoth, hint,
                          localPathLbl](StoragePrefs::Mode m) {
            btnLocal->setChecked(m == StoragePrefs::Mode::LocalOnly);
            btnCloud->setChecked(m == StoragePrefs::Mode::CloudOnly);
            btnBoth->setChecked(m == StoragePrefs::Mode::LocalAndCloud);
            StoragePrefs::setMode(m);
            hint->setText(StoragePrefs::modeHint(m));
            if (localPathLbl) {
              const QString p = StoragePrefs::ensureLocalLibraryRoot();
              QFontMetrics fm(localPathLbl->font());
              localPathLbl->setToolTip(p);
              localPathLbl->setText(
                  fm.elidedText(p, Qt::ElideMiddle, UiScale::dp(280)));
            }
            emit storagePrefsChanged();
        };
        QObject::connect(btnLocal, &QPushButton::clicked, this,
                         [applyMode]() {
                             applyMode(StoragePrefs::Mode::LocalOnly);
                         });
        QObject::connect(btnCloud, &QPushButton::clicked, this,
                         [applyMode]() {
                             applyMode(StoragePrefs::Mode::CloudOnly);
                         });
        QObject::connect(btnBoth, &QPushButton::clicked, this,
                         [applyMode]() {
                             applyMode(StoragePrefs::Mode::LocalAndCloud);
                         });

        // Custom WebDAV / own cloud — one compact row.
        auto *customUrl = new QLineEdit(cardStorage);
        customUrl->setPlaceholderText(QStringLiteral("https://…"));
        customUrl->setFixedHeight(UiScale::dp(32));
        customUrl->setMinimumWidth(UiScale::dp(220));
        customUrl->setMaximumWidth(UiScale::dp(280));
#ifndef Q_OS_ANDROID
        setLiteralQss(customUrl, QStringLiteral(
            "QLineEdit {"
            "  background: #FFFFFF; color: #0F172A;"
            "  border: 1px solid #CBD5E1; border-radius: 6px;"
            "  padding: 0 10px; font-size: 12px;"
            "}"
            "QLineEdit:focus { border-color: #2563EB; }"));
#else
        setTokenQss(customUrl, "input");
#endif
        auto *btnEmbed =
            makeQuietAction(cardStorage, QStringLiteral("Verbinden →"));
        connect(btnEmbed, &QPushButton::clicked, this,
                [this, customUrl, requestCloud]() {
                  const QString typed = customUrl->text().trimmed();
                  if (typed.isEmpty())
                    return;
                  CloudStorageEntry e;
                  e.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
                  e.type = QStringLiteral("custom");
                  e.name = QStringLiteral("Eigene Cloud (WebDAV)");
                  e.webUrl = QUrl::fromUserInput(typed).toString();
                  CloudStorageStore::upsert(e);
                  requestCloud(e);
                });

        auto *customActions = new QWidget(cardStorage);
        auto *customLay = new QHBoxLayout(customActions);
        customLay->setContentsMargins(0, 0, 0, 0);
        customLay->setSpacing(UiScale::dp(6));
        customLay->addWidget(customUrl, 0);
        customLay->addWidget(btnEmbed, 0, Qt::AlignVCenter);
        customLay->addStretch(1);
        cardStorage->addBodyWidget(makePropertyRow(
            cardStorage, QStringLiteral("Eigene Cloud (WebDAV)"), customActions,
            true,
            QStringLiteral("eigene cloud custom url verbinden webdav")));
    }

    // ----- Card: Kalender ------------------------------------------------
    auto *cardMore = new BlopSettingsCard(
        QStringLiteral("Kalender"),
        QStringLiteral("Google Kalender verbinden"),
        contentWidget);
    cardMore->setSectionKeywords(
        QStringLiteral("kalender calendar google termine sync"));
    {
      auto *calStatusLbl = new QLabel(cardMore);
      calStatusLbl->hide(); // status text mirrored into named row
      auto refreshCalStatusText = []() -> QString {
        const bool on = CalendarService::instance().hasGoogleAccess();
        return on ? QStringLiteral(
                        "<span style='color:#10B981;'>●</span> Verbunden")
                  : QStringLiteral(
                        "<span style='color:#94A3B8;'>●</span> Nicht verbunden");
      };
      calStatusLbl->setTextFormat(Qt::RichText);
      calStatusLbl->setText(refreshCalStatusText());

      auto *btnCalConnect =
          makeQuietAction(cardMore, QStringLiteral("Verbinden"));
      auto *btnCalSync = makeQuietAction(cardMore, QStringLiteral("Sync"));
      auto *btnCalDisconnect =
          makeQuietAction(cardMore, QStringLiteral("Trennen"), true);
      auto updateCalButtons = [btnCalConnect, btnCalSync, btnCalDisconnect]() {
        const bool on = CalendarService::instance().hasGoogleAccess();
        btnCalConnect->setVisible(!on);
        btnCalSync->setVisible(on);
        btnCalDisconnect->setVisible(on);
      };
      updateCalButtons();
      connect(btnCalConnect, &QPushButton::clicked, this, []() {
        CalendarService::instance().connectGoogle();
      });
      connect(btnCalSync, &QPushButton::clicked, this, []() {
        CalendarService::instance().refreshGoogle();
      });
      connect(btnCalDisconnect, &QPushButton::clicked, this,
              [updateCalButtons, calStatusLbl, refreshCalStatusText]() {
                CalendarService::instance().disconnectGoogle();
                calStatusLbl->setText(refreshCalStatusText());
                updateCalButtons();
              });

      auto *calBtns = new QWidget(cardMore);
      auto *calLay = new QHBoxLayout(calBtns);
      calLay->setContentsMargins(0, 0, 0, 0);
      calLay->setSpacing(UiScale::dp(4));
      calLay->addWidget(btnCalConnect);
      calLay->addWidget(btnCalSync);
      calLay->addWidget(btnCalDisconnect);
      if (phoneUi) {
        for (QPushButton *b : {btnCalConnect, btnCalSync, btnCalDisconnect}) {
          b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
          b->setMinimumHeight(UiScale::dp(BlopStyle::touchTargetMinDp()));
        }
      }

      auto *calRow = makeNamedPropertyRow(
          cardMore, QStringLiteral("Google Kalender"),
          refreshCalStatusText(), calBtns, true,
          QStringLiteral("kalender calendar google termine sync"));
      cardMore->addBodyWidget(calRow);
      auto *calStatusInRow =
          calRow->findChild<QLabel *>(QStringLiteral("PropStatus"));
      if (calStatusInRow) {
        calStatusInRow->setTextFormat(Qt::RichText);
        calStatusInRow->setText(refreshCalStatusText());
      }
      auto syncCalStatusUi = [calStatusInRow, calStatusLbl, refreshCalStatusText,
                              updateCalButtons]() {
        const QString t = refreshCalStatusText();
        calStatusLbl->setText(t);
        if (calStatusInRow)
          calStatusInRow->setText(t);
        updateCalButtons();
      };
      connect(&CalendarService::instance(), &CalendarService::eventsChanged,
              cardMore, syncCalStatusUi);
      connect(&GoogleAuthManager::instance(),
              &GoogleAuthManager::calendarTokenUpdated, cardMore,
              syncCalStatusUi);
    }
    cardMore->setExpanded(true);

    // ----- Card: System --------------------------------------------------
    auto *cardSystem = new BlopSettingsCard(
        QStringLiteral("System"),
        QStringLiteral("Version, Build und Diagnose"),
        contentWidget);
    cardSystem->setSectionKeywords(
        QStringLiteral("version diagnose trace debug entwickler pdf webengine "
                       "build onboarding einrichtung assistent"));
    {
      auto *btnOnboarding =
          makeQuietAction(cardSystem, QStringLiteral("Erneut zeigen"));
      btnOnboarding->setToolTip(QStringLiteral(
          "Willkommen, Speicherwahl und Kurzrundgang noch einmal. "
          "Offene Notizen werden vorher gespeichert und geschlossen."));
      connect(btnOnboarding, &QPushButton::clicked, this, [this]() {
        QSettings s(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
        s.setValue(QStringLiteral("ui/onboardingDone"), false);
        emit onboardingReplayRequested();
      });
      cardSystem->addBodyWidget(makePropertyRow(
          cardSystem, QStringLiteral("Einrichtung"), btnOnboarding, false,
          QStringLiteral("onboarding einrichtung assistent erneut wizard")));

      const QString version = QString(BLOP_VERSION_STR);
      const QString versionLabel =
          (version.startsWith(QLatin1Char('v')) ? QStringLiteral("Blop ")
                                                : QStringLiteral("Blop v")) +
          version;
      auto *info = new QLabel(versionLabel, cardSystem);
      setLiteralQss(info, QStringLiteral(
          "color: %1; font-size: 12px; font-weight: 500;"
          "background: transparent;")
          .arg(settingsInkMuted()));
      cardSystem->addBodyWidget(makePropertyRow(
          cardSystem, QStringLiteral("Version"), info, false,
          QStringLiteral("version about über blop")));

#ifdef BLOP_HAS_PDF
      {
        auto *lbl = new QLabel(QStringLiteral("Verfügbar"), cardSystem);
        setLiteralQss(lbl, QStringLiteral(
            "color: %1; font-size: 12px; font-weight: 500;"
            "background: transparent;")
            .arg(settingsInkMuted()));
        cardSystem->addBodyWidget(makePropertyRow(
            cardSystem, QStringLiteral("PDF Import/Export"), lbl, false,
            QStringLiteral("pdf export import")));
      }
#endif
#ifdef BLOP_HAS_WEBENGINE
      {
        auto *lbl = new QLabel(QStringLiteral("Verfügbar"), cardSystem);
        setLiteralQss(lbl, QStringLiteral(
            "color: %1; font-size: 12px; font-weight: 500;"
            "background: transparent;")
            .arg(settingsInkMuted()));
        cardSystem->addBodyWidget(makePropertyRow(
            cardSystem, QStringLiteral("Eingebettetes Study (WebEngine)"), lbl,
            false, QStringLiteral("webengine study browser")));
      }
#endif

      {
        // Opt-in crash upload (enforced in blopInitCrashReporting; applies
        // after the next start). Default: off.
        auto *btnCrash = makeQuietAction(cardSystem, QStringLiteral("Aus"));
        btnCrash->setCheckable(true);
        btnCrash->setChecked(
            QSettings(QStringLiteral("Blop"), QStringLiteral("BlopApp"))
                .value(QStringLiteral("privacy/crashUpload"), false)
                .toBool());
        btnCrash->setText(btnCrash->isChecked() ? QStringLiteral("An")
                                                : QStringLiteral("Aus"));
        btnCrash->setToolTip(QStringLiteral(
            "Anonyme Absturzberichte an die Entwickler senden "
            "(gilt ab dem nächsten Start)"));
        connect(btnCrash, &QPushButton::toggled, this, [btnCrash](bool on) {
          QSettings(QStringLiteral("Blop"), QStringLiteral("BlopApp"))
              .setValue(QStringLiteral("privacy/crashUpload"), on);
          btnCrash->setText(on ? QStringLiteral("An") : QStringLiteral("Aus"));
        });
        cardSystem->addBodyWidget(makePropertyRow(
            cardSystem, QStringLiteral("Absturzberichte senden"), btnCrash,
            false,
            QStringLiteral("absturz crash bericht datenschutz sentry")));
      }

      auto *btnTrace = makeQuietAction(cardSystem, QStringLiteral("Aus"));
      btnTrace->setCheckable(true);
      btnTrace->setChecked(BlopDiag::sessionTraceActive());
      btnTrace->setText(btnTrace->isChecked() ? QStringLiteral("An")
                                              : QStringLiteral("Aus"));
      btnTrace->setToolTip(QStringLiteral(
          "Session-Trace nur für lokale Agent-/QA-Sessions"));
      connect(btnTrace, &QPushButton::toggled, this, [btnTrace](bool on) {
        BlopDiag::setSessionTraceEnabled(on);
        btnTrace->setText(on ? QStringLiteral("An") : QStringLiteral("Aus"));
      });
      cardSystem->addBodyWidget(makePropertyRow(
          cardSystem, QStringLiteral("Session-Trace"), btnTrace,
#ifndef Q_OS_ANDROID
          false,
#else
          true,
#endif
          QStringLiteral("trace diagnose debug entwickler")));

#ifndef Q_OS_ANDROID
      auto *btnTbDebug = makeQuietAction(cardSystem, QStringLiteral("Aus"));
      btnTbDebug->setCheckable(true);
      {
        QSettings s(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
        btnTbDebug->setChecked(
            s.value(QStringLiteral("diag/toolbarDebug"), false).toBool() ||
            qEnvironmentVariableIsSet("BLOP_TOOLBAR_DEBUG"));
      }
      btnTbDebug->setText(btnTbDebug->isChecked() ? QStringLiteral("An")
                                                  : QStringLiteral("Aus"));
      btnTbDebug->setToolTip(
          QStringLiteral("Toolbar-Debug-Palette in der Notiz"));
      connect(btnTbDebug, &QPushButton::toggled, this, [btnTbDebug](bool on) {
        QSettings s(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
        s.setValue(QStringLiteral("diag/toolbarDebug"), on);
        btnTbDebug->setText(on ? QStringLiteral("An") : QStringLiteral("Aus"));
      });
      cardSystem->addBodyWidget(makePropertyRow(
          cardSystem, QStringLiteral("Toolbar-Debug"), btnTbDebug, true,
          QStringLiteral("toolbar debug entwickler")));
#endif
    }
    cardSystem->setExpanded(true);

    const QList<BlopSettingsCard *> allCards = {
        cardKonto, cardTheme, cardLook,
#ifndef Q_OS_ANDROID
        cardShortcuts,
#endif
        cardBehavior, cardStorage, cardMore, cardSystem};

#ifndef Q_OS_ANDROID
    if (!phoneUi) {
        // --- Full window: Obsidian left nav (Hauptmenü-Stil) + paper pages ---
        root->removeWidget(hero);
        root->removeWidget(searchRow);
        root->removeWidget(scroll);
        scroll->hide();
        cardsHost->hide();

        auto *split = new QWidget(tabDesign);
        split->setObjectName(QStringLiteral("SettingsNotionSplit"));
        m_shellSplit = split;
        split->setAttribute(Qt::WA_StyledBackground, true);
        split->setStyleSheet(QStringLiteral(
            "QWidget#SettingsNotionSplit { background: %1; border: none; }")
                                 .arg(BlopStyle::obsidianDesk().name(
                                     QColor::HexRgb)));
        auto *splitLay = new QHBoxLayout(split);
        splitLay->setContentsMargins(0, 0, 0, 0);
        splitLay->setSpacing(0);

        const QColor navBg = BlopStyle::obsidianNav();
        const QColor navInk = BlopStyle::obsidianText();
        const QColor navMuted = QColor(0xB8, 0xBC, 0xC4);
        auto *navCol = new QWidget(split);
        navCol->setObjectName(QStringLiteral("SettingsNavCol"));
        m_navCol = navCol;
        navCol->setFixedWidth(UiScale::dp(220));
        navCol->setAttribute(Qt::WA_StyledBackground, true);
        navCol->setAutoFillBackground(true);
        {
            QPalette np = navCol->palette();
            np.setColor(QPalette::Window, navBg);
            np.setColor(QPalette::Base, navBg);
            np.setColor(QPalette::Text, navInk);
            navCol->setPalette(np);
        }
        navCol->setStyleSheet(QStringLiteral(
            "QWidget#SettingsNavCol {"
            "  background: %1;"
            "  border-right: 1px solid rgba(255,255,255,0.10);"
            "}")
                                 .arg(navBg.name(QColor::HexRgb)));
        auto *navLay = new QVBoxLayout(navCol);
        navLay->setContentsMargins(UiScale::dp(BlopStyle::controlGapDp()),
                                   UiScale::dp(BlopStyle::edgeInsetDp()),
                                   UiScale::dp(BlopStyle::controlGapDp()),
                                   UiScale::dp(BlopStyle::sectionGapDp()));
        navLay->setSpacing(UiScale::dp(4));

        auto *navTitle = new QLabel(QStringLiteral("EINSTELLUNGEN"), navCol);
        navTitle->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 11px; font-weight: 700;"
            "letter-spacing: 0.6px; background: transparent;"
            "padding: 2px 8px 10px 8px;")
                                    .arg(navMuted.name(QColor::HexRgb)));
        navLay->addWidget(navTitle, 0);

        auto *nav = new QListWidget(navCol);
        nav->setObjectName(QStringLiteral("SettingsNavList"));
        m_sectionNav = nav;
        nav->setFocusPolicy(Qt::NoFocus);
        nav->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        nav->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        // Same language as library sidebar rows (quiet select + accent).
        const QColor acc = BlopTheme::accentPrimary();
        nav->setStyleSheet(QStringLiteral(
            "QListWidget#SettingsNavList {"
            "  background: transparent; border: none; outline: none;"
            "  color: %1; font-size: 13px; font-weight: 500;"
            "}"
            "QListWidget#SettingsNavList::item {"
            "  padding: 10px 10px; margin: 1px 0;"
            "  border: none; border-radius: 8px; min-height: %2px;"
            "  color: %1;"
            "}"
            "QListWidget#SettingsNavList::item:selected {"
            "  background: rgba(%4,%5,%6,0.22);"
            "  color: %3;"
            "  font-weight: 600;"
            "  border: 1px solid rgba(%4,%5,%6,0.55);"
            "}"
            "QListWidget#SettingsNavList::item:hover:!selected {"
            "  background: rgba(255,255,255,0.06);"
            "}")
            .arg(navMuted.name(QColor::HexRgb),
                 QString::number(UiScale::dp(BlopStyle::touchTargetMinDp())),
                 navInk.name(QColor::HexRgb))
            .arg(acc.red())
            .arg(acc.green())
            .arg(acc.blue()));
        BlopScroll::enableFingerScroll(nav);
        navLay->addWidget(nav, 1);

        // Profile chip at bottom of nav — quiet, like sidebar account.
        hero->setParent(navCol);
        hero->setAttribute(Qt::WA_StyledBackground, true);
        hero->setCursor(Qt::PointingHandCursor);
        hero->setStyleSheet(QStringLiteral(
            "#SettingsHero {"
            "  background: rgba(255,255,255,0.06);"
            "  border: 1px solid rgba(255,255,255,0.08);"
            "  border-radius: 8px;"
            "}"));
        if (auto *hl = qobject_cast<QHBoxLayout *>(hero->layout())) {
            hl->setContentsMargins(UiScale::dp(8), UiScale::dp(8),
                                   UiScale::dp(6), UiScale::dp(8));
            hl->setSpacing(UiScale::dp(8));
        }
        if (auto *name = hero->findChild<QLabel *>(
                QStringLiteral("SettingsHeroName"))) {
            name->setWordWrap(false);
            name->setStyleSheet(QStringLiteral(
                "color: %1; font-size: 13px; font-weight: 600;"
                "background: transparent;")
                                    .arg(navInk.name(QColor::HexRgb)));
            QFontMetrics fm(name->font());
            name->setText(fm.elidedText(name->text(), Qt::ElideRight,
                                        UiScale::dp(110)));
        }
        if (auto *sub = hero->findChild<QLabel *>(
                QStringLiteral("SettingsHeroSub"))) {
            sub->setWordWrap(false);
            sub->setText(studyLoggedIn ? QStringLiteral("Angemeldet")
                                       : QStringLiteral("Gast"));
            setLiteralQss(sub, QStringLiteral(
                "color: %1; font-size: 11px; background: transparent;")
                .arg(navMuted.name(QColor::HexRgb)));
        }
        navLay->addWidget(hero, 0);

        auto *contentCol = new QWidget(split);
        contentCol->setObjectName(QStringLiteral("SettingsContentCol"));
        m_contentCol = contentCol;
        paintSettingsContentSurface(contentCol,
                                    QStringLiteral("SettingsContentCol"));
        contentCol->setStyleSheet(QStringLiteral(
            "QWidget#SettingsContentCol {"
            "  background: %1;"
            "  border: none;"
            "}")
                                      .arg(settingsContentBg().name(
                                          QColor::HexRgb)));
        auto *contentColLay = new QVBoxLayout(contentCol);
        contentColLay->setContentsMargins(UiScale::dp(32), UiScale::dp(24),
                                          UiScale::dp(32), UiScale::dp(24));
        contentColLay->setSpacing(UiScale::dp(16));

        // Toolbar: quiet search + Fertig (Notion corner close).
        searchRow->setParent(contentCol);
        searchRow->setMaximumWidth(UiScale::dp(780));
        searchRow->setAttribute(Qt::WA_StyledBackground, true);
        searchRow->setStyleSheet(QStringLiteral("background: %1;")
                                     .arg(settingsContentBg().name(
                                         QColor::HexRgb)));
        searchLay->setContentsMargins(UiScale::dp(BlopStyle::edgeInsetDp()),
                                      UiScale::dp(BlopStyle::sectionGapDp()),
                                      UiScale::dp(BlopStyle::edgeInsetDp()),
                                      UiScale::dp(BlopStyle::controlGapDp()));
        searchLay->setSpacing(UiScale::dp(BlopStyle::controlGapDp()));
        if (search) {
            search->setPlaceholderText(QStringLiteral("Suchen…"));
            search->setMinimumWidth(0);
            search->setMaximumWidth(QWIDGETSIZE_MAX);
            search->setMinimumHeight(UiScale::dp(34));
            search->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            const QString searchBg = useSettingsPaper()
                                         ? QStringLiteral("rgba(55,53,47,0.06)")
                                         : QStringLiteral("rgba(255,255,255,0.07)");
            const QString searchFocus =
                useSettingsPaper() ? QStringLiteral("rgba(55,53,47,0.08)")
                                   : QStringLiteral("rgba(255,255,255,0.10)");
            const QColor accFocus = BlopTheme::accentPrimary();
            setLiteralQss(search, QStringLiteral(
                "QLineEdit {"
                "  background: %1; color: %2;"
                "  border: none; border-radius: 6px;"
                "  padding: 6px 12px; font-size: 13px;"
                "}"
                "QLineEdit:focus {"
                "  background: %3;"
                "  border: 1px solid rgba(%4,%5,%6,0.45);"
                "}")
                .arg(searchBg, settingsInk(), searchFocus)
                .arg(accFocus.red())
                .arg(accFocus.green())
                .arg(accFocus.blue()));
        }
        // Drop the desktop stretch that capped search width.
        if (searchLay->count() >= 2) {
            if (QLayoutItem *extra = searchLay->takeAt(1))
                delete extra;
        }
        if (ui->btnClose)
            ui->btnClose->hide();
        contentColLay->addWidget(searchRow);

        auto *stack = new QStackedWidget(contentCol);
        stack->setObjectName(QStringLiteral("SettingsSectionStack"));
        m_sectionStack = stack;
        paintSettingsContentSurface(stack,
                                    QStringLiteral("SettingsSectionStack"));

        m_sectionTitles.clear();
        const QString contentHex = settingsContentBg().name(QColor::HexRgb);
        const QString pageScrollQss =
            QStringLiteral(
                "QScrollArea { background: %1; border: none; }"
                "QScrollArea > QWidget { background: %1; }"
                "QScrollArea > QWidget > QWidget { background: %1; }")
                .arg(contentHex) +
            (useSettingsPaper() ? BlopStyle::paperScrollbarQss()
                                : QString());

        for (BlopSettingsCard *c : allCards) {
            c->setNavPanelMode(true);
            auto *pageScroll = new QScrollArea(stack);
            pageScroll->setObjectName(QStringLiteral("SettingsPaperScroll"));
            pageScroll->setWidgetResizable(true);
            pageScroll->setFrameShape(QFrame::NoFrame);
            pageScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            pageScroll->setStyleSheet(pageScrollQss);
            if (QWidget *vp = pageScroll->viewport()) {
                vp->setAutoFillBackground(true);
                QPalette vpPal = vp->palette();
                vpPal.setColor(QPalette::Window, settingsContentBg());
                vpPal.setColor(QPalette::Base, settingsContentBg());
                vp->setPalette(vpPal);
            }
            BlopScroll::enableFingerScroll(pageScroll);

            auto *page = new QWidget();
            page->setObjectName(QStringLiteral("SettingsStackPage"));
            paintSettingsContentSurface(page,
                                        QStringLiteral("SettingsStackPage"));
            auto *pageLay = new QVBoxLayout(page);
            pageLay->setContentsMargins(UiScale::dp(BlopStyle::edgeInsetDp()),
                                        UiScale::dp(BlopStyle::sectionGapDp()),
                                        UiScale::dp(BlopStyle::edgeInsetDp()),
                                        UiScale::dp(BlopStyle::edgeInsetDp() * 3));
            pageLay->setSpacing(0);
            c->setParent(page);
            c->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
            pageLay->addWidget(c, 0);
            pageLay->addStretch(1);

            pageScroll->setWidget(page);
            stack->addWidget(pageScroll);
            nav->addItem(c->title());
            m_sectionTitles.append(c->title());
        }
        stack->setMaximumWidth(UiScale::dp(780));
        contentColLay->addWidget(stack, 1);

        splitLay->addWidget(navCol, 0);
        splitLay->addWidget(contentCol, 1);
        root->addWidget(split, 1);

        setLiteralQss(this, QStringLiteral(
            "QDialog { background-color: %1; border: none; border-radius: 0px; }")
            .arg(BlopStyle::obsidianDesk().name(QColor::HexRgb)));

        nav->setCurrentRow(0);
        stack->setCurrentIndex(0);
        connect(nav, &QListWidget::currentRowChanged, this,
                [this, stack](int row) {
                    if (!stack || row < 0 || row >= stack->count())
                        return;
                    stack->setCurrentIndex(row);
                    animateSectionPage(stack->widget(row));
                });

        connect(search, &QLineEdit::textChanged, this,
                [nav, stack, allCards](const QString &q) {
                    const QString needle = q.trimmed().toLower();
                    int firstVisible = -1;
                    for (int i = 0; i < allCards.size(); ++i) {
                        BlopSettingsCard *c = allCards[i];
                        const bool titleHit =
                            !needle.isEmpty() &&
                            (c->title().toLower().contains(needle) ||
                             c->subtitle().toLower().contains(needle) ||
                             c->sectionKeywords().toLower().contains(needle));
                        const bool hit = sectionMatchesSearch(
                            c->bodyWidget() ? c->bodyWidget() : c, needle,
                            c->title(), c->subtitle(), c->sectionKeywords());
                        applyRowSearchFilter(
                            c->bodyWidget() ? c->bodyWidget() : c, needle,
                            titleHit);
                        if (auto *item = nav->item(i))
                            item->setHidden(!hit);
                        if (hit && firstVisible < 0)
                            firstVisible = i;
                    }
                    if (firstVisible >= 0 &&
                        (nav->currentRow() < 0 ||
                         (nav->item(nav->currentRow()) &&
                          nav->item(nav->currentRow())->isHidden()))) {
                        nav->setCurrentRow(firstVisible);
                        stack->setCurrentIndex(firstVisible);
                    }
                });
    } else
#endif
    {
        // Phone / fallback: stacked panels (no left rail).
        auto *stack = new QVBoxLayout();
        stack->setContentsMargins(0, 0, 0, 0);
        stack->setSpacing(UiScale::dp(12));
        for (BlopSettingsCard *c : allCards) {
            c->setNavPanelMode(false);
            c->setExpanded(true);
            stack->addWidget(c);
        }
        hostLay->addLayout(stack, 0);
        hostLay->addStretch(1);
        contentLay->addWidget(cardsHost, 1);

        connect(search, &QLineEdit::textChanged, this, [=](const QString &q) {
            const QString needle = q.trimmed().toLower();
            for (BlopSettingsCard *c : allCards) {
                if (needle.isEmpty()) {
                    c->setVisible(true);
                    applyRowSearchFilter(c->bodyWidget() ? c->bodyWidget() : c,
                                         QString(), false);
                    continue;
                }
                const bool titleHit =
                    c->title().toLower().contains(needle) ||
                    c->subtitle().toLower().contains(needle) ||
                    c->sectionKeywords().toLower().contains(needle);
                const bool hit = sectionMatchesSearch(
                    c->bodyWidget() ? c->bodyWidget() : c, needle, c->title(),
                    c->subtitle(), c->sectionKeywords());
                applyRowSearchFilter(c->bodyWidget() ? c->bodyWidget() : c,
                                     needle, titleHit);
                c->setVisible(hit);
                if (hit)
                    c->setExpanded(true);
            }
        });
    }

    refreshProfileList();

    connect(&BlopTheme::instance(), &BlopTheme::themeChanged, this,
            &SettingsDialog::refreshTheme);
}

SettingsDialog::~SettingsDialog() { delete ui; }

void SettingsDialog::refreshTheme() {
    setProperty("blopForcePaper", useSettingsPaper());
    const QString dialogBg =
        m_appShellMode ? settingsContentBg().name(QColor::HexRgb)
                       : BlopStyle::obsidianDesk().name(QColor::HexRgb);
    setLiteralQss(this, QStringLiteral(
        "QDialog { background-color: %1; border: none; border-radius: 0px; }")
        .arg(dialogBg));
    refreshThemedTree(this);
#ifndef Q_OS_ANDROID
    const QColor contentBg = settingsContentBg();
    const QString contentHex = contentBg.name(QColor::HexRgb);
    if (m_appShellMode && m_contentCol) {
        paintSettingsContentSurface(m_contentCol,
                                    QStringLiteral("SettingsContentCol"));
        m_contentCol->setStyleSheet(QStringLiteral(
            "QWidget#SettingsContentCol { background: %1; border: none; }")
                                        .arg(contentHex));
    }
    if (auto *col = findChild<QWidget *>(QStringLiteral("SettingsContentCol"))) {
        paintSettingsContentSurface(col, QStringLiteral("SettingsContentCol"));
        col->setStyleSheet(QStringLiteral(
            "QWidget#SettingsContentCol { background: %1; border: none; }")
                               .arg(contentHex));
    }
    if (m_sectionNav) {
        const QColor acc = BlopTheme::accentPrimary();
        const QColor navMuted = BlopTheme::textSecondary();
        const QColor navInk = BlopTheme::textPrimary();
        m_sectionNav->setStyleSheet(QStringLiteral(
            "QListWidget#SettingsNavList {"
            "  background: transparent; border: none; outline: none;"
            "  color: %1; font-size: 13px; font-weight: 500;"
            "}"
            "QListWidget#SettingsNavList::item {"
            "  padding: 10px 10px; margin: 1px 0;"
            "  border: none; border-radius: 8px; min-height: %2px;"
            "  color: %1;"
            "}"
            "QListWidget#SettingsNavList::item:selected {"
            "  background: rgba(%4,%5,%6,0.22);"
            "  color: %3;"
            "  font-weight: 600;"
            "  border: 1px solid rgba(%4,%5,%6,0.55);"
            "}"
            "QListWidget#SettingsNavList::item:hover:!selected {"
            "  background: rgba(255,255,255,0.06);"
            "}")
            .arg(navMuted.name(QColor::HexRgb),
                 QString::number(UiScale::dp(BlopStyle::touchTargetMinDp())),
                 navInk.name(QColor::HexRgb))
            .arg(acc.red())
            .arg(acc.green())
            .arg(acc.blue()));
    }
    if (auto *stack =
            findChild<QStackedWidget *>(QStringLiteral("SettingsSectionStack")))
        paintSettingsContentSurface(stack,
                                    QStringLiteral("SettingsSectionStack"));
    for (QWidget *page :
         findChildren<QWidget *>(QStringLiteral("SettingsStackPage"))) {
        if (page)
            paintSettingsContentSurface(page,
                                        QStringLiteral("SettingsStackPage"));
    }
    // Re-skin nav-panel cards for Light/Dark (titles, body, header).
    for (QFrame *card :
         findChildren<QFrame *>(QStringLiteral("BlopSettingsCard"))) {
        if (!card || !card->property("blopNavPaper").toBool())
            continue;
        card->setAttribute(Qt::WA_StyledBackground, true);
        card->setAutoFillBackground(true);
        QPalette pal = card->palette();
        pal.setColor(QPalette::Window, contentBg);
        pal.setColor(QPalette::Base, contentBg);
        pal.setColor(QPalette::WindowText, QColor(settingsInk()));
        card->setPalette(pal);
        card->setStyleSheet(QStringLiteral(
            "#BlopSettingsCard { background-color: %1; border: none; }")
                                .arg(contentHex));
        if (auto *body =
                card->findChild<QWidget *>(QStringLiteral("SettingsCardBody"))) {
            body->setAttribute(Qt::WA_StyledBackground, true);
            body->setAutoFillBackground(true);
            QPalette bp = body->palette();
            bp.setColor(QPalette::Window, settingsContentRowBg());
            bp.setColor(QPalette::Base, settingsContentRowBg());
            bp.setColor(QPalette::WindowText, QColor(settingsInk()));
            body->setPalette(bp);
            body->setStyleSheet(QStringLiteral(
                "QWidget#SettingsCardBody {"
                "  background: transparent; border: none; }"));
        }
        for (QWidget *child : card->findChildren<QWidget *>()) {
            if (!child || child->objectName() == QStringLiteral("SettingsCardBody"))
                continue;
            if (child->parentWidget() != card)
                continue;
            child->setAutoFillBackground(true);
            QPalette hp = child->palette();
            hp.setColor(QPalette::Window, contentBg);
            child->setPalette(hp);
            child->setStyleSheet(
                QStringLiteral("background: %1; border: none;").arg(contentHex));
            for (QLabel *lbl : child->findChildren<QLabel *>()) {
                if (!lbl)
                    continue;
                const QString ss = lbl->styleSheet();
                const bool isTitle =
                    ss.contains(QLatin1String("font-size: 15")) ||
                    ss.contains(QLatin1String("font-size: 22"));
                setLiteralQss(
                    lbl, QStringLiteral(
                             "color: %1; font-size: %2px; font-weight: %3;"
                             "background: transparent; %4")
                             .arg(isTitle ? settingsInk() : settingsInkMuted())
                             .arg(isTitle ? 15 : 12)
                             .arg(isTitle ? 650 : 400)
                             .arg(isTitle ? QStringLiteral("letter-spacing: -0.25px;")
                                          : QStringLiteral("padding-top: 1px;")));
            }
        }
    }
    for (QScrollArea *sa :
         findChildren<QScrollArea *>(QStringLiteral("SettingsPaperScroll"))) {
        if (!sa)
            continue;
        const QString scrollQss =
            QStringLiteral(
                "QScrollArea { background: %1; border: none; }"
                "QScrollArea > QWidget { background: %1; }"
                "QScrollArea > QWidget > QWidget { background: %1; }")
                .arg(contentHex) +
            (useSettingsPaper() ? BlopStyle::paperScrollbarQss() : QString());
        sa->setStyleSheet(scrollQss);
        if (sa->viewport()) {
            sa->viewport()->setAutoFillBackground(true);
            QPalette vp = sa->viewport()->palette();
            vp.setColor(QPalette::Window, contentBg);
            vp.setColor(QPalette::Base, contentBg);
            sa->viewport()->setPalette(vp);
        }
    }
    // Search toolbar row + field.
    if (auto *searchRow = findChild<QWidget *>(QStringLiteral("SettingsSearchRow"))) {
        searchRow->setStyleSheet(
            QStringLiteral("background: %1;").arg(contentHex));
    }
    if (auto *search = findChild<QLineEdit *>(QStringLiteral("SettingsSearch"))) {
        const QString searchBg = useSettingsPaper()
                                     ? QStringLiteral("rgba(55,53,47,0.06)")
                                     : QStringLiteral("rgba(255,255,255,0.07)");
        const QString searchFocus =
            useSettingsPaper() ? QStringLiteral("rgba(55,53,47,0.08)")
                               : QStringLiteral("rgba(255,255,255,0.10)");
        const QColor accFocus = BlopTheme::accentPrimary();
        setLiteralQss(search, QStringLiteral(
            "QLineEdit {"
            "  background: %1; color: %2;"
            "  border: none; border-radius: 6px;"
            "  padding: 6px 12px; font-size: 13px;"
            "}"
            "QLineEdit:focus {"
            "  background: %3;"
            "  border: 1px solid rgba(%4,%5,%6,0.45);"
            "}")
            .arg(searchBg, settingsInk(), searchFocus)
            .arg(accFocus.red())
            .arg(accFocus.green())
            .arg(accFocus.blue()));
    }
#endif
    retintSettingsControls(this);
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
        const bool active = (p.id == currentId);
        const QString label =
            active ? QStringLiteral("●  %1          Aktiv").arg(p.name)
                   : QStringLiteral("○  %1").arg(p.name);
        auto *item = new QListWidgetItem(label);
        item->setData(Qt::UserRole, p.id);
        item->setSizeHint(QSize(0, UiScale::dp(44)));
        m_profileList->addItem(item);
        if (active)
            m_profileList->setCurrentItem(item);
    }
}

void SettingsDialog::onProfileClicked(QListWidgetItem *item) {
    if (!item) return;
    QString id = item->data(Qt::UserRole).toString();
    m_profileManager->setCurrentProfile(id);
    refreshProfileList();
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
    // Workspace tab keeps settings open and emits; modal overlay closes with
    // EditProfileCode so MainWindow can open the profile editor next.
    if (property("blopWorkspaceEmbed").toBool()) {
        emit profileEditRequested(profileId);
        return;
    }
    done(EditProfileCode);
}

void SettingsDialog::setAppShellMode(bool on)
{
    m_appShellMode = on;
    setProperty("blopAppShell", on);
    if (m_navCol)
        m_navCol->setVisible(!on);
    const QString contentHex = settingsContentBg().name(QColor::HexRgb);
    if (m_contentCol) {
        m_contentCol->setStyleSheet(QStringLiteral(
            "QWidget#SettingsContentCol {"
            "  background: %1;"
            "  border: none;"
            "}")
                                        .arg(contentHex));
        paintSettingsContentSurface(m_contentCol,
                                    QStringLiteral("SettingsContentCol"));
    }
    // Follow Light/Dark content desk — never force paper white in Dark.
    setLiteralQss(this, QStringLiteral(
        "QDialog { background-color: %1; border: none; border-radius: 0px; }")
        .arg(contentHex));
}

void SettingsDialog::focusSearchField() {
  if (auto *search = findChild<QLineEdit *>(QStringLiteral("SettingsSearch"))) {
    search->setFocus(Qt::ShortcutFocusReason);
    search->selectAll();
  }
}

void SettingsDialog::setSectionIndex(int index)
{
    if (!m_sectionStack || index < 0 || index >= m_sectionStack->count())
        return;
    if (m_sectionNav && m_sectionNav->currentRow() != index) {
        m_sectionNav->setCurrentRow(index); // anim via currentRowChanged
        return;
    }
    if (m_sectionStack->currentIndex() == index) {
        // Same page — do not re-run fade (can leave opacity stuck at 0).
        clearSectionPageEffects();
        return;
    }
    m_sectionStack->setCurrentIndex(index);
    animateSectionPage(m_sectionStack->widget(index));
}

void SettingsDialog::clearSectionPageEffects()
{
    if (!m_sectionStack)
        return;
    for (int i = 0; i < m_sectionStack->count(); ++i) {
        QWidget *page = m_sectionStack->widget(i);
        if (!page)
            continue;
        if (page->graphicsEffect())
            page->setGraphicsEffect(nullptr);
        if (auto *sa = qobject_cast<QScrollArea *>(page)) {
            if (sa->viewport() && sa->viewport()->graphicsEffect())
                sa->viewport()->setGraphicsEffect(nullptr);
            if (QWidget *inner = sa->widget()) {
                if (inner->graphicsEffect())
                    inner->setGraphicsEffect(nullptr);
            }
        }
    }
}

void SettingsDialog::animateSectionPage(QWidget *page)
{
    if (!page)
        return;
    clearSectionPageEffects();
    if (!isVisible() || !page->isVisible())
        return;

    if (BlopMotion::reduceMotion())
        return;

    auto *fx = new QGraphicsOpacityEffect(page);
    page->setGraphicsEffect(fx);
    fx->setOpacity(0.0);

    QWidget *inner = page;
    if (auto *sa = qobject_cast<QScrollArea *>(page)) {
        if (sa->widget())
            inner = sa->widget();
    }

    QVBoxLayout *lay = qobject_cast<QVBoxLayout *>(inner->layout());
    const int baseTop = UiScale::dp(8);
    const int slide = UiScale::dp(8);
    int left = UiScale::dp(22);
    int right = UiScale::dp(28);
    int bottom = UiScale::dp(48);
    if (lay) {
        const QMargins m = lay->contentsMargins();
        left = m.left();
        right = m.right();
        bottom = m.bottom();
        lay->setContentsMargins(left, baseTop + slide, right, bottom);
    }

    auto *group = new QParallelAnimationGroup(page);
    group->setObjectName(QStringLiteral("SettingsSectionAnim"));
    auto *fade = new QPropertyAnimation(fx, "opacity", group);
    fade->setDuration(BlopMotion::kFast);
    fade->setStartValue(0.0);
    fade->setEndValue(1.0);
    fade->setEasingCurve(BlopMotion::kEaseStandard);

    auto *slideAnim = new QVariantAnimation(group);
    slideAnim->setDuration(BlopMotion::kFast);
    slideAnim->setStartValue(baseTop + slide);
    slideAnim->setEndValue(baseTop);
    slideAnim->setEasingCurve(BlopMotion::kEaseStandard);
    if (lay) {
        QObject::connect(slideAnim, &QVariantAnimation::valueChanged, page,
                         [lay, left, right, bottom](const QVariant &v) {
                             lay->setContentsMargins(left, v.toInt(), right,
                                                     bottom);
                         });
    }
    group->addAnimation(fade);
    group->addAnimation(slideAnim);
    QObject::connect(group, &QParallelAnimationGroup::finished, page,
                     [page, fx]() {
                         if (page->graphicsEffect() == fx)
                             page->setGraphicsEffect(nullptr);
                     });
    // If the page is hidden mid-anim, drop the effect so reopen is never blank.
    QObject::connect(page, &QObject::destroyed, group, &QObject::deleteLater);
    group->start(QAbstractAnimation::DeleteWhenStopped);
}

int SettingsDialog::sectionIndex() const
{
    if (m_sectionStack)
        return m_sectionStack->currentIndex();
    if (m_sectionNav)
        return m_sectionNav->currentRow();
    return 0;
}

void SettingsDialog::embedInWorkspace(bool asWorkspaceTab) {
    setWindowFlags(Qt::Widget);
    setModal(false);
    setAttribute(Qt::WA_QuitOnClose, false);
    setProperty("blopWorkspaceEmbed", asWorkspaceTab);
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
    setProperty("blopOwnsBackground", true);
#ifndef Q_OS_ANDROID
    // Floating Notion card keeps 12px radius; workspace tab card already
    // provides the outer radius so the dialog fill is square inside.
    const int radius = asWorkspaceTab ? 0 : UiScale::dp(12);
    setLiteralQss(this, QStringLiteral(
        "QDialog { background: %1; border: none; border-radius: %2px; }")
        .arg(settingsContentBg().name(QColor::HexRgb),
             QString::number(radius)));
#else
    setThemedQss(this, QStringLiteral(
        "QDialog { background-color: %1; border: none; border-radius: 0px; }")
                    .arg(settingsContentBg().name(QColor::HexRgb)));
#endif
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
