#pragma once

#include <QColor>
#include <QList>
#include <QWidget>

class QAbstractButton;
class QButtonGroup;
class QLabel;
class QPushButton;
class QSlider;
class QWidget;

/// Floating pen studio: live stroke, ink style, width, color, opacity, smart ink.
class PenStudioPopover : public QWidget {
  Q_OBJECT
public:
  explicit PenStudioPopover(QWidget *parent = nullptr);

  void syncFromTool();
  void present(const QPoint &globalAnchor);

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  void applyStyle(int style);
  void refreshPreview();
  void applyChrome();

  QWidget *m_preview{nullptr};
  QButtonGroup *m_styles{nullptr};
  QLabel *m_widthLabel{nullptr};
  QSlider *m_width{nullptr};
  QLabel *m_widthValue{nullptr};
  QLabel *m_colorLabel{nullptr};
  QLabel *m_opacityLabel{nullptr};
  QSlider *m_opacity{nullptr};
  QLabel *m_opacityValue{nullptr};
  QLabel *m_smoothLabel{nullptr};
  QSlider *m_smooth{nullptr};
  QLabel *m_smoothValue{nullptr};
  QAbstractButton *m_holdLine{nullptr};
  QAbstractButton *m_holdShape{nullptr};
  QWidget *m_rule{nullptr};
  QList<QPushButton *> m_presets;
  QList<QPushButton *> m_colorBtns;
  QList<QColor> m_colors;
  QPushButton *m_plus{nullptr};
  bool m_syncing{false};
};
