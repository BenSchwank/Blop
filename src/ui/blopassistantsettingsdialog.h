#pragma once

#include <QDialog>

class QCheckBox;
class QComboBox;
class QKeySequenceEdit;
class QLabel;
class QLineEdit;
class QPushButton;
class QShowEvent;
class QSlider;

// Standalone settings window for the companion. Opened from the tray /
// taskbar — never from the notch.
class BlopAssistantSettingsDialog : public QDialog {
  Q_OBJECT
public:
  explicit BlopAssistantSettingsDialog(QWidget *parent = nullptr);

signals:
  void prefsChanged();

protected:
  void showEvent(QShowEvent *event) override;

private:
  void load();
  void save();

  QKeySequenceEdit *m_openChat{nullptr};
  QKeySequenceEdit *m_pushToTalk{nullptr};
  QCheckBox *m_speak{nullptr};
  QSlider *m_rate{nullptr};
  QLabel *m_rateValue{nullptr};
  QComboBox *m_voice{nullptr};
  QCheckBox *m_llmOn{nullptr};
  QLineEdit *m_llmKey{nullptr};
  QComboBox *m_llmPreset{nullptr};
  QLineEdit *m_llmUrl{nullptr};
  QLineEdit *m_llmModel{nullptr};
  QPushButton *m_closeBtn{nullptr};
};
