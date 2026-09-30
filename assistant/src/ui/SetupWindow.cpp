#include "SetupWindow.h"

#include "core/SettingsSync.h"

#include <QCloseEvent>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSettings>
#include <QVBoxLayout>

namespace {

QString portable(const QKeySequenceEdit *edit) {
    return edit->keySequence().toString(QKeySequence::PortableText);
}

} // namespace

SetupWindow::SetupWindow(QWidget *parent) : QWidget(parent) {
    setWindowTitle(QStringLiteral("Blop Assistent"));
    setObjectName(QStringLiteral("setup"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    root->addWidget(scroll);

    auto *page = new QWidget(scroll);
    scroll->setWidget(page);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(0);

    auto *card = new QWidget(page);
    card->setObjectName(QStringLiteral("card"));
    card->setMaximumWidth(560);
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(28, 28, 28, 28);
    cardLayout->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("Blop Assistent"), card);
    title->setObjectName(QStringLiteral("title"));
    cardLayout->addWidget(title);

    auto *lead = new QLabel(
        QStringLiteral("Melde dich an und verbinde die Tasten, bevor die Notch am "
                       "Bildschirmrand liegt."),
        card);
    lead->setObjectName(QStringLiteral("lead"));
    lead->setWordWrap(true);
    cardLayout->addWidget(lead);

    m_status = new QLabel(card);
    m_status->setObjectName(QStringLiteral("status"));
    m_status->setWordWrap(true);
    cardLayout->addWidget(m_status);

    auto *google = new QPushButton(QStringLiteral("Mit Google anmelden"), card);
    auto *pull = new QPushButton(QStringLiteral("Einstellungen laden"), card);
    cardLayout->addWidget(google);
    cardLayout->addWidget(pull);

    auto *hotkeyTitle = new QLabel(QStringLiteral("Spracheingabe"), card);
    hotkeyTitle->setObjectName(QStringLiteral("section"));
    cardLayout->addWidget(hotkeyTitle);
    auto *voiceHint = new QLabel(
        QStringLiteral("Diese Taste startet und beendet das Sprechen."), card);
    voiceHint->setObjectName(QStringLiteral("lead"));
    voiceHint->setWordWrap(true);
    cardLayout->addWidget(voiceHint);

    auto *voice = new QKeySequenceEdit(
        QKeySequence(SettingsSync::voiceHotkey(), QKeySequence::PortableText), card);
    voice->setMaximumSequenceLength(1);
    cardLayout->addWidget(voice);

    auto *toolsTitle = new QLabel(QStringLiteral("Werkzeuge in Blop"), card);
    toolsTitle->setObjectName(QStringLiteral("section"));
    cardLayout->addWidget(toolsTitle);

    for (const ToolBinding &binding : SettingsSync::toolBindings()) {
        auto *row = new QWidget(card);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        auto *label = new QLabel(binding.label, row);
        label->setMinimumWidth(140);
        auto *edit = new QKeySequenceEdit(
            QKeySequence(binding.keys, QKeySequence::PortableText), row);
        edit->setMaximumSequenceLength(1);
        rowLayout->addWidget(label);
        rowLayout->addWidget(edit, 1);
        cardLayout->addWidget(row);
        m_toolEdits.insert(binding.id, edit);
        const QString id = binding.id;
        connect(edit, &QKeySequenceEdit::editingFinished, this, [this, edit, id]() {
            SettingsSync::setToolBinding(id, portable(edit));
            if (SettingsSync::signedIn()) {
                QString error;
                SettingsSync::upload(&error);
            }
            refresh();
        });
    }

    auto *start = new QPushButton(QStringLiteral("Assistent starten"), card);
    start->setObjectName(QStringLiteral("start"));
    cardLayout->addWidget(start);

    layout->addWidget(card, 0, Qt::AlignHCenter);
    layout->addStretch(1);

    setStyleSheet(QStringLiteral(
        "QWidget#setup { background: #1A1C1F; }"
        "QScrollArea { background: #1A1C1F; border: none; }"
        "QWidget#card { background: #24262B; border-radius: 20px; }"
        "QLabel#title { color: #5B9DFF; font-size: 28px; font-weight: 700; }"
        "QLabel#section { color: #F4F6F8; font-size: 16px; font-weight: 650; }"
        "QLabel#lead, QLabel#status { color: #C5CAD3; }"
        "QLabel { color: #F4F6F8; }"
        "QLineEdit, QKeySequenceEdit { background: #16181C; color: #F4F6F8;"
        " border: 1px solid #3A3F48; border-radius: 10px; padding: 6px 10px; }"
        "QPushButton { background: #343840; color: #F4F6F8; border: none;"
        " border-radius: 10px; padding: 10px 14px; text-align: left; }"
        "QPushButton#start { background: #5B9DFF; color: #0E1116; font-weight: 700;"
        " text-align: center; }"));

    connect(google, &QPushButton::clicked, this, [this]() {
        m_status->setText(QStringLiteral("Browser öffnet sich …"));
        QString error;
        if (SettingsSync::signIn(&error).isEmpty())
            m_status->setText(error);
        else
            refresh();
    });
    connect(pull, &QPushButton::clicked, this, [this, voice]() {
        QString error;
        if (!SettingsSync::pull(&error)) {
            m_status->setText(error.isEmpty() ? QStringLiteral("Laden fehlgeschlagen.") : error);
            return;
        }
        voice->setKeySequence(
            QKeySequence(SettingsSync::voiceHotkey(), QKeySequence::PortableText));
        for (const ToolBinding &binding : SettingsSync::toolBindings()) {
            if (QKeySequenceEdit *edit = m_toolEdits.value(binding.id))
                edit->setKeySequence(
                    QKeySequence(binding.keys, QKeySequence::PortableText));
        }
        refresh();
        m_status->setText(QStringLiteral("Einstellungen geladen."));
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

    refresh();
}

void SetupWindow::present() {
    resize(440, 720);
    setMinimumSize(400, 520);
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
    m_status->setText(
        QStringLiteral("Google: %1\nBlop-Sitzung: %2\n"
                       "Die KI nutzt das Token-Guthaben dieser Sitzung.")
            .arg(google ? QStringLiteral("verbunden") : QStringLiteral("nicht verbunden"),
                 study ? QStringLiteral("angemeldet") : QStringLiteral("nicht angemeldet")));
}

void SetupWindow::saveAndClose() {
    QSettings settings;
    settings.setValue(QStringLiteral("assistant/setupDone"), true);
    hide();
    emit finished();
}
