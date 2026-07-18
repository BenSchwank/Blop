#pragma once

// Persistent right-side Drawboard-style tool properties dock:
// color swatches, stroke width, opacity — bound to ToolManager.

#include <QColor>
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
  QPushButton *makeSwatch(const QColor &c);

  ToolMode m_mode{ToolMode::Pen};
  ToolConfig m_config;
  QColor m_accent{QColor(91, 157, 255)};

  QLabel *m_title{nullptr};
  QLabel *m_widthLbl{nullptr};
  QLabel *m_opacityLbl{nullptr};
  QSlider *m_widthSlider{nullptr};
  QSlider *m_opacitySlider{nullptr};
  QWidget *m_colorRow{nullptr};
  QVBoxLayout *m_root{nullptr};
};
