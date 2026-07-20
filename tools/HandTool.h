#pragma once
#include "AbstractTool.h"

/// Pan/Hand: no stroke tool — canvas pans via MultiPageNoteView / space-hold.
class HandTool : public AbstractTool {
public:
  using AbstractTool::AbstractTool;

  ToolMode mode() const override { return ToolMode::Hand; }
  QString name() const override { return QStringLiteral("Hand"); }
  QString iconName() const override { return QStringLiteral("hand"); }

  bool handleMousePress(QGraphicsSceneMouseEvent *,
                        QGraphicsScene *) override {
    return false; // Let the view handle pan.
  }
};
