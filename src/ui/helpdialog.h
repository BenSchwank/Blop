#pragma once

#include <QDialog>

/// In-app help: first steps, keyboard shortcuts, replay of the onboarding and
/// a shortcut to the settings. Shown through BlopModal (no top-level window
/// tricks needed, safe on Android).
class HelpDialog : public QDialog {
  Q_OBJECT
public:
  explicit HelpDialog(const QString &versionText, QWidget *parent = nullptr);

signals:
  void replayOnboardingRequested();
  void openSettingsRequested();
};
