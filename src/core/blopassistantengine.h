#pragma once

#include <QString>

// Natural-language intents for the in-app Blop Assistant (VoiceOS-style).
// Parsing is local and deterministic — no network, no model.

enum class BlopAssistantAction {
  CreateNote,
  OpenNote,
  SearchNotes,
  ShowLibrary,
  Help,
  Unknown
};

struct BlopAssistantIntent {
  BlopAssistantAction action{BlopAssistantAction::Unknown};
  QString title;
  QString query;
  QString body;
  bool infinite{false};
  QString spokenReply;
  QString statusLine;
};

class BlopAssistantEngine {
public:
  static BlopAssistantIntent parse(const QString &utterance);
  static QString helpText();
};
