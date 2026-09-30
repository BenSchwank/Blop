#include "SetupWindow.h"

#include "AssistantLogo.h"
#include "core/SettingsSync.h"

#include <QFont>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QSettings>
#include <QShowEvent>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

#if defined(Q_OS_WIN)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

enum class Glyph { Person, Wave, Pen };

QString portable(const QKeySequenceEdit *edit) {
    return edit->keySequence().toString(QKeySequence::PortableText);
}

QIcon glyphIcon(Glyph glyph, const QColor &color, int px) {
    QPixmap pixmap(px, px);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    if (glyph == Glyph::Wave) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        const qreal width = px * 0.14;
        const qreal gap = px * 0.08;
        const qreal heights[] = {0.36, 0.64, 0.46};
        qreal x = px * 0.22;
        for (qreal height : heights) {
            const qreal bar = px * height;
            painter.drawRoundedRect(QRectF(x, (px - bar) / 2.0, width, bar), width / 2.0, width / 2.0);
            x += width + gap;
        }
        return QIcon(pixmap);
    }

    painter.setPen(QPen(color, qMax(1.3, px / 9.0), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    if (glyph == Glyph::Person) {
        painter.drawEllipse(QRectF(px * 0.34, px * 0.14, px * 0.32, px * 0.32));
        painter.drawArc(QRectF(px * 0.16, px * 0.50, px * 0.68, px * 0.48), 20 * 16, 140 * 16);
    } else {
        painter.drawLine(QPointF(px * 0.30, px * 0.74), QPointF(px * 0.70, px * 0.26));
        painter.drawLine(QPointF(px * 0.58, px * 0.20), QPointF(px * 0.80, px * 0.42));
    }
    return QIcon(pixmap);
}

QWidget *fieldRow(QWidget *parent, const QString &label, QWidget *trailing, int page,
                  const QString &needle, QList<QWidget *> *rows) {
    auto *row = new QWidget(parent);
    row->setObjectName(QStringLiteral("row"));
    row->setAttribute(Qt::WA_StyledBackground, true);
    row->setFixedHeight(46);
    row->setProperty("page", page);
    row->setProperty("needle", needle.toLower());
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(2, 0, 2, 0);
    layout->setSpacing(16);
    auto *text = new QLabel(label, row);
    text->setObjectName(QStringLiteral("fieldLabel"));
    layout->addWidget(text, 1);
    layout->addWidget(trailing, 0, Qt::AlignRight | Qt::AlignVCenter);
    rows->append(row);
    return row;
}

#if defined(Q_OS_WIN)
void darkCaption(HWND hwnd) {
    if (!hwnd)
        return;
    const HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
    if (!dwm)
        return;
    using Fn = HRESULT(WINAPI *)(HWND, DWORD, LPCVOID, DWORD);
    const auto setAttr = reinterpret_cast<Fn>(GetProcAddress(dwm, "DwmSetWindowAttribute"));
    if (setAttr) {
        const BOOL dark = TRUE;
        setAttr(hwnd, 20, &dark, sizeof(dark));
        const COLORREF caption = 0x0016191A;
        const COLORREF text = 0x00F7F5F4;
        const COLORREF border = 0x002A2523;
        setAttr(hwnd, 35, &caption, sizeof(caption));
        setAttr(hwnd, 36, &text, sizeof(text));
        setAttr(hwnd, 34, &border, sizeof(border));
    }
    FreeLibrary(dwm);
}
#endif

} // namespace

SetupWindow::SetupWindow(QWidget *parent) : QWidget(parent) {
    setWindowTitle(QStringLiteral("Einstellungen"));
    setWindowIcon(assistantLogoIcon());
    setObjectName(QStringLiteral("setup"));

    QFont ui(QStringLiteral("Segoe UI"));
    ui.setPixelSize(13);
    setFont(ui);

    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *rail = new QWidget(this);
    rail->setObjectName(QStringLiteral("rail"));
    rail->setAttribute(Qt::WA_StyledBackground, true);
    rail->setFixedWidth(56);
    auto *railLayout = new QVBoxLayout(rail);
    railLayout->setContentsMargins(0, 14, 0, 14);
    railLayout->setSpacing(6);
    auto *logo = new QLabel(rail);
    logo->setPixmap(assistantLogoPixmap(40));
    logo->setFixedSize(40, 40);
    logo->setAlignment(Qt::AlignCenter);
    railLayout->addWidget(logo, 0, Qt::AlignHCenter);
    railLayout->addSpacing(8);

    auto *nav = new QWidget(this);
    nav->setObjectName(QStringLiteral("nav"));
    nav->setAttribute(Qt::WA_StyledBackground, true);
    nav->setFixedWidth(240);
    auto *navLayout = new QVBoxLayout(nav);
    navLayout->setContentsMargins(12, 14, 12, 16);
    navLayout->setSpacing(2);

    m_navSearch = new QLineEdit(nav);
    m_navSearch->setObjectName(QStringLiteral("search"));
    m_navSearch->setPlaceholderText(QStringLiteral("Suche"));
    m_navSearch->setClearButtonEnabled(true);
    m_navSearch->setFixedHeight(34);
    navLayout->addWidget(m_navSearch);
    auto *section = new QLabel(QStringLiteral("ASSISTENT"), nav);
    section->setObjectName(QStringLiteral("section"));
    navLayout->addWidget(section);

    auto *content = new QWidget(this);
    content->setObjectName(QStringLiteral("content"));
    content->setAttribute(Qt::WA_StyledBackground, true);
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    auto *header = new QWidget(content);
    header->setObjectName(QStringLiteral("header"));
    header->setAttribute(Qt::WA_StyledBackground, true);
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(22, 14, 16, 8);
    headerLayout->setSpacing(12);
    m_pageSearch = new QLineEdit(header);
    m_pageSearch->setObjectName(QStringLiteral("search"));
    m_pageSearch->setPlaceholderText(QStringLiteral("Suchen…"));
    m_pageSearch->setClearButtonEnabled(true);
    m_pageSearch->setFixedHeight(34);
    headerLayout->addWidget(m_pageSearch, 1);
    auto *done = new QPushButton(QStringLiteral("Fertig"), header);
    done->setObjectName(QStringLiteral("fertig"));
    done->setCursor(Qt::PointingHandCursor);
    done->setFocusPolicy(Qt::NoFocus);
    headerLayout->addWidget(done);
    contentLayout->addWidget(header);

    m_pages = new QStackedWidget(content);
    auto *pages = m_pages;
    pages->setObjectName(QStringLiteral("pages"));
    pages->setAttribute(Qt::WA_StyledBackground, true);

    const Glyph glyphs[] = {Glyph::Person, Glyph::Wave, Glyph::Pen};
    const QString names[] = {QStringLiteral("Konto"), QStringLiteral("Sprache"),
                             QStringLiteral("Werkzeuge")};
    const QString pageTitles[] = {QStringLiteral("Konto"), QStringLiteral("Spracheingabe"),
                                  QStringLiteral("Werkzeuge")};
    const QString leads[] = {
        QStringLiteral("Anmeldung und die Sitzung, über die die KI Tokens verbraucht."),
        QStringLiteral("Die Taste startet das Sprechen und beendet es wieder."),
        QStringLiteral("Dieselben Kürzel wie in den Blop-Einstellungen."),
    };
    const QString keywords[] = {
        QStringLiteral("konto google sitzung anmeldung laden"),
        QStringLiteral("sprache spracheingabe taste hotkey"),
        QStringLiteral("werkzeuge werkzeug kürzel stift radierer marker text hand"),
    };

    auto *account = new QWidget(pages);
    account->setObjectName(QStringLiteral("page"));
    account->setAttribute(Qt::WA_StyledBackground, true);
    auto *accountLayout = new QVBoxLayout(account);
    accountLayout->setContentsMargins(28, 8, 28, 28);
    accountLayout->setSpacing(0);

    m_googleValue = new QLabel(account);
    m_googleValue->setObjectName(QStringLiteral("value"));
    m_sessionValue = new QLabel(account);
    m_sessionValue->setObjectName(QStringLiteral("value"));
    auto *google = new QPushButton(QStringLiteral("Anmelden"), account);
    google->setObjectName(QStringLiteral("link"));
    google->setCursor(Qt::PointingHandCursor);
    auto *pull = new QPushButton(QStringLiteral("Laden"), account);
    pull->setObjectName(QStringLiteral("link"));
    pull->setCursor(Qt::PointingHandCursor);
    accountLayout->addWidget(
        fieldRow(account, QStringLiteral("Google"), m_googleValue, 0, QStringLiteral("google"),
                 &m_rows));
    accountLayout->addWidget(fieldRow(account, QStringLiteral("Anmeldung"), google, 0,
                                      QStringLiteral("anmeldung anmelden google"), &m_rows));
    accountLayout->addWidget(fieldRow(account, QStringLiteral("Blop-Sitzung"), m_sessionValue, 0,
                                      QStringLiteral("sitzung blop"), &m_rows));
    accountLayout->addWidget(fieldRow(account, QStringLiteral("Einstellungen"), pull, 0,
                                      QStringLiteral("einstellungen laden"), &m_rows));
    accountLayout->addStretch(1);

    auto *voicePage = new QWidget(pages);
    voicePage->setObjectName(QStringLiteral("page"));
    voicePage->setAttribute(Qt::WA_StyledBackground, true);
    auto *voiceLayout = new QVBoxLayout(voicePage);
    voiceLayout->setContentsMargins(28, 8, 28, 28);
    voiceLayout->setSpacing(0);
    auto *voice = new QKeySequenceEdit(
        QKeySequence(SettingsSync::voiceHotkey(), QKeySequence::PortableText), voicePage);
    voice->setMaximumSequenceLength(1);
    voice->setClearButtonEnabled(true);
    voice->setFixedSize(148, 30);
    voiceLayout->addWidget(
        fieldRow(voicePage, QStringLiteral("Taste"), voice, 1, QStringLiteral("taste sprache"),
                 &m_rows));
    voiceLayout->addStretch(1);

    auto *toolsPage = new QWidget(pages);
    toolsPage->setObjectName(QStringLiteral("page"));
    toolsPage->setAttribute(Qt::WA_StyledBackground, true);
    auto *toolsLayout = new QVBoxLayout(toolsPage);
    toolsLayout->setContentsMargins(28, 8, 28, 28);
    toolsLayout->setSpacing(0);
    for (const ToolBinding &binding : SettingsSync::toolBindings()) {
        auto *edit = new QKeySequenceEdit(
            QKeySequence(binding.keys, QKeySequence::PortableText), toolsPage);
        edit->setMaximumSequenceLength(1);
        edit->setClearButtonEnabled(true);
        edit->setFixedSize(148, 30);
        toolsLayout->addWidget(
            fieldRow(toolsPage, binding.label, edit, 2, binding.label, &m_rows));
        m_toolEdits.insert(binding.id, edit);
        const QString id = binding.id;
        connect(edit, &QKeySequenceEdit::editingFinished, this, [this, edit, id]() {
            SettingsSync::setToolBinding(id, portable(edit));
            if (SettingsSync::signedIn()) {
                QString error;
                SettingsSync::upload(&error);
            }
        });
    }
    toolsLayout->addStretch(1);

    pages->addWidget(account);
    pages->addWidget(voicePage);
    pages->addWidget(toolsPage);

    auto *titleBlock = new QWidget(content);
    titleBlock->setObjectName(QStringLiteral("titles"));
    titleBlock->setAttribute(Qt::WA_StyledBackground, true);
    auto *titleLayout = new QVBoxLayout(titleBlock);
    titleLayout->setContentsMargins(28, 6, 28, 10);
    titleLayout->setSpacing(2);
    m_pageTitle = new QLabel(titleBlock);
    m_pageTitle->setObjectName(QStringLiteral("pageTitle"));
    m_pageLead = new QLabel(titleBlock);
    m_pageLead->setObjectName(QStringLiteral("pageLead"));
    m_pageLead->setWordWrap(true);
    titleLayout->addWidget(m_pageTitle);
    titleLayout->addWidget(m_pageLead);

    contentLayout->addWidget(titleBlock);
    contentLayout->addWidget(pages, 1);

    for (int i = 0; i < 3; ++i) {
        auto *railButton = new QToolButton(rail);
        railButton->setObjectName(QStringLiteral("railButton"));
        railButton->setCheckable(true);
        railButton->setAutoRaise(true);
        railButton->setFocusPolicy(Qt::NoFocus);
        railButton->setCursor(Qt::PointingHandCursor);
        railButton->setFixedSize(36, 36);
        railButton->setIconSize(QSize(18, 18));
        railButton->setToolTip(names[i]);
        railLayout->addWidget(railButton, 0, Qt::AlignHCenter);
        m_railButtons.append(railButton);

        auto *navButton = new QPushButton(names[i], nav);
        navButton->setObjectName(QStringLiteral("navButton"));
        navButton->setCheckable(true);
        navButton->setFocusPolicy(Qt::NoFocus);
        navButton->setCursor(Qt::PointingHandCursor);
        navButton->setIconSize(QSize(16, 16));
        navButton->setProperty("keywords", keywords[i]);
        navButton->setProperty("title", pageTitles[i]);
        navButton->setProperty("lead", leads[i]);
        navLayout->addWidget(navButton);
        m_navButtons.append(navButton);

        connect(railButton, &QToolButton::clicked, this, [this, i]() { showPage(i); });
        connect(navButton, &QPushButton::clicked, this, [this, i]() { showPage(i); });
    }
    railLayout->addStretch(1);
    navLayout->addStretch(1);

    root->addWidget(rail);
    root->addWidget(nav);
    root->addWidget(content, 1);

    setStyleSheet(QStringLiteral(
        "QWidget#setup { background: #23252A; }"
        "QWidget#rail, QWidget#nav { background: #1A1916; }"
        "QWidget#nav { border-right: 1px solid rgba(255,255,255,0.06); }"
        "QWidget#content, QWidget#pages, QWidget#page, QWidget#header, QWidget#titles {"
        " background: #23252A; }"
        "QLabel#section { color: #8B909A; font-size: 11px; font-weight: 700;"
        " padding: 16px 10px 6px 10px; background: transparent; }"
        "QLabel#pageTitle { color: #F4F5F7; font-size: 20px; font-weight: 650;"
        " background: transparent; }"
        "QLabel#pageLead { color: #9AA0AA; font-size: 13px; background: transparent; }"
        "QLabel#fieldLabel { color: #F4F5F7; font-size: 13px; background: transparent; }"
        "QLabel#value { color: #C8CDD6; font-size: 13px; background: transparent; }"
        "QWidget#row { background: transparent; border-bottom: 1px solid rgba(255,255,255,0.08); }"
        "QLineEdit#search { background: rgba(255,255,255,0.07); color: #F4F5F7; border: none;"
        " border-radius: 8px; padding: 6px 12px; }"
        "QLineEdit#search:focus { background: rgba(255,255,255,0.10);"
        " border: 1px solid rgba(62,123,255,0.55); }"
        "QToolButton#railButton { background: transparent; border: none; border-radius: 8px; }"
        "QToolButton#railButton:hover { background: rgba(255,255,255,0.06); }"
        "QToolButton#railButton:checked { background: rgba(62,123,255,0.22); }"
        "QPushButton#navButton { background: transparent; color: #C8CDD6; border: none;"
        " border-radius: 8px; padding: 8px 10px; text-align: left; font-size: 13px; font-weight: 500; }"
        "QPushButton#navButton:hover:!checked { background: rgba(255,255,255,0.05); color: #F4F5F7; }"
        "QPushButton#navButton:checked { background: #3E7BFF; color: #FFFFFF; font-weight: 600; }"
        "QKeySequenceEdit { background: rgba(255,255,255,0.07); color: #F4F5F7; border: none;"
        " border-radius: 6px; padding: 0 8px; selection-background-color: #3E7BFF; }"
        "QKeySequenceEdit:focus { border: 1px solid rgba(62,123,255,0.7); }"
        "QPushButton#fertig, QPushButton#link { background: transparent; color: #5B9DFF;"
        " border: none; font-weight: 600; }"
        "QPushButton#fertig { border-radius: 6px; padding: 6px 10px; }"
        "QPushButton#fertig:hover { background: rgba(255,255,255,0.08); }"
        "QPushButton#link:hover { color: #8BB6FF; }"));

    auto bindSearch = [this](QLineEdit *edit) {
        connect(edit, &QLineEdit::textChanged, this, [this, edit](const QString &text) {
            if (m_filterLock)
                return;
            m_filterLock = true;
            if (edit != m_navSearch)
                m_navSearch->setText(text);
            if (edit != m_pageSearch)
                m_pageSearch->setText(text);
            m_filterLock = false;
            applyFilter(text);
        });
    };
    bindSearch(m_navSearch);
    bindSearch(m_pageSearch);

    connect(google, &QPushButton::clicked, this, [this]() {
        m_googleValue->setText(QStringLiteral("Browser öffnet sich …"));
        QString error;
        if (SettingsSync::signIn(&error).isEmpty())
            m_googleValue->setText(error);
        else
            refresh();
    });
    connect(pull, &QPushButton::clicked, this, [this, voice]() {
        QString error;
        if (!SettingsSync::pull(&error)) {
            m_googleValue->setText(error.isEmpty() ? QStringLiteral("Laden fehlgeschlagen.")
                                                   : error);
            return;
        }
        voice->setKeySequence(
            QKeySequence(SettingsSync::voiceHotkey(), QKeySequence::PortableText));
        for (const ToolBinding &binding : SettingsSync::toolBindings()) {
            if (QKeySequenceEdit *edit = m_toolEdits.value(binding.id))
                edit->setKeySequence(QKeySequence(binding.keys, QKeySequence::PortableText));
        }
        refresh();
        emit voiceHotkeyChanged();
    });
    connect(voice, &QKeySequenceEdit::editingFinished, this, [this, voice]() {
        SettingsSync::setVoiceHotkey(portable(voice));
        if (SettingsSync::signedIn()) {
            QString error;
            SettingsSync::upload(&error);
        }
        emit voiceHotkeyChanged();
    });
    connect(done, &QPushButton::clicked, this, &SetupWindow::saveAndClose);

    showPage(0);
    refresh();
}

void SetupWindow::showPage(int index) {
    if (index < 0 || index >= m_navButtons.size())
        return;
    m_page = index;
    if (m_pages)
        m_pages->setCurrentIndex(index);
    for (int i = 0; i < m_navButtons.size(); ++i) {
        m_navButtons.at(i)->setChecked(i == index);
        m_railButtons.at(i)->setChecked(i == index);
        const bool on = i == index;
        const Glyph glyph = i == 0 ? Glyph::Person : (i == 1 ? Glyph::Wave : Glyph::Pen);
        m_navButtons.at(i)->setIcon(glyphIcon(glyph, on ? Qt::white : QColor(0xB8, 0xBC, 0xC4), 16));
        m_railButtons.at(i)->setIcon(
            glyphIcon(glyph, on ? QColor(0x3E, 0x7B, 0xFF) : QColor(0xB8, 0xBC, 0xC4), 18));
    }
    if (m_pageTitle)
        m_pageTitle->setText(m_navButtons.at(index)->property("title").toString());
    if (m_pageLead)
        m_pageLead->setText(m_navButtons.at(index)->property("lead").toString());
}

void SetupWindow::applyFilter(const QString &text) {
    const QString needle = text.trimmed().toLower();
    QList<bool> titleHit;
    titleHit.reserve(m_navButtons.size());
    for (QPushButton *button : m_navButtons) {
        const QString keywords = button->property("keywords").toString();
        titleHit.append(needle.isEmpty() || button->text().toLower().contains(needle) ||
                        keywords.contains(needle));
    }
    QList<bool> rowHit;
    rowHit.reserve(m_navButtons.size());
    for (int i = 0; i < m_navButtons.size(); ++i)
        rowHit.append(false);
    for (QWidget *row : m_rows) {
        const int page = row->property("page").toInt();
        const bool hit = needle.isEmpty() || titleHit.value(page) ||
                         row->property("needle").toString().contains(needle);
        if (hit && page >= 0 && page < rowHit.size())
            rowHit[page] = true;
        row->setVisible(hit);
    }
    int first = -1;
    for (int i = 0; i < m_navButtons.size(); ++i) {
        const bool show = needle.isEmpty() || titleHit.at(i) || rowHit.at(i);
        m_navButtons.at(i)->setVisible(show);
        m_railButtons.at(i)->setVisible(show);
        if (show && first < 0)
            first = i;
    }
    if (first >= 0 && !m_navButtons.at(m_page)->isVisible())
        showPage(first);
}

void SetupWindow::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
#if defined(Q_OS_WIN)
    darkCaption(reinterpret_cast<HWND>(winId()));
#endif
}

void SetupWindow::present() {
    resize(1080, 640);
    setMinimumSize(920, 520);
    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        const QRect area = screen->availableGeometry();
        move(area.center().x() - width() / 2, area.center().y() - height() / 2);
    }
    showNormal();
    raise();
    activateWindow();
}

void SetupWindow::refresh() {
    const bool google = SettingsSync::signedIn();
    const bool study = !SettingsSync::studySessionId().isEmpty();
    if (m_googleValue)
        m_googleValue->setText(google ? QStringLiteral("Verbunden")
                                      : QStringLiteral("Nicht verbunden"));
    if (m_sessionValue)
        m_sessionValue->setText(study ? QStringLiteral("Angemeldet")
                                      : QStringLiteral("Nicht angemeldet"));
}

void SetupWindow::saveAndClose() {
    QSettings settings;
    settings.setValue(QStringLiteral("assistant/setupDone"), true);
    hide();
    emit finished();
}
