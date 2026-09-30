#pragma once

#include <QString>

class BlopBridge {
public:
    static QString ask(const QString &message, QString *error);
    static bool sendHotkey(const QString &portable, QString *error);
};
