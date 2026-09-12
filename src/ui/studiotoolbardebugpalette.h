#pragma once

#include <QWidget>

class ModernToolbar;
class QButtonGroup;

/// Opt-in desktop overlay to switch studio toolbar variants (A–D) and
/// preview every ToolMode glyph. Enable via BLOP_TOOLBAR_DEBUG=1 or Settings.
class StudioToolbarDebugPalette : public QWidget {
  Q_OBJECT
public:
  explicit StudioToolbarDebugPalette(ModernToolbar *toolbar,
                                     QWidget *parent = nullptr);

  void refreshFromToolbar();

signals:
  void variantChosen(int variant);

private:
  ModernToolbar *m_toolbar{nullptr};
  QButtonGroup *m_variantGroup{nullptr};
};
