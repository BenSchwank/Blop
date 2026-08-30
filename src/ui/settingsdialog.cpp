#include "settingsdialog.h"
#include "calendarservice.h"
#include "cloudstoragestore.h"
#include "googleauthmanager.h"
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
#include <QAbstractItemView>
#include <QStackedWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPalette>
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
#include <memory>

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

#ifndef Q_OS_ANDROID
// Desktop Settings always sits on Notion paper — use dark ink regardless of
// app Dark mode (sidebar stays Obsidian; this panel is the light content).
constexpr bool kSettingsPaper = true;
#else
// Android phone/tablet: same Notion paper content inside BottomSheet/Card.
constexpr bool kSettingsPaper = true;
#endif

void applyStoredQss(QWidget *w) {
    if (!w)
        return;
    // Desktop Settings nav-panel pages stay on Notion paper — never re-apply
    // dark BlopStyle::surfaceStyle after setNavPanelMode cleared the look.
    if (w->property("blopNavPaper").toBool())
        return;
    const QString surface = w->property(kSurfaceNameProp).toString();
    if (!surface.isEmpty()) {
        w->setStyleSheet(BlopStyle::surfaceStyle(surface));
        return;
    }
    const QByteArray token = w->property(kTokenQssProp).toByteArray();
    if (token == "input") {
        w->setStyleSheet(kSettingsPaper ? BlopStyle::paperInputQss()
                                        : BlopTheme::inputQss());
        return;
    }
    if (token == "primary") {
        w->setStyleSheet(kSettingsPaper ? BlopStyle::paperPrimaryButtonQss()
                                        : BlopTheme::primaryButtonQss());
        return;
    }
    if (token == "secondary") {
        w->setStyleSheet(kSettingsPaper ? BlopStyle::paperSecondaryButtonQss()
                                        : BlopTheme::secondaryButtonQss());
        return;
    }
    if (token == "destructive") {
        w->setStyleSheet(kSettingsPaper ? BlopStyle::paperDestructiveButtonQss()
                                        : BlopTheme::secondaryButtonQss());
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

/// Literal QSS (never BlopTheme::themed) — for Notion paper / Obsidian chrome.
void setLiteralQss(QWidget *w, const QString &raw) {
    if (!w)
        return;
    w->setProperty(kRawQssProp, QVariant()); // clear so refreshTheme won't re-theme
    w->setStyleSheet(raw);
}

void setSurfaceQss(QWidget *w, const QString &name) {
    if (!w)
        return;
    w->setObjectName(name);
    w->setProperty(kSurfaceNameProp, name);
    w->setStyleSheet(BlopStyle::surfaceStyle(name));
}

QString accentRgba(int alpha) {
    const QColor c = BlopTheme::accentPrimary();
    return QStringLiteral("rgba(%1,%2,%3,%4)")
        .arg(c.red())
        .arg(c.green())
        .arg(c.blue())
        .arg(alpha);
}

QString settingsInk() {
    return kSettingsPaper ? BlopStyle::paperInk().name(QColor::HexRgb)
                          : BlopTheme::textPrimary().name(QColor::HexRgb);
}
QString settingsInkMuted() {
    return kSettingsPaper ? BlopStyle::paperInkMuted().name(QColor::HexRgb)
                          : BlopTheme::textSecondary().name(QColor::HexRgb);
}
QString settingsChipBg() {
    return kSettingsPaper ? BlopStyle::paperChipBg().name(QColor::HexRgb)
                          : BlopTheme::surfaceMuted().name(QColor::HexRgb);
}

QString segmentedControlQss() {
    if (!kSettingsPaper)
        return BlopStyle::segmentQss();
    return BlopStyle::paperSegmentQss();
}

/// Notion-style property row: label left, quiet action right.
QString propertyRowShellQss(bool last) {
    return QStringLiteral(
               "QWidget#SettingsPropRow {"
               "  background: transparent;"
               "  border: none;"
               "  border-bottom: %1;"
               "}")
        .arg(last ? QStringLiteral("none")
                  : QStringLiteral("1px solid rgba(20,24,40,0.06)"));
}

QString propertyActionQss(bool destructive = false) {
    if (destructive) {
        return QStringLiteral(
            "QPushButton {"
            "  background: transparent; color: #C0392B; border: none;"
            "  text-align: right; font-size: 13px; font-weight: 500;"
            "  padding: 4px 2px;"
            "}"
            "QPushButton:hover { color: #A93226; }");
    }
    return QStringLiteral(
               "QPushButton {"
               "  background: transparent; color: %1; border: none;"
               "  text-align: right; font-size: 13px; font-weight: 500;"
               "  padding: 4px 2px;"
               "}"
               "QPushButton:hover { color: %2; }")
        .arg(BlopStyle::paperInkMuted().name(QColor::HexRgb),
             BlopTheme::accentPrimary().name(QColor::HexRgb));
}

QWidget *makePropertyRow(QWidget *parent, const QString &label,
                         QWidget *action, bool last = false) {
    auto *row = new QWidget(parent);
    row->setObjectName(QStringLiteral("SettingsPropRow"));
    row->setMinimumHeight(UiScale::dp(44));
    row->setStyleSheet(propertyRowShellQss(last));
    auto *lay = new QHBoxLayout(row);
    lay->setContentsMargins(UiScale::dp(14), UiScale::dp(8), UiScale::dp(14),
                            UiScale::dp(8));
    lay->setSpacing(UiScale::dp(12));
    auto *lbl = new QLabel(label, row);
    lbl->setStyleSheet(QStringLiteral(
                           "color: %1; font-size: 13px; font-weight: 500;"
                           "background: transparent;")
                           .arg(BlopStyle::paperInk().name(QColor::HexRgb)));
    lay->addWidget(lbl, 1);
    if (action) {
        action->setParent(row);
        if (auto *btn = qobject_cast<QPushButton *>(action)) {
            btn->setCursor(Qt::PointingHandCursor);
            btn->setFlat(true);
        }
        lay->addWidget(action, 0, Qt::AlignVCenter);
    }
    return row;
}

/// Notion-style row with title + muted status on the left, quiet actions right.
QWidget *makeNamedPropertyRow(QWidget *parent, const QString &title,
                              const QString &status, QWidget *actions,
                              bool last = false) {
    auto *row = new QWidget(parent);
    row->setObjectName(QStringLiteral("SettingsPropRow"));
    row->setMinimumHeight(UiScale::dp(48));
    row->setStyleSheet(propertyRowShellQss(last));
    auto *lay = new QHBoxLayout(row);
    lay->setContentsMargins(UiScale::dp(14), UiScale::dp(10), UiScale::dp(12),
                            UiScale::dp(10));
    lay->setSpacing(UiScale::dp(10));

    auto *textCol = new QVBoxLayout();
    textCol->setContentsMargins(0, 0, 0, 0);
    textCol->setSpacing(2);
    auto *titleLbl = new QLabel(title, row);
    titleLbl->setStyleSheet(QStringLiteral(
        "color: %1; font-size: 13px; font-weight: 600;"
        "background: transparent;")
                                .arg(BlopStyle::paperInk().name(QColor::HexRgb)));
    textCol->addWidget(titleLbl);
    if (!status.isEmpty()) {
        auto *st = new QLabel(status, row);
        st->setObjectName(QStringLiteral("PropStatus"));
        st->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 11px; font-weight: 400;"
            "background: transparent;")
                              .arg(BlopStyle::paperInkMuted().name(
                                  QColor::HexRgb)));
        textCol->addWidget(st);
    }
    lay->addLayout(textCol, 1);

    if (actions) {
        actions->setParent(row);
        lay->addWidget(actions, 0, Qt::AlignVCenter);
    }
    return row;
}

QPushButton *makeQuietAction(QWidget *parent, const QString &text,
                             bool destructive = false) {
    auto *b = new QPushButton(text, parent);
    b->setCursor(Qt::PointingHandCursor);
    b->setFlat(true);
    setLiteralQss(b, propertyActionQss(destructive));
    return b;
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
        const QString titleCol = BlopTheme::textPrimary().name(QColor::HexRgb);
        const QString subCol = BlopTheme::textSecondary().name(QColor::HexRgb);
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
        m_bodyLay->setContentsMargins(0, 16, 0, 0);
        m_bodyLay->setSpacing(12);
        root->addWidget(m_body);

        m_header->installEventFilter(new HeaderClickFilter(this));
    }

    void addBodyWidget(QWidget *w) { m_bodyLay->addWidget(w); }
    void addBodyLayout(QLayout *l) { m_bodyLay->addLayout(l); }

    QString title() const { return m_title; }
    QString subtitle() const { return m_subtitle; }

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
            setProperty(kSurfaceNameProp, QVariant());
            setProperty("blopNavPaper", true);
            setAttribute(Qt::WA_StyledBackground, true);
            setAutoFillBackground(true);
            {
                QPalette pal = palette();
                pal.setColor(QPalette::Window, BlopStyle::paperBg());
                pal.setColor(QPalette::Base, BlopStyle::paperBg());
                pal.setColor(QPalette::WindowText, BlopStyle::paperInk());
                setPalette(pal);
            }
            setStyleSheet(QStringLiteral(
                "#BlopSettingsCard {"
                "  background-color: %1;"
                "  border: none;"
                "}")
                              .arg(BlopStyle::paperBg().name(QColor::HexRgb)));
            setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
            if (auto *rootLay = qobject_cast<QVBoxLayout *>(layout())) {
                rootLay->setContentsMargins(0, 0, 0, 0);
                rootLay->setSpacing(UiScale::dp(8));
            }
            if (m_header) {
                m_header->setAttribute(Qt::WA_StyledBackground, true);
                m_header->setAutoFillBackground(true);
                QPalette hp = m_header->palette();
                hp.setColor(QPalette::Window, BlopStyle::paperBg());
                hp.setColor(QPalette::WindowText, BlopStyle::paperInk());
                m_header->setPalette(hp);
                m_header->setStyleSheet(QStringLiteral(
                    "background: %1; border: none;")
                                            .arg(BlopStyle::paperBg().name(
                                                QColor::HexRgb)));
            }
            // Flat property container — hairline only, no nested “card on black”.
            if (m_body) {
                m_body->setObjectName(QStringLiteral("SettingsCardBody"));
                m_body->setAttribute(Qt::WA_StyledBackground, true);
                m_body->setAutoFillBackground(true);
                QPalette pal = m_body->palette();
                pal.setColor(QPalette::Window, BlopStyle::paperRowBg());
                pal.setColor(QPalette::Base, BlopStyle::paperRowBg());
                pal.setColor(QPalette::WindowText, BlopStyle::paperInk());
                m_body->setPalette(pal);
                m_body->setStyleSheet(QStringLiteral(
                    "QWidget#SettingsCardBody {"
                    "  background-color: %1;"
                    "  border: 1px solid rgba(55,53,47,0.09);"
                    "  border-radius: 8px;"
                    "}")
                                         .arg(BlopStyle::paperRowBg().name(
                                             QColor::HexRgb)));
                if (m_bodyLay) {
                    m_bodyLay->setContentsMargins(0, 0, 0, 0);
                    m_bodyLay->setSpacing(0);
                }
            }
            if (m_titleLbl) {
                m_titleLbl->setProperty(kRawQssProp, QVariant());
                m_titleLbl->setStyleSheet(QStringLiteral(
                    "color: %1; font-size: 20px; font-weight: 700;"
                    "letter-spacing: -0.3px; background: transparent;")
                                              .arg(BlopStyle::paperInk().name(
                                                  QColor::HexRgb)));
            }
            if (m_subtitleLbl) {
                m_subtitleLbl->setProperty(kRawQssProp, QVariant());
                m_subtitleLbl->setStyleSheet(QStringLiteral(
                    "color: %1; font-size: 13px; font-weight: 400;"
                    "background: transparent;")
                                                 .arg(BlopStyle::paperInkMuted()
                                                          .name(QColor::HexRgb)));
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
#ifndef Q_OS_ANDROID
    // Concept B / Notion overlay: dialog owns its paper fill (BlopModal must
    // not replace this with transparent — see blopOwnsBackground).
    setProperty("blopOwnsBackground", true);
    setLiteralQss(this, QStringLiteral(
        "QDialog { background-color: %1; border: none; border-radius: 12px; }")
        .arg(BlopStyle::paperBg().name(QColor::HexRgb)));
#else
    setThemedQss(this, QStringLiteral(
        "QDialog { background-color: #1A1A24; border: none; border-radius: 0px; }"));
#endif
    const bool phoneUi = UiScale::isAndroidPhoneUi(parent);
    const int pagePad = phoneUi ? UiScale::dp(14) : 0;
    const int cardGap = phoneUi ? UiScale::dp(12) : 0;
    if (phoneUi)
        setMinimumSize(0, 0);
    else
        setMinimumSize(UiScale::dp(720), UiScale::dp(520));
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
        "  background-color: #F7F7F5;"
        "  border-bottom: 1px solid rgba(20,24,40,0.10);"
        "}"));
#else
    setThemedQss(hero, QStringLiteral(
        "#SettingsHero {"
        "  background-color: rgba(255, 255, 255, 0.03);"
        "  border-bottom: 1px solid rgba(120, 130, 160, 0.18);"
        "}"));
#endif
    auto *heroLay = new QHBoxLayout(hero);
    heroLay->setContentsMargins(pagePad, phoneUi ? UiScale::dp(14) : 22,
                                pagePad, phoneUi ? UiScale::dp(14) : 22);
    heroLay->setSpacing(phoneUi ? UiScale::dp(10) : 16);

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
    auto *searchLay = new QHBoxLayout(searchRow);
    searchLay->setContentsMargins(pagePad, phoneUi ? UiScale::dp(10) : 18,
                                  pagePad, phoneUi ? UiScale::dp(8) : 12);
    auto *search = new QLineEdit(searchRow);
    search->setPlaceholderText(QStringLiteral("Einstellungen durchsuchen..."));
#ifndef Q_OS_ANDROID
    setLiteralQss(search, BlopStyle::paperInputQss());
#else
    setThemedQss(search, QStringLiteral(
        "QLineEdit {"
        "  background: %1;"
        "  color: %2;"
        "  border: 1px solid rgba(20, 24, 40, 0.12);"
        "  border-radius: 10px;"
        "  padding: 12px 16px; font-size: 14px;"
        "}"
        "QLineEdit:focus { border: 1px solid %3; }")
        .arg(BlopTheme::surfaceMuted().name(QColor::HexRgb),
             BlopTheme::textPrimary().name(QColor::HexRgb),
             BlopTheme::accentPrimary().name(QColor::HexRgb)));
#endif
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
            setLiteralQss(who, QStringLiteral(
                "color: %1; font-size: 13px; font-weight: 500;"
                "background: transparent; padding: 12px 14px 4px 14px;")
                .arg(settingsInkMuted()));
            cardKonto->addBodyWidget(who);
        } else {
            auto *hint = new QLabel(
                QStringLiteral(
                    "Melde dich bei Study an, um Notizen zu teilen."),
                cardKonto);
            hint->setWordWrap(true);
            setLiteralQss(hint, QStringLiteral(
                "color: %1; font-size: 13px;"
                "background: transparent; padding: 12px 14px 4px 14px;")
                .arg(settingsInkMuted()));
            cardKonto->addBodyWidget(hint);
        }

        auto *btnAuthScreen = new QPushButton(
            studyLoggedIn ? QStringLiteral("Öffnen →")
                          : QStringLiteral("Anmelden →"),
            cardKonto);
        setLiteralQss(btnAuthScreen, propertyActionQss(false));
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
            false));

        if (!studyLoggedIn) {
            auto *btnGoogle = new QPushButton(QStringLiteral("Google →"),
                                              cardKonto);
            setLiteralQss(btnGoogle, propertyActionQss(false));
            connect(btnGoogle, &QPushButton::clicked, this,
                    [this, closeAfterAccountAction]() {
                      emit googleLoginRequested();
                      closeAfterAccountAction();
                    });
            cardKonto->addBodyWidget(makePropertyRow(
                cardKonto, QStringLiteral("Mit Google anmelden"), btnGoogle,
                false));
        }

        auto *btnEdit = new QPushButton(QStringLiteral("Bearbeiten →"), cardKonto);
        setLiteralQss(btnEdit, propertyActionQss(false));
        connect(btnEdit, &QPushButton::clicked, this, [this]() {
            openEditor(m_profileManager ? m_profileManager->currentProfile().id
                                        : QString());
        });
        cardKonto->addBodyWidget(makePropertyRow(
            cardKonto, QStringLiteral("Aktuelles Profil"), btnEdit,
            !studyLoggedIn));

        if (studyLoggedIn) {
            auto *btnLogout = new QPushButton(QStringLiteral("Abmelden"),
                                              cardKonto);
            setLiteralQss(btnLogout, propertyActionQss(true));
            connect(btnLogout, &QPushButton::clicked, this, [this]() {
                emit logoutRequested();
                accept();
            });
            cardKonto->addBodyWidget(makePropertyRow(
                cardKonto, QStringLiteral("Sitzung"), btnLogout, true));
        }
    }

    // ----- Card: Darstellung (Light/Dark Mode) --------------------------
    // v3.17.0: new theme switcher backed by BlopTheme. Lets the user pick
    // a light or dark surface palette while the accent stays the same
    // in both modes (Blue / Green / Pink — never hardcoded purple chrome).
    auto *cardTheme = new BlopSettingsCard(
        QStringLiteral("Thema"),
        QStringLiteral("Hell, Dunkel und Akzentfarbe"),
        contentWidget);
    {
        auto *lblMode = new QLabel(QStringLiteral("Modus"), cardTheme);
        setThemedQss(lblMode, QStringLiteral(
            "color: %1; font-size: 12px; font-weight: 600;"
            "background: transparent;")
            .arg(settingsInk()));
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
        const QString segStyle = segmentedControlQss();
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
            "color: %1; font-size: 12px;"
            "background: transparent; padding-top: 6px;")
            .arg(settingsInkMuted()));
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
            "color: %1; font-size: 12px; "
            "font-weight: 600; background: transparent; padding-top: 8px;")
            .arg(settingsInk()));
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
            "QPushButton { background: %1; color: %2;"
            "  border: 1px solid rgba(20,24,40,0.12); border-radius: 10px;"
            "  padding: 10px 14px; text-align: left; font-weight: 600; }"
            "QPushButton:checked { background: %3;"
            "  border-color: %4; }")
                .arg(settingsChipBg(), settingsInk(), accentRgba(70),
                     BlopTheme::accentPrimary().name(QColor::HexRgb)));
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
        setLiteralQss(burgerHint, QStringLiteral(
            "color: %1; font-size: 11px;"
            "background: transparent; padding: 2px 0 4px 0;")
            .arg(settingsInkMuted()));
        cardTheme->addBodyWidget(burgerHint);
    }

    // ----- Card: Erscheinungsbild ---------------------------------------
    // v3.17.1/B4: the standalone "Akzentfarbe" row is removed -- the
    // Darstellung card above now owns the accent picker (persistent +
    // BlopTheme-backed). Toolbar mode stays here.
    // Desktop Drawboard: Favorites rail is locked; Radial stays Android-only.
    auto *cardLook = new BlopSettingsCard(
        QStringLiteral("Werkzeuge"),
        QStringLiteral("Favorites-Leiste und Toolbar"),
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
            "QRadioButton { color: %1; background: transparent; "
            "padding: 6px 0; font-size: 13px; min-height: 36px; }")
            .arg(settingsInk());
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
        setLiteralQss(hint, QStringLiteral(
            "color: %1; font-size: 11px;"
            "background: transparent; padding: 2px 0 4px 0;")
            .arg(settingsInkMuted()));
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
            "  background: %1;"
            "  border: 1px solid rgba(20, 24, 40, 0.12);"
            "  border-radius: 10px;"
            "  color: %2;"
            "  padding: 4px;"
            "}"
            "QListWidget::item {"
            "  padding: 10px 12px;"
            "  border-radius: 6px;"
            "  margin: 2px;"
            "  min-height: 36px;"
            "}"
            "QListWidget::item:selected {"
            "  background: %3;"
            "}")
            .arg(settingsChipBg(), settingsInk(), accentRgba(140)));
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
            "QPushButton { background-color: %1; color: %2;"
            "  border: 1px solid rgba(20,24,40,0.12); border-radius: 10px;"
            "  padding: 10px 14px; font-weight: 600; min-height: 40px; }"
            "QPushButton:hover { border-color: %3; }")
                .arg(settingsChipBg(), settingsInk(), accentRgba(166)));
        connect(btnNewProfile, &QPushButton::clicked, this,
                &SettingsDialog::onCreateProfile);
        cardBehavior->addBodyWidget(btnNewProfile);
    }

    // ----- Card: Speicher — Notion property rows for clouds ---------------
    auto *cardStorage = new BlopSettingsCard(
        QStringLiteral("Speicher"),
        QStringLiteral("Notizen lokal — Clouds öffnen in Blop"),
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
        setLiteralQss(hint, QStringLiteral(
            "color: %1; font-size: 12px;"
            "background: transparent; padding: 10px 14px 6px 14px;")
            .arg(settingsInkMuted()));
        cardStorage->addBodyWidget(hint);

        auto *modeRow = new QWidget(cardStorage);
        auto *modeLay = new QHBoxLayout(modeRow);
        modeLay->setContentsMargins(UiScale::dp(10), UiScale::dp(4),
                                    UiScale::dp(10), UiScale::dp(4));
        modeLay->setSpacing(6);

        const QString segStyle = segmentedControlQss();
        auto *btnLocal = new QPushButton(QStringLiteral("Nur lokal"), modeRow);
        auto *btnCloud = new QPushButton(QStringLiteral("Nur Cloud"), modeRow);
        auto *btnBoth = new QPushButton(QStringLiteral("Lokal + Cloud"), modeRow);
        for (QPushButton *b : {btnLocal, btnCloud, btnBoth}) {
            b->setCheckable(true);
            b->setCursor(Qt::PointingHandCursor);
            b->setMinimumHeight(UiScale::dp(34));
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
            QStringLiteral("Lokal: %1")
                .arg(StoragePrefs::ensureLocalLibraryRoot()),
            cardStorage);
        localPathLbl->setWordWrap(true);
        setLiteralQss(localPathLbl, QStringLiteral(
            "color: %1; font-size: 11px;"
            "background: transparent; padding: 2px 14px 10px 14px;")
            .arg(settingsInkMuted()));
        cardStorage->addBodyWidget(localPathLbl);

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
                        "über „Öffnen“ — nicht über einen Android-Dateiordner."));
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
            QPushButton *openBtn{nullptr};
            QPushButton *primaryBtn{nullptr};
            QPushButton *folderBtn{nullptr};
        };
        auto cloudRefs = std::make_shared<QVector<CloudRowRef>>();

        auto refreshCloudUi = [cloudRefs]() {
            const QString primary = StoragePrefs::primaryCloudId();
            for (CloudRowRef &r : *cloudRefs) {
                const bool linked = StoragePrefs::isProviderLinked(r.id);
                bool webOk = false;
                QVector<CloudStorageEntry> rows = CloudStorageStore::load();
                if (CloudStorageEntry *cur =
                        CloudStorageStore::findMutable(rows, r.id))
                    webOk = cur->webConnected;
                const QString st =
                    (linked || webOk) ? QStringLiteral("Verbunden")
                                      : QStringLiteral("Nicht verbunden");
                if (r.statusLbl)
                    r.statusLbl->setText(st);
                if (r.openBtn)
                    r.openBtn->setText((linked || webOk)
                                           ? QStringLiteral("Öffnen →")
                                           : QStringLiteral("Anmelden →"));
                if (r.primaryBtn) {
                    r.primaryBtn->setEnabled(linked);
                    r.primaryBtn->setText(primary == r.id
                                             ? QStringLiteral("Primär ✓")
                                             : QStringLiteral("Primär"));
                }
            }
        };

        auto addCloudRow = [&](const CloudStorageEntry &e, bool last) {
            auto *actions = new QWidget(cardStorage);
            auto *al = new QHBoxLayout(actions);
            al->setContentsMargins(0, 0, 0, 0);
            al->setSpacing(UiScale::dp(6));

            auto *btnPrimary =
                makeQuietAction(actions, QStringLiteral("Primär"));
            auto *btnFolder =
                makeQuietAction(actions, QStringLiteral("Ordner"));
            auto *btnOpen =
                makeQuietAction(actions, QStringLiteral("Öffnen →"));
            al->addWidget(btnPrimary);
            al->addWidget(btnFolder);
            al->addWidget(btnOpen);

            const bool linked = StoragePrefs::isProviderLinked(e.id);
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
            ref.openBtn = btnOpen;
            ref.primaryBtn = btnPrimary;
            ref.folderBtn = btnFolder;
            cloudRefs->append(ref);

            const QString id = e.id;
            const QString displayName = e.name;
            QObject::connect(btnOpen, &QPushButton::clicked, this,
                             [this, id, displayName, requestCloud,
                              refreshCloudUi]() {
                                 QVector<CloudStorageEntry> entries =
                                     CloudStorageStore::load();
                                 CloudStorageEntry entry;
                                 if (CloudStorageEntry *found =
                                         CloudStorageStore::findMutable(
                                             entries, id))
                                     entry = *found;
                                 else {
                                     entry.id = id;
                                     entry.name = displayName;
                                     entry.type = id;
                                 }
                                 requestCloud(entry);
                                 refreshCloudUi();
                                 emit storagePrefsChanged();
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

            cardStorage->addBodyWidget(row);
        };

        // Prefer Drive first, then other providers from the store.
        {
            QVector<CloudStorageEntry> entries = CloudStorageStore::load();
            QVector<CloudStorageEntry> ordered;
            CloudStorageEntry drive;
            bool haveDrive = false;
            for (const CloudStorageEntry &e : entries) {
                const bool isDrive =
                    e.id.compare(QLatin1String("googledrive"),
                                 Qt::CaseInsensitive) == 0 ||
                    e.type.compare(QLatin1String("googledrive"),
                                   Qt::CaseInsensitive) == 0 ||
                    e.name.compare(QLatin1String("Google Drive"),
                                   Qt::CaseInsensitive) == 0;
                if (isDrive) {
                    drive = e;
                    drive.id = QStringLiteral("googledrive");
                    drive.name = QStringLiteral("Google Drive");
                    drive.type = QStringLiteral("googledrive");
                    haveDrive = true;
                } else {
                    ordered.append(e);
                }
            }
            if (!haveDrive) {
                drive.id = QStringLiteral("googledrive");
                drive.type = drive.id;
                drive.name = QStringLiteral("Google Drive");
            }
            ordered.prepend(drive);

            for (int i = 0; i < ordered.size(); ++i)
                addCloudRow(ordered[i], i == ordered.size() - 1);
            refreshCloudUi();
        }

        auto applyMode = [this, btnLocal, btnCloud, btnBoth, hint](
                             StoragePrefs::Mode m) {
            btnLocal->setChecked(m == StoragePrefs::Mode::LocalOnly);
            btnCloud->setChecked(m == StoragePrefs::Mode::CloudOnly);
            btnBoth->setChecked(m == StoragePrefs::Mode::LocalAndCloud);
            StoragePrefs::setMode(m);
            QString text = StoragePrefs::modeHint(m);
            if (m != StoragePrefs::Mode::LocalOnly &&
                StoragePrefs::primaryLinkedCloudPath().isEmpty()) {
                text += QStringLiteral(
                    "\nTipp: Verknüpfe unten einen Cloud-Anbieter.");
            }
            hint->setText(text);
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

        // Custom embed — compact Notion footer.
        auto *customUrl = new QLineEdit(cardStorage);
        customUrl->setPlaceholderText(
            QStringLiteral("https://cloud.example.com"));
        customUrl->setMinimumHeight(UiScale::dp(34));
#ifndef Q_OS_ANDROID
        setLiteralQss(customUrl, BlopStyle::paperInputQss());
#else
        setTokenQss(customUrl, "input");
#endif
        auto *customName = new QLineEdit(cardStorage);
        customName->setPlaceholderText(QStringLiteral("Name (optional)"));
        customName->setMinimumHeight(UiScale::dp(34));
#ifndef Q_OS_ANDROID
        setLiteralQss(customName, BlopStyle::paperInputQss());
#else
        setTokenQss(customName, "input");
#endif
        auto *btnEmbed =
            makeQuietAction(cardStorage, QStringLiteral("Einbetten →"));
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

        auto *customWrap = new QWidget(cardStorage);
        customWrap->setObjectName(QStringLiteral("SettingsPropRow"));
        customWrap->setStyleSheet(propertyRowShellQss(true));
        auto *customLay = new QVBoxLayout(customWrap);
        customLay->setContentsMargins(UiScale::dp(14), UiScale::dp(10),
                                      UiScale::dp(14), UiScale::dp(12));
        customLay->setSpacing(UiScale::dp(6));
        auto *customTitle = new QLabel(QStringLiteral("Eigene Cloud"), customWrap);
        customTitle->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 13px; font-weight: 600;"
            "background: transparent;")
                                       .arg(BlopStyle::paperInk().name(
                                           QColor::HexRgb)));
        customLay->addWidget(customTitle);
        customLay->addWidget(customUrl);
        auto *nameRow = new QHBoxLayout();
        nameRow->setContentsMargins(0, 0, 0, 0);
        nameRow->setSpacing(UiScale::dp(8));
        nameRow->addWidget(customName, 1);
        nameRow->addWidget(btnEmbed, 0, Qt::AlignVCenter);
        customLay->addLayout(nameRow);
        cardStorage->addBodyWidget(customWrap);
    }

    // ----- Card: Integrationen (Cloud, Kalender, zukünftige APIs) ----------
    auto *cardIntegrations = new BlopSettingsCard(
        QStringLiteral("Integrationen"),
        QStringLiteral("Cloud, Google Kalender und weitere Verbindungen"),
        contentWidget);
    {
      // --- Cloud (Kurzlink in den Speicher-Bereich) ---
      auto *cloudHint = new QLabel(
          QStringLiteral(
              "Cloud-Ordner (Drive, Nextcloud, …) verwaltest du unter Speicher."),
          cardIntegrations);
      cloudHint->setWordWrap(true);
      setLiteralQss(cloudHint, QStringLiteral(
          "color: %1; font-size: 12px; background: transparent;")
          .arg(settingsInkMuted()));
      cardIntegrations->addBodyWidget(makePropertyRow(
          cardIntegrations, QStringLiteral("Cloud"), cloudHint, true));

      // --- Google Calendar ---
      auto *calStatus = new QLabel(cardIntegrations);
      auto refreshCalStatus = [calStatus]() {
        if (CalendarService::instance().hasGoogleAccess()) {
          calStatus->setText(QStringLiteral("Verbunden — Termine werden synchronisiert"));
        } else {
          calStatus->setText(QStringLiteral("Nicht verbunden"));
        }
      };
      refreshCalStatus();
      setLiteralQss(calStatus, QStringLiteral(
          "color: %1; font-size: 13px; background: transparent;")
          .arg(settingsInkMuted()));
      cardIntegrations->addBodyWidget(makePropertyRow(
          cardIntegrations, QStringLiteral("Google Kalender"), calStatus, true));

      auto *btnCalConnect =
          new QPushButton(QStringLiteral("Verbinden"), cardIntegrations);
      btnCalConnect->setCursor(Qt::PointingHandCursor);
      auto *btnCalSync =
          new QPushButton(QStringLiteral("Jetzt sync"), cardIntegrations);
      btnCalSync->setCursor(Qt::PointingHandCursor);
      auto *btnCalDisconnect =
          new QPushButton(QStringLiteral("Trennen"), cardIntegrations);
      btnCalDisconnect->setCursor(Qt::PointingHandCursor);
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
              [refreshCalStatus, updateCalButtons]() {
                CalendarService::instance().disconnectGoogle();
                refreshCalStatus();
                updateCalButtons();
              });
      connect(&CalendarService::instance(), &CalendarService::eventsChanged,
              cardIntegrations, [refreshCalStatus, updateCalButtons]() {
                refreshCalStatus();
                updateCalButtons();
              });
      connect(&GoogleAuthManager::instance(),
              &GoogleAuthManager::calendarTokenUpdated, cardIntegrations,
              [refreshCalStatus, updateCalButtons]() {
                refreshCalStatus();
                updateCalButtons();
              });

      auto *calBtns = new QWidget(cardIntegrations);
      auto *calLay = new QHBoxLayout(calBtns);
      calLay->setContentsMargins(0, 0, 0, 0);
      calLay->setSpacing(UiScale::dp(8));
      calLay->addWidget(btnCalConnect, 0);
      calLay->addWidget(btnCalSync, 0);
      calLay->addWidget(btnCalDisconnect, 0);
      calLay->addStretch(1);
      cardIntegrations->addBodyWidget(makePropertyRow(
          cardIntegrations, QStringLiteral("Kalender-Aktionen"), calBtns, true));

      auto *future = new QLabel(
          QStringLiteral("Weitere Integrationen (z. B. Tasks, Mail) folgen hier."),
          cardIntegrations);
      future->setWordWrap(true);
      setLiteralQss(future, QStringLiteral(
          "color: %1; font-size: 12px; background: transparent;")
          .arg(settingsInkMuted()));
      cardIntegrations->addBodyWidget(makePropertyRow(
          cardIntegrations, QStringLiteral("Demnächst"), future, true));
    }
    cardIntegrations->setExpanded(true);

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
        setLiteralQss(info, QStringLiteral(
            "color: %1; font-size: 13px; font-weight: 500;"
            "background: transparent;")
            .arg(settingsInkMuted()));
        cardAdv->addBodyWidget(makePropertyRow(
            cardAdv, QStringLiteral("Version"), info, true));
    }
    cardAdv->setExpanded(true);

    const QList<BlopSettingsCard *> allCards = {
        cardKonto, cardTheme, cardLook, cardBehavior, cardStorage,
        cardIntegrations, cardAdv};

#ifndef Q_OS_ANDROID
    if (!phoneUi) {
        // --- Notion float: warm nav + paper pages, no dark chrome ----------
        root->removeWidget(hero);
        root->removeWidget(searchRow);
        root->removeWidget(scroll);
        scroll->hide();
        cardsHost->hide();

        auto *split = new QWidget(tabDesign);
        split->setObjectName(QStringLiteral("SettingsNotionSplit"));
        BlopStyle::paintPaperSurface(split, QStringLiteral("SettingsNotionSplit"));
        auto *splitLay = new QHBoxLayout(split);
        splitLay->setContentsMargins(0, 0, 0, 0);
        splitLay->setSpacing(0);

        const QString navWarm = QStringLiteral("#F1F1EC");
        auto *navCol = new QWidget(split);
        navCol->setObjectName(QStringLiteral("SettingsNavCol"));
        navCol->setFixedWidth(UiScale::dp(212));
        navCol->setAttribute(Qt::WA_StyledBackground, true);
        navCol->setAutoFillBackground(true);
        {
            QPalette np = navCol->palette();
            np.setColor(QPalette::Window, QColor(navWarm));
            np.setColor(QPalette::Base, QColor(navWarm));
            np.setColor(QPalette::Text, BlopStyle::paperInk());
            navCol->setPalette(np);
        }
        navCol->setStyleSheet(QStringLiteral(
            "QWidget#SettingsNavCol {"
            "  background: %1;"
            "  border-right: 1px solid rgba(20,24,40,0.07);"
            "  border-top-left-radius: 12px;"
            "  border-bottom-left-radius: 12px;"
            "}")
                                 .arg(navWarm));
        auto *navLay = new QVBoxLayout(navCol);
        navLay->setContentsMargins(UiScale::dp(10), UiScale::dp(14),
                                   UiScale::dp(10), UiScale::dp(12));
        navLay->setSpacing(UiScale::dp(4));

        auto *navTitle = new QLabel(QStringLiteral("EINSTELLUNGEN"), navCol);
        navTitle->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 11px; font-weight: 600;"
            "letter-spacing: 0.6px; background: transparent;"
            "padding: 2px 8px 10px 8px;")
                                    .arg(BlopStyle::paperInkMuted().name(
                                        QColor::HexRgb)));
        navLay->addWidget(navTitle, 0);

        auto *nav = new QListWidget(navCol);
        nav->setObjectName(QStringLiteral("SettingsNavList"));
        nav->setFocusPolicy(Qt::NoFocus);
        nav->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        nav->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        // Soft Notion pill — no thick accent rail.
        nav->setStyleSheet(QStringLiteral(
            "QListWidget#SettingsNavList {"
            "  background: transparent; border: none; outline: none;"
            "  color: %1; font-size: 13px; font-weight: 500;"
            "}"
            "QListWidget#SettingsNavList::item {"
            "  padding: 8px 10px; margin: 1px 0;"
            "  border: none; border-radius: 6px; min-height: %2px;"
            "}"
            "QListWidget#SettingsNavList::item:selected {"
            "  background: rgba(55,53,47,0.08);"
            "  color: %3;"
            "  font-weight: 600;"
            "}"
            "QListWidget#SettingsNavList::item:hover:!selected {"
            "  background: rgba(55,53,47,0.04);"
            "}")
            .arg(BlopStyle::paperInkMuted().name(QColor::HexRgb),
                 QString::number(UiScale::dp(30)),
                 BlopStyle::paperInk().name(QColor::HexRgb)));
        BlopScroll::enableFingerScroll(nav);
        navLay->addWidget(nav, 1);

        // Profile chip — Notion workspace switcher (avatar + name + ›).
        hero->setParent(navCol);
        hero->setAttribute(Qt::WA_StyledBackground, true);
        hero->setCursor(Qt::PointingHandCursor);
        hero->setStyleSheet(QStringLiteral(
            "#SettingsHero {"
            "  background: rgba(255,255,255,0.55);"
            "  border: 1px solid rgba(55,53,47,0.08);"
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
                .arg(BlopStyle::paperInkMuted().name(QColor::HexRgb)));
        }
        navLay->addWidget(hero, 0);

        auto *contentCol = new QWidget(split);
        contentCol->setObjectName(QStringLiteral("SettingsContentCol"));
        BlopStyle::paintPaperSurface(contentCol,
                                     QStringLiteral("SettingsContentCol"));
        contentCol->setStyleSheet(QStringLiteral(
            "QWidget#SettingsContentCol {"
            "  background: %1;"
            "  border-top-right-radius: 12px;"
            "  border-bottom-right-radius: 12px;"
            "}")
                                      .arg(BlopStyle::paperBg().name(
                                          QColor::HexRgb)));
        auto *contentColLay = new QVBoxLayout(contentCol);
        contentColLay->setContentsMargins(0, 0, 0, 0);
        contentColLay->setSpacing(0);

        // Toolbar: quiet search + Fertig (Notion corner close).
        searchRow->setParent(contentCol);
        searchRow->setAttribute(Qt::WA_StyledBackground, true);
        searchRow->setStyleSheet(QStringLiteral("background: %1;")
                                     .arg(BlopStyle::paperBg().name(QColor::HexRgb)));
        searchLay->setContentsMargins(UiScale::dp(24), UiScale::dp(12),
                                      UiScale::dp(16), UiScale::dp(6));
        searchLay->setSpacing(UiScale::dp(10));
        if (search) {
            search->setPlaceholderText(QStringLiteral("Suchen…"));
            search->setMinimumWidth(0);
            search->setMaximumWidth(QWIDGETSIZE_MAX);
            search->setMinimumHeight(UiScale::dp(34));
            search->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            setLiteralQss(search, QStringLiteral(
                "QLineEdit {"
                "  background: rgba(55,53,47,0.06); color: %1;"
                "  border: none; border-radius: 6px;"
                "  padding: 6px 12px; font-size: 13px;"
                "}"
                "QLineEdit:focus {"
                "  background: rgba(55,53,47,0.08);"
                "  border: 1px solid rgba(91,157,255,0.45);"
                "}")
                .arg(BlopStyle::paperInk().name(QColor::HexRgb)));
        }
        // Drop the desktop stretch that capped search width.
        if (searchLay->count() >= 2) {
            if (QLayoutItem *extra = searchLay->takeAt(1))
                delete extra;
        }
        if (ui->btnClose) {
            ui->btnClose->setText(QStringLiteral("Fertig"));
            ui->btnClose->setCursor(Qt::PointingHandCursor);
            ui->btnClose->setMinimumHeight(UiScale::dp(32));
            ui->btnClose->setMinimumWidth(UiScale::dp(64));
            ui->btnClose->setParent(searchRow);
            ui->btnClose->setStyleSheet(QStringLiteral(
                "QPushButton {"
                "  background: transparent; color: %1; border: none;"
                "  border-radius: 6px; padding: 6px 10px;"
                "  font-weight: 600; font-size: 13px;"
                "}"
                "QPushButton:hover { background: rgba(55,53,47,0.06); }")
                                            .arg(BlopTheme::accentPrimary().name(
                                                QColor::HexRgb)));
            searchLay->addWidget(ui->btnClose, 0, Qt::AlignVCenter);
        }
        contentColLay->addWidget(searchRow);

        auto *stack = new QStackedWidget(contentCol);
        stack->setObjectName(QStringLiteral("SettingsSectionStack"));
        BlopStyle::paintPaperSurface(stack,
                                     QStringLiteral("SettingsSectionStack"));

        const QString paperHex = BlopStyle::paperBg().name(QColor::HexRgb);
        const QString pageScrollQss =
            QStringLiteral(
                "QScrollArea { background: %1; border: none; }"
                "QScrollArea > QWidget { background: %1; }"
                "QScrollArea > QWidget > QWidget { background: %1; }")
                .arg(paperHex) +
            BlopStyle::paperScrollbarQss();

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
                vpPal.setColor(QPalette::Window, BlopStyle::paperBg());
                vpPal.setColor(QPalette::Base, BlopStyle::paperBg());
                vp->setPalette(vpPal);
            }
            BlopScroll::enableFingerScroll(pageScroll);

            auto *page = new QWidget();
            page->setObjectName(QStringLiteral("SettingsStackPage"));
            BlopStyle::paintPaperSurface(page, QStringLiteral("SettingsStackPage"));
            auto *pageLay = new QVBoxLayout(page);
            pageLay->setContentsMargins(UiScale::dp(24), UiScale::dp(4),
                                        UiScale::dp(28), UiScale::dp(28));
            pageLay->setSpacing(0);
            c->setParent(page);
            c->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
            pageLay->addWidget(c, 0);
            pageLay->addStretch(1);

            pageScroll->setWidget(page);
            stack->addWidget(pageScroll);
            nav->addItem(c->title());
        }
        contentColLay->addWidget(stack, 1);

        splitLay->addWidget(navCol, 0);
        splitLay->addWidget(contentCol, 1);
        root->addWidget(split, 1);

        setLiteralQss(this, QStringLiteral(
            "QDialog { background-color: %1; border: none; border-radius: 12px; }")
            .arg(BlopStyle::paperBg().name(QColor::HexRgb)));

        nav->setCurrentRow(0);
        stack->setCurrentIndex(0);
        connect(nav, &QListWidget::currentRowChanged, stack,
                &QStackedWidget::setCurrentIndex);

        connect(search, &QLineEdit::textChanged, this,
                [nav, stack, allCards](const QString &q) {
                    const QString needle = q.trimmed().toLower();
                    int firstVisible = -1;
                    for (int i = 0; i < allCards.size(); ++i) {
                        BlopSettingsCard *c = allCards[i];
                        const bool hit =
                            needle.isEmpty() ||
                            c->title().toLower().contains(needle) ||
                            c->subtitle().toLower().contains(needle);
                        if (auto *item = nav->item(i))
                            item->setHidden(!hit);
                        if (hit && firstVisible < 0)
                            firstVisible = i;
                    }
                    if (firstVisible >= 0 &&
                        (nav->currentRow() < 0 ||
                         nav->item(nav->currentRow())->isHidden())) {
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
                    continue;
                }
                const bool hit = c->title().toLower().contains(needle) ||
                                 c->subtitle().toLower().contains(needle);
                c->setVisible(hit);
            }
        });
    }

    refreshProfileList();

    connect(&BlopTheme::instance(), &BlopTheme::themeChanged, this,
            &SettingsDialog::refreshTheme);
}

SettingsDialog::~SettingsDialog() { delete ui; }

void SettingsDialog::refreshTheme() {
    refreshThemedTree(this);
#ifndef Q_OS_ANDROID
    if (auto *col = findChild<QWidget *>(QStringLiteral("SettingsContentCol")))
        BlopStyle::paintPaperSurface(col, QStringLiteral("SettingsContentCol"));
    if (auto *stack =
            findChild<QStackedWidget *>(QStringLiteral("SettingsSectionStack")))
        BlopStyle::paintPaperSurface(stack,
                                     QStringLiteral("SettingsSectionStack"));
    for (QWidget *page :
         findChildren<QWidget *>(QStringLiteral("SettingsStackPage"))) {
        if (page)
            BlopStyle::paintPaperSurface(page,
                                         QStringLiteral("SettingsStackPage"));
    }
    for (QFrame *card :
         findChildren<QFrame *>(QStringLiteral("BlopSettingsCard"))) {
        if (!card || !card->property("blopNavPaper").toBool())
            continue;
        card->setProperty("blopSurfaceName", QVariant());
        card->setAttribute(Qt::WA_StyledBackground, true);
        card->setAutoFillBackground(true);
        QPalette pal = card->palette();
        pal.setColor(QPalette::Window, BlopStyle::paperBg());
        pal.setColor(QPalette::Base, BlopStyle::paperBg());
        pal.setColor(QPalette::WindowText, BlopStyle::paperInk());
        card->setPalette(pal);
        card->setStyleSheet(QStringLiteral(
            "#BlopSettingsCard {"
            "  background-color: %1; border: none;"
            "}")
                                .arg(BlopStyle::paperBg().name(QColor::HexRgb)));
        if (auto *body =
                card->findChild<QWidget *>(QStringLiteral("SettingsCardBody"))) {
            body->setAttribute(Qt::WA_StyledBackground, true);
            body->setAutoFillBackground(true);
            QPalette bp = body->palette();
            bp.setColor(QPalette::Window, BlopStyle::paperRowBg());
            bp.setColor(QPalette::Base, BlopStyle::paperRowBg());
            bp.setColor(QPalette::WindowText, BlopStyle::paperInk());
            body->setPalette(bp);
            body->setStyleSheet(QStringLiteral(
                "QWidget#SettingsCardBody {"
                "  background-color: %1;"
                "  border: 1px solid rgba(20,24,40,0.08);"
                "  border-radius: 10px;"
                "}")
                                    .arg(BlopStyle::paperRowBg().name(
                                        QColor::HexRgb)));
        }
        // Ensure page title header never paints dark.
        for (QWidget *child : card->findChildren<QWidget *>()) {
            if (!child || child->objectName() == QStringLiteral("SettingsCardBody"))
                continue;
            if (child->parentWidget() != card)
                continue;
            child->setAutoFillBackground(true);
            QPalette hp = child->palette();
            hp.setColor(QPalette::Window, BlopStyle::paperBg());
            child->setPalette(hp);
            child->setStyleSheet(QStringLiteral("background: %1; border: none;")
                                     .arg(BlopStyle::paperBg().name(
                                         QColor::HexRgb)));
        }
    }
    for (QScrollArea *sa :
         findChildren<QScrollArea *>(QStringLiteral("SettingsPaperScroll"))) {
        if (!sa || !sa->viewport())
            continue;
        sa->viewport()->setAutoFillBackground(true);
        QPalette vp = sa->viewport()->palette();
        vp.setColor(QPalette::Window, BlopStyle::paperBg());
        vp.setColor(QPalette::Base, BlopStyle::paperBg());
        sa->viewport()->setPalette(vp);
    }
#endif
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
    // Workspace tab keeps settings open and emits; modal overlay closes with
    // EditProfileCode so MainWindow can open the profile editor next.
    if (property("blopWorkspaceEmbed").toBool()) {
        emit profileEditRequested(profileId);
        return;
    }
    done(EditProfileCode);
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
        .arg(BlopStyle::paperBg().name(QColor::HexRgb),
             QString::number(radius)));
#else
    setThemedQss(this, QStringLiteral(
        "QDialog { background-color: #1A1A24; border: none; border-radius: 0px; }"));
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
