#include "blopassistantengine.h"

#include <QRegularExpression>
#include <QStringList>

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
  out.statusLine =
      QStringLiteral("Erstelle „%1“…").arg(out.title);
  out.spokenReply =
      QStringLiteral("Ich lege die Notiz %1 an.").arg(out.title);
  return out;
}

} // namespace

QString BlopAssistantEngine::helpText() {
  return QStringLiteral(
      "Sag oder tippe zum Beispiel:\n"
      "• neue Notiz Einkauf\n"
      "• Notiz Meeting: Agenda morgen 10 Uhr\n"
      "• unendliche Notiz Skizze\n"
      "• öffne Einkauf\n"
      "• suche Physik\n"
      "• Bibliothek");
}

BlopAssistantIntent BlopAssistantEngine::parse(const QString &utterance) {
  const QString raw = foldWs(utterance);
  const QString n = norm(raw);
  BlopAssistantIntent unknown;
  unknown.query = raw;
  unknown.statusLine = QStringLiteral("Das habe ich nicht verstanden.");
  unknown.spokenReply = QStringLiteral(
      "Sag zum Beispiel: neue Notiz Einkauf, oder öffne gefolgt vom Namen.");
  if (n.isEmpty())
    return unknown;

  if (n == QLatin1String("hilfe") || n == QLatin1String("help") ||
      n == QLatin1String("was kannst du") ||
      n.startsWith(QLatin1String("was kannst du")) ||
      n == QLatin1String("commands") || n == QLatin1String("befehle")) {
    BlopAssistantIntent h;
    h.action = BlopAssistantAction::Help;
    h.statusLine = QStringLiteral("Ich kann Notizen anlegen, öffnen und suchen.");
    h.spokenReply = helpText();
    return h;
  }

  if (n == QLatin1String("bibliothek") || n == QLatin1String("library") ||
      n == QLatin1String("home") || n == QLatin1String("start") ||
      n == QLatin1String("übersicht") || n == QLatin1String("uebersicht") ||
      n == QLatin1String("overview") || n == QLatin1String("notizen") ||
      n == QLatin1String("zurück") || n == QLatin1String("zurueck")) {
    BlopAssistantIntent lib;
    lib.action = BlopAssistantAction::ShowLibrary;
    lib.statusLine = QStringLiteral("Öffne die Bibliothek…");
    lib.spokenReply = QStringLiteral("Hier ist deine Notizbibliothek.");
    return lib;
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
      s.spokenReply =
          QStringLiteral("Ich filtere die Bibliothek nach %1.").arg(s.query);
      return s;
    }
  }

  static const QRegularExpression openRe(
      QStringLiteral("^(?:öffne|oeffne|open|zeig(?:e)?|show)\\s+"
                     "(?:die\\s+|the\\s+)?(?:notiz|note)?\\s*(.+)$"),
      QRegularExpression::CaseInsensitiveOption);
  {
    const QRegularExpressionMatch m = openRe.match(raw);
    if (m.hasMatch()) {
      BlopAssistantIntent o;
      o.action = BlopAssistantAction::OpenNote;
      o.query = foldWs(m.captured(1));
      o.statusLine = QStringLiteral("Öffne „%1“…").arg(o.query);
      o.spokenReply = QStringLiteral("Ich öffne %1.").arg(o.query);
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
    if (m.hasMatch()) {
      return makeCreate(m.captured(2), !m.captured(1).isEmpty(), raw);
    }
  }

  // Bare title: "Einkaufsliste" → create that note.
  const int words = n.split(QLatin1Char(' '), Qt::SkipEmptyParts).size();
  const bool question = n.startsWith(QLatin1String("wie ")) ||
                        n.startsWith(QLatin1String("was ")) ||
                        n.startsWith(QLatin1String("warum ")) ||
                        n.startsWith(QLatin1String("why ")) ||
                        n.startsWith(QLatin1String("how "));
  if (!question && words >= 1 && words <= 8 && raw.size() <= 80) {
    return makeCreate(raw, looksInfinite(raw), raw);
  }

  return unknown;
}
