#pragma once

// PenPresetBar — "Blop Chips": eine schlanke, immer sichtbare Favoritenleiste
// mit gespeicherten Werkzeug-Kombis (Tool + Farbe + Dicke + Deckkraft).
//
// Inspiriert von Drawboards Pen-Presets, aber mit eigener Blop-Identität:
//   * abgerundete Squircle-Chips statt runder Punkte,
//   * jeder Chip zeigt einen echten Mini-Pinselstrich-Preview in seiner
//     Farbe/Dicke (nicht nur einen Farbkreis),
//   * kurzes Antippen wendet das Preset an,
//   * langes Drücken überschreibt den Chip mit der aktuell aktiven
//     Werkzeug-Konfiguration ("merken").
//
// Persistenz über QSettings("Blop","BlopApp"), Key "tools/pen_presets".

#include <QColor>
#include <QPointer>
#include <QPropertyAnimation>
#include <QVector>
#include <QWidget>

#include "ToolMode.h"

struct PenPreset {
  ToolMode mode{ToolMode::Pen};
  QColor color{QColor(0, 0, 0)};
  int width{3};
  double opacity{1.0};

  bool operator==(const PenPreset &o) const {
    return mode == o.mode && color == o.color && width == o.width &&
           qFuzzyCompare(opacity + 1.0, o.opacity + 1.0);
  }
};

class PresetChip; // internal

class PenPresetBar : public QWidget {
  Q_OBJECT
public:
  explicit PenPresetBar(QWidget *parent = nullptr);

  void setAccentColor(const QColor &c);

  // Marks the chip whose preset matches (mode+color+width) as active, so the
  // bar reflects the currently selected tool. Pass an invalid combination to
  // clear the highlight.
  void syncActive(ToolMode mode, const QColor &color, int width);

  int preferredHeightPx() const;

signals:
  // Emitted when the user taps a chip: caller should apply the preset to the
  // active tool / toolbar / canvas.
  void presetSelected(const PenPreset &preset);

protected:
  void paintEvent(QPaintEvent *) override;
  void resizeEvent(QResizeEvent *) override;

private:
  void loadPresets();
  void savePresets() const;
  void rebuildChips();
  void relayoutChips();

  // Captures the current ToolManager configuration into slot `index` and
  // persists it (long-press handler).
  void captureCurrentInto(int index);

  QVector<PenPreset> m_presets;
  QVector<PresetChip *> m_chips;
  QColor m_accent{QColor("#7C5CFC")};
  int m_activeIndex{-1};
};
