<div align="center">

  <!-- Header Banner Placeholder -->
  <img src="https://via.placeholder.com/1200x300/1E1E2E/FFFFFF?text=BLOP+NOTES+%26+BLOP+STUDY" alt="Blop Banner" width="100%">

  <br/><br/>

  <h1>🚀 Blop Notes & Blop Study</h1>
  <p><b>Die moderne, plattformübergreifende Notiz- und Lernplattform für hochproduktives Arbeiten</b></p>

  <!-- Shields.io Badges -->
  <p>
    <a href="https://github.com/BenSchwank/Blop/actions/workflows/windows_build.yml">
      <img src="https://img.shields.io/badge/Windows-Build_Passing-0078D6?style=for-the-badge&logo=windows&logoColor=white" alt="Windows Build">
    </a>
    <a href="https://github.com/BenSchwank/Blop/actions/workflows/android_build.yml">
      <img src="https://img.shields.io/badge/Android-Build_Passing-3DDC84?style=for-the-badge&logo=android&logoColor=white" alt="Android Build">
    </a>
    <a href="#">
      <img src="https://img.shields.io/badge/C%2B%2B-20-00599C?style=for-the-badge&logo=cplusplus&logoColor=white" alt="C++20">
    </a>
    <a href="https://www.qt.io/">
      <img src="https://img.shields.io/badge/Framework-Qt_6.x-41CD52?style=for-the-badge&logo=qt&logoColor=white" alt="Qt 6">
    </a>
    <a href="#">
      <img src="https://img.shields.io/badge/License-MIT-blue.style=for-the-badge" alt="License">
    </a>
  </p>

  <p>
    <a href="#-features">Features</a> •
    <a href="#-architektur--technologies-stack">Architektur</a> •
    <a href="#-screenshots">Screenshots</a> •
    <a href="#-installation--build">Installation & Build</a> •
    <a href="#-mitwirken">Mitwirken</a>
  </p>

</div>

---

## 📌 Über das Projekt

**Blop** (bestehend aus *Blop Notes* und der Erweiterung *Blop Study*) ist eine leistungsstarke, flexible Anwendung zur digitalen Notizerstellung und Wissensorganisation. Entwickelt auf Basis von C++20 und dem Qt 6 Framework, kombiniert Blop die Performance nativer Anwendungsentwicklung mit einer modernen, anpassbaren Benutzeroberfläche.

Ob dynamische Canvas-Ansichten, Freigitter-Layouts oder strukturierte Mehrseiten-Dokumente – Blop liefert Werkzeuge für Studierende, Entwickler und Profis, um Ideen nahtlos festzuhalten und aufzubereiten.

---

## ✨ Features

| Feature | Beschreibung |
| :--- | :--- |
| 🖌️ **Multi-Page & Canvas View** | Flexible Bearbeitung in unendlichen Canvas-Räumen (`CanvasView`) oder strukturierten Dokumenten (`MultiPageNoteView`). |
| 🎨 **Erweiterte Zeichenwerkzeuge** | Unterstützung dynamischer Strichstärken, Farben und intelligenter Werkzeugmodi (`ToolMode`, `ModernToolBar`). |
| 🗂️ **UI-Profilverwaltung** | Erstellung und Wechsel individueller UI-Profile (`UIProfileManager`, `ProfileEditorDialog`). |
| ⚡ **Asynchroner Rendering-Core** | Ruckelfreies Laden von Bildern und Inhalten über benutzerdefinierte Image-Provider (`BlopAsyncImageProvider`). |
| 📱 **Cross-Platform Readiness** | Native Unterstützung für Windows und Android inklusive automatisierter CI/CD-Pipelines via GitHub Actions. |
| 🌐 **Blop Study Integration** | Integrierbares Framework für strukturierte Lernkarten, Wissensüberprüfungen und Lern-Dashboards. |

---

## 🖼️ Screenshots

<div align="center">

| Hauptansicht & Editor | UI-Profile & Einstellungen |
| :---: | :---: |
| <img src="https://via.placeholder.com/500x300/2A2A3C/FFFFFF?text=Main+Note+Editor+View" alt="Note Editor" width="100%"> | <img src="https://via.placeholder.com/500x300/2A2A3C/FFFFFF?text=UI+Profile+Manager" alt="Profile Manager" width="100%"> |

| Canvas & Zeichenbereich | Mobile (Android) UI |
| :---: | :---: |
| <img src="https://via.placeholder.com/500x300/2A2A3C/FFFFFF?text=Free+Grid+%26+Canvas+View" alt="Canvas View" width="100%"> | <img src="https://via.placeholder.com/500x300/2A2A3C/FFFFFF?text=Android+Interface" alt="Android UI" width="100%"> |

</div>

---

## 🛠️ Architektur & Technology Stack

Das Projekt setzt auf ein modernes C++/Qt-Layout mit sauberer Trennung von Daten- und Präsentationslogik:

* **Sprache:** C++20
* **UI-Framework:** Qt 6 (QML & Qt Widgets)
* **Build-System:** CMake (>= 3.16)
* **CI/CD:** GitHub Actions (`windows_build.yml`, `android_build.yml`)
* **Kernkomponenten:**
  * `NoteManager` & `Note`: Datenhaltung und Notizverwaltung
  * `CanvasView` / `FreeGridView`: Dynamische Rendering-Flächen
  * `UIProfileManager`: Dynamisches Styling & Anpassen der Werkzeugleisten

---

## 💻 Installation & Build

### Voraussetzungen

* **C++ Compiler:** MSVC 2019/2022, GCC oder Clang mit C++20 Unterstützung
* **Qt Framework:** Qt 6.x (inklusive QML und Quick-Modulen)
* **CMake:** Version 3.16 oder höher
