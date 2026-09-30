#include "CommandEngine.h"

#include <QRegularExpression>
#include <QUrl>

namespace {

QString fold(QString text) {
    text.replace(QChar(0x00A0), QLatin1Char(' '));
    text.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    return text.trimmed();
}

QString stripLead(QString text) {
    const QStringList leads = {
        QStringLiteral("bitte "),
        QStringLiteral("hey blop "),
        QStringLiteral("ok blop "),
        QStringLiteral("hallo blop "),
        QStringLiteral("hey "),
    };
    bool again = true;
    while (again) {
        again = false;
        for (const QString &lead : leads) {
            if (text.startsWith(lead, Qt::CaseInsensitive)) {
                text = text.mid(lead.size()).trimmed();
                again = true;
            }
        }
    }
    return text;
}

Command unknown(const QString &message) {
    Command command;
    command.kind = CommandKind::Unknown;
    command.error = message;
    return command;
}

bool isHelp(const QString &lower) {
    return lower == QLatin1String("hilfe") || lower == QLatin1String("help") ||
           lower == QLatin1String("befehle") || lower == QLatin1String("was kannst du") ||
           lower == QLatin1String("was kannst du tun");
}

void splitNote(const QString &rest, QString *title, QString *body) {
    const QRegularExpression mitInhalt(
        QStringLiteral("\\s+mit\\s+inhalt\\s+"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpression inhalt(QStringLiteral("\\s+inhalt\\s+"),
                                    QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = mitInhalt.match(rest);
    if (!match.hasMatch())
        match = inhalt.match(rest);
    if (match.hasMatch()) {
        *title = rest.left(match.capturedStart()).trimmed();
        *body = rest.mid(match.capturedEnd()).trimmed();
        return;
    }
    const int colon = rest.indexOf(QLatin1Char(':'));
    if (colon >= 0) {
        *title = rest.left(colon).trimmed();
        *body = rest.mid(colon + 1).trimmed();
        return;
    }
    *title = rest.trimmed();
    *body = QString();
}

Command makeNote(const QString &rest) {
    QString title;
    QString body;
    splitNote(rest, &title, &body);
    if (title.isEmpty()) {
        return unknown(QStringLiteral(
            "Wie soll die Notiz heißen? Zum Beispiel: erstelle notiz Einkauf: Milch"));
    }
    Command command;
    command.kind = CommandKind::CreateNote;
    command.title = title;
    command.text = body;
    return command;
}

bool takeNote(const QString &raw, Command *out) {
    const QRegularExpression withBody(
        QStringLiteral(
            "^(?:(?:erstelle|neue|schreibe|mach|anlegen)\\s+)?(?:eine\\s+|die\\s+)?notiz\\s+(.+)$"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpression notiere(QStringLiteral("^notiere\\s+(.+)$"),
                                     QRegularExpression::CaseInsensitiveOption);
    if (const auto match = withBody.match(raw); match.hasMatch()) {
        *out = makeNote(match.captured(1));
        return true;
    }
    if (const auto match = notiere.match(raw); match.hasMatch()) {
        *out = makeNote(QStringLiteral("Notiz: ") + match.captured(1));
        return true;
    }
    const QRegularExpression bare(
        QStringLiteral("^(?:erstelle|neue|schreibe|mach|anlegen)\\s+(?:eine\\s+|die\\s+)?notiz\\s*$"),
        QRegularExpression::CaseInsensitiveOption);
    if (bare.match(raw).hasMatch()) {
        *out = unknown(QStringLiteral(
            "Wie soll die Notiz heißen? Zum Beispiel: erstelle notiz Einkauf: Milch"));
        return true;
    }
    return false;
}

QUrl addressFrom(const QString &raw) {
    const QString text = raw.trimmed();
    if (text.startsWith(QLatin1String("http://"), Qt::CaseInsensitive) ||
        text.startsWith(QLatin1String("https://"), Qt::CaseInsensitive)) {
        return QUrl::fromUserInput(text);
    }
    if (!text.contains(QLatin1Char(' ')) && text.contains(QLatin1Char('.')))
        return QUrl(QStringLiteral("https://") + text);
    return {};
}

bool looksLikePath(const QString &text) {
    return text.contains(QLatin1Char('\\')) || text.contains(QLatin1Char('/')) ||
           QRegularExpression(QStringLiteral("^[A-Za-z]:")).match(text).hasMatch();
}

AppKind appFrom(const QString &lower) {
    if (lower == QLatin1String("explorer") || lower == QLatin1String("dateiexplorer") ||
        lower == QLatin1String("dateien"))
        return AppKind::Explorer;
    if (lower == QLatin1String("rechner") || lower == QLatin1String("taschenrechner") ||
        lower == QLatin1String("calc"))
        return AppKind::Calculator;
    if (lower == QLatin1String("editor") || lower == QLatin1String("notepad") ||
        lower == QLatin1String("notizblock"))
        return AppKind::Notepad;
    if (lower == QLatin1String("browser") || lower == QLatin1String("chrome") ||
        lower == QLatin1String("edge") || lower == QLatin1String("firefox") ||
        lower == QLatin1String("google"))
        return AppKind::Browser;
    if (lower == QLatin1String("blop"))
        return AppKind::Blop;
    return AppKind::None;
}

bool isFolderAlias(const QString &lower) {
    static const QStringList keys = {
        QStringLiteral("downloads"),     QStringLiteral("download"),
        QStringLiteral("dokumente"),     QStringLiteral("dokument"),
        QStringLiteral("documents"),     QStringLiteral("desktop"),
        QStringLiteral("schreibtisch"),  QStringLiteral("bilder"),
        QStringLiteral("bilderordner"),  QStringLiteral("fotos"),
        QStringLiteral("musik"),         QStringLiteral("videos"),
        QStringLiteral("video"),         QStringLiteral("filme"),
        QStringLiteral("home"),          QStringLiteral("benutzer"),
        QStringLiteral("benutzerordner"),
        QStringLiteral("blopnotizen"),   QStringLiteral("blop notizen"),
        QStringLiteral("notizen"),       QStringLiteral("notizordner"),
    };
    return keys.contains(lower);
}

bool splitVerb(const QString &raw, QString *object) {
    const QRegularExpression re(
        QStringLiteral("^(?:öffne|oeffne|zeige|starte)\\s+(?:mir\\s+)?(?:bitte\\s+)?"
                       "(?:den\\s+|die\\s+|das\\s+|dem\\s+)?(.+)$"),
        QRegularExpression::CaseInsensitiveOption);
    const auto match = re.match(raw);
    if (!match.hasMatch())
        return false;
    *object = match.captured(1).trimmed();
    return !object->isEmpty();
}

QString stripPrefix(const QString &text, const QStringList &prefixes) {
    for (const QString &prefix : prefixes) {
        if (text.startsWith(prefix, Qt::CaseInsensitive))
            return text.mid(prefix.size()).trimmed();
    }
    return text;
}

} // namespace

QString CommandEngine::helpText() {
    return QStringLiteral(
        "Befehle\n"
        "öffne ordner downloads\n"
        "öffne ordner dokumente\n"
        "öffne browser\n"
        "öffne seite example.com\n"
        "starte rechner\n"
        "starte blop\n"
        "erstelle notiz Titel: Inhalt");
}

Command CommandEngine::parse(const QString &input) const {
    const QString raw = stripLead(fold(input));
    if (raw.isEmpty())
        return unknown(QStringLiteral("Sag einen Befehl, oder „Hilfe“."));

    const QString lower = raw.toLower();
    if (isHelp(lower)) {
        Command command;
        command.kind = CommandKind::Help;
        return command;
    }

    if (Command note; takeNote(raw, &note))
        return note;

    QString object;
    if (!splitVerb(raw, &object) && lower.startsWith(QLatin1String("ordner ")))
        object = raw.mid(QStringLiteral("ordner ").size()).trimmed();
    else if (!splitVerb(raw, &object)) {
        const QUrl direct = addressFrom(raw);
        if (direct.isValid()) {
            Command command;
            command.kind = CommandKind::OpenUrl;
            command.text = direct.toString();
            return command;
        }
        return unknown(QStringLiteral("Das kenne ich nicht. Sag „Hilfe“."));
    }

    const QString folderArg = stripPrefix(
        object, {QStringLiteral("ordner ")});
    const bool explicitFolder = folderArg.size() != object.size();
    const QString addressArg = stripPrefix(
        object, {QStringLiteral("seite "), QStringLiteral("url "), QStringLiteral("link "),
                 QStringLiteral("website ")});
    const bool explicitAddress = addressArg.size() != object.size();

    if (explicitFolder) {
        if (folderArg.isEmpty())
            return unknown(QStringLiteral("Welchen Ordner soll ich öffnen?"));
        Command command;
        command.kind = CommandKind::OpenFolder;
        command.text = folderArg;
        return command;
    }

    if (explicitAddress) {
        const QUrl url = addressFrom(addressArg);
        if (!url.isValid())
            return unknown(QStringLiteral("Das ist keine Adresse."));
        Command command;
        command.kind = CommandKind::OpenUrl;
        command.text = url.toString();
        return command;
    }

    const QString key = object.toLower();
    if (key == QLatin1String("ordner"))
        return unknown(QStringLiteral("Welchen Ordner soll ich öffnen?"));
    if (const AppKind app = appFrom(key); app != AppKind::None) {
        Command command;
        command.kind = CommandKind::LaunchApp;
        command.app = app;
        return command;
    }
    if (isFolderAlias(key) || looksLikePath(object)) {
        Command command;
        command.kind = CommandKind::OpenFolder;
        command.text = object;
        return command;
    }
    if (const QUrl url = addressFrom(object); url.isValid()) {
        Command command;
        command.kind = CommandKind::OpenUrl;
        command.text = url.toString();
        return command;
    }
    return unknown(QStringLiteral("Das kenne ich nicht. Sag „Hilfe“."));
}
