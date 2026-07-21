#pragma once

// Edge dock for the Drawboard utilities bar (undo / page / zoom).

enum class NoteChromeEdge {
  Top = 0,
  Bottom = 1,
  Left = 2,
  Right = 3,
};

inline bool noteChromeEdgeIsHorizontal(NoteChromeEdge e) {
  return e == NoteChromeEdge::Top || e == NoteChromeEdge::Bottom;
}
