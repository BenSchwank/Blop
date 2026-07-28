# Blop Release Roadmap

Ziel: Blop als vollwertige, gut aussehende, professionelle App releasen.
Jedes Tool muss einwandfrei funktionieren (erstellen → undo → redo → speichern → neu öffnen).

> Keine Kalender-Schätzungen. Aufwand = technische Scope / Risiko.
> Status pflegen: `[ ]` offen · `[~]` in Arbeit · `[x]` erledigt

Zuletzt aktualisiert: 2026-07-27 (Lasso/Transform + Text/Sticky/Image ctests)

---

## Ist-Stand (Kurz)

Vorhanden: Tool-Framework, A4-JSON-Persistenz, Infinite-V5, Desktop-NoteChrome,
Android-Phone-Toolbar, Windows/Android-CI, Crash-Consent + Privacy-Doc,
Share-UX ohne File-ID, Study offline-tolerant, Persistenz- + Tool-Sequenz-ctests.

Für Public Store-Release fehlen vor allem: Windows Authenticode + Clean-VM-Smoke,
Android Play-Signing-Secrets / OAuth-SHA-Verifikation + Data-Safety-Eintrag in der
Console (Checklisten liegen unter `docs/`), optional Tablet/Touch-Matrix und
Infinite-Graph-V6.

---

## Kritische Blocker (aktuell)

1. Windows: Code-Signing + Clean-VM-Smoke (Secrets/VM; Checkliste: `docs/windows-release-checklist.md`)
2. Android: Play Signing Secrets + OAuth SHA-1 in Cloud Console + Data Safety Form
   (Entwurf: `docs/android-play-data-safety.md`; CI signiert bereits wenn Secrets gesetzt)
3. Optional: Tablet/Touch-Event-Matrix für Writing/Eraser; Transform-Resize-Undo (nur pos-Move heute)
4. Infinite Graph-Persistenz (V6) — bewusst eingeschränkt; A4 = Release-Editor für Graphen

Erledigt / nicht mehr blockierend: A4-Undo-Kernpfade, Persistenz-ctests, Versionsquelle,
Share async + Friendly Errors, Crash-Consent, Study optional offline,
headless Lasso/Transform-Move + Text/Sticky/Image-Create.

---

## Tool-Matrix

| Tool | A4 Desktop | Infinite | Android | Hauptlücke |
|------|------------|----------|---------|------------|
| Pen / Pencil / Highlighter | stark | ok | ok | Touch/Tablet-Sequenz-Tests |
| Eraser | ok (Undo+Semantik) | ok | ok | Tablet-Druck-Pfad testen |
| Lasso / Transform | ok + ctest | ok | ok | Resize-Undo (nur Move-pos); Touch |
| Shape | ok | ok | ok | Interaktiver Create-Pfad (Path-Geometrie ctest vorhanden) |
| Graph | stark | **eingeschränkt** | ok (A4) | Infinite V6 oder weiter blockiert |
| Text / Sticky / Image | ok + create-ctest | ok | ok | Content-Edit-Undo headless; Image via Placement-Helper |
| Ruler / Hand | ok | ok | ok | Touch/Pen-Matrix |
| Markup / Favorites | ok | ok | anders | Parität Desktop↔Phone feinjustieren |

---

## Phase 0 — Daten dürfen nicht verloren gehen

**Ziel:** Round-Trip und Editor-Vertrag festnageln.

- [x] Roadmap als `docs/release-roadmap.md` ablegen
- [x] Persistenz-Roundtrip-Harness (A4 JSON: strokes, pressure, graphs, stickies, shapes, texts, images, page meta)
- [x] Infinite Binary Roundtrip (V5) analog (`InfiniteCanvasStore` + `blop_test_infinite_persistence_roundtrip`)
- [x] Eine Versionsquelle: Runtime (`BLOP_VERSION_STR`), Settings-UI, NSIS (CI-Override)
- [x] Editor-Entscheidung dokumentieren: A4 = Release-Editor; Infinite Parität oder klar eingeschränkt
- [x] Graphen in Infinite (V6) **oder** Graph-Erstellung dort deaktivieren
  - **Entscheidung 2026-07-27:** A4 ist Release-Editor für Graphen. Infinite blockiert
    `ShapeToolKind::CoordinateGraph` mit Hinweis (bis V6-Persistenz).
- [x] Share-Flows async (kein blockierendes `QEventLoop` auf `/api/shares/*` POSTs)

**Dateien:** `src/core/notemanager.*`, `src/core/Note.h`, `src/ui/canvasview.*`,
`CMakeLists.txt`, `installer.nsi`, `src/ui/settingsdialog.cpp`, `src/ui/mainwindow.cpp`

---

## Phase 1 — Jedes Tool vertrauenswürdig

**Ziel:** Jede sichtbare Änderung = Undo + Persist + Reload.

Pro Tool (Pen → Pencil → Highlighter → Eraser → Lasso → Shape → Graph → Sticky → Text → Image → Ruler → Hand):

1. Event-Sequenz (Maus + Tablet + Touch)
2. Undo / Redo
3. Speichern / Neuladen
4. Props klar: „nächster Strich“ vs. „Selektion ändern“

### Work items

- [x] QUndoCommands: Eraser (pixel/object) — A4 via `SceneEraseCommand` / `EraserTool::eraseSessionFinished`
- [x] QUndoCommands: Object move / transform — A4 selection move via `SceneItemsMoveCommand` (transform session follows)
- [x] QUndoCommands: Shape create/edit/delete — create + selection delete (A4); edit via move
- [x] QUndoCommands: Text / Sticky content changes — create + focus-session content undo (A4)
- [x] QUndoCommands: Image insert/move/scale/delete — insert + move + delete (A4); scale via transform follows
- [x] QUndoCommands: Graph function edits — A4 `GraphDataCommand` (add/remove/toggle/tangent/axes)
- [x] Scene→Note-Sync an Commands koppeln (`persistSceneToNote` / Infinite autosave) — delete/move/erase/create A4
- [x] Object-Inspector vs. Tool-Props trennen
  - Tool-Props = nächster Strich/Geste; Auswahl-HUD = Selektion; Eraser „Nur Marker löschen“
- [x] Pixel-Eraser-Semantik dokumentieren + testen — `docs/eraser-semantics.md` + `eraser_tool_sequence`
- [x] Event-Sequenz-Tests Writing + Eraser + Shape-Path + Lasso + Create
  - Pen / Pencil / Highlighter / Eraser: `blop_test_eraser_tool_sequence`
  - Shape-Geometrie: `blop_test_shape_path_sequence` (`tools/ShapePath.h`)
  - Lasso + Transform-Center-Move: `blop_test_lasso_tool_sequence`
  - Text / Sticky / Image-Placement: `blop_test_create_tool_sequence`
  - Offen: Tablet/Touch-Matrix; Transform-Resize-Undo

**Dateien:** `src/ui/multipagenoteview.*`, `src/ui/canvasview.*`, `tools/*`,
`src/ui/toolpropertiespanel.*`, `tools/ToolManager.*`

---

## Phase 2 — Professionelle Oberfläche

**Ziel:** Sieht und fühlt sich nach fertigem Produkt an.

- [x] In-Notiz-Suche: Ergebnisliste + Highlight auf der Seite
- [x] History: Kontext (Seite/Objekt), klare Disabled-States — Undo-Texte mit „· Seite N“; Disabled-Labels
- [x] Settings-IA: App / Editor / Tool / Selektion
  - **App:** SettingsDialog (Tab „App“; Cloud-Sync-Tab entfernt; Karte „Werkzeuge“)
  - **Editor:** Note-Sheet „Editor“ mit Tabs Seite / Notiz; App-Toolbar/Profile nur Hinweis
  - **Tool vs Selektion:** Props-Titel „Lasso“; Hinweise „nächster Strich“ vs Auswahl-HUD
- [x] Theme: `BlopTheme` vs. `NoteChrome` bereinigen; Light-Mode ohne dunkle Inseln — MorphTray/Props/NewTab/Radial
- [x] Share/Account: Status, Offline, Retry; kein File-ID im Happy Path
  - Keine Cloud-Datei-ID-Prompts/Toasts; Ordnerliste ohne IDs
  - Friendly Offline/401/5xx-Texte + „Erneut versuchen“; Resolve mit Progress
  - Sidebar-Konto-Status + Settings Konto-Zeile
- [x] Desktop- und Phone-Chrome konsistent (ohne Feature-Verlust)
  - Phone-Pill/Overflow/Brush → NoteChrome (charcoal + `#5B9DFF`) bei offener Notiz
  - Overflow: Suche, Verlauf, Editor…, Teilen… (wie Desktop-Rail)
  - Undo/Redo Enabled+Tooltips auf Phone-Pill; Seitenlabel „von“; Left-Rail bei Phone-UI ausgeblendet

**Dateien:** `src/ui/mainwindow.*`, `src/ui/settingsdialog.*`, `src/ui/notechrome.h`,
`src/ui/blop_theme.*`, `src/ui/moderntoolbar.*`, `src/ui/androidphonetoolbar.*`

---

## Phase 3 — Packaging & Launch

**Ziel:** Installierbar, signiert, policy-konform.

- [ ] Windows: Code-Signing, Installer-Version aus Git, Clean-VM-Smoke
  - Installer-Version aus Git: erledigt (CI/`installer.nsi`)
  - Signing + Clean-VM: Checkliste `docs/windows-release-checklist.md` (braucht Secrets/VM)
- [ ] Android: Play Signing / OAuth-SHA, Privacy/Data Safety, AAB-Smoke
  - CI signiert APK/AAB wenn Keystore-Secrets gesetzt
  - Console-Form + SHA-Verifikation: `docs/android-play-data-safety.md`
- [x] CI: ctest Persistenz + Tool-Sequenzen
  - Windows CI: `ctest -R persistence_roundtrip|infinite_persistence_roundtrip|eraser_tool_sequence|shape_path_sequence|lasso_tool_sequence|create_tool_sequence`
- [x] Crash-Consent-UI + Privacy Policy
  - First-run Prompt + Settings → Erweitert; Sentry erst nach Consent; `docs/privacy-policy.md`
- [x] Study als optionaler Dienst: Blop offline/ohne Backend stabil
  - SSO leert Session nicht bei leerem WebView; Gäste behalten Notes-Zugang
  - Study-Load-Fail Toast / Overlay „nicht erreichbar“; Notes weiter nutzbar

**Dateien:** `.github/workflows/*`, `installer.nsi`, `android/*`,
`src/observability/*`, `StudyFlow/backend/*`

---

## Definition of Done (Release-Bar)

Eine Version ist releasbar, wenn:

1. Jedes Tool auf Desktop-A4 und Android-Phone: erstellen → undo → redo → speichern → neu öffnen
2. Kein Datenverlust bei Crash (Autosave-Vertrag klar)
3. Light/Dark ohne tote dunkle Inseln
4. Share/Study degradieren graceful
5. Eine Versionsnummer überall gleich
6. Automatisierte Roundtrip- + Undo-Tests in CI grün

---

## Empfohlene Abarbeitungsreihenfolge

1. Phase 0.1 — Persistenz-Roundtrip-Tests
2. Phase 0.2 — Versionsvereinheitlichung
3. Phase 0.3 — Infinite↔A4 + Graph-Parität
4. Phase 1.1 — Undo Eraser + Selection Delete/Move
5. Phase 1.2 — Undo/Persist Text, Sticky, Image, Shape
6. Phase 1.3 — Graph-Edit-Undo
7. Phase 2 — Suche, Settings-IA, Theme, Share-UX
8. Phase 3 — Signing, Consent, Store-Checklisten
9. Rest: optional Tablet/Touch → Store Secrets → Tag

---

## Fortschritt loggen

Bei jedem abgeschlossenen Block: Checkbox hier setzen und kurzer Eintrag:

| Datum | Commit / Branch | Erledigt |
|-------|-----------------|----------|
| 2026-07-27 | `cursor/notepad-trust-polish-869e` | Roadmap angelegt; Phase 0.1 A4-Roundtrip-Test + Versions-Sync (Settings/NSIS/CI) |
| 2026-07-27 | `cursor/notepad-trust-polish-869e` | Phase 0.3: A4=Graph-Release-Editor; CoordinateGraph auf Infinite blockiert |
| 2026-07-27 | `cursor/notepad-trust-polish-869e` | Phase 0: Infinite V5 Store+Test; Share POSTs async; Phase 1.1 A4 Undo Delete/Move/Erase |
| 2026-07-27 | `cursor/notepad-trust-polish-869e` | Phase 1.2: A4 Undo Shape/Text/Sticky/Image create + text content |
| 2026-07-27 | `cursor/notepad-trust-polish-869e` | Phase 1.3: A4 Graph-Edit-Undo (Funktionen, Achsen, Graph löschen) |
| 2026-07-27 | `cursor/notepad-trust-polish-869e` | Phase 2: In-Notiz-Suche mit Trefferliste + Puls-Highlight auf der Seite |
| 2026-07-27 | `cursor/notepad-trust-polish-869e` | Phase 2: History „· Seite N“; Light-Mode MorphTray/Props/NewTab/Radial |
| 2026-07-27 | `cursor/notepad-trust-polish-869e` | Phase 2: Settings-IA — App/Editor/Tool/Selektion Ownership + Copy |
| 2026-07-27 | `cursor/notepad-trust-polish-869e` | Phase 2: Share/Account — kein File-ID im Happy Path; Retry/Offline; Status |
| 2026-07-27 | `cursor/notepad-trust-polish-869e` | Phase 2: Desktop/Phone NoteChrome — Phone-Pill + Overflow-Parität |
| 2026-07-27 | `cursor/notepad-trust-polish-869e` | Install-Skript: NetworkAuth-Paket + apt clock-skew Retry; Phase 3 ctest+Consent |
| 2026-07-27 | `cursor/notepad-trust-polish-869e` | Phase 3: Study optional/offline — kein SSO-Logout; Notes ohne Study |
| 2026-07-27 | `cursor/notepad-trust-polish-869e` | Phase 1: eraser-semantics.md + eraser_tool_sequence ctest; KeepInk in Props |
| 2026-07-27 | `cursor/notepad-trust-polish-869e` | Phase 1/3: Pencil+Highlighter Sequenz; ShapePath extract + ctest; Play/Windows Checklisten; Blocker-Refresh |
| 2026-07-27 | `cursor/notepad-trust-polish-869e` | Phase 1: lasso+transform-move ctest; Text/Sticky/Image create ctest; ImagePlacement helper |
