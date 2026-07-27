# Eraser semantics (Pixel vs Object)

Status: Phase 1 documentation + headless tests (`blop_test_eraser_tool_sequence`).

## Modes

Configured via `ToolConfig::eraserMode` (`EraserMode::Pixel` | `EraserMode::Object`).
Props panel / MorphTray expose **Pixel** and **Objekt**. Settings apply to the
**next** erase gesture (not an existing selection).

### Pixel

- Hit radius = `penWidth / 2`.
- For each intersecting path stroke:
  - First cut: stroke the path outline with the current pen, then
    `outline.subtracted(eraserEllipse)`; set `pen = NoPen` and brush from the
    former pen brush; clear `StrokeItem::points()` so pressure segments cannot
    resurrect the cut.
  - Later cuts: subtract the eraser ellipse from the filled outline path.
  - Empty path after subtract → remove the item from the scene.
- Tagged non-path objects (`text`, `image`, `sticky_note`, `shape`) are
  **ignored** in Pixel mode.

### Object

- Hit radius is fixed at **5** scene units (independent of brush width).
- Whole path strokes (and tagged text/image/sticky/shape) that intersect the
  eraser ellipse are removed from the scene.

### Keep ink (`eraserKeepInk`)

When enabled (“Nur Marker löschen”), path items with `zValue() >= 10` are
skipped. Pen strokes use z≈10; highlighters typically sit lower — so Keep Ink
protects fountain-pen ink while still erasing markers.

## Session / Undo (A4)

`EraserTool` accumulates removals and pre-erase path snapshots during a
press→move→release gesture, then emits:

`eraseSessionFinished(removed, pathBefore)`

`MultiPageNoteView` wraps that in `SceneEraseCommand` for undo/redo and
persists via `persistSceneToNote`. Path-only / `NoPen` harvest must not rebuild
stale point lists.

## Persist

Harvested eraser cuts store the outline path with `NoPen` semantics. Reload
must keep the cut geometry; do not refill from cleared `points()`.

## Tests

`tests/eraser_tool_sequence.cpp` (ctest `eraser_tool_sequence`):

1. Pen press→move→release creates a `StrokeItem`
2. Pixel erase converts stroke to `NoPen` outline / may shrink path
3. Object erase removes the stroke
4. Tagged `text` ignored in Pixel, removed in Object
5. `eraserKeepInk` skips z≥10 strokes
