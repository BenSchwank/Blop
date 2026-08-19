#pragma once

#include <QString>
#include <QUrl>

// Natural-language intents for the standalone Blop Assistant (VoiceOS-style).
// Parsing is local and deterministic — no network, no model.

enum class BlopAssistantAction {
  CreateNote,
  OpenNote,
  SearchNotes,
  ShowLibrary,
  OpenUrl,
  LaunchApp,
  Help,
  Unknown
};

struct BlopAssistantIntent {
  BlopAssistantAction action{BlopAssistantAction::Unknown};
  QString title;
  QString query;
  QString body;
  QUrl url;
  QString appName;
  bool infinite{false};
  bool needsConfirm{false};
  QString spokenReply;
  QString statusLine;
};

class BlopAssistantEngine {
public:
  static BlopAssistantIntent parse(const QString &utterance);
  static QString helpText();
};
