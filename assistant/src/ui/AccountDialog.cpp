#include "AccountDialog.h"

#include "core/SettingsSync.h"

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

AccountDialog::AccountDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("Blop-Konto"));
    setModal(true);
    resize(420, 280);

    auto *layout = new QVBoxLayout(this);
    auto *status = new QLabel(this);
    status->setWordWrap(true);
    auto refresh = [status]() {
        status->setText(SettingsSync::signedIn()
                            ? QStringLiteral("Google-Konto von Blop ist verbunden. "
                                             "Die Werkzeug-Tasten werden daraus übernommen.")
                            : QStringLiteral("Noch kein Google-Konto. Melde dich mit demselben "
                                             "Konto an wie in Blop."));
    };
    refresh();
    layout->addWidget(status);

    auto *google = new QPushButton(QStringLiteral("Mit Google anmelden"), this);
    layout->addWidget(google);
    auto *pull = new QPushButton(QStringLiteral("Einstellungen laden"), this);
    layout->addWidget(pull);

    auto *model = new QLineEdit(SettingsSync::openRouterModel(), this);
    model->setPlaceholderText(QStringLiteral("OpenRouter-Modell"));
    auto *key = new QLineEdit(SettingsSync::openRouterKey(), this);
    key->setEchoMode(QLineEdit::Password);
    key->setPlaceholderText(QStringLiteral("OpenRouter-Schlüssel"));
    layout->addWidget(model);
    layout->addWidget(key);

    auto *save = new QPushButton(QStringLiteral("Schlüssel speichern"), this);
    layout->addWidget(save);
    auto *hint = new QLabel(
        QStringLiteral("Der Schlüssel liegt in deiner Datei assistent-einstellungen.json "
                       "und geht mit dem Google-Konto mit."),
        this);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    connect(google, &QPushButton::clicked, this, [status, refresh]() {
        QString error;
        status->setText(QStringLiteral("Browser öffnet sich …"));
        if (SettingsSync::signIn(&error).isEmpty())
            status->setText(error);
        else
            refresh();
    });
    connect(pull, &QPushButton::clicked, this, [status]() {
        QString error;
        if (!SettingsSync::pull(&error))
            status->setText(error.isEmpty() ? QStringLiteral("Laden fehlgeschlagen.") : error);
        else
            status->setText(QStringLiteral("Einstellungen geladen."));
    });
    connect(save, &QPushButton::clicked, this, [model, key, status]() {
        SettingsSync::setOpenRouter(model->text(), key->text());
        QString error;
        if (SettingsSync::signedIn())
            SettingsSync::upload(&error);
        status->setText(error.isEmpty() ? QStringLiteral("Gespeichert.") : error);
    });
}
