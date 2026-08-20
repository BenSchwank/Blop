#include "blopassistantllm.h"

#include "blopassistantprefs.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

namespace {

QString stripTrailingSlash(QString s) {
  while (s.endsWith(QLatin1Char('/')))
    s.chop(1);
  return s;
}

QString systemPrompt(const QStringList &notes) {
  QString notesLine = notes.isEmpty()
                          ? QStringLiteral("Der Nutzer hat noch keine Notizen.")
                          : QStringLiteral("Notizen: %1").arg(notes.join(QStringLiteral(", ")));
  return QStringLiteral(
             "Du bist Blop, ein Alltags-Assistent. Sprich wie ein ruhiger "
             "Mensch — kurz, warm, ohne Floskeln, ohne dass man merkt dass du "
             "eine KI bist. Kein Marketing, keine Aufzählungen außer wenn sie "
             "wirklich helfen, kein „Als KI…“.\n"
             "Deutsch, es sei denn der Nutzer schreibt anders.\n"
             "Smalltalk und Wissensfragen beantwortest du selbst.\n"
             "Tools nur, wenn der Nutzer wirklich eine Notiz, eine Website "
             "oder eine App will. Aus „hallo“ oder einer Frage machst du "
             "keine Notiz.\n"
             "%1")
      .arg(notesLine);
}

QUrl chatCompletionsUrl() {
  QString base = stripTrailingSlash(BlopAssistantPrefs::llmBaseUrl());
  if (base.endsWith(QLatin1String("/chat/completions")))
    return QUrl(base);
  return QUrl(base + QStringLiteral("/chat/completions"));
}

} // namespace

bool BlopAssistantLlm::intentFromTool(const QString &name,
                                      const QJsonObject &args,
                                      BlopAssistantIntent *out) {
  if (!out)
    return false;
  *out = {};
  if (name == QLatin1String("create_note")) {
    out->action = BlopAssistantAction::CreateNote;
    out->title = args.value(QStringLiteral("title")).toString().trimmed();
    out->body = args.value(QStringLiteral("body")).toString();
    out->infinite = args.value(QStringLiteral("infinite")).toBool();
    if (out->title.isEmpty())
      out->title = QStringLiteral("Neue Notiz");
    out->spokenReply = QStringLiteral("Notiz %1.").arg(out->title);
    return true;
  }
  if (name == QLatin1String("open_note")) {
    out->action = BlopAssistantAction::OpenNote;
    out->query = args.value(QStringLiteral("query")).toString().trimmed();
    return !out->query.isEmpty();
  }
  if (name == QLatin1String("list_notes")) {
    out->action = BlopAssistantAction::ShowLibrary;
    return true;
  }
  if (name == QLatin1String("search_notes")) {
    out->action = BlopAssistantAction::SearchNotes;
    out->query = args.value(QStringLiteral("query")).toString().trimmed();
    return !out->query.isEmpty();
  }
  if (name == QLatin1String("open_url") || name == QLatin1String("search_web")) {
    QString raw = args.value(QStringLiteral("url")).toString().trimmed();
    if (raw.isEmpty())
      raw = args.value(QStringLiteral("query")).toString().trimmed();
    if (raw.isEmpty())
      return false;
    QUrl url(raw);
    if (name == QLatin1String("search_web") || !url.isValid() ||
        url.scheme().isEmpty()) {
      url = QUrl(QStringLiteral("https://www.google.com/search?q=") +
                 QString::fromUtf8(QUrl::toPercentEncoding(raw)));
    }
    out->action = BlopAssistantAction::OpenUrl;
    out->url = url;
    out->needsConfirm = true;
    out->statusLine = QStringLiteral("Browser: %1").arg(url.host());
    out->spokenReply = QStringLiteral("Ich öffne %1.").arg(url.host());
    return true;
  }
  if (name == QLatin1String("launch_app")) {
    out->action = BlopAssistantAction::LaunchApp;
    out->appName = args.value(QStringLiteral("name")).toString().trimmed();
    if (out->appName.isEmpty())
      return false;
    out->needsConfirm = true;
    out->statusLine = QStringLiteral("App: %1").arg(out->appName);
    out->spokenReply = QStringLiteral("Starte %1.").arg(out->appName);
    return true;
  }
  return false;
}

BlopAssistantLlm::BlopAssistantLlm(QObject *parent)
    : QObject(parent), m_nam(new QNetworkAccessManager(this)) {}

bool BlopAssistantLlm::isReady() { return BlopAssistantPrefs::llmReady(); }

void BlopAssistantLlm::resetConversation() {
  m_messages = QJsonArray();
  m_rounds = 0;
  m_busy = false;
}

void BlopAssistantLlm::setToolHandler(
    std::function<QString(const BlopAssistantIntent &)> handler) {
  m_tools = std::move(handler);
}

QJsonObject BlopAssistantLlm::messageObject(const QString &role,
                                            const QString &content) const {
  QJsonObject o;
  o.insert(QStringLiteral("role"), role);
  o.insert(QStringLiteral("content"), content);
  return o;
}

QJsonArray BlopAssistantLlm::toolsJson() const {
  auto fn = [](const QString &name, const QString &desc,
               const QJsonObject &props, const QJsonArray &required) {
    QJsonObject schema;
    schema.insert(QStringLiteral("type"), QStringLiteral("object"));
    schema.insert(QStringLiteral("properties"), props);
    if (!required.isEmpty())
      schema.insert(QStringLiteral("required"), required);
    QJsonObject fun;
    fun.insert(QStringLiteral("name"), name);
    fun.insert(QStringLiteral("description"), desc);
    fun.insert(QStringLiteral("parameters"), schema);
    QJsonObject tool;
    tool.insert(QStringLiteral("type"), QStringLiteral("function"));
    tool.insert(QStringLiteral("function"), fun);
    return tool;
  };

  QJsonObject title;
  title.insert(QStringLiteral("title"),
               QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}});
  title.insert(QStringLiteral("body"),
               QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}});
  title.insert(QStringLiteral("infinite"),
               QJsonObject{{QStringLiteral("type"), QStringLiteral("boolean")}});

  QJsonObject q;
  q.insert(QStringLiteral("query"),
           QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}});

  QJsonObject url;
  url.insert(QStringLiteral("url"),
             QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}});

  QJsonObject web;
  web.insert(QStringLiteral("query"),
             QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}});

  QJsonObject app;
  app.insert(QStringLiteral("name"),
             QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}});

  QJsonArray tools;
  tools.append(fn(QStringLiteral("create_note"),
                  QStringLiteral("Legt eine Notiz an, nur auf Wunsch."), title,
                  QJsonArray{QStringLiteral("title")}));
  tools.append(fn(QStringLiteral("open_note"),
                  QStringLiteral("Findet eine vorhandene Notiz."), q,
                  QJsonArray{QStringLiteral("query")}));
  tools.append(fn(QStringLiteral("list_notes"),
                  QStringLiteral("Listet die Notizen."), QJsonObject(),
                  QJsonArray()));
  tools.append(fn(QStringLiteral("search_notes"),
                  QStringLiteral("Sucht in den Notizen."), q,
                  QJsonArray{QStringLiteral("query")}));
  tools.append(fn(QStringLiteral("open_url"),
                  QStringLiteral("Öffnet eine Website im Browser."), url,
                  QJsonArray{QStringLiteral("url")}));
  tools.append(fn(QStringLiteral("search_web"),
                  QStringLiteral("Sucht im Web und öffnet Google."), web,
                  QJsonArray{QStringLiteral("query")}));
  tools.append(fn(QStringLiteral("launch_app"),
                  QStringLiteral("Startet eine App auf dem Rechner."), app,
                  QJsonArray{QStringLiteral("name")}));
  return tools;
}

void BlopAssistantLlm::ask(const QString &userText, const QStringList &noteNames) {
  if (m_busy)
    return;
  if (!isReady()) {
    emit failed(QStringLiteral(
        "Kein API-Key. Unter Einstellungen → KI kannst du OpenAI, "
        "OpenRouter oder Groq eintragen."));
    return;
  }
  m_notes = noteNames;
  m_busy = true;
  m_rounds = 0;
  if (m_messages.isEmpty())
    m_messages.append(messageObject(QStringLiteral("system"),
                                    systemPrompt(noteNames)));
  m_messages.append(messageObject(QStringLiteral("user"), userText.trimmed()));
  emit thinking();
  postTurn();
}

void BlopAssistantLlm::postTurn() {
  if (++m_rounds > 4) {
    m_busy = false;
    emit failed(QStringLiteral("Die KI dreht sich im Kreis. Versuch's nochmal."));
    return;
  }
  QJsonObject body;
  body.insert(QStringLiteral("model"), BlopAssistantPrefs::llmModel());
  body.insert(QStringLiteral("messages"), m_messages);
  body.insert(QStringLiteral("tools"), toolsJson());
  body.insert(QStringLiteral("temperature"), 0.7);
  body.insert(QStringLiteral("max_tokens"), 500);

  QNetworkRequest req(chatCompletionsUrl());
  req.setHeader(QNetworkRequest::ContentTypeHeader,
                QStringLiteral("application/json"));
  req.setRawHeader("Authorization",
                   QByteArray("Bearer ") +
                       BlopAssistantPrefs::llmApiKey().toUtf8());
  req.setTransferTimeout(45000);

  const QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);
  QNetworkReply *reply = m_nam->post(req, payload);
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    handleHttp(reply);
    reply->deleteLater();
  });
}

void BlopAssistantLlm::handleHttp(QNetworkReply *reply) {
  if (!reply) {
    m_busy = false;
    emit failed(QStringLiteral("Keine Antwort von der KI."));
    return;
  }
  const QByteArray raw = reply->readAll();
  if (reply->error() != QNetworkReply::NoError) {
    m_busy = false;
    QString detail = QString::fromUtf8(raw).left(280);
    if (detail.isEmpty())
      detail = reply->errorString();
    emit failed(QStringLiteral("KI-Fehler: %1").arg(detail));
    return;
  }
  const QJsonObject root = QJsonDocument::fromJson(raw).object();
  const QJsonArray choices = root.value(QStringLiteral("choices")).toArray();
  if (choices.isEmpty()) {
    m_busy = false;
    emit failed(QStringLiteral("Die KI hat leer geantwortet."));
    return;
  }
  const QJsonObject message =
      choices.at(0).toObject().value(QStringLiteral("message")).toObject();
  m_messages.append(message);

  const QJsonArray toolCalls = message.value(QStringLiteral("tool_calls")).toArray();
  if (!toolCalls.isEmpty()) {
    for (const QJsonValue &tv : toolCalls) {
      const QJsonObject call = tv.toObject();
      const QJsonObject fn = call.value(QStringLiteral("function")).toObject();
      const QString name = fn.value(QStringLiteral("name")).toString();
      const QJsonObject args =
          QJsonDocument::fromJson(
              fn.value(QStringLiteral("arguments")).toString().toUtf8())
              .object();
      BlopAssistantIntent intent;
      QString result = QStringLiteral("ok");
      if (intentFromTool(name, args, &intent)) {
        emit wantsAction(intent);
        if (m_tools)
          result = m_tools(intent);
      } else {
        result = QStringLiteral("unbekanntes tool");
      }
      QJsonObject toolMsg;
      toolMsg.insert(QStringLiteral("role"), QStringLiteral("tool"));
      toolMsg.insert(QStringLiteral("tool_call_id"),
                     call.value(QStringLiteral("id")).toString());
      toolMsg.insert(QStringLiteral("content"), result);
      m_messages.append(toolMsg);
    }
    postTurn();
    return;
  }

  QString text = message.value(QStringLiteral("content")).toString().trimmed();
  m_busy = false;
  if (text.isEmpty())
    text = QStringLiteral("Mhm, dazu fällt mir gerade nichts Vernünftiges ein.");
  emit assistantSaid(text);
}
