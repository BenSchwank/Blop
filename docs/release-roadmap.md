# Blop Release Roadmap

Ziel: Blop als vollwertige, gut aussehende, professionelle App releasen.
Jedes Tool muss einwandfrei funktionieren (erstellen → undo → redo → speichern → neu öffnen).

> Keine Kalender-Schätzungen. Aufwand = technische Scope / Risiko.
> Status pflegen: `[ ]` offen · `[~]` in Arbeit · `[x]` erledigt

Zuletzt aktualisiert: 2026-07-27 (Start Phase 0)

---

## Ist-Stand (Kurz)

Vorhanden: Tool-Framework, A4-JSON-Persistenz, Infinite-V5, Desktop-NoteChrome,
Android-Phone-Toolbar, Windows/Android-CI, Crash-Observability.

Fehlt für Public Release vor allem: vollständiges Undo, A4↔Infinite-Parität,
automatisierte Regressionstests, Versions-Einheit, Share/Privacy-Produktreife.

---

## Kritische Blocker

1. Undo deckt nicht alle Edits ab (Eraser, Move/Transform, Text/Sticky, Graph, Image).
2. A4 und Infinite sind nicht verhaltensgleich (u. a. Graphen fehlen in Infinite-V5).
3. Keine Tool-/Persistenz-Regressionstests in CI.
4. Versionierung inkonsistent (CMake / Settings / NSIS / Android).
5. Share/Cloud: blockierende Network-Loops, fragile UX.
6. Privacy/Consent für Crash-Reporting unvollständig.

---

## Tool-Matrix

| Tool | A4 Desktop | Infinite | Android | Hauptlücke |
|------|------------|----------|---------|------------|
| Pen / Pencil / Highlighter | stark | ok | ok | Undo + visuelle Tests |
| Eraser | riskant | riskant | riskant | Undo; Pixel-Outline-Semantik |
| Lasso / Transform | ok | ok | ok | Undo Move/Resize |
| Shape | ok | ok | ok | Editierbare Parameter + Undo |
| Graph | stark | **Lücke** | ok (A4) | Infinite-Persistenz |
| Text / Sticky / Image | ok | ok | ok | Edit-Undo, Mobile-UX |
| Ruler / Hand | ok | ok | ok | Interaktionsmatrix Touch/Pen |
| Markup / Favorites | ok | ok | anders | Parität Desktop↔Phone |

---

## Phase 0 — Daten dürfen nicht verloren gehen

**Ziel:** Round-Trip und Editor-Vertrag festnageln.

- [x] Roadmap als `docs/release-roadmap.md` ablegen
- [x] Persistenz-Roundtrip-Harness (A4 JSON: strokes, pressure, graphs, stickies, shapes, texts, images, page meta)
- [ ] Infinite Binary Roundtrip (V5/V6) analog
- [x] Eine Versionsquelle: Runtime (`BLOP_VERSION_STR`), Settings-UI, NSIS (CI-Override)
- [x] Editor-Entscheidung dokumentieren: A4 = Release-Editor; Infinite Parität oder klar eingeschränkt
- [x] Graphen in Infinite (V6) **oder** Graph-Erstellung dort deaktivieren
  - **Entscheidung 2026-07-27:** A4 ist Release-Editor für Graphen. Infinite blockiert
    `ShapeToolKind::CoordinateGraph` mit Hinweis (bis V6-Persistenz).
- [ ] Share-Flows async (kein blockierendes `QEventLoop`)

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

- [ ] QUndoCommands: Eraser (pixel/object)
- [ ] QUndoCommands: Object move / transform
- [ ] QUndoCommands: Shape create/edit/delete
- [ ] QUndoCommands: Text / Sticky content changes
- [ ] QUndoCommands: Image insert/move/scale/delete
- [ ] QUndoCommands: Graph function edits
- [ ] Scene→Note-Sync an Commands koppeln (`persistSceneToNote` / Infinite autosave)
- [ ] Object-Inspector vs. Tool-Props trennen
- [ ] Pixel-Eraser-Semantik dokumentieren + testen
- [ ] Event-Sequenz-Tests pro Tool (headless wo möglich)

**Dateien:** `src/ui/multipagenoteview.*`, `src/ui/canvasview.*`, `tools/*`,
`src/ui/toolpropertiespanel.*`, `tools/ToolManager.*`

---

## Phase 2 — Professionelle Oberfläche

**Ziel:** Sieht und fühlt sich nach fertigem Produkt an.

- [ ] In-Notiz-Suche: Ergebnisliste + Highlight auf der Seite
- [ ] History: Kontext (Seite/Objekt), klare Disabled-States
- [ ] Settings-IA: App / Editor / Tool / Selektion
- [ ] Theme: `BlopTheme` vs. `NoteChrome` bereinigen; Light-Mode ohne dunkle Inseln
- [ ] Share/Account: Status, Offline, Retry; kein File-ID im Happy Path
- [ ] Desktop- und Phone-Chrome konsistent (ohne Feature-Verlust)

**Dateien:** `src/ui/mainwindow.*`, `src/ui/settingsdialog.*`, `src/ui/notechrome.h`,
`src/ui/blop_theme.*`, `src/ui/moderntoolbar.*`, `src/ui/androidphonetoolbar.*`

---

## Phase 3 — Packaging & Launch

**Ziel:** Installierbar, signiert, policy-konform.

- [ ] Windows: Code-Signing, Installer-Version aus Git, Clean-VM-Smoke
- [ ] Android: Play Signing / OAuth-SHA, Privacy/Data Safety, AAB-Smoke
- [ ] CI: ctest für Persistenz + Tool-Sequenzen (Build allein reicht nicht)
- [ ] Crash-Consent-UI + Privacy Policy
- [ ] Study als optionaler Dienst: Blop offline/ohne Backend stabil

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

---

## Fortschritt loggen

Bei jedem abgeschlossenen Block: Checkbox hier setzen und kurzer Eintrag:

| Datum | Commit / Branch | Erledigt |
|-------|-----------------|----------|
| 2026-07-27 | `cursor/notepad-trust-polish-869e` | Roadmap angelegt; Phase 0.1 A4-Roundtrip-Test + Versions-Sync (Settings/NSIS/CI) |
| 2026-07-27 | `cursor/notepad-trust-polish-869e` | Phase 0.3: A4=Graph-Release-Editor; CoordinateGraph auf Infinite blockiert |
