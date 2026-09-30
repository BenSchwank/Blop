#pragma once

#include "CommandEngine.h"

#include <QString>

struct ActionResult {
    bool ok = false;
    QString message;
};

class QUrl;

class ActionRunner {
public:
    ActionResult run(const Command &command) const;

private:
    QString resolveFolder(const QString &raw, QString *error) const;
    bool openUrl(const QUrl &url, QString *error) const;
    bool launchApp(AppKind app, QString *error) const;
    bool launchBlop(QString *error) const;
    QString createFolder(const QString &name, const QString &place, QString *error) const;
};
