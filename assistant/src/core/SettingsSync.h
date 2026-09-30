#pragma once

#include <QJsonObject>
#include <QString>

class SettingsSync {
public:
    static QString settingsPath();
    static QJsonObject load();
    static QString hotkeyFor(const QString &id);
    static QString openRouterModel();
    static QString openRouterKey();
    static void setOpenRouter(const QString &model, const QString &key);

    static bool signedIn();
    static QString signIn(QString *error);
    static bool pull(QString *error);
    static bool upload(QString *error);
};
