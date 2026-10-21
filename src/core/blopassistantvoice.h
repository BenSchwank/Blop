#pragma once

#include <QString>

// Local TTS for the sandbox companion. Prefers calmer system voices over
// the default high-speed SAPI/espeak voice.
namespace BlopAssistantVoice {

void speak(const QString &text);

} // namespace BlopAssistantVoice
