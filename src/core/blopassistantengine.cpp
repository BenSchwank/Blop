#include "blopassistantengine.h"

#include <QRegularExpression>
#include <QStringList>
#include <QUrlQuery>

namespace {

QString foldWs(QString s) {
  s = s.trimmed();
  s.replace(QLatin1Char('\n'), QLatin1Char(' '));
  s.replace(QLatin1Char('\t'), QLatin1Char(' '));
  static const QRegularExpression multiSpace(QStringLiteral("\\s+"));
  s.replace(multiSpace, QStringLiteral(" "));
  while (s.endsWith(QLatin1Char('.')) || s.endsWith(QLatin1Char('!')) ||
         s.endsWith(QLatin1Char('?'))) {
    s.chop(1);
    s = s.trimmed();
  }
  return s;
}

QString norm(const QString &s) { return foldWs(s).toLower(); }

void splitTitleBody(const QString &rest, QString *title, QString *body) {
  const QString t = foldWs(rest);
  const int colon = t.indexOf(QLatin1Char(':'));
  if (colon > 0) {
    *title = t.left(colon).trimmed();
    *body = t.mid(colon + 1).trimmed();
    return;
  }
  static const QRegularExpression withText(
      QStringLiteral("^(.*?)\\s+(?:mit text|that says|saying)\\s+(.+)$"),
      QRegularExpression::CaseInsensitiveOption);
  const QRegularExpressionMatch m = withText.match(t);
  if (m.hasMatch()) {
    *title = m.captured(1).trimmed();
    *body = m.captured(2).trimmed();
    return;
  }
  *title = t;
  *body = QString();
}

bool looksInfinite(const QString &s) {
  const QString n = norm(s);
  return n.contains(QStringLiteral("unendlich")) ||
         n.contains(QStringLiteral("infinite")) ||
         n.contains(QStringLiteral("leinwand")) ||
         n.contains(QStringLiteral("canvas")) ||
         n.contains(QStringLiteral("freihand"));
}

BlopAssistantIntent makeCreate(const QString &rest, bool infiniteHint,
                               const QString &raw) {
  BlopAssistantIntent out;
  out.action = BlopAssistantAction::CreateNote;
  splitTitleBody(rest, &out.title, &out.body);
  if (out.title.isEmpty())
    out.title = QStringLiteral("Neue Notiz");
  out.infinite = infiniteHint || looksInfinite(raw) || looksInfinite(out.title);
  if (out.infinite) {
    QString cleaned = out.title;
    cleaned.replace(QRegularExpression(QStringLiteral("(?i)\\bunendliche?n?\\b")),
                    QString());
    cleaned.replace(QRegularExpression(QStringLiteral("(?i)\\binfinite\\b")),
                    QString());
    cleaned.replace(QRegularExpression(QStringLiteral("(?i)\\bcanvas\\b")),
                    QString());
    cleaned.replace(QRegularExpression(QStringLiteral("(?i)\\bleinwand\\b")),
                    QString());
    cleaned = foldWs(cleaned);
    if (!cleaned.isEmpty())
      out.title = cleaned;
  }
  out.statusLine = QStringLiteral("Erstelle „%1“…").arg(out.title);
  out.spokenReply = QStringLiteral("Notiz %1 ist angelegt.").arg(out.title);
  return out;
}

QUrl knownSiteUrl(const QString &token) {
  const QString t = norm(token).remove(QLatin1Char('.'));
  if (t == QLatin1String("youtube") || t == QLatin1String("yt"))
    return QUrl(QStringLiteral("https://www.youtube.com"));
  if (t == QLatin1String("google"))
    return QUrl(QStringLiteral("https://www.google.com"));
  if (t == QLatin1String("gmail"))
    return QUrl(QStringLiteral("https://mail.google.com"));
  if (t == QLatin1String("github"))
    return QUrl(QStringLiteral("https://github.com"));
  if (t == QLatin1String("wikipedia"))
    return QUrl(QStringLiteral("https://wikipedia.org"));
  if (t == QLatin1String("maps"))
    return QUrl(QStringLiteral("https://maps.google.com"));
  if (t == QLatin1String("netflix"))
    return QUrl(QStringLiteral("https://www.netflix.com"));
  if (t == QLatin1String("spotify"))
    return QUrl(QStringLiteral("https://open.spotify.com"));
  return {};
}

QUrl urlFromToken(const QString &raw) {
  const QString t = foldWs(raw);
  if (t.startsWith(QLatin1String("http://"), Qt::CaseInsensitive) ||
      t.startsWith(QLatin1String("https://"), Qt::CaseInsensitive))
    return QUrl(t);
  if (const QUrl known = knownSiteUrl(t); known.isValid())
    return known;
  if (t.contains(QLatin1Char('.')) && !t.contains(QLatin1Char(' '))) {
    QUrl u(QStringLiteral("https://") + t);
    if (u.isValid())
      return u;
  }
  return {};
}

QString knownApp(const QString &token) {
  const QString t = norm(token);
  if (t == QLatin1String("chrome") || t == QLatin1String("google chrome") ||
      t == QLatin1String("browser"))
    return QStringLiteral("chrome");
  if (t == QLatin1String("firefox"))
    return QStringLiteral("firefox");
  if (t == QLatin1String("edge") || t == QLatin1String("msedge"))
    return QStringLiteral("msedge");
  if (t == QLatin1String("calculator") || t == QLatin1String("rechner") ||
      t == QLatin1String("calc"))
    return QStringLiteral("calculator");
  if (t == QLatin1String("code") || t == QLatin1String("vscode") ||
      t == QLatin1String("visual studio code"))
    return QStringLiteral("code");
  if (t == QLatin1String("notepad") || t == QLatin1String("editor"))
    return QStringLiteral("notepad");
  if (t == QLatin1String("explorer") || t == QLatin1String("files") ||
      t == QLatin1String("dateien") || t == QLatin1String("ordner"))
    return QStringLiteral("explorer");
  if (t == QLatin1String("spotify"))
    return QStringLiteral("spotify");
  if (t == QLatin1String("slack"))
    return QStringLiteral("slack");
  if (t == QLatin1String("discord"))
    return QStringLiteral("discord");
  if (t == QLatin1String("terminal") || t == QLatin1String("cmd") ||
      t == QLatin1String("konsole"))
    return QStringLiteral("terminal");
  return {};
}

BlopAssistantIntent makeUrl(const QUrl &url) {
  BlopAssistantIntent o;
  o.action = BlopAssistantAction::OpenUrl;
  o.url = url;
  o.query = url.toString();
  o.needsConfirm = true;
  o.statusLine = QStringLiteral("Browser öffnen: %1").arg(url.host());
  o.spokenReply = QStringLiteral("Ich öffne %1.").arg(url.host());
  return o;
}

BlopAssistantIntent makeApp(const QString &app) {
  BlopAssistantIntent o;
  o.action = BlopAssistantAction::LaunchApp;
  o.appName = app;
  o.query = app;
  o.needsConfirm = true;
  o.statusLine = QStringLiteral("App starten: %1").arg(app);
  o.spokenReply = QStringLiteral("Starte %1.").arg(app);
  return o;
}

} // namespace

QString BlopAssistantEngine::helpText() {
  return QStringLiteral(
      "Zum Beispiel:\n"
      "• neue Notiz Einkauf\n"
      "• öffne YouTube\n"
      "• starte Calculator\n"
      "• suche im Web Qt\n"
      "• öffne Einkauf");
}

BlopAssistantIntent BlopAssistantEngine::parse(const QString &utterance) {
  const QString raw = foldWs(utterance);
  const QString n = norm(raw);
  BlopAssistantIntent unknown;
  unknown.query = raw;
  unknown.statusLine = QStringLiteral("Das habe ich nicht verstanden.");
  unknown.spokenReply =
      QStringLiteral("Sag zum Beispiel neue Notiz Einkauf, oder öffne YouTube.");
  if (n.isEmpty())
    return unknown;

  if (n == QLatin1String("hilfe") || n == QLatin1String("help") ||
      n == QLatin1String("was kannst du") ||
      n.startsWith(QLatin1String("was kannst du")) ||
      n == QLatin1String("commands") || n == QLatin1String("befehle")) {
    BlopAssistantIntent h;
    h.action = BlopAssistantAction::Help;
    h.statusLine = QStringLiteral("Notizen, Browser, Apps.");
    h.spokenReply =
        QStringLiteral("Zum Beispiel: neue Notiz Einkauf, oder öffne YouTube.");
    return h;
  }

  if (n == QLatin1String("bibliothek") || n == QLatin1String("library") ||
      n == QLatin1String("home") || n == QLatin1String("start") ||
      n == QLatin1String("übersicht") || n == QLatin1String("uebersicht") ||
      n == QLatin1String("overview") || n == QLatin1String("notizen") ||
      n == QLatin1String("zurück") || n == QLatin1String("zurueck")) {
    BlopAssistantIntent lib;
    lib.action = BlopAssistantAction::ShowLibrary;
    lib.statusLine = QStringLiteral("Öffne die Notizliste…");
    lib.spokenReply = QStringLiteral("Hier sind deine Notizen.");
    return lib;
  }

  static const QRegularExpression webSearchRe(
      QStringLiteral("^(?:suche im web|search the web(?: for)?|google|web)\\s+"
                     "(?:nach\\s+|for\\s+)?(.+)$"),
      QRegularExpression::CaseInsensitiveOption);
  {
    const QRegularExpressionMatch m = webSearchRe.match(raw);
    if (m.hasMatch()) {
      QUrl url(QStringLiteral("https://www.google.com/search"));
      QUrlQuery q;
      q.addQueryItem(QStringLiteral("q"), foldWs(m.captured(1)));
      url.setQuery(q);
      BlopAssistantIntent o = makeUrl(url);
      o.statusLine =
          QStringLiteral("Websuche: %1").arg(foldWs(m.captured(1)));
      return o;
    }
  }

  static const QRegularExpression searchRe(
      QStringLiteral(
          "^(?:suche|search|find|finde|filter)\\s+(?:nach\\s+|for\\s+)?(.+)$"),
      QRegularExpression::CaseInsensitiveOption);
  {
    const QRegularExpressionMatch m = searchRe.match(raw);
    if (m.hasMatch()) {
      BlopAssistantIntent s;
      s.action = BlopAssistantAction::SearchNotes;
      s.query = foldWs(m.captured(1));
      s.statusLine = QStringLiteral("Suche „%1“…").arg(s.query);
      s.spokenReply = QStringLiteral("Suche nach %1.").arg(s.query);
      return s;
    }
  }

  static const QRegularExpression launchRe(
      QStringLiteral(
          "^(?:starte|starten|start|launch|öffne app|oeffne app)\\s+(.+)$"),
      QRegularExpression::CaseInsensitiveOption);
  {
    const QRegularExpressionMatch m = launchRe.match(raw);
    if (m.hasMatch()) {
      const QString rest = foldWs(m.captured(1));
      if (const QUrl u = urlFromToken(rest); u.isValid())
        return makeUrl(u);
      const QString app = knownApp(rest);
      return makeApp(app.isEmpty() ? rest : app);
    }
  }

  static const QRegularExpression openRe(
      QStringLiteral("^(?:öffne|oeffne|open|zeig(?:e)?|show)\\s+"
                     "(?:im browser\\s+|in (?:the )?browser\\s+)?"
                     "(?:die\\s+|the\\s+)?(?:notiz|note)?\\s*(.+)$"),
      QRegularExpression::CaseInsensitiveOption);
  {
    const QRegularExpressionMatch m = openRe.match(raw);
    if (m.hasMatch()) {
      const QString rest = foldWs(m.captured(1));
      if (const QUrl u = urlFromToken(rest); u.isValid())
        return makeUrl(u);
      if (const QString app = knownApp(rest); !app.isEmpty())
        return makeApp(app);
      BlopAssistantIntent o;
      o.action = BlopAssistantAction::OpenNote;
      o.query = rest;
      o.statusLine = QStringLiteral("Öffne „%1“…").arg(o.query);
      o.spokenReply = QStringLiteral("Öffne %1.").arg(o.query);
      return o;
    }
  }

  static const QRegularExpression createRe(
      QStringLiteral(
          "^(?:(?:bitte\\s+)?(?:erstell(?:e|en)?|mach(?:e|en)?|create|make|"
          "schreib(?:e)?|write)\\s+)?"
          "(?:(?:eine?|a|an)\\s+)?"
          "(?:(?:neue[sn]?|new)\\s+)?"
          "(unendliche\\s+|infinite\\s+|freie[sn]?\\s+)?"
          "(?:notiz|note|notebook|leinwand|canvas)"
          "(?:\\s+(?:namens|mit titel|titled?|called|named|über|ueber|about|zu))?"
          "\\s*(.*)$"),
      QRegularExpression::CaseInsensitiveOption);
  {
    const QRegularExpressionMatch m = createRe.match(raw);
    if (m.hasMatch()) {
      const bool inf = !m.captured(1).isEmpty() || looksInfinite(raw);
      QString rest = foldWs(m.captured(2));
      if (rest.isEmpty() && looksInfinite(raw))
        rest = QStringLiteral("Skizze");
      return makeCreate(rest, inf, raw);
    }
  }

  static const QRegularExpression newNoteRe(
      QStringLiteral(
          "^(?:neue|new)\\s+(unendliche\\s+|infinite\\s+)?(?:notiz|note)\\s*(.*)$"),
      QRegularExpression::CaseInsensitiveOption);
  {
    const QRegularExpressionMatch m = newNoteRe.match(raw);
    if (m.hasMatch())
      return makeCreate(m.captured(2), !m.captured(1).isEmpty(), raw);
  }

  if (const QUrl u = urlFromToken(raw); u.isValid() && n.contains('.'))
    return makeUrl(u);

  const int words = n.split(QLatin1Char(' '), Qt::SkipEmptyParts).size();
  const bool question = n.startsWith(QLatin1String("wie ")) ||
                        n.startsWith(QLatin1String("was ")) ||
                        n.startsWith(QLatin1String("warum ")) ||
                        n.startsWith(QLatin1String("why ")) ||
                        n.startsWith(QLatin1String("how "));
  if (!question && words >= 1 && words <= 8 && raw.size() <= 80)
    return makeCreate(raw, looksInfinite(raw), raw);

  return unknown;
}
