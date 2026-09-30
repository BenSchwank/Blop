#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>

struct ToolBinding {
    QString id;
    QString label;
    QString keys;
};

class SettingsSync {
public:
    static QString settingsPath();
    static QJsonObject load();
    static QString hotkeyFor(const QString &id);
    static QString openRouterModel();
    static QString openRouterKey();
    static QString studySessionId();
    static QString voiceHotkey();
    static QVector<ToolBinding> toolBindings();
    static void setOpenRouter(const QString &model, const QString &key);
    static void setVoiceHotkey(const QString &keys);
    static void setToolBinding(const QString &id, const QString &keys);

    static bool signedIn();
    static QString signIn(QString *error);
    static bool pull(QString *error, bool keepNewerLocal = false);
    static bool upload(QString *error);
};
