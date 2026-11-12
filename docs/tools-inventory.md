# Blop Werkzeug-Inventar und Verbesserungsworkflow

Quellen: [`ToolMode.h`](../src/core/ToolMode.h), [`MainWindow::setupTools`](../src/ui/mainwindow.cpp), [`tools/`](../tools/), [`tooloptionsstrip.cpp`](../src/ui/tooloptionsstrip.cpp), [`moderntoolbar.cpp`](../src/ui/moderntoolbar.cpp).

## Inventar (Stand Code)

| UI | Mode | Klasse | Status | Persistenz A4 | Chrome/Strip | Bekannte Lücken |
|---|---|---|---|---|---|---|
| Stift | Pen | `PenTool` | Voll | Ja (Strokes harvest) | Farbe, Dicke-Overlay, Druck | Basis neu geschrieben (clean v2) |
| Bleistift | Pencil | `PencilTool` | Voll | Ja | Farbe, Dicke, Textur, Härte | Basis neu geschrieben (clean v2) |
| Marker | Highlighter | `HighlighterTool` | Voll | Ja | Farbe, Dicke, Gerade | Basis neu geschrieben (clean v2) |
| Radierer | Eraser | `EraserTool` | Teil→P0 gefixt | `contentModified` → `scheduleStrokeSync`; filled-Pfade | Pixel/Objekt, Breite, Tinte behalten | Reload manuell prüfen |
| Auswahl | Lasso | `LassoTool` | Voll (Auswahl) | N/A (Selektion) | Freihand/Rechteck | — |
| Lineal | Ruler | `RulerTool` | Voll | Ja (`note.ruler` in JSON; versteckt restauriert) | Snap-Hinweis | Geodreieck-Modus + Größe persistiert |
| Bild | Image | `ImageTool` | Voll | Ja (`harvestImageItemsToNote`) | Opacity | — |
| Formen | Shape | `ShapeTool` | Voll | Ja (`shape`-Stroke: `fp`-Elemente + `ps`/`fc` in JSON) | Viele Shape-Kinds | Reload hält Kontur+Fill+Dashes; Rundtrip prüfen |
| Notiz | StickyNote | `StickyNoteTool` | Voll | Ja (`stickies`) | Farbe, Größe | — |
| Text | Text | `TextTool` + WordText | Voll (Word-Frames) | Ja (html/freeMove) | Font, Größe, Format | Keine Tabellen/Styles |
| Hand | Hand | `HandTool` | Voll | N/A (Pan) | Hinweis | View pannt; Tool gibt false zurück |
| Formel | Formula | `FormulaTool` | Voll | Graph via `syncGraphItemsToNote` | ehrlicher Hinweis | Sichtbar in Rail/Picker; Tablet-Shim |
| Molekül | Molecule | `MoleculeTool` | Stub | Nein | Stub-Hinweis | **Nicht in `setupTools` registriert**; Toolbar hidden (bewusst) |
| Bemaßung | Dimension | `DimensionTool` | Voll | Ja (`shape`-Stroke, Label im Pfad gebacken) | Kind-Chips + Maßstab | Fläche = leichte Schraffur; Tablet-Shim |
| Laser | Laser | `LaserTool` | Voll (ephemer) | Absichtlich nie | default | Overlay-Fade; kein Undo |

Zusätzlich: Graph-UI (`GraphCanvasItem`, Formelzonen) hängt an Formula/Shape; Math unter [`tools/math/`](../tools/math/).

## Prioritätsqueue

1. ~~Eraser-Persistenz~~ ✅ (`contentModified`→`scheduleStrokeSync`)  
2. ~~Shape-Persistenz~~ ✅ (`Stroke::shape` + fillColor/penStyle, `fp`-Serialisierung)  
3. ~~Formula Strip/Toolbar-Wahrheit~~ ✅ (Rail-Button, ehrlicher Strip, Panel-Titel)  
4. ~~Dimension Persistenz + Strip~~ ✅ (`shape_item`-Harvest, Label gebacken, Multi-Click fix)  
5. ~~Molecule registrieren oder UI entfernen~~ ✅ (bleibt Stub, überall ehrlich versteckt)  
6. ~~Ruler Persistenz-Audit~~ ✅ (`RulerItem::changed` → `syncStrokesToNote` → `note.ruler`)  

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
