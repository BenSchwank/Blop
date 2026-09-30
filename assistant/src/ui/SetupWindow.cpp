#include "SetupWindow.h"

#include "core/SettingsSync.h"

#include <QButtonGroup>
#include <QFont>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QSettings>
#include <QShowEvent>
#include <QStackedWidget>
#include <QVBoxLayout>

#if defined(Q_OS_WIN)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

QString portable(const QKeySequenceEdit *edit) {
    return edit->keySequence().toString(QKeySequence::PortableText);
}

class Mark : public QWidget {
public:
    explicit Mark(QWidget *parent = nullptr) : QWidget(parent) { setFixedSize(28, 28); }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0x2A, 0x2D, 0x33));
        painter.drawEllipse(rect().adjusted(0, 0, -1, -1));
        painter.setBrush(QColor(0x5B, 0x9D, 0xFF));
        painter.drawEllipse(QRectF(9, 9, 10, 10));
    }
};

QWidget *fieldRow(QWidget *parent, const QString &label, QWidget *editor) {
    auto *row = new QWidget(parent);
    row->setObjectName(QStringLiteral("fieldRow"));
    row->setFixedHeight(48);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(16, 6, 16, 6);
    layout->setSpacing(16);
    auto *text = new QLabel(label, row);
    text->setObjectName(QStringLiteral("fieldLabel"));
    editor->setObjectName(QStringLiteral("keyEdit"));
    editor->setFixedSize(148, 32);
    layout->addWidget(text, 1);
    layout->addWidget(editor, 0, Qt::AlignRight | Qt::AlignVCenter);
    return row;
}

QFrame *hairline(QWidget *parent) {
    auto *line = new QFrame(parent);
    line->setObjectName(QStringLiteral("hairline"));
    line->setAttribute(Qt::WA_StyledBackground, true);
    line->setFrameShape(QFrame::NoFrame);
    line->setFixedHeight(1);
    return line;
}

void styleKeyEdit(QKeySequenceEdit *edit) {
    edit->setMaximumSequenceLength(1);
    edit->setClearButtonEnabled(true);
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
    setWindowTitle(QStringLiteral("Blop Assistent"));
    setObjectName(QStringLiteral("setup"));

    QFont ui(QStringLiteral("Segoe UI"));
    ui.setPixelSize(13);
    setFont(ui);

    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *nav = new QWidget(this);
    nav->setObjectName(QStringLiteral("nav"));
    nav->setAttribute(Qt::WA_StyledBackground, true);
    nav->setFixedWidth(232);
    auto *navLayout = new QVBoxLayout(nav);
    navLayout->setContentsMargins(14, 18, 14, 16);
    navLayout->setSpacing(4);

    auto *brand = new QWidget(nav);
    auto *brandLayout = new QHBoxLayout(brand);
    brandLayout->setContentsMargins(6, 0, 6, 12);
    brandLayout->setSpacing(10);
    brandLayout->addWidget(new Mark(brand));
    auto *brandText = new QVBoxLayout;
    brandText->setSpacing(0);
    auto *brandName = new QLabel(QStringLiteral("Blop Assistent"), brand);
    brandName->setObjectName(QStringLiteral("brand"));
    auto *brandSub = new QLabel(QStringLiteral("Einstellungen"), brand);
    brandSub->setObjectName(QStringLiteral("brandSub"));
    brandText->addWidget(brandName);
    brandText->addWidget(brandSub);
    brandLayout->addLayout(brandText, 1);
    navLayout->addWidget(brand);

    auto *pages = new QStackedWidget(this);
    pages->setObjectName(QStringLiteral("pages"));
    pages->setAttribute(Qt::WA_StyledBackground, true);
    auto *group = new QButtonGroup(this);
    group->setExclusive(true);

    struct PageCopy {
        QString title;
        QString lead;
    };
    const PageCopy copy[3] = {
        {QStringLiteral("Konto"),
         QStringLiteral("Anmeldung und die Verbindung, über die die KI Tokens verbraucht.")},
        {QStringLiteral("Spracheingabe"),
         QStringLiteral("Die Taste startet das Sprechen und beendet es wieder.")},
        {QStringLiteral("Werkzeuge"),
         QStringLiteral("Dieselben Kürzel wie in den Blop-Einstellungen.")},
    };

    auto addNav = [&](const QString &name, int index) {
        auto *button = new QPushButton(name, nav);
        button->setObjectName(QStringLiteral("navButton"));
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        button->setFocusPolicy(Qt::NoFocus);
        navLayout->addWidget(button);
        group->addButton(button);
        connect(button, &QPushButton::clicked, this, [this, pages, index]() {
            pages->setCurrentIndex(index);
            showPage(index);
        });
        return button;
    };

    auto *account = new QWidget(pages);
    account->setObjectName(QStringLiteral("page"));
    account->setAttribute(Qt::WA_StyledBackground, true);
    auto *accountLayout = new QVBoxLayout(account);
    accountLayout->setContentsMargins(28, 8, 28, 24);
    accountLayout->setSpacing(16);

    auto *accountCard = new QWidget(account);
    accountCard->setObjectName(QStringLiteral("card"));
    accountCard->setAttribute(Qt::WA_StyledBackground, true);
    auto *accountCardLayout = new QVBoxLayout(accountCard);
    accountCardLayout->setContentsMargins(0, 6, 0, 6);
    accountCardLayout->setSpacing(0);
    auto googleRow = [&](const QString &label, QLabel **value) {
        auto *row = new QWidget(accountCard);
        row->setObjectName(QStringLiteral("fieldRow"));
        row->setFixedHeight(52);
        auto *box = new QHBoxLayout(row);
        box->setContentsMargins(16, 8, 16, 8);
        auto *name = new QLabel(label, row);
        name->setObjectName(QStringLiteral("fieldLabel"));
        *value = new QLabel(row);
        (*value)->setObjectName(QStringLiteral("value"));
        box->addWidget(name);
        box->addStretch(1);
        box->addWidget(*value);
        return row;
    };
    accountCardLayout->addWidget(googleRow(QStringLiteral("Google"), &m_googleValue));
    accountCardLayout->addWidget(hairline(accountCard));
    accountCardLayout->addWidget(googleRow(QStringLiteral("Blop-Sitzung"), &m_sessionValue));
    accountLayout->addWidget(accountCard);

    auto *actions = new QHBoxLayout;
    actions->setSpacing(8);
    auto *google = new QPushButton(QStringLiteral("Mit Google anmelden"), account);
    google->setObjectName(QStringLiteral("primary"));
    google->setCursor(Qt::PointingHandCursor);
    auto *pull = new QPushButton(QStringLiteral("Einstellungen laden"), account);
    pull->setObjectName(QStringLiteral("quiet"));
    pull->setCursor(Qt::PointingHandCursor);
    actions->addWidget(google);
    actions->addWidget(pull);
    actions->addStretch(1);
    accountLayout->addLayout(actions);
    accountLayout->addStretch(1);

    auto *voicePage = new QWidget(pages);
    voicePage->setObjectName(QStringLiteral("page"));
    voicePage->setAttribute(Qt::WA_StyledBackground, true);
    auto *voiceLayout = new QVBoxLayout(voicePage);
    voiceLayout->setContentsMargins(28, 8, 28, 24);
    voiceLayout->setSpacing(0);
    auto *voiceCard = new QWidget(voicePage);
    voiceCard->setObjectName(QStringLiteral("card"));
    voiceCard->setAttribute(Qt::WA_StyledBackground, true);
    auto *voiceCardLayout = new QVBoxLayout(voiceCard);
    voiceCardLayout->setContentsMargins(0, 6, 0, 6);
    auto *voice = new QKeySequenceEdit(
        QKeySequence(SettingsSync::voiceHotkey(), QKeySequence::PortableText), voiceCard);
    styleKeyEdit(voice);
    voiceCardLayout->addWidget(fieldRow(voiceCard, QStringLiteral("Taste"), voice));
    voiceLayout->addWidget(voiceCard);
    voiceLayout->addStretch(1);

    auto *toolsPage = new QWidget(pages);
    toolsPage->setObjectName(QStringLiteral("page"));
    toolsPage->setAttribute(Qt::WA_StyledBackground, true);
    auto *toolsOuter = new QVBoxLayout(toolsPage);
    toolsOuter->setContentsMargins(28, 8, 28, 24);
    toolsOuter->setSpacing(0);
    auto *toolsCard = new QWidget(toolsPage);
    toolsCard->setObjectName(QStringLiteral("card"));
    toolsCard->setAttribute(Qt::WA_StyledBackground, true);
    auto *columns = new QHBoxLayout(toolsCard);
    columns->setContentsMargins(0, 6, 0, 6);
    columns->setSpacing(0);
    auto *leftCol = new QVBoxLayout;
    auto *rightCol = new QVBoxLayout;
    leftCol->setSpacing(0);
    rightCol->setSpacing(0);
    const QVector<ToolBinding> bindings = SettingsSync::toolBindings();
    const int splitAt = (bindings.size() + 1) / 2;
    for (int i = 0; i < bindings.size(); ++i) {
        const ToolBinding &binding = bindings.at(i);
        auto *edit = new QKeySequenceEdit(
            QKeySequence(binding.keys, QKeySequence::PortableText), toolsCard);
        styleKeyEdit(edit);
        QVBoxLayout *column = i < splitAt ? leftCol : rightCol;
        if (column->count() > 0)
            column->addWidget(hairline(toolsCard));
        column->addWidget(fieldRow(toolsCard, binding.label, edit));
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
    columns->addLayout(leftCol, 1);
    auto *split = new QFrame(toolsCard);
    split->setObjectName(QStringLiteral("split"));
    split->setAttribute(Qt::WA_StyledBackground, true);
    split->setFrameShape(QFrame::NoFrame);
    split->setFixedWidth(1);
    columns->addWidget(split);
    columns->addLayout(rightCol, 1);
    toolsOuter->addWidget(toolsCard);
    toolsOuter->addStretch(1);

    pages->addWidget(account);
    pages->addWidget(voicePage);
    pages->addWidget(toolsPage);

    QPushButton *accountNav = addNav(QStringLiteral("Konto"), 0);
    addNav(QStringLiteral("Sprache"), 1);
    addNav(QStringLiteral("Werkzeuge"), 2);
    accountNav->setChecked(true);
    navLayout->addStretch(1);

    auto *chip = new QWidget(nav);
    chip->setObjectName(QStringLiteral("chip"));
    chip->setAttribute(Qt::WA_StyledBackground, true);
    auto *chipLayout = new QVBoxLayout(chip);
    chipLayout->setContentsMargins(12, 10, 12, 10);
    chipLayout->setSpacing(2);
    m_chipGoogle = new QLabel(chip);
    m_chipGoogle->setObjectName(QStringLiteral("chipLine"));
    m_chipSession = new QLabel(chip);
    m_chipSession->setObjectName(QStringLiteral("chipMuted"));
    chipLayout->addWidget(m_chipGoogle);
    chipLayout->addWidget(m_chipSession);
    navLayout->addWidget(chip);

    auto *content = new QWidget(this);
    content->setObjectName(QStringLiteral("content"));
    content->setAttribute(Qt::WA_StyledBackground, true);
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    auto *header = new QWidget(content);
    header->setObjectName(QStringLiteral("header"));
    header->setAttribute(Qt::WA_StyledBackground, true);
    header->setFixedHeight(84);
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(28, 16, 20, 12);
    headerLayout->setSpacing(16);
    auto *titles = new QVBoxLayout;
    titles->setSpacing(2);
    m_pageTitle = new QLabel(copy[0].title, header);
    m_pageTitle->setObjectName(QStringLiteral("pageTitle"));
    m_pageLead = new QLabel(copy[0].lead, header);
    m_pageLead->setObjectName(QStringLiteral("pageLead"));
    m_pageLead->setWordWrap(true);
    titles->addWidget(m_pageTitle);
    titles->addWidget(m_pageLead);
    headerLayout->addLayout(titles, 1);
    auto *start = new QPushButton(QStringLiteral("Fertig"), header);
    start->setObjectName(QStringLiteral("primary"));
    start->setCursor(Qt::PointingHandCursor);
    start->setFixedHeight(34);
    headerLayout->addWidget(start, 0, Qt::AlignTop);
    contentLayout->addWidget(header);
    contentLayout->addWidget(pages, 1);

    root->addWidget(nav);
    root->addWidget(content, 1);

    setProperty("pageCopy0", copy[0].title);
    setProperty("pageLead0", copy[0].lead);
    setProperty("pageCopy1", copy[1].title);
    setProperty("pageLead1", copy[1].lead);
    setProperty("pageCopy2", copy[2].title);
    setProperty("pageLead2", copy[2].lead);

    setStyleSheet(QStringLiteral(
        "QWidget#setup { background: #23252A; }"
        "QWidget#nav { background: #1A1916; }"
        "QWidget#content, QWidget#pages, QWidget#page, QWidget#header { background: #23252A; }"
        "QLabel#brand { color: #F4F5F7; font-size: 14px; font-weight: 650; background: transparent; }"
        "QLabel#brandSub, QLabel#pageLead, QLabel#chipMuted { color: #B8BCC4; font-size: 12px;"
        " background: transparent; }"
        "QLabel#pageTitle { color: #F4F5F7; font-size: 22px; font-weight: 650; background: transparent; }"
        "QLabel#fieldLabel { color: #F4F5F7; font-size: 13px; background: transparent; }"
        "QLabel#value, QLabel#chipLine { color: #F4F5F7; font-size: 13px; font-weight: 600;"
        " background: transparent; }"
        "QWidget#card { background: #2A2D33; border: 1px solid rgba(255,255,255,0.08);"
        " border-radius: 12px; }"
        "QWidget#fieldRow { background: transparent; }"
        "QFrame#hairline { background: rgba(255,255,255,0.08); border: none; max-height: 1px; }"
        "QFrame#split { background: rgba(255,255,255,0.08); border: none; }"
        "QWidget#chip { background: rgba(255,255,255,0.05);"
        " border: 1px solid rgba(255,255,255,0.08); border-radius: 10px; }"
        "QPushButton#navButton { background: transparent; color: #B8BCC4; border: 1px solid transparent;"
        " border-radius: 8px; padding: 9px 12px; text-align: left; font-size: 13px; font-weight: 500; }"
        "QPushButton#navButton:hover { background: rgba(255,255,255,0.05); color: #F4F5F7; }"
        "QPushButton#navButton:checked { background: rgba(91,157,255,0.18); color: #F4F5F7;"
        " border: 1px solid rgba(91,157,255,0.45); font-weight: 600; }"
        "QKeySequenceEdit { background: #1A1916; color: #F4F5F7;"
        " border: 1px solid rgba(255,255,255,0.12); border-radius: 8px; padding: 0 8px;"
        " selection-background-color: #5B9DFF; }"
        "QKeySequenceEdit:focus { border: 1px solid rgba(91,157,255,0.75); }"
        "QPushButton#primary { background: #5B9DFF; color: #0E1116; border: none;"
        " border-radius: 8px; padding: 8px 16px; font-weight: 650; }"
        "QPushButton#primary:hover { background: #74ABFF; }"
        "QPushButton#primary:pressed { background: #3E86F5; }"
        "QPushButton#quiet { background: transparent; color: #F4F5F7;"
        " border: 1px solid rgba(255,255,255,0.16); border-radius: 8px;"
        " padding: 8px 16px; font-weight: 600; }"
        "QPushButton#quiet:hover { background: rgba(91,157,255,0.12); }"));

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
    connect(start, &QPushButton::clicked, this, &SetupWindow::saveAndClose);

    showPage(0);
    refresh();
}

void SetupWindow::showPage(int index) {
    const QString titleKey = QStringLiteral("pageCopy%1").arg(index);
    const QString leadKey = QStringLiteral("pageLead%1").arg(index);
    if (m_pageTitle)
        m_pageTitle->setText(property(titleKey.toUtf8().constData()).toString());
    if (m_pageLead)
        m_pageLead->setText(property(leadKey.toUtf8().constData()).toString());
}

void SetupWindow::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
#if defined(Q_OS_WIN)
    darkCaption(reinterpret_cast<HWND>(winId()));
#else
    Q_UNUSED(event);
#endif
}

void SetupWindow::present() {
    resize(980, 540);
    setMinimumSize(860, 480);
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
    const QString googleText =
        google ? QStringLiteral("Verbunden") : QStringLiteral("Nicht verbunden");
    const QString sessionText =
        study ? QStringLiteral("Angemeldet") : QStringLiteral("Nicht angemeldet");
    if (m_googleValue)
        m_googleValue->setText(googleText);
    if (m_sessionValue)
        m_sessionValue->setText(sessionText);
    if (m_chipGoogle)
        m_chipGoogle->setText(google ? QStringLiteral("Google verbunden")
                                     : QStringLiteral("Google offen"));
    if (m_chipSession)
        m_chipSession->setText(study ? QStringLiteral("Blop angemeldet")
                                     : QStringLiteral("Blop nicht angemeldet"));
}

void SetupWindow::saveAndClose() {
    QSettings settings;
    settings.setValue(QStringLiteral("assistant/setupDone"), true);
    hide();
    emit finished();
}
