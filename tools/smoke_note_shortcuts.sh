#!/usr/bin/env bash
# Desktop smoke helpers for the note editor (CI / local).
# Prefer keyboard shortcuts over coordinate clicks (QScroller eats clicks).
#
# Object names (findChild / AT-SPI):
#   NoteBottomChrome, NoteBtnUndo, NoteBtnRedo, NoteBtnZoomIn/Out,
#   NoteBtnFitWidth, NoteBtnFitPage, NoteLblZoom
# Left rail accessibleName: note_rail_<id> (pages, export, props, …)
#
# Shortcuts (editor focus, not in a text field):
#   P=Pen  H=Hand  M=Highlighter  E=Eraser  V=Lasso  T=Text
#   Ctrl+0=Fit page/content  Ctrl+1=Fit width  Delete=selection
set -euo pipefail
echo "Note editor shortcut map:"
echo "  P Pen | H Hand | M Highlighter | E Eraser | V Lasso | T Text"
echo "  Ctrl+0 Fit page | Ctrl+1 Fit width | Delete selection"
echo "Bottom chrome objectNames: NoteBtnFitWidth NoteBtnFitPage NoteBtnZoomIn …"
