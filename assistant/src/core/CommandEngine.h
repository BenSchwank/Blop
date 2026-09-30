#pragma once

#include <QString>

enum class CommandKind {
    Unknown,
    Help,
    OpenFolder,
    OpenUrl,
    LaunchApp,
    CreateNote
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
    QString error;
};

class CommandEngine {
public:
    Command parse(const QString &input) const;
    static QString helpText();
};
