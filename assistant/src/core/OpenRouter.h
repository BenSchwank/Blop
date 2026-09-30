#pragma once

#include "CommandEngine.h"

#include <QList>
#include <QString>
#include <QStringList>

class OpenRouter {
public:
    static bool note(const QString &prompt, QString *heading, QStringList *points,
                     QString *error);
    static bool explain(const QString &prompt, QString *answer, QString *error);
    static bool plan(const QString &utterance, QList<Command> *commands, QString *error);
};
