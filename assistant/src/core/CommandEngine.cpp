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
        QStringLiteral("hey blop assistent "),
        QStringLiteral("blop assistent "),
        QStringLiteral("blop-assistent "),
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

bool takeCreateFolder(const QString &raw, Command *out) {
    const QRegularExpression re(
        QStringLiteral(
            "^(?:erstelle|neuer|neues|mache)\\s+(?:mir\\s+)?(?:einen\\s+|ein\\s+)?ordner\\s+(.+)$"),
        QRegularExpression::CaseInsensitiveOption);
    const auto match = re.match(raw);
    if (!match.hasMatch())
        return false;

    QString rest = match.captured(1).trimmed();
    QString place = QStringLiteral("desktop");
    const QRegularExpression trailingPlace(
        QStringLiteral("\\s+auf\\s+(?:dem\\s+|meinem\\s+)?(desktop|schreibtisch)\\s*$"),
        QRegularExpression::CaseInsensitiveOption);
    if (const auto placeMatch = trailingPlace.match(rest); placeMatch.hasMatch()) {
        place = placeMatch.captured(1).toLower();
        rest = rest.left(placeMatch.capturedStart()).trimmed();
    }
    const QRegularExpression leadingPlace(
        QStringLiteral("^(?:auf\\s+(?:dem\\s+|meinem\\s+)?)?(desktop|schreibtisch)\\s+(?:namens\\s+)?(.+)$"),
        QRegularExpression::CaseInsensitiveOption);
    if (const auto lead = leadingPlace.match(rest); lead.hasMatch()) {
        place = lead.captured(1).toLower();
        rest = lead.captured(2).trimmed();
    } else if (rest.startsWith(QLatin1String("namens "), Qt::CaseInsensitive)) {
        rest = rest.mid(QStringLiteral("namens ").size()).trimmed();
    }
    rest.remove(QRegularExpression(QStringLiteral("^[\"']|[\"']$")));
    if (rest.isEmpty() || rest.contains(QLatin1Char('/')) || rest.contains(QLatin1Char('\\')) ||
        rest.contains(QRegularExpression(QStringLiteral("[<>:\"|?*]")))) {
        *out = unknown(QStringLiteral(
            "Wie soll der Ordner heißen? Zum Beispiel: erstelle ordner Test 1 2 auf dem desktop"));
        return true;
    }
    Command command;
    command.kind = CommandKind::CreateFolder;
    command.title = place;
    command.text = rest;
    *out = command;
    return true;
}

bool takeTextFile(const QString &raw, Command *out) {
    const QRegularExpression re(
        QStringLiteral(
            "^(?:erstelle|schreibe|mach|mache|anlegen|neue|lege)\\s+(?:mir\\s+)?"
            "(?:eine\\s+|die\\s+)?(?:txt[\\s-]*datei|text[\\s-]*datei|textdatei|txt)"
            "(?:\\s+(.+))?$"),
        QRegularExpression::CaseInsensitiveOption);
    const auto match = re.match(raw);
    if (!match.hasMatch())
        return false;

    QString rest = match.captured(1).trimmed();
    QString body;
    const QRegularExpression inhalt(
        QStringLiteral("(?:^|\\s)(?:mit\\s+(?:dem\\s+)?)?inhalt\\s+(.+)$"),
        QRegularExpression::CaseInsensitiveOption);
    if (const auto bodyMatch = inhalt.match(rest); bodyMatch.hasMatch()) {
        body = bodyMatch.captured(1).trimmed();
        rest = rest.left(bodyMatch.capturedStart()).trimmed();
    }
    rest.remove(QRegularExpression(QStringLiteral("^(?:namens|name)\\s+"),
                                   QRegularExpression::CaseInsensitiveOption));
    rest.remove(QRegularExpression(QStringLiteral("^[\"']|[\"']$")));
    rest.remove(QRegularExpression(
        QStringLiteral("\\s+auf\\s+(?:dem\\s+|meinem\\s+)?(?:desktop|schreibtisch)\\s*$"),
        QRegularExpression::CaseInsensitiveOption));
    if (body.isEmpty()) {
        *out = unknown(QStringLiteral(
            "Was soll in die Datei? Zum Beispiel: erstelle eine txt datei mit dem inhalt Hallo"));
        return true;
    }
    Command command;
    command.kind = CommandKind::CreateTextFile;
    command.title = rest;
    command.text = body;
    *out = command;
    return true;
}

QStringList splitClauses(const QString &raw) {
    const QStringList verbs = {
        QStringLiteral("öffne"),     QStringLiteral("oeffne"),   QStringLiteral("zeige"),
        QStringLiteral("starte"),    QStringLiteral("erstelle"), QStringLiteral("neue"),
        QStringLiteral("schreibe"),  QStringLiteral("schreib"),  QStringLiteral("mach"),
        QStringLiteral("mache"),     QStringLiteral("anlegen"),  QStringLiteral("lege"),
        QStringLiteral("notiere"),   QStringLiteral("nimm"),
    };
    const QRegularExpression und(QStringLiteral("\\s+und\\s+"),
                                 QRegularExpression::CaseInsensitiveOption);
    QStringList parts;
    int last = 0;
    auto matches = und.globalMatch(raw);
    while (matches.hasNext()) {
        const auto match = matches.next();
        const QString after = raw.mid(match.capturedEnd()).trimmed().toLower();
        bool startsVerb = false;
        for (const QString &verb : verbs) {
            if (after.startsWith(verb)) {
                startsVerb = true;
                break;
            }
        }
        if (!startsVerb)
            continue;
        parts.append(raw.mid(last, match.capturedStart() - last).trimmed());
        last = match.capturedEnd();
    }
    parts.append(raw.mid(last).trimmed());
    parts.removeAll(QString());
    return parts;
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

QString clipTitle(QString title) {
    title = title.trimmed();
    title.remove(QRegularExpression(QStringLiteral("^[\"']|[\"']$")));
    title.remove(QRegularExpression(QStringLiteral("[.]+$")));
    const QStringList stop = {
        QStringLiteral(" und "), QStringLiteral(","), QStringLiteral(" mach"),
        QStringLiteral(" mit "), QStringLiteral(" überschrift"),
        QStringLiteral(" ueberschrift"), QStringLiteral(" schreib"),
        QStringLiteral(" notier"), QStringLiteral(" punkt"),
    };
    int cut = title.size();
    const QString lower = title.toLower();
    for (const QString &mark : stop) {
        const int at = lower.indexOf(mark);
        if (at > 0 && at < cut)
            cut = at;
    }
    return title.left(cut).trimmed();
}

bool takeCompose(const QString &raw, Command *out) {
    const QString lower = raw.toLower();
    const bool explain = lower.startsWith(QLatin1String("erklär")) ||
                         lower.startsWith(QLatin1String("erklaer")) ||
                         lower.startsWith(QLatin1String("erklaere"));
    if (explain) {
        Command command;
        command.kind = CommandKind::Explain;
        command.generate = true;
        command.text = raw;
        command.alsoWrite = lower.contains(QLatin1String("schreib")) ||
                            lower.contains(QLatin1String("notiz")) ||
                            lower.contains(QLatin1String("notier"));
        QRegularExpression named(
            QStringLiteral("notiz\\s+(?:namens\\s+)?([^,]+)$"),
            QRegularExpression::CaseInsensitiveOption);
        if (const auto match = named.match(raw); match.hasMatch())
            command.title = clipTitle(match.captured(1));
        *out = command;
        return true;
    }

    QString toolId;
    if (lower.contains(QLatin1String("textmarker")) ||
        lower.contains(QLatin1String("highlighter")))
        toolId = QStringLiteral("marker");
    else if (lower.contains(QLatin1String("stift 3")) ||
             lower.contains(QLatin1String("stift drei")))
        toolId = QStringLiteral("pen3");
    else if (lower.contains(QLatin1String("stift 2")) ||
             lower.contains(QLatin1String("stift zwei")))
        toolId = QStringLiteral("pen2");
    else if (lower.contains(QLatin1String("stift 1")) ||
             lower.contains(QLatin1String("stift eins")) ||
             lower.contains(QLatin1String("den stift")) ||
             lower.contains(QLatin1String("marker")))
        toolId = lower.contains(QLatin1String("marker")) ? QStringLiteral("marker")
                                                        : QStringLiteral("pen1");

    const bool mentionsNote =
        lower.contains(QLatin1String("notiz")) || lower.contains(QLatin1String("nenn")) ||
        lower.contains(QLatin1String("überschrift")) ||
        lower.contains(QLatin1String("ueberschrift")) ||
        lower.contains(QLatin1String("einen blop")) ||
        lower.contains(QLatin1String("einen blog"));
    const bool toolOnly =
        !mentionsNote && !toolId.isEmpty() &&
        (lower.contains(QLatin1String("nimm")) || lower.contains(QLatin1String("wähl")) ||
         lower.contains(QLatin1String("waehl")) || lower.contains(QLatin1String("nimm den")));
    if (toolOnly) {
        Command command;
        command.kind = CommandKind::SelectTool;
        command.toolId = toolId;
        *out = command;
        return true;
    }
    const bool generateSentence =
        lower.contains(QLatin1String("schreib mir")) ||
        lower.contains(QLatin1String("schreibe mir")) ||
        lower.contains(QLatin1String("selber")) ||
        lower.contains(QLatin1String("selbst")) ||
        lower.contains(QLatin1String("wichtigsten"));
    if (!mentionsNote && !generateSentence)
        return false;

    QString title;
    const QRegularExpression nenne(
        QStringLiteral(
            "nenn(?:e|en)\\s+(?:sie|ihn|es|die\\s+notiz\\s+)?(.+)$"),
        QRegularExpression::CaseInsensitiveOption);
    if (const auto match = nenne.match(raw); match.hasMatch())
        title = clipTitle(match.captured(1));
    if (title.isEmpty()) {
        const QRegularExpression named(
            QStringLiteral(
                "(?:notiz|blop|blog)\\s+(?:namens\\s+)?([A-Za-z0-9][^,]*)"),
            QRegularExpression::CaseInsensitiveOption);
        if (const auto match = named.match(raw); match.hasMatch()) {
            const QString candidate = clipTitle(match.captured(1));
            if (candidate.compare(QLatin1String("mir"), Qt::CaseInsensitive) != 0 &&
                candidate.compare(QLatin1String("einen"), Qt::CaseInsensitive) != 0 &&
                candidate.compare(QLatin1String("eine"), Qt::CaseInsensitive) != 0 &&
                candidate.compare(QLatin1String("oder"), Qt::CaseInsensitive) != 0)
                title = candidate;
        }
    }
    if (title.isEmpty()) {
        if (!generateSentence)
            return false;
        title = QStringLiteral("Notiz");
    }
    if (raw.contains(QLatin1Char(':')) &&
        !lower.contains(QLatin1String("überschrift")) &&
        !lower.contains(QLatin1String("ueberschrift")) &&
        !lower.contains(QLatin1String("nenn")))
        return false;

    QString heading;
    const QRegularExpression head(
        QStringLiteral(
            "(?:überschrift|ueberschrift)\\s+(?:mit\\s+(?:den\\s+)?(?:themen\\s+)?)?(.+)$"),
        QRegularExpression::CaseInsensitiveOption);
    if (const auto match = head.match(raw); match.hasMatch())
        heading = clipTitle(match.captured(1));

    QString writeClause;
    const QRegularExpression write(
        QStringLiteral("(?:schreib(?:e)?(?:\\s+mir)?|notiere|punkte)\\s*:?\\s+(.+)$"),
        QRegularExpression::CaseInsensitiveOption);
    if (const auto match = write.match(raw); match.hasMatch())
        writeClause = match.captured(1).trimmed();

    Command command;
    command.kind = CommandKind::ComposeNote;
    command.title = title;
    command.heading = heading;
    command.toolId = toolId;
    const QString writeLower = writeClause.toLower();
    const bool generate =
        writeLower.contains(QLatin1String("selber")) ||
        writeLower.contains(QLatin1String("selbst")) ||
        writeLower.contains(QLatin1String("wichtigsten")) ||
        writeLower.contains(QLatin1String("zwei der")) ||
        writeLower.contains(QLatin1String("drei der"));
    if (generate) {
        command.generate = true;
        command.text = writeClause;
    } else if (!writeClause.isEmpty()) {
        const QStringList parts = writeClause.split(
            QRegularExpression(QStringLiteral("\\s+und\\s+|\\s*,\\s*")),
            Qt::SkipEmptyParts);
        for (QString part : parts) {
            part = part.trimmed();
            part.remove(QRegularExpression(QStringLiteral("[.]+$")));
            if (!part.isEmpty())
                command.points.append(part);
        }
    }
    *out = command;
    return true;
}

} // namespace

QString CommandEngine::helpText() {
    return QStringLiteral(
        "Befehle\n"
        "öffne ordner downloads\n"
        "öffne ordner dokumente\n"
        "öffne browser\n"
        "öffne seite example.com\n"
        "starte explorer\n"
        "starte rechner\n"
        "öffne yt\n"
        "erstelle ordner Test 1 2 auf dem desktop\n"
        "erstelle eine txt datei mit dem inhalt Hallo\n"
        "erstelle notiz Titel: Inhalt\n"
        "öffne notiz Test123, Überschrift Themen, schreibe: günstig und schnell\n"
        "schreib mir die zwei wichtigsten Vorteile\n"
        "nimm den Textmarker\n"
        "erklär mir …\n"
        "Umständliche Sätze gehen über OpenRouter.");
}

Command CommandEngine::parseClause(const QString &input) const {
    const QString raw = stripLead(fold(input));
    if (raw.isEmpty())
        return unknown(QStringLiteral("Sag einen Befehl, oder „Hilfe“."));

    const QString lower = raw.toLower();
    if (isHelp(lower)) {
        Command command;
        command.kind = CommandKind::Help;
        return command;
    }

    if (Command composed; takeCompose(raw, &composed))
        return composed;

    if (Command note; takeNote(raw, &note))
        return note;

    if (Command folder; takeCreateFolder(raw, &folder))
        return folder;

    if (Command textFile; takeTextFile(raw, &textFile))
        return textFile;

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
    if (key == QLatin1String("yt") || key == QLatin1String("youtube")) {
        Command command;
        command.kind = CommandKind::OpenUrl;
        command.text = QStringLiteral("https://www.youtube.com");
        return command;
    }
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

QList<Command> CommandEngine::parseAll(const QString &input) const {
    const QString raw = stripLead(fold(input));
    if (raw.isEmpty())
        return {unknown(QStringLiteral("Sag einen Befehl, oder „Hilfe“."))};
    QList<Command> commands;
    for (const QString &clause : splitClauses(raw))
        commands.append(parseClause(clause));
    return commands;
}

Command CommandEngine::parse(const QString &input) const {
    const QList<Command> commands = parseAll(input);
    if (commands.isEmpty())
        return unknown(QStringLiteral("Sag einen Befehl, oder „Hilfe“."));
    return commands.first();
}
