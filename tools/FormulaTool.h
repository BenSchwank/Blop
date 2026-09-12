#pragma once
#include "AbstractTool.h"

/// Formula (π): selects math/formula mode. Canvas interaction is view-driven
/// for now; toolbar + ToolManager wiring is the Phase-1 contract.
class FormulaTool : public AbstractTool {
public:
  using AbstractTool::AbstractTool;

  ToolMode mode() const override { return ToolMode::Formula; }
  QString name() const override { return QStringLiteral("Formel"); }
  QString iconName() const override { return QStringLiteral("pi"); }

  bool handleMousePress(QGraphicsSceneMouseEvent *,
                        QGraphicsScene *) override {
    return false;
  }
};
