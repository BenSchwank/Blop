#include "settings_ui_helpers.h"

#include "blop_theme.h"
#include "blopripple.h"
#include "blopstyle.h"
#include "uiscale.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPaintEvent>
#include <QPushButton>
#include <QVariantAnimation>
#include <QVBoxLayout>

namespace SettingsUi {


constexpr const char *kRawQssProp = "blopRawQss";
constexpr const char *kTokenQssProp = "blopTokenQss";
constexpr const char *kSurfaceNameProp = "blopSurfaceName";


constexpr const char *kSearchKeysProp = "blopSearchKeys";

void tagSearchKeys(QWidget *w, const QString &keys) {
    if (!w)
        return;
    w->setProperty(kSearchKeysProp, keys);
}

bool widgetMatchesSearch(QWidget *w, const QString &needle) {
    if (!w || needle.isEmpty())
        return true;
    const QString keys = w->property(kSearchKeysProp).toString().toLower();
    if (!keys.isEmpty() && keys.contains(needle))
        return true;
    if (auto *lbl = qobject_cast<QLabel *>(w)) {
        if (lbl->text().toLower().contains(needle))
            return true;
    }
    if (auto *btn = qobject_cast<QPushButton *>(w)) {
        if (btn->text().toLower().contains(needle))
            return true;
    }
    if (auto *edit = qobject_cast<QLineEdit *>(w)) {
        if (edit->placeholderText().toLower().contains(needle)
            || edit->text().toLower().contains(needle))
            return true;
    }
    return false;
}

bool sectionMatchesSearch(QWidget *sectionRoot, const QString &needle,
                          const QString &title, const QString &subtitle,
                          const QString &sectionKeywords) {
    if (needle.isEmpty())
        return true;
    if (title.toLower().contains(needle) || subtitle.toLower().contains(needle))
        return true;
    if (sectionKeywords.toLower().contains(needle))
        return true;
    if (!sectionRoot)
        return false;
    const auto kids = sectionRoot->findChildren<QWidget *>();
    for (QWidget *w : kids) {
        if (widgetMatchesSearch(w, needle))
            return true;
    }
    return false;
}

void applyRowSearchFilter(QWidget *sectionRoot, const QString &needle) {
    if (!sectionRoot)
        return;
    const auto rows = sectionRoot->findChildren<QWidget *>(
        QStringLiteral("SettingsPropRow"), Qt::FindDirectChildrenOnly);
    // Also scan nested rows (body may wrap).
    const auto allRows = sectionRoot->findChildren<QWidget *>(
        QStringLiteral("SettingsPropRow"));
    for (QWidget *row : allRows) {
        if (needle.isEmpty()) {
            row->setVisible(true);
            continue;
        }
        bool hit = widgetMatchesSearch(row, needle);
        if (!hit) {
            const auto kids = row->findChildren<QWidget *>();
            for (QWidget *k : kids) {
                if (widgetMatchesSearch(k, needle)) {
                    hit = true;
                    break;
                }
            }
        }
        // Keep unlabeled chrome (mode rows etc.) visible when empty needle only;
        // if tagged empty and no match on children, hide when searching.
        if (!hit && row->property(kSearchKeysProp).toString().isEmpty()) {
            // Untagged body widgets (segment rows): match against all child text.
            const auto kids = row->findChildren<QWidget *>();
            for (QWidget *k : kids) {
                if (widgetMatchesSearch(k, needle)) {
                    hit = true;
                    break;
                }
            }
            if (!hit) {
                // Leave untagged structural widgets visible if any sibling/section hit —
                // handled by caller; default hide when searching.
                hit = false;
            }
        }
        row->setVisible(hit || needle.isEmpty());
    }
    Q_UNUSED(rows);
}

#ifndef Q_OS_ANDROID
// Desktop Settings: Notion paper in Light, Obsidian content in Dark (follows Modus).
bool useSettingsPaper() { return !BlopTheme::instance().isDark(); }
#else
// Android: Paper in Light, Obsidian/theme tokens in Dark (matches desktop language).
bool useSettingsPaper() { return !BlopTheme::instance().isDark(); }
#endif

QColor settingsContentBg() {
    return useSettingsPaper() ? BlopStyle::paperBg()
                              : BlopStyle::obsidianContent();
}

QColor settingsContentRowBg() {
    return useSettingsPaper() ? BlopStyle::paperRowBg()
                              : BlopStyle::obsidianContent();
}

void paintSettingsContentSurface(QWidget *w, const QString &objectName) {
    if (!w)
        return;
    if (useSettingsPaper()) {
        BlopStyle::paintPaperSurface(w, objectName);
        return;
    }
    if (!objectName.isEmpty())
        w->setObjectName(objectName);
    w->setAttribute(Qt::WA_StyledBackground, true);
    w->setAutoFillBackground(true);
    w->setProperty("blopOwnsBackground", true);
    const QColor bg = settingsContentBg();
    const QColor ink = BlopTheme::textPrimary();
    QPalette pal = w->palette();
    pal.setColor(QPalette::Window, bg);
    pal.setColor(QPalette::Base, bg);
    pal.setColor(QPalette::Text, ink);
    pal.setColor(QPalette::WindowText, ink);
    w->setPalette(pal);
    const QString name =
        w->objectName().isEmpty() ? QStringLiteral("BlopSettingsSurf")
                                  : w->objectName();
    w->setStyleSheet(
        QStringLiteral("QWidget#%1 { background-color: %2; color: %3; }")
            .arg(name, bg.name(QColor::HexRgb), ink.name(QColor::HexRgb)));
}

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
        w->setStyleSheet(useSettingsPaper() ? BlopStyle::paperInputQss()
                                            : BlopTheme::inputQss());
        return;
    }
    if (token == "primary") {
        w->setStyleSheet(useSettingsPaper() ? BlopStyle::paperPrimaryButtonQss()
                                            : BlopTheme::primaryButtonQss());
        return;
    }
    if (token == "secondary") {
        w->setStyleSheet(useSettingsPaper() ? BlopStyle::paperSecondaryButtonQss()
                                            : BlopTheme::secondaryButtonQss());
        return;
    }
    if (token == "destructive") {
        w->setStyleSheet(useSettingsPaper() ? BlopStyle::paperDestructiveButtonQss()
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
    return useSettingsPaper() ? BlopStyle::paperInk().name(QColor::HexRgb)
                              : BlopTheme::textPrimary().name(QColor::HexRgb);
}
QString settingsInkMuted() {
    return useSettingsPaper() ? BlopStyle::paperInkMuted().name(QColor::HexRgb)
                              : BlopTheme::textSecondary().name(QColor::HexRgb);
}
QString settingsChipBg() {
    return useSettingsPaper() ? BlopStyle::paperChipBg().name(QColor::HexRgb)
                              : BlopTheme::surfaceMuted().name(QColor::HexRgb);
}

QString segmentedControlQss() {
    // Soft borderless pills — seamless track feel without hard outlines.
    if (UiScale::isAndroidPhoneUi(nullptr)) {
        return useSettingsPaper() ? BlopStyle::paperSegmentQss()
                                  : BlopStyle::segmentQss();
    }
    const QColor acc = BlopTheme::accentPrimary();
    if (useSettingsPaper()) {
        return QStringLiteral(
                   "QPushButton {"
                   "  background: transparent;"
                   "  color: %1;"
                   "  border: none;"
                   "  border-radius: 8px;"
                   "  padding: 0px 10px;"
                   "  min-height: 26px;"
                   "  max-height: 26px;"
                   "  font-size: 12px;"
                   "  font-weight: 550;"
                   "}"
                   "QPushButton:checked {"
                   "  background: %2;"
                   "  color: %3;"
                   "  font-weight: 650;"
                   "}"
                   "QPushButton:hover:!checked { background: rgba(55,53,47,0.05); }"
                   "QPushButton:pressed { background: %2; }")
            .arg(BlopStyle::paperInkMuted().name(QColor::HexRgb),
                 QStringLiteral("rgba(%1,%2,%3,0.16)")
                     .arg(acc.red())
                     .arg(acc.green())
                     .arg(acc.blue()),
                 BlopStyle::paperInk().name(QColor::HexRgb));
    }
    // Dark Settings: soft pills on Obsidian content.
    return QStringLiteral(
               "QPushButton {"
               "  background: transparent;"
               "  color: %1;"
               "  border: none;"
               "  border-radius: 8px;"
               "  padding: 0px 10px;"
               "  min-height: 26px;"
               "  max-height: 26px;"
               "  font-size: 12px;"
               "  font-weight: 550;"
               "}"
               "QPushButton:checked {"
               "  background: %2;"
               "  color: %3;"
               "  font-weight: 650;"
               "}"
               "QPushButton:hover:!checked { background: rgba(255,255,255,0.06); }"
               "QPushButton:pressed { background: %2; }")
        .arg(BlopTheme::textSecondary().name(QColor::HexRgb),
             QStringLiteral("rgba(%1,%2,%3,0.22)")
                 .arg(acc.red())
                 .arg(acc.green())
                 .arg(acc.blue()),
             BlopTheme::textPrimary().name(QColor::HexRgb));
}

/// Compact desktop segment height for settings property rows (not phone touch).
int settingsSegmentMinHeight() {
    if (UiScale::isAndroidPhoneUi(nullptr))
        return UiScale::dp(BlopStyle::touchTargetMinDp());
    return UiScale::dp(26);
}

void styleSegmentTrack(QWidget *track) {
    if (!track || UiScale::isAndroidPhoneUi(track))
        return;
    track->setObjectName(QStringLiteral("SettingsSegmentTrack"));
    track->setAttribute(Qt::WA_StyledBackground, true);
    const QString trackBg = useSettingsPaper()
                                ? QStringLiteral("rgba(55,53,47,0.045)")
                                : QStringLiteral("rgba(255,255,255,0.06)");
    track->setStyleSheet(QStringLiteral(
                             "QWidget {"
                             "  background: %1;"
                             "  border: none;"
                             "  border-radius: 10px;"
                             "}")
                             .arg(trackBg));
}

void retintSettingsControls(QWidget *root) {
    if (!root)
        return;
    const QString segQss = segmentedControlQss();
    for (QWidget *track : root->findChildren<QWidget *>(
             QStringLiteral("SettingsSegmentTrack"))) {
        styleSegmentTrack(track);
        for (QPushButton *b : track->findChildren<QPushButton *>()) {
            if (!b)
                continue;
            setLiteralQss(b, segQss);
        }
    }
    for (QWidget *row : root->findChildren<QWidget *>(
             QStringLiteral("SettingsPropRow"))) {
        if (!row)
            continue;
        const bool last = row->property("blopPropLast").toBool();
        row->setStyleSheet(propertyRowShellQss(last));
        for (QLabel *lbl : row->findChildren<QLabel *>()) {
            if (!lbl)
                continue;
            if (lbl->objectName() == QLatin1String("PropStatus")) {
                setLiteralQss(lbl,
                              QStringLiteral("color: %1; font-size: 10px; "
                                             "font-weight: 400; background: "
                                             "transparent;")
                                  .arg(settingsInkMuted()));
            } else {
                setLiteralQss(lbl,
                              QStringLiteral("color: %1; font-size: 12px; "
                                             "font-weight: 500; background: "
                                             "transparent;")
                                  .arg(settingsInk()));
            }
        }
    }
    for (QPushButton *b : root->findChildren<QPushButton *>()) {
        if (!b || !b->property("blopQuietAction").isValid())
            continue;
        setLiteralQss(b, propertyActionQss(b->property("blopQuietAction").toBool()));
    }
}

/// Soft continuous list — faint inset rules, no boxed corners.
QString propertyRowShellQss(bool last) {
    const QString rule = useSettingsPaper()
                             ? QStringLiteral("1px solid rgba(55,53,47,0.05)")
                             : QStringLiteral("1px solid rgba(255,255,255,0.06)");
    const QString hover = useSettingsPaper()
                              ? QStringLiteral("rgba(55,53,47,0.035)")
                              : QStringLiteral("rgba(255,255,255,0.05)");
    const QString press = useSettingsPaper()
                              ? QStringLiteral("rgba(55,53,47,0.06)")
                              : QStringLiteral("rgba(255,255,255,0.08)");
    return QStringLiteral(
               "QWidget#SettingsPropRow {"
               "  background: transparent;"
               "  border: none;"
               "  border-bottom: %1;"
               "  border-radius: 0px;"
               "  margin: 0px;"
               "}"
               "QWidget#SettingsPropRow:hover {"
               "  background: %2;"
               "  border-radius: 8px;"
               "}"
               "QWidget#SettingsPropRow:pressed { background: %3; }")
        .arg(last ? QStringLiteral("none") : rule, hover, press);
}

QString propertyActionQss(bool destructive) {
    if (destructive) {
        return QStringLiteral(
            "QPushButton {"
            "  background: transparent; color: #C0392B; border: none;"
            "  text-align: right; font-size: 11px; font-weight: 600;"
            "  padding: 0px 4px; min-height: 22px; max-height: 22px;"
            "  border-radius: 4px;"
            "}"
            "QPushButton:hover { color: #A93226; background: rgba(192,57,43,0.08); }"
            "QPushButton:pressed { background: rgba(192,57,43,0.14); }");
    }
    return QStringLiteral(
               "QPushButton {"
               "  background: transparent; color: %1; border: none;"
               "  text-align: right; font-size: 11px; font-weight: 600;"
               "  padding: 0px 4px; min-height: 22px; max-height: 22px;"
               "  border-radius: 4px;"
               "}"
               "QPushButton:hover { color: %2; background: rgba(%3,%4,%5,0.10); }"
               "QPushButton:pressed { background: rgba(%3,%4,%5,0.16); }"
               "QPushButton:checked { color: %2; background: rgba(%3,%4,%5,0.12); }")
        .arg(settingsInkMuted(),
             BlopTheme::accentPrimary().name(QColor::HexRgb))
        .arg(BlopTheme::accentPrimary().red())
        .arg(BlopTheme::accentPrimary().green())
        .arg(BlopTheme::accentPrimary().blue());
}

QWidget *makePropertyRow(QWidget *parent, const QString &label,
                         QWidget *action, bool last, const QString &searchKeys) {
    auto *row = new QWidget(parent);
    row->setObjectName(QStringLiteral("SettingsPropRow"));
    row->setProperty("blopPropLast", last);
    tagSearchKeys(row, searchKeys.isEmpty()
                           ? label
                           : (label + QLatin1Char(' ') + searchKeys));
    row->setAttribute(Qt::WA_StyledBackground, true);
    row->setAttribute(Qt::WA_Hover, true);
    const bool phoneStack = UiScale::isAndroidPhoneUi(parent);
    row->setMinimumHeight(phoneStack
                              ? UiScale::dp(BlopStyle::touchTargetMinDp())
                              : UiScale::dp(30));
    row->setMaximumHeight(phoneStack ? QWIDGETSIZE_MAX : UiScale::dp(36));
    row->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    row->setCursor(Qt::PointingHandCursor);
    row->setStyleSheet(propertyRowShellQss(last));
    auto *lbl = new QLabel(label, row);
    lbl->setWordWrap(false);
    lbl->setStyleSheet(QStringLiteral(
                           "color: %1; font-size: 12px; font-weight: 500;"
                           "background: transparent;")
                           .arg(settingsInk()));
    if (phoneStack) {
        auto *lay = new QVBoxLayout(row);
        lay->setContentsMargins(UiScale::dp(12), UiScale::dp(6), UiScale::dp(12),
                                UiScale::dp(6));
        lay->setSpacing(UiScale::dp(6));
        lay->addWidget(lbl);
        if (action) {
            action->setParent(row);
            action->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            if (auto *btn = qobject_cast<QPushButton *>(action)) {
                btn->setCursor(Qt::PointingHandCursor);
                btn->setFlat(true);
                BlopRipple::attachPressFeedback(btn, 0.96);
            }
            lay->addWidget(action);
        }
    } else {
        auto *lay = new QHBoxLayout(row);
        lay->setContentsMargins(UiScale::dp(10), UiScale::dp(3), UiScale::dp(8),
                                UiScale::dp(3));
        lay->setSpacing(UiScale::dp(8));
        lbl->setMinimumWidth(UiScale::dp(88));
        lbl->setMaximumWidth(UiScale::dp(110));
        lay->addWidget(lbl, 0, Qt::AlignVCenter);
        if (action) {
            action->setParent(row);
            if (auto *btn = qobject_cast<QPushButton *>(action)) {
                btn->setCursor(Qt::PointingHandCursor);
                btn->setFlat(true);
                BlopRipple::attachPressFeedback(btn, 0.96);
            }
            lay->addWidget(action, 1, Qt::AlignVCenter | Qt::AlignRight);
        }
    }
    return row;
}

/// Notion-style row with title + muted status on the left, quiet actions right.
QWidget *makeNamedPropertyRow(QWidget *parent, const QString &title,
                              const QString &status, QWidget *actions,
                              bool last, const QString &searchKeys) {
    auto *row = new QWidget(parent);
    row->setObjectName(QStringLiteral("SettingsPropRow"));
    row->setProperty("blopPropLast", last);
    tagSearchKeys(row, searchKeys.isEmpty()
                           ? (title + QLatin1Char(' ') + status)
                           : (title + QLatin1Char(' ') + status + QLatin1Char(' ')
                              + searchKeys));
    row->setAttribute(Qt::WA_StyledBackground, true);
    row->setAttribute(Qt::WA_Hover, true);
    const bool phoneStack = UiScale::isAndroidPhoneUi(parent);
    row->setMinimumHeight(phoneStack ? UiScale::dp(44) : UiScale::dp(32));
    row->setMaximumHeight(phoneStack ? QWIDGETSIZE_MAX : UiScale::dp(40));
    row->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    row->setStyleSheet(propertyRowShellQss(last));

    auto *textCol = new QVBoxLayout();
    textCol->setContentsMargins(0, 0, 0, 0);
    textCol->setSpacing(0);
    auto *titleLbl = new QLabel(title, row);
    titleLbl->setWordWrap(false);
    titleLbl->setStyleSheet(QStringLiteral(
        "color: %1; font-size: 12px; font-weight: 600;"
        "background: transparent;")
                                .arg(settingsInk()));
    textCol->addWidget(titleLbl);
    if (!status.isEmpty()) {
        auto *st = new QLabel(status, row);
        st->setObjectName(QStringLiteral("PropStatus"));
        st->setWordWrap(false);
        st->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 10px; font-weight: 400;"
            "background: transparent;")
                              .arg(settingsInkMuted()));
        textCol->addWidget(st);
    }

    if (phoneStack) {
        auto *lay = new QVBoxLayout(row);
        lay->setContentsMargins(UiScale::dp(12), UiScale::dp(6), UiScale::dp(10),
                                UiScale::dp(6));
        lay->setSpacing(UiScale::dp(4));
        lay->addLayout(textCol);
        if (actions) {
            actions->setParent(row);
            actions->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            lay->addWidget(actions);
        }
    } else {
        auto *lay = new QHBoxLayout(row);
        lay->setContentsMargins(UiScale::dp(10), UiScale::dp(3), UiScale::dp(8),
                                UiScale::dp(3));
        lay->setSpacing(UiScale::dp(6));
        lay->addLayout(textCol, 1);
        if (actions) {
            actions->setParent(row);
            lay->addWidget(actions, 0, Qt::AlignVCenter);
        }
    }
    return row;
}

QPushButton *makeQuietAction(QWidget *parent, const QString &text,
                             bool destructive) {
    auto *b = new QPushButton(text, parent);
    b->setCursor(Qt::PointingHandCursor);
    b->setFlat(true);
    b->setProperty("blopQuietAction", destructive);
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

} // namespace SettingsUi
