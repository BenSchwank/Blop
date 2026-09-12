#pragma once
#include "AbstractTool.h"

/// Molecule: structure tool slot. Phase-1 selects the mode and shows chrome;
/// drawing/editor behavior follows in a later pass.
class MoleculeTool : public AbstractTool {
public:
  using AbstractTool::AbstractTool;

  ToolMode mode() const override { return ToolMode::Molecule; }
  QString name() const override { return QStringLiteral("Molekül"); }
  QString iconName() const override { return QStringLiteral("molecule"); }

  bool handleMousePress(QGraphicsSceneMouseEvent *,
                        QGraphicsScene *) override {
    return false;
  }
};
