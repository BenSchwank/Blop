#pragma once

// Persistent right-side Drawboard-style tool properties dock:
// color swatches, stroke width, opacity — bound to ToolManager.

#include <QColor>
#include <QList>
#include <QWidget>

#include "ToolMode.h"
#include "ToolSettings.h"

class QLabel;
class QSlider;
class QVBoxLayout;
class QHBoxLayout;
class QPushButton;

class ToolPropertiesPanel : public QWidget {
  Q_OBJECT
public:
  explicit ToolPropertiesPanel(QWidget *parent = nullptr);

  void setAccentColor(const QColor &c);
  void syncFromToolManager();
  /// Sync panel for a tool that may not be registered in ToolManager (e.g. Hand).
  void syncForMode(ToolMode mode);
  void setVisibleForTool(ToolMode mode);

  int preferredWidth() const;

signals:
  void closeRequested();

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  void rebuild();
  void applyConfig();
  void addColorRow(QVBoxLayout *lay);
  void refreshSwatchSelection();
  QPushButton *makeSwatch(const QColor &c);

  ToolMode m_mode{ToolMode::Pen};
  ToolConfig m_config;
  QColor m_accent{QColor(91, 157, 255)};

  QLabel *m_title{nullptr};
  QLabel *m_widthLbl{nullptr};
  QLabel *m_opacityLbl{nullptr};
  QLabel *m_modeLbl{nullptr};
  QLabel *m_hintLbl{nullptr};
  QSlider *m_widthSlider{nullptr};
  QSlider *m_opacitySlider{nullptr};
  QWidget *m_colorRow{nullptr};
  QWidget *m_modeRow{nullptr};
  QPushButton *m_modeA{nullptr};
  QPushButton *m_modeB{nullptr};
  QPushButton *m_modeC{nullptr};
  QPushButton *m_modeD{nullptr};
  QPushButton *m_modeE{nullptr};
  QPushButton *m_modeF{nullptr};
  QPushButton *m_modeG{nullptr};
  QPushButton *m_customColorBtn{nullptr};
  QList<QPushButton *> m_swatches;
  QVBoxLayout *m_root{nullptr};
};
