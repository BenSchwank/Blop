#pragma once

#include <QDialog>

class QCheckBox;
class QComboBox;
class QKeySequenceEdit;
class QSlider;
class QLabel;

// Standalone settings window for the companion. Opened from the tray /
// taskbar — never from the notch.
class BlopAssistantSettingsDialog : public QDialog {
  Q_OBJECT
public:
  explicit BlopAssistantSettingsDialog(QWidget *parent = nullptr);

signals:
  void prefsChanged();

private:
  void load();
  void save();

  QKeySequenceEdit *m_openChat{nullptr};
  QKeySequenceEdit *m_pushToTalk{nullptr};
  QCheckBox *m_speak{nullptr};
  QSlider *m_rate{nullptr};
  QLabel *m_rateValue{nullptr};
  QComboBox *m_voice{nullptr};
};
