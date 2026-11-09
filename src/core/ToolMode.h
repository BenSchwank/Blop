#pragma once

enum class ToolMode {
    Pen,
    Pencil,
    Highlighter,
    Eraser,
    Lasso,
    Image,
    Ruler,
    Shape,
    StickyNote,
    Text,
    Hand,
    Formula,  // π / math block
    Molecule, // structure / chemistry
    Dimension, // technical length / angle / area dimensioning
    Laser     // ephemeral presentation pointer (not persisted)
};
