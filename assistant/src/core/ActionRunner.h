#pragma once

#include "CommandEngine.h"

#include <QList>
#include <QString>

struct ActionResult {
    bool ok = false;
    QString message;
};

class QUrl;

class ActionRunner {
public:
    ActionResult run(const Command &command) const;
    ActionResult runAll(const QList<Command> &commands) const;
    ActionResult runText(const QString &text) const;

private:
    QString resolveFolder(const QString &raw, QString *error) const;
    bool openUrl(const QUrl &url, QString *error) const;
    bool launchApp(AppKind app, QString *error) const;
    bool launchBlop(QString *error) const;
    QString createFolder(const QString &name, const QString &place, QString *error) const;
    QString createTextFile(const QString &name, const QString &content, QString *error) const;
};
