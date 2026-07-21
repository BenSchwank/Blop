#pragma once

// Floating Drawboard-style tool options card:
// style tiles, color/fill swatches, stroke width, opacity, Smart Ink — bound
// to ToolManager.

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
class QCheckBox;
class QScrollArea;

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
  /// Height of the floating options card for the current tool.
  int preferredHeight() const;

signals:
  void closeRequested();

protected:
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  enum class PenInkStyle { Einfach = 0, Pro = 1, Kalligrafie = 2 };

  void rebuild();
  void applyConfig();
  void addColorRow(QVBoxLayout *lay);
  void addFillColorRow(QVBoxLayout *lay);
  void refreshSwatchSelection();
  void refreshFillSwatchSelection();
  void applyPenInkStyle(PenInkStyle style);
  PenInkStyle detectPenInkStyle() const;
  void refreshStyleTiles();
  void refreshSmartInk();
  void updateScrollMetrics();
  QPushButton *makeSwatch(const QColor &c, bool fill);
  QPushButton *makeStyleTile(const QString &title, const QString &subtitle,
                             PenInkStyle style);

  ToolMode m_mode{ToolMode::Pen};
  ToolConfig m_config;
  QColor m_accent{QColor(91, 157, 255)};

  QScrollArea *m_scroll{nullptr};
  QWidget *m_body{nullptr};

  QLabel *m_title{nullptr};
  QLabel *m_styleLbl{nullptr};
  QWidget *m_styleRow{nullptr};
  QPushButton *m_styleEinfach{nullptr};
  QPushButton *m_stylePro{nullptr};
  QPushButton *m_styleKalli{nullptr};

  QLabel *m_widthLbl{nullptr};
  QLabel *m_opacityLbl{nullptr};
  QLabel *m_colorLbl{nullptr};
  QLabel *m_modeLbl{nullptr};
  QLabel *m_fillLbl{nullptr};
  QLabel *m_fontLbl{nullptr};
  QLabel *m_smartLbl{nullptr};
  QLabel *m_hintLbl{nullptr};
  QSlider *m_widthSlider{nullptr};
  QSlider *m_opacitySlider{nullptr};
  QWidget *m_colorRow{nullptr};
  QWidget *m_fillRow{nullptr};
  QWidget *m_fontRow{nullptr};
  QWidget *m_modeRow{nullptr};
  QWidget *m_smartRow{nullptr};
  QCheckBox *m_chkPressure{nullptr};
  QCheckBox *m_chkInkToShape{nullptr};
  QCheckBox *m_chkSmartLine{nullptr};
  QPushButton *m_modeA{nullptr};
  QPushButton *m_modeB{nullptr};
  QPushButton *m_modeC{nullptr};
  QPushButton *m_modeD{nullptr};
  QPushButton *m_modeE{nullptr};
  QPushButton *m_modeF{nullptr};
  QPushButton *m_modeG{nullptr};
  QPushButton *m_modeH{nullptr};
  QPushButton *m_customColorBtn{nullptr};
  QPushButton *m_customFillBtn{nullptr};
  QList<QPushButton *> m_swatches;
  QList<QPushButton *> m_fillSwatches;
  QVBoxLayout *m_outer{nullptr};
  QVBoxLayout *m_root{nullptr};
};
