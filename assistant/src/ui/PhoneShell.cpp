#include "PhoneShell.h"

#include "AccountDialog.h"
#include "platform/SpeechInput.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

PhoneShell::PhoneShell(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("phone"));
    setWindowTitle(QStringLiteral("Blop Assistent"));

    m_speech = new SpeechInput(this);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 18, 16, 16);
    root->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("Blop Assistent"), this);
    title->setObjectName(QStringLiteral("title"));
    root->addWidget(title);

    auto *bar = new QWidget(this);
    bar->setObjectName(QStringLiteral("bar"));
    auto *row = new QHBoxLayout(bar);
    row->setContentsMargins(10, 8, 8, 8);
    row->setSpacing(8);

    m_edit = new QLineEdit(bar);
    m_edit->setPlaceholderText(QStringLiteral("Befehl eingeben…"));
    m_edit->setClearButtonEnabled(true);
    row->addWidget(m_edit, 1);

    m_mic = new QPushButton(QStringLiteral("●"), bar);
    m_mic->setObjectName(QStringLiteral("mic"));
    m_mic->setFixedSize(40, 40);
    m_mic->setCursor(Qt::PointingHandCursor);
    m_mic->setToolTip(QStringLiteral("Tippen und sprechen"));
    row->addWidget(m_mic);
    auto *account = new QPushButton(QStringLiteral("Konto"), bar);
    row->addWidget(account);
    root->addWidget(bar);

    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("status"));
    m_status->setWordWrap(true);
    root->addWidget(m_status);

    auto *hint = new QLabel(CommandEngine::helpText(), this);
    hint->setObjectName(QStringLiteral("hint"));
    hint->setWordWrap(true);
    root->addWidget(hint);
    root->addStretch(1);

    setStyleSheet(QStringLiteral(
        "QWidget#phone { background: #1A1C1F; }"
        "QWidget#bar { background: #24262B; border: 1px solid #3A3F48; border-radius: 16px; }"
        "QLabel#title { color: #5B9DFF; font-size: 20px; font-weight: 700; }"
        "QLabel#status { color: #F2F4F7; font-size: 15px; }"
        "QLabel#hint { color: #A8AEB8; }"
        "QLineEdit { background: #16181C; color: #F4F6F8; border: 1px solid #3A3F48;"
        " border-radius: 10px; padding: 8px 10px; selection-background-color: #5B9DFF; }"
        "QLineEdit:focus { border: 1px solid #5B9DFF; }"
        "QPushButton#mic { background: #343840; color: #F4F6F8; border: none; border-radius: 20px; }"
        "QPushButton#mic[listening=\"true\"] { background: #5B9DFF; color: #0E1116; }"));

    connect(account, &QPushButton::clicked, this, [this]() {
        AccountDialog dialog(this);
        dialog.exec();
    });
    connect(m_edit, &QLineEdit::returnPressed, this, [this]() { runCommand(m_edit->text()); });
    connect(m_mic, &QPushButton::clicked, m_speech, &SpeechInput::start);
    connect(m_speech, &SpeechInput::recognized, this, [this](const QString &text) {
        m_edit->setText(text);
        runCommand(text);
    });
    connect(m_speech, &SpeechInput::failed, this, [this](const QString &reason) {
        m_status->setText(reason);
    });
    connect(m_speech, &SpeechInput::listeningChanged, this, &PhoneShell::setListening);
}

void PhoneShell::runCommand(const QString &text) {
    const ActionResult result = m_runner.run(m_engine.parse(text));
    m_status->setText(result.message);
    m_edit->selectAll();
}

void PhoneShell::setListening(bool on) {
    m_mic->setProperty("listening", on);
    m_mic->style()->unpolish(m_mic);
    m_mic->style()->polish(m_mic);
    m_mic->update();
}
