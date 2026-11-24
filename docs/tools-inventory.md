# Blop Werkzeug-Inventar und Verbesserungsworkflow

Quellen: [`ToolMode.h`](../src/core/ToolMode.h), [`MainWindow::setupTools`](../src/ui/mainwindow.cpp), [`tools/`](../tools/), [`tooloptionsstrip.cpp`](../src/ui/tooloptionsstrip.cpp), [`moderntoolbar.cpp`](../src/ui/moderntoolbar.cpp).

## Inventar (Stand Code)

| UI | Mode | Klasse | Status | Persistenz A4 | Chrome/Strip | Bekannte Lücken |
|---|---|---|---|---|---|---|
| Stift | Pen | `PenTool` | Voll | Ja (Strokes harvest; `ps` für Dash/Dot) | Farbe, Dicke-Overlay, Druck | Hold-Config + Dash/Dot commit+JSON |
| Bleistift | Pencil | `PencilTool` | Voll | Ja | Farbe, Dicke, Textur, Härte | Hold-Config verdrahtet (P0 Inventory-Pass) |
| Marker | Highlighter | `HighlighterTool` | Voll | Ja | Farbe, Dicke, Gerade | Hold-Config verdrahtet (P0 Inventory-Pass) |
| Radierer | Eraser | `EraserTool` | Voll | `contentModified` → `scheduleStrokeSync`; filled-Pfade (`fp`+OddEvenFill) | Pixel/Objekt, Breite, Tinte behalten | Livesmoke Reload: Pixel-Löcher bleiben (JSON `f=true` + UI) |
| Auswahl | Lasso | `LassoTool` | Voll (Auswahl) | Duplizieren via harvest (`shape`/`fc`/`ps`) + Text/Sticky/Image/Graph; Move+TransformOverlay | Freihand/Rechteck; Ctrl+D/C/X | Markup-Library nutzt denselben Harvest-Pfad |
| Lineal | Ruler | `RulerTool` | Voll | Ja (`note.ruler`: pos/rot/`compass`/w/h) | Lineal/Geodreieck-Chips + Einrasten | Größe bleibt bei Snap/ensureRulerExists; Modus-Wechsel setzt Default-Größe |
| Bild | Image | `ImageTool` | Voll | Ja (`harvestImageItemsToNote`); Lasso-Duplikat via `createImageItem` | Opacity | — |
| Formen | Shape | `ShapeTool` | Voll | Ja (`shape`-Stroke: `fp`-Elemente + `ps`/`fc` in JSON) | Viele Shape-Kinds | Livesmoke: Kinds + Gestrichelt (`ps=2`) + Fill (`fc`) nach Reload |
| Notiz | StickyNote | `StickyNoteTool` | Voll | Ja (`stickies`) | Farbe, Größe | — |
| Text | Text | `TextTool` + WordText | Voll (Word-Frames) | Ja (html/freeMove) | Font, Größe, F/K/U, Align, Listen | Styles verdrahtet (Strip+Ctrl+B/I/U); Tabellen bewusst später |
| Hand | Hand | `HandTool` | Voll | N/A (Pan) | Hinweis | View pannt; Tool gibt false zurück |
| Formel | Formula | `FormulaTool` | Voll | Graph via `syncGraphItemsToNote` | ehrlicher Hinweis | Sichtbar in Rail/Picker; Tablet-Shim |
| Molekül | Molecule | `MoleculeTool` | Stub | Nein | Stub-Hinweis | **Nicht in `setupTools` registriert**; Toolbar hidden (bewusst) |
| Bemaßung | Dimension | `DimensionTool` | Voll | Ja (`shape`-Stroke, Label+Hatch im Pfad gebacken) | Kind-Chips + Maßstab; Fläche-Hinweis | Fläche = N-Eck + diagonale Hatch + `A=… u²`; Schließen am Start/Doppelklick |
| Laser | Laser | `LaserTool` | Voll (ephemer) | Absichtlich nie | default | Overlay-Fade; kein Undo |

Zusätzlich: Graph-UI (`GraphCanvasItem`, Formelzonen) hängt an Formula/Shape; Math unter [`tools/math/`](../tools/math/).

## Prioritätsqueue

1. ~~Eraser-Persistenz~~ ✅ (`contentModified`→`scheduleStrokeSync`)  
2. ~~Shape-Persistenz~~ ✅ (`Stroke::shape` + fillColor/penStyle, `fp`-Serialisierung)  
3. ~~Formula Strip/Toolbar-Wahrheit~~ ✅ (Rail-Button, ehrlicher Strip, Panel-Titel)  
4. ~~Dimension Persistenz + Strip~~ ✅ (`shape_item`-Harvest, Label gebacken, Multi-Click fix)  
5. ~~Molecule registrieren oder UI entfernen~~ ✅ (bleibt Stub, überall ehrlich versteckt)  
6. ~~Ruler Persistenz-Audit~~ ✅ (`RulerItem::changed` → `syncStrokesToNote` → `note.ruler`)  

### Inventory-Pass (Funktionen, schwächstes zuerst)

1. ~~Ink Hold-Config ehrlich~~ ✅ (`AbstractStrokeTool`: `holdEnable*` + `holdStillDelayMs` + `holdShapeSensitivity`; Linie bleibt offen; Test `inkHoldDisabledKeepsFreehand`)  
2. ~~Eraser Reload-Smoke~~ ✅ Pixel-Löcher bleiben nach Speichern/Relaunch (`f`+`fp` in `.bnote`; UI-Zoom). Hole-Roundtrip-Test in `tst_core`. Objekt-Modus Strip vorhanden (Pixel/Objekt); Delete-Pfad = `contentModified`→sync  

3. ~~Shape Rundtrip~~ ✅ Livesmoke: Rechteck/Ellipse/Linie; Gestrichelt zeichnen+JSON `ps=2`+Reload; Fill `fc` hydrate (Unit `note_roundTripKeepsShapeFillDashAndPath`)  
4. ~~Lasso Nachbearbeitung~~ ✅ Duplicate nutzt `cloneHarvestedStroke` (behält `shape`/`fc`/`ps`); Ctrl+D; Livesmoke: rotes Fill-Rechteck dupliziert → JSON `fc×2`; TransformOverlay+Move  
5. ~~**Dimension Fläche**~~ ✅ N-Eck (schließen am Start/Doppelklick), diagonale Hatch im Pfad, Label `A=… u²`, Strip-Hinweis; Livesmoke: Preview-Hatch sichtbar  
6. ~~**Text Styles**~~ ✅ Config→`applyNewFrameStyle` + Strip F/K/U/Align/Listen + Ctrl+B/I/U/L/E/R in `WordText::handleKey`; HTML-Persistenz; Tabellen bewusst später  
7. ~~Formula/Molecule Caps~~ ✅ Formula: Strip-Hinweis + Graph-Panel; Molecule: Stub, `btnMolecule` hidden, Picker „folgt später“, nicht in `setupTools` — kein Feature-Creep  
8. ~~**Lasso Image-Duplicate**~~ ✅ `duplicateSelection` klont `image_item` (Pos+20, Opacity/Rotation/Pixels) → `harvestImageItemsToNote`; Test `image_offsetCloneKeepsPixels`  
9. ~~**Geodreieck Persistenz**~~ ✅ `setConfig` behält Größe außer bei Modus-Wechsel; Restore sync't `ToolManager.compassMode`; Strip-Chips Lineal/Geodreieck; `bindRulerSignals`+Sync nach ensure/toggle; Ctrl+S flushed harvest; Tests `rulerKeepsSizeWhenSnapToggles` / `note_roundTripKeepsRulerCompassAndSize`



## Gemini-Argon Master-Prompt

Ein Tool pro Lauf. Modell: `gemini-4-argon-medium` (bei Limit: `inherit` / Auto).

```
Du arbeitest im Blop-Repo (Qt 6 / C++17). Ziel: EIN Werkzeug verbessern.

Tool: <NAME> / ToolMode::<Mode> / Klasse <Class> in <path>

Kontext (unverändert lassen, außer nötig):
- Persistenz A4: MultiPageNoteView harvest/hydrate + NoteManager JSON
- Infinite: CanvasView
- Chrome: ToolOptionsStrip, ModernToolbar
- AGENTS.md Build: ./scripts/blop-local.ps1 build

Auftrag:
1) Lies Klasse + alle Call-Sites (Registrierung, Strip, Toolbar, Persistenz).
2) Liste konkrete Bugs/Lücken mit Datei:Zeile.
3) Schlage maximal 3 priorisierte Fixes vor (P0 Persistenz/Crash, P1 UX, P2 Polish).
4) Wenn environment=agent: implementiere nur P0 (+ P1 wenn klein), baue mit blop-local.ps1 build, keine Commits ohne Auftrag.
5) Antworte auf Deutsch: Status vorher/nachher, Diff-Kurzfassung, offene Risiken.

Nicht anfassen: Molecule/Formula UI-Lügen in einem anderen Lauf; keine Massen-Refactors.
```

### Eraser-Lauf (ausgefüllt)

```
Tool: Radierer / ToolMode::Eraser / Klasse EraserTool in tools/EraserTool.h
P0: Nach Pixel-/Objekt-Radieren Note-Modell syncen (contentModified → scheduleStrokeSync;
    filled-Pfade in harvest/hydrate behalten). Reload muss Löcher behalten.
```

## Workflow

1. Queue-Eintrag wählen.  
2. Agent mit Master-Prompt + einem Tool starten.  
3. Verify: `./scripts/blop-local.ps1 br` → zeichnen, speichern, Notiz neu öffnen, Strip prüfen.  
4. Gate: nächstes Tool erst wenn Reload den Inhalt behält (außer Laser).  
5. Diese Datei: Status/Persistenz-Zeile aktualisieren.  
