import os
import google.generativeai as genai
import json
from typing import List, Dict, Any

# Configure GenAI
api_key = os.environ.get("GOOGLE_API_KEY")
if api_key:
    genai.configure(api_key=api_key)


# Priority list of preferred models (newest/best first)
_PREFERRED_MODELS = [
    "gemini-2.5-pro",
    "gemini-2.0-pro-exp",
    "gemini-2.0-flash",
    "gemini-2.0-flash-lite",
    "gemini-1.5-pro",
    "gemini-1.5-flash",
    "gemini-1.5-flash-8b",
    "gemini-pro",
]

def get_best_model() -> str:
    """Dynamically detect the best available Gemini model for this API key."""
    try:
        available = []
        for m in genai.list_models():
            name = m.name.replace("models/", "")
            if hasattr(m, "supported_generation_methods") and "generateContent" in m.supported_generation_methods:
                available.append(name)

        print(f"Available models: {available}")

        for preferred in _PREFERRED_MODELS:
            if preferred in available:
                print(f"Auto-selected model: {preferred}")
                return preferred

        if available:
            print(f"Fallback model: {available[0]}")
            return available[0]
    except Exception as e:
        print(f"Could not list models: {e}")

    return "gemini-1.5-flash"  # last resort fallback


# Safety Settings - Allow all content to prevent blocking of valid study materials
SAFETY_SETTINGS = [
    {"category": "HARM_CATEGORY_HARASSMENT", "threshold": "BLOCK_NONE"},
    {"category": "HARM_CATEGORY_HATE_SPEECH", "threshold": "BLOCK_NONE"},
    {"category": "HARM_CATEGORY_SEXUALLY_EXPLICIT", "threshold": "BLOCK_NONE"},
    {"category": "HARM_CATEGORY_DANGEROUS_CONTENT", "threshold": "BLOCK_NONE"},
]

class AIService:
    @staticmethod
    def generate_summary(content: List[Any], detail_level: str = "Normal") -> str:
        """Generates a comprehensive summary from text or multimodal content."""
        try:
            model = genai.GenerativeModel(get_best_model())

            prompt = f"""
Du bist ein erfahrener Tutor und Lernassistent. Deine Aufgabe ist es, eine UMFASSENDE und DETAILLIERTE Zusammenfassung des gesamten Lernmaterials auf Deutsch zu erstellen.

Detailgrad: {detail_level}

WICHTIGE ANFORDERUNGEN:
- Die Zusammenfassung muss VOLLSTÄNDIG sein – decke ALLE wichtigen Themen, Konzepte und Details ab
- Sei so detailliert wie möglich – ein Schüler soll nur mit dieser Zusammenfassung lernen können
- Erkläre Konzepte verständlich und präzise
- Verwende konkrete Beispiele wo vorhanden
- Hebe prüfungsrelevante Inhalte besonders hervor

STRUKTUR (Markdown-Format):

# 📚 Zusammenfassung: [Titel des Themas]

## 🎯 Überblick
[Kurze Einführung: Worum geht es? Was sind die Kernthemen?]

## 📖 Hauptthemen

### [Thema 1]
**Definition/Erklärung:** [Ausführliche Erklärung]
**Wichtige Punkte:**
- [Punkt 1 mit Erklärung]
- [Punkt 2 mit Erklärung]
**Beispiel:** [Konkretes Beispiel wenn vorhanden]

### [Thema 2]
[Gleiche Struktur...]

[Alle weiteren Themen...]

## 🔑 Schlüsselbegriffe & Definitionen
| Begriff | Definition |
|---------|-----------|
| [Begriff] | [Präzise Definition] |
[Alle wichtigen Begriffe...]

## ⚡ Formeln & Regeln
[Falls vorhanden – alle Formeln, Regeln, Gesetze mit Erklärung]

## 🔗 Zusammenhänge & Verbindungen
[Wie hängen die Themen zusammen? Welche Beziehungen gibt es?]

## ⭐ Prüfungsrelevante Kernpunkte
[Die absolut wichtigsten Punkte die man kennen muss, nummeriert]

## 💡 Tipps & Merkhilfen
[Eselsbrücken, Merktechniken, häufige Fehler vermeiden]

Erstelle jetzt die vollständige, detaillierte Zusammenfassung basierend auf dem folgenden Material:
"""

            input_parts = [prompt]
            if isinstance(content, list):
                input_parts.extend(content)
            else:
                input_parts.append(content)

            response = model.generate_content(input_parts, safety_settings=SAFETY_SETTINGS)
            if not response.text:
                raise Exception("Leere Antwort vom Modell erhalten.")
            return response.text
        except Exception as e:
            print(f"Summary Error: {e}")
            raise Exception(f"Fehler bei der Zusammenfassung: {str(e)}")

    @staticmethod
    def generate_quiz(content: List[Any]) -> List[Dict[str, Any]]:
        """Generates a comprehensive quiz from multimodal content."""
        try:
            model = genai.GenerativeModel(get_best_model(), generation_config={"response_mime_type": "application/json"})
            prompt = """
Du bist ein erfahrener Lehrer. Erstelle ein anspruchsvolles Quiz mit 10 Fragen basierend auf dem Lernmaterial.

Anforderungen:
- Decke ALLE wichtigen Themen des Materials ab
- Mische verschiedene Schwierigkeitsgrade (leicht, mittel, schwer)
- Die Antwortoptionen sollen plausibel aber unterschiedlich sein
- Achte auf klare, eindeutige Fragestellungen
- Formuliere auf Deutsch

Ausgabe-Format: JSON Array mit genau 10 Objekten:
[
    {
        "question": "Präzise Frage auf Deutsch?",
        "options": ["Option A", "Option B", "Option C", "Option D"],
        "answer": "Option A",
        "explanation": "Kurze Erklärung warum diese Antwort richtig ist"
    }
]

Erstelle das Quiz für das folgende Material:
"""

            input_parts = [prompt]
            if isinstance(content, list):
                input_parts.extend(content)
            else:
                input_parts.append(content)

            response = model.generate_content(input_parts, safety_settings=SAFETY_SETTINGS)
            if not response.text:
                raise Exception("Leere Antwort vom Modell erhalten.")
            return json.loads(response.text)
        except Exception as e:
            print(f"Quiz Error: {e}")
            raise Exception(f"Fehler beim Quiz-Generieren: {str(e)}")

    @staticmethod
    def generate_flashcards(content: List[Any]) -> List[Dict[str, str]]:
        """Generates comprehensive flashcards from multimodal content."""
        try:
            model = genai.GenerativeModel(get_best_model(), generation_config={"response_mime_type": "application/json"})
            prompt = """
Du bist ein erfahrener Tutor. Erstelle 20 hochwertige Karteikarten aus dem Lernmaterial.

Anforderungen:
- Decke ALLE wichtigen Konzepte, Begriffe und Fakten ab
- Vorderseite: präzise Frage oder Begriff
- Rückseite: ausführliche, vollständige Antwort/Erklärung
- Variiere zwischen Definitionsfragen, Erklärungsfragen und Anwendungsfragen
- Deutsch

Format: JSON Array mit genau 20 Objekten:
[
    {
        "front": "Präzise Frage oder Begriff?",
        "back": "Vollständige Antwort mit allen relevanten Details und Erklärungen"
    }
]

Erstelle die Karteikarten für das folgende Material:
"""
            input_parts = [prompt]
            if isinstance(content, list):
                input_parts.extend(content)
            else:
                input_parts.append(content)

            response = model.generate_content(input_parts, safety_settings=SAFETY_SETTINGS)
            if not response.text:
                raise Exception("Leere Antwort vom Modell erhalten.")
            return json.loads(response.text)
        except Exception as e:
            print(f"Flashcard Error: {e}")
            raise Exception(f"Fehler beim Karteikarten-Generieren: {str(e)}")

    @staticmethod
    def generate_study_plan(content: List[Any], duration_days: int, hours_per_day: float = 2.0) -> List[Dict[str, Any]]:
        """Generates a detailed, actionable study plan from multimodal content."""
        try:
            model = genai.GenerativeModel(get_best_model(), generation_config={"response_mime_type": "application/json"})
            total_hours = duration_days * hours_per_day

            prompt = f"""
Du bist ein persönlicher Lerncoach und erstellst einen DETAILLIERTEN, UMSETZBAREN Lernplan.

RAHMENDATEN:
- Lernzeitraum: {duration_days} Tage
- Lernzeit pro Tag: {hours_per_day} Stunden
- Gesamtlernzeit: {total_hours} Stunden

ANFORDERUNGEN AN DEN LERNPLAN:
1. Teile den gesamten Stoff GLEICHMÄSSIG und SINNVOLL auf {duration_days} Tage auf
2. Jede Aufgabe muss KONKRET und UMSETZBAR sein (keine vagen Anweisungen wie "lerne Kapitel X")
3. Gib GENAUE Zeitangaben für jede Aufgabe (Summe = {hours_per_day}h pro Tag)
4. Berücksichtige Lernpsychologie: Wiederholungen einbauen, Pausen vorschlagen
5. Priorisiere schwierige Themen früh im Plan
6. Die letzten 1-2 Tage für Wiederholung und Übungen reservieren

FÜR JEDE AUFGABE ANGEBEN:
- Was genau zu tun ist (spezifisch, nicht vage)
- Wie lange es dauert
- Welche Methode empfohlen wird (lesen, zusammenfassen, Karteikarten, Übungen lösen...)

Ausgabe: JSON Array mit genau {duration_days} Einträgen:
[
    {{
        "day": 1,
        "topic": "Prägnanter Titel des Tagesthemas",
        "goal": "Was soll der Lernende am Ende dieses Tages können/wissen?",
        "tasks": [
            "✅ [Aufgabe 1] – Methode: [Methode] (ca. XX Min)",
            "✅ [Aufgabe 2] – Methode: [Methode] (ca. XX Min)",
            "✅ [Aufgabe 3] – Methode: [Methode] (ca. XX Min)"
        ],
        "focus": "⭐ Besonders wichtig: [Was ist der Kern dieses Tages?]",
        "tip": "💡 Lerntipp: [Spezifischer Tipp für diesen Tag]"
    }}
]

Analysiere das folgende Material und erstelle den vollständigen, detaillierten Lernplan:
"""
            input_parts = [prompt]
            if isinstance(content, list):
                input_parts.extend(content)
            else:
                input_parts.append(content)

            print(f"Generating study plan with {len(input_parts)} input parts...")
            response = model.generate_content(input_parts, safety_settings=SAFETY_SETTINGS)
            print(f"Plan response received: {response.text[:200] if response.text else 'EMPTY'}")

            if not response.text:
                raise Exception("Leere Antwort vom Modell erhalten. Prüfe ob die Sicherheitsfilter greifen.")

            result = json.loads(response.text)
            print(f"Parsed plan: {len(result)} days")
            return result
        except Exception as e:
            print(f"Plan Error: {e}")
            raise Exception(f"Fehler beim Lernplan-Generieren: {str(e)}")

    @staticmethod
    def transcribe_audio(audio_file_path: str) -> str:
        """Transcribes an audio file using Gemini's multimodal capabilities."""
        try:
            model = genai.GenerativeModel(get_best_model()) # Use the dynamically selected best model
            
            # Upload the file to Gemini API temporarily
            print(f"Uploading audio to Gemini: {audio_file_path}")
            uploaded_file = genai.upload_file(path=audio_file_path)
            
            prompt = """
Bitte erstelle ein präzises, zusammenhängendes und vollständiges Transkript dieser Audioaufnahme auf Deutsch. 
Erfasse alle wichtigen Details, Fachbegriffe und Zusammenhänge.
Lasse Füllwörter (Ähm, öhm) weg, um den Text lesbar zu machen.
"""
            response = model.generate_content([prompt, uploaded_file], safety_settings=SAFETY_SETTINGS)
            
            # Cleanup the file from Google's servers
            genai.delete_file(uploaded_file.name)
            
            if not response.text:
                raise Exception("Leeres Transkript vom Modell erhalten.")
            return response.text
        except Exception as e:
            print(f"Audio Transcription Error: {e}")
            raise Exception(f"Fehler bei der Audio-Transkription: {str(e)}")

    @staticmethod
    def generate_elaboration(content: List[Any], detail_level: str = "Normal", custom_rules: str = "") -> str:
        """Generates a detailed elaboration/essay based on the material and user instructions."""
        try:
            model = genai.GenerativeModel(get_best_model())

            prompt = f"""
Du bist ein akademischer Assistent. Erstelle eine fundierte Ausarbeitung (Essay, Textentwurf, Hausaufgabe) basierend auf dem folgenden Material.

Gewünschter Detailgrad / Länge: {detail_level}

{"Spezifische Anweisungen des Nutzers:" if custom_rules else "Generelle Anweisung:"}
{custom_rules if custom_rules else "Fasse die wichtigsten Aspekte des Materials in einem gut strukturierten Text zusammen."}

Achte auf eine klare Struktur:
1. Einleitung (Heranführung ans Thema)
2. Hauptteil (Logische, detaillierte Abhandlung mit Argumenten/Konzepten aus dem Material)
3. Fazit (Zusammenfassung und Schlussfolgerung)

Schreibe im Fließtext auf Deutsch, verwende Absätze zur besseren Lesbarkeit und formatiere Zwischenüberschriften mit Markdown.

Hier ist das Quellenmaterial:
"""

            input_parts = [prompt]
            if isinstance(content, list):
                input_parts.extend(content)
            else:
                input_parts.append(content)

            response = model.generate_content(input_parts, safety_settings=SAFETY_SETTINGS)
            if not response.text:
                raise Exception("Leere Antwort vom Modell erhalten.")
            return response.text
        except Exception as e:
            print(f"Elaboration Error: {e}")
            raise Exception(f"Fehler bei der Ausarbeitung: {str(e)}")

    @staticmethod
    def generate_repetition(content: List[Any], custom_rules: str = "") -> str:
        """Generates targeted spaced-repetition / review material."""
        try:
            model = genai.GenerativeModel(get_best_model())

            prompt = f"""
Du bist ein Lernexperte (wie Anki oder Mochi). Erstelle ein intensives Wiederholungsblatt (Review Sheet) basierend auf dem Lernmaterial.
Der Fokus liegt auf aktivem Abruf (Active Recall) und dem Schließen von Wissenslücken.

{"Spezifische Anweisungen/Schwerpunkte des Nutzers:" if custom_rules else "Genereller Fokus:"}
{custom_rules if custom_rules else "Extrahiere die wichtigsten Konzepte, die ein Schüler typischerweise vor der Prüfung vergessen könnte."}

Struktur der Wiederholung:
1. 🎯 **Kernkonzepte (TL;DR):** Die 3-5 allerwichtigsten Erkenntnisse in je einem Satz.
2. ❓ **Active Recall Fragen:** 5-10 anspruchsvolle Prüfungsfragen (ohne die Antworten direkt dazuzuschreiben, schreibe die Antworten in einen Spoiler-Block oder ans Ende).
3. 🧠 **Häufige Stolpersteine:** Welche Details verwechselt man leicht?
4. 🔗 **Kontext-Check:** Wie hängt das Hauptthema in das größere Ganze (Themenübergreifend)?

Nutze Markdown für ein übersichtliches und ansprechendes Layout auf Deutsch.

Hier ist das Quellenmaterial:
"""

            input_parts = [prompt]
            if isinstance(content, list):
                input_parts.extend(content)
            else:
                input_parts.append(content)

            response = model.generate_content(input_parts, safety_settings=SAFETY_SETTINGS)
            if not response.text:
                raise Exception("Leere Antwort vom Modell erhalten.")
            return response.text
        except Exception as e:
            print(f"Repetition Error: {e}")
            raise Exception(f"Fehler bei der Wiederholung: {str(e)}")

