#pragma once

#include <QList>
#include <QString>
#include <QStringList>

enum class CommandKind {
    Unknown,
    Help,
    OpenFolder,
    OpenUrl,
    LaunchApp,
    CreateNote,
    CreateFolder,
    CreateTextFile,
    ComposeNote,
    Explain,
    SelectTool
};

enum class AppKind {
    None,
    Explorer,
    Calculator,
    Notepad,
    Browser,
    Blop
};

struct Command {
    CommandKind kind = CommandKind::Unknown;
    AppKind app = AppKind::None;
    QString title;
    QString text;
    QString heading;
    QStringList points;
    QString toolId;
    bool generate = false;
    bool alsoWrite = false;
    QString error;
};

class CommandEngine {
public:
    Command parse(const QString &input) const;
    QList<Command> parseAll(const QString &input) const;
    static QString helpText();

private:
    Command parseClause(const QString &input) const;
};
