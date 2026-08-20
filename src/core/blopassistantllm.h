#pragma once

#include "blopassistantengine.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QVector>
#include <functional>

class QNetworkAccessManager;
class QNetworkReply;

// OpenAI-compatible chat (tools) for the sandbox companion.
class BlopAssistantLlm : public QObject {
  Q_OBJECT
public:
  explicit BlopAssistantLlm(QObject *parent = nullptr);

  static bool isReady();
  void resetConversation();
  void setToolHandler(std::function<QString(const BlopAssistantIntent &)> handler);
  void ask(const QString &userText, const QStringList &noteNames);

  // Test helper: map an OpenAI tool call into an intent.
  static bool intentFromTool(const QString &name, const QJsonObject &args,
                             BlopAssistantIntent *out);

signals:
  void thinking();
  void assistantSaid(const QString &text);
  void wantsAction(const BlopAssistantIntent &intent);
  void failed(const QString &error);

private:
  void postTurn();
  void handleHttp(QNetworkReply *reply);
  QJsonArray toolsJson() const;
  QJsonObject messageObject(const QString &role, const QString &content) const;

  QNetworkAccessManager *m_nam{nullptr};
  QJsonArray m_messages;
  QStringList m_notes;
  std::function<QString(const BlopAssistantIntent &)> m_tools;
  int m_rounds{0};
  bool m_busy{false};
};
