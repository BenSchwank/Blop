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

def get_best_model(model_preference: str = None) -> str:
    """Dynamically detect the best available Gemini model for this API key."""
    if model_preference:
        return model_preference
        
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

# In alle mathematisch/naturwissenschaftlich relevanten Prompts einfügen (Rechenfehler in Musterlösungen vermeiden).
MATH_ACCURACY_INSTRUCTIONS = """
NUMERISCHE GENAUIGKEIT UND PLAUSIBILITÄT (verbindlich, sobald Zahlen, Funktionen, Ableitungen, Integrale oder Modelle vorkommen):
- Leite jede behauptete Zahl und jedes Näherungsergebnis Schritt für Schritt aus den gegebenen Formeln ab. Zeige explizite Zwischenergebnisse (z. B. Potenzen von $e$, Produkte, Summen im Nenner), bevor du dividierst oder rundest. Keine Endwerte raten oder „aus dem Gedächtnis“ erfinden.
- Wenn sich ein Zwischenwert ändert, müssen alle folgenden Werte (Differenzenquotient, Gleichungen, Lösungen von CAS-Aufgaben) konsistent neu berechnet werden — keine inkonsistente Kette.
- Nach dem Lösen von Gleichungen oder beim Nennen einer konkreten Stelle $x$: immer eine kurze PROBE — setze den Wert in die passende Originalfunktion, die Ableitung oder die Ausgangsgleichung ein und zeige, dass das Ergebnis (bis auf Rundung) stimmt. Beispiel: Bei „$f'(x) = k$“ das gefundene $x$ in $f'(x)$ einsetzen und mit $k$ vergleichen.
- Unterscheide streng: durchschnittliche Änderungsrate / Differenzenquotient über ein Intervall vs. momentane Änderungsrate / Ableitung an einem Punkt — nicht verwechseln und nicht dieselbe Zahl ohne Begründung für beides verwenden.
- Rundung transparent machen (Zwischenergebnisse mit ausreichend Stellen); wo sinnvoll, exakte Brüche oder algebraische Ausdrücke angeben.
- Wenn du dir bei einer komplexen Zahl nicht sicher bist: den exakten Ausdruck angeben und ausdrücklich zur Nachrechnung mit dem Taschenrechner auffordern — lieber ehrlich als falsch.
- Vor dem Abschluss: kurze Plausibilitätsprüfung (Größenordnung, Monotonie, Randwerte, ob das Ergebnis zum Modell passt).
"""

class AIService:
    @staticmethod
    def generate_summary(content: List[Any], detail_level: str = "Normal", model_preference: str = None, learning_mode: str = "normal") -> str:
        """Generates a comprehensive summary from text or multimodal content."""
        try:
            model = genai.GenerativeModel(get_best_model(model_preference))

            # --- Learning Mode Preamble ---
            if learning_mode == "exercise":
                mode_preamble = """
DU BIST IM KONZEPTLERNEN-MODUS.
Das hochgeladene Material enthält Übungsaufgaben, Prüfungen oder Arbeitsblätter.
DEINE AUFGABE: Erkläre NICHT die Lösungen der Aufgaben direkt.
Erstelle stattdessen eine Zusammenfassung der KONZEPTE, METHODEN und STRATEGIEN
die ein Schüler beherrschen muss, um diese Art von Aufgaben selbst lösen zu können.
Denke: "Was muss ich WISSEN und KÖNNEN, um solche Aufgaben zu meistern?"
"""
            else:
                mode_preamble = ""

            prompt = f"""
Du bist ein erfahrener Tutor und Lernassistent. Deine Aufgabe ist es, eine UMFASSENDE und DETAILLIERTE Zusammenfassung des gesamten Lernmaterials auf Deutsch zu erstellen.
{mode_preamble}
Detailgrad: {detail_level}

WICHTIGE FORMATIERUNGSREGEL FÜR MATHEMATIK:
Verwende IMMER die LaTeX-Notation für mathematische Formeln und Ausdrücke. 
- Für Inline-Formeln (im Textfluss) verwende ein einzelnes Dollarzeichen: $E = mc^2$
- Für Block-Formeln (eigene Zeile) verwende doppelte Dollarzeichen: $$E = mc^2$$

{MATH_ACCURACY_INSTRUCTIONS}

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
    def generate_quiz(content: List[Any], model_preference: str = None) -> List[Dict[str, Any]]:
        """Generates a comprehensive quiz from multimodal content."""
        try:
            model = genai.GenerativeModel(get_best_model(model_preference), generation_config={"response_mime_type": "application/json"})
            prompt = """
Du bist ein erfahrener Lehrer. Erstelle ein anspruchsvolles Quiz mit 10 Fragen basierend auf dem Lernmaterial.

WICHTIGE FORMATIERUNGSREGEL FÜR MATHEMATIK:
Verwende IMMER die LaTeX-Notation für mathematische Formeln und Ausdrücke in den Fragen und Antworten. 
- Für Inline-Formeln (im Textfluss) verwende ein einzelnes Dollarzeichen: $E = mc^2$
- Für Block-Formeln (eigene Zeile) verwende doppelte Dollarzeichen: $$E = mc^2$$

""" + MATH_ACCURACY_INSTRUCTIONS + """
- Die als richtig markierte Antwort und die Erklärung müssen rechnerisch stimmig sein; falsche Zahlen in Erklärungen sind inakzeptabel.

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
    def generate_flashcards(content: List[Any], model_preference: str = None) -> List[Dict[str, str]]:
        """Generates comprehensive flashcards from multimodal content."""
        try:
            model = genai.GenerativeModel(get_best_model(model_preference), generation_config={"response_mime_type": "application/json"})
            prompt = """
Du bist ein erfahrener Tutor. Erstelle 20 hochwertige Karteikarten aus dem Lernmaterial.

WICHTIGE FORMATIERUNGSREGEL FÜR MATHEMATIK:
Verwende IMMER die LaTeX-Notation für mathematische Formeln und Ausdrücke. 
- Für Inline-Formeln (im Textfluss) verwende ein einzelnes Dollarzeichen: $E = mc^2$
- Für Block-Formeln (eigene Zeile) verwende doppelte Dollarzeichen: $$E = mc^2$$

""" + MATH_ACCURACY_INSTRUCTIONS + """
- Auf der Rückseite bei Rechenaufgaben: nur Ergebnisse mit nachvollziehbarer Kette und Probe angeben.

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
    def generate_study_plan(content: List[Any], duration_days: int, hours_per_day: float = 2.0, model_preference: str = None, active_days: list[int] = None, learning_mode: str = "normal") -> List[Dict[str, Any]]:
        """Generates a detailed, actionable study plan from multimodal content."""
        try:
            model = genai.GenerativeModel(get_best_model(model_preference), generation_config={"response_mime_type": "application/json"})
            total_hours = duration_days * hours_per_day

            # Map the active_days array to a string of weekday names for the prompt
            weekday_map = {0: "Montag", 1: "Dienstag", 2: "Mittwoch", 3: "Donnerstag", 4: "Freitag", 5: "Samstag", 6: "Sonntag"}
            active_days_str = "Alle Tage"
            if active_days is not None and len(active_days) > 0:
                active_days_str = ", ".join([weekday_map.get(d, "") for d in active_days])

            # --- Learning Mode Preamble ---
            if learning_mode == "exercise":
                mode_preamble = """
KONZEPTLERNEN-MODUS AKTIV:
Das hochgeladene Material enthält Übungsaufgaben, Prüfungsaufgaben oder Arbeitsblätter.
Erstelle KEINEN Plan der die Aufgaben direkt löst oder abarbeitet.
Erstelle stattdessen einen LERNPLAN der Schritt für Schritt die KONZEPTE, METHODEN und LÖSUNGSSTRATEGIEN
vermittelt, die ein Schüler benötigt, um diese Art von Aufgaben eigenständig zu lösen.
Bei jedem Tag: "Was muss der Schüler VERSTEHEN und ÜBEN, damit er diese Aufgaben selbst meistern kann?"
Die Aufgaben im Plan sollen das ERLERNEN der Methoden beinhalten, z.B. "Lerne die Methode X anhand von Beispielen aus dem Material".
"""
            else:
                mode_preamble = ""

            prompt = f"""
Du bist ein persönlicher Lerncoach und erstellst einen DETAILLIERTEN, UMSETZBAREN Lernplan.
{mode_preamble}
RAHMENDATEN:
- Lernzeitraum: {duration_days} Tage
- Lernzeit pro Tag: {hours_per_day} Stunden
- Gesamtlernzeit: {total_hours} Stunden
- Aktive Lerntage: {active_days_str}

WICHTIGE FORMATIERUNGSREGEL FÜR MATHEMATIK:
Verwende IMMER die LaTeX-Notation für mathematische Formeln und Ausdrücke in den Beschreibungen und Zusammenfassungen. 
- Für Inline-Formeln (im Textfluss) verwende ein einzelnes Dollarzeichen: $E = mc^2$
- Für Block-Formeln (eigene Zeile) verwende doppelte Dollarzeichen: $$E = mc^2$$

{MATH_ACCURACY_INSTRUCTIONS}

ANCORDERUNGEN AN DEN LERNPLAN:
1. Teile den gesamten Stoff GLEICHMÄSSIG und SINNVOLL auf {duration_days} Tage auf. ACHTUNG: Plane NUR an den "Aktiven Lerntagen" Aufgaben ein. Wenn der Tag der Woche KEIN aktiver Lerntag ist, weise dem Tag KEINE Aufgaben zu und setze als Ziel/Summary "Ruhetag" oder "Pause".
   WICHTIGE REGEL: Tag 1 (der ERSTE Tag des Plans) darf unter keinen Umständen ein Ruhetag sein. Beginne den Plan IMMER an einem aktiven Lerntag.
2. Jede Aufgabe muss KONKRET und UMSETZBAR sein (keine vagen Anweisungen wie "lerne Kapitel X")
3. Gib GENAUE Zeitangaben für jede Aufgabe (Summe = {hours_per_day}h pro Tag)
4. Berücksichtige Lernpsychologie: Wiederholungen einbauen, Pausen vorschlagen
5. Erstelle für jeden Tag eine kurze INHALTLICHE ZUSAMMENFASSUNG (3-5 Sätze) des Tagesstoffs, sodass aus dem Lernplan direkt gelernt werden kann. (Mache dies nicht für Ruhetage).
6. Die Aufgaben sollen als ein JSON-Array von Objekten mit einem `description`-Feld und einem `completed`-Feld (immer auf false setzen) zurückgegeben werden.

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
        "summary": "Die wichtigste Zusammenfassung / der zu lernende Inhalt für diesen Tag in 3-5 kompletten Sätzen (z.B. Definitionen, Kernkonzepte).",
        "tasks": [
            {{"description": "✅ [Aufgabe 1] – Methode: [Methode] (ca. XX Min)", "completed": false}},
            {{"description": "✅ [Aufgabe 2] – Methode: [Methode] (ca. XX Min)", "completed": false}}
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
    def generate_task_help(content: List[Any], task_description: str, model_preference: str = None) -> str:
        """Generates a concise, context-aware explanation/help text for a specific study plan task."""
        try:
            model = genai.GenerativeModel(get_best_model(model_preference))
            
            prompt = f"""
Du bist ein hilfreicher Lern-Kompagnon. 
Der Student hat folgende Aufgabe in seinem Lernplan:
"{task_description}"

Erkläre KURZ, KONKRET und HILFREICH, was bei dieser Aufgabe zu tun ist und welche Inhalte aus dem beiliegenden Material relevant sind. 
Die Erklärung soll maximal 3-5 Sätze lang sein und direkt dabei helfen, die Aufgabe zu starten oder zu verstehen. Antworte in Markdown.

WICHTIGE FORMATIERUNGSREGEL FÜR MATHEMATIK:
Verwende IMMER die LaTeX-Notation für mathematische Formeln und Ausdrücke. 
- Für Inline-Formeln verwende ein einzelnes Dollarzeichen: $E = mc^2$
- Für Block-Formeln verwende doppelte Dollarzeichen: $$E = mc^2$$

{MATH_ACCURACY_INSTRUCTIONS}

Analysiere dazu folgendes Material aus dem Ordner des Studenten:
"""
            input_parts = [prompt]
            if isinstance(content, list):
                input_parts.extend(content)
            else:
                input_parts.append(content)

            print(f"Generating task help for: {task_description[:50]}...")
            response = model.generate_content(input_parts, safety_settings=SAFETY_SETTINGS)
            
            if not response.text:
                raise Exception("Leere Antwort vom Modell erhalten.")
                
            return response.text
        except Exception as e:
            print(f"Task Help Error: {e}")
            raise Exception(f"Fehler bei der Aufgaben-Hilfe: {str(e)}")

    @staticmethod
    def transcribe_audio(audio_file_path: str, model_preference: str = None) -> str:
        """Transcribes an audio file using Gemini's multimodal capabilities."""
        try:
            model = genai.GenerativeModel(get_best_model(model_preference), generation_config={"response_mime_type": "application/json"})
            
            # Upload the file to Gemini API temporarily
            print(f"Uploading audio to Gemini: {audio_file_path}")
            import time
            
            # Manually set mime_type because .webm defaults to video/webm
            # which fails Gemini's video frame processing for audio-only files!
            mime_type = None
            if audio_file_path.endswith('.webm'):
                mime_type = "audio/webm"
            elif audio_file_path.endswith('.mp3'):
                mime_type = "audio/mp3"
            elif audio_file_path.endswith('.wav'):
                mime_type = "audio/wav"
            elif audio_file_path.endswith('.m4a'):
                mime_type = "audio/mp4"

            uploaded_file = genai.upload_file(path=audio_file_path, mime_type=mime_type)
            
            while uploaded_file.state.name == "PROCESSING":
                print("Waiting for audio processing...")
                time.sleep(2)
                uploaded_file = genai.get_file(uploaded_file.name)
                
            if uploaded_file.state.name == "FAILED":
                raise Exception("Audio file processing failed on Gemini's servers.")
            prompt = """
Du bist ein professioneller Mitschriften-Assistent. Du hast Zugriff auf eine Audioaufnahme einer Vorlesung / eines Unterrichts.

Deine Aufgabe: Erstelle eine VOLLSTÄNDIGE, AUSFÜHRLICHE und DETAILLIERTE Mitschrift — so als ob ein sehr fleißiger Schüler mitgeschrieben hätte. Die Mitschrift soll LANG sein und den gesamten Inhalt des Audios abdecken.

WICHTIGE FORMATIERUNGSREGEL FÜR MATHEMATIK:
Verwende IMMER die LaTeX-Notation für mathematische Formeln und Ausdrücke. 
- Für Inline-Formeln (im Textfluss) verwende ein einzelnes Dollarzeichen: $E = mc^2$
- Für Block-Formeln (eigene Zeile) verwende doppelte Dollarzeichen: $$E = mc^2$$

""" + MATH_ACCURACY_INSTRUCTIONS + """
WICHTIGE ANWEISUNG ZUR LÄNGE:
- Bei einem kurzen Audio (< 5 Min): Mindestens 300 Wörter Mitschrift.
- Bei einem mittleren Audio (5-20 Min): Mindestens 800 Wörter Mitschrift.
- Bei einem langen Audio (20+ Min): Mindestens 1500-3000 Wörter Mitschrift. Gehe durch das Audio CHRONOLOGISCH und lasse NICHTS aus!
- Wenn der Lehrer Beispiele nennt → schreibe sie auf.
- Wenn Formeln erklärt werden → schreibe sie auf.
- Wenn Definitionen gegeben werden → schreibe sie wörtlich auf.
- Lasse KEINE Inhalte weg — auch scheinbar unwichtige Erklärungen oder Wiederholungen.

Die Mitschrift MUSS folgende Struktur haben:

# [Passender Titel für das Thema der Stunde]

## 📝 Zusammenfassung (kurz)
[2-3 Sätze: Gesamteindruck der Stunde]

## 📖 Vollständige Mitschrift

### [Thema 1 / Abschnitt 1]
[Ausführliche, detaillierte Notizen zum ersten Thema]

### [Thema 2 / Abschnitt 2]
[Ausführliche, detaillierte Notizen zum zweiten Thema]

... [So viele Abschnitte wie nötig, um den gesamten Stoff abzubilden]

## 🎯 Wichtigste Konzepte & Definitionen
[Tabellarische oder aufgelistete, wichtige Definitionen, Formeln, Regeln aus dem Audio]

## 💡 Fazit / Lernziele
[Was ist das wichtigste Lernziel dieser Stunde?]

WICHTIG:
- Schreibe auf Deutsch.
- Lasse Füllwörter (ähm, öhm) weg.
- Gehe das Audio CHRONOLOGISCH durch — nichts auslassen!
- Die Mitschrift MUSS dem Umfang des Audios entsprechen. Ein 55-minütiges Audio → sehr lange, mehrseitige Mitschrift!

Gebe als Antwort AUSSCHLIESSLICH ein valides JSON-Objekt im folgenden Format zurück (KEIN Markdown-Code-Block drumherum):
{
  "title": "Der Titel der Stunde",
  "content": "Die vollständige Mitschrift als Markdown-Text"
}
"""
            model = genai.GenerativeModel(get_best_model(model_preference), generation_config={"response_mime_type": "application/json"})
            response = model.generate_content([prompt, uploaded_file], safety_settings=SAFETY_SETTINGS)
            
            # Cleanup the file from Google's servers
            genai.delete_file(uploaded_file.name)
            
            if not response.text:
                raise Exception("Leeres Transkript vom Modell erhalten.")
                
            result = json.loads(response.text)
            return result
        except Exception as e:
            print(f"Audio Transcription Error: {e}")
            raise Exception(f"Fehler bei der Audio-Transkription: {str(e)}")

    @staticmethod
    def generate_elaboration(content: List[Any], detail_level: str = "Normal", custom_rules: str = "", model_preference: str = None) -> str:
        """Generates a detailed elaboration/essay based on the material and user instructions."""
        try:
            model = genai.GenerativeModel(get_best_model(model_preference))

            prompt = f"""
Du bist ein akademischer Assistent. Erstelle eine fundierte Ausarbeitung (Essay, Textentwurf, Hausaufgabe) basierend auf dem folgenden Material.

Gewünschter Detailgrad / Länge: {detail_level}

{"Spezifische Anweisungen des Nutzers:" if custom_rules else "Generelle Anweisung:"}
{custom_rules if custom_rules else "Fasse die wichtigsten Aspekte des Materials in einem gut strukturierten Text zusammen."}

WICHTIGE FORMATIERUNGSREGEL FÜR MATHEMATIK:
Verwende IMMER die LaTeX-Notation für mathematische Formeln und Ausdrücke. 
- Für Inline-Formeln (im Textfluss) verwende ein einzelnes Dollarzeichen: $E = mc^2$
- Für Block-Formeln (eigene Zeile) verwende doppelte Dollarzeichen: $$E = mc^2$$

{MATH_ACCURACY_INSTRUCTIONS}

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
    def generate_repetition(content: List[Any], custom_rules: str = "", model_preference: str = None, learning_mode: str = "normal") -> Any:
        """Generates targeted spaced-repetition / review material."""
        try:
            # Lange, exam-taugliche Ausgaben (Default-Modell-Limit sonst oft zu knapp)
            repetition_generation_config = {
                "max_output_tokens": 16384,
                "temperature": 0.45,
            }
            model = genai.GenerativeModel(
                get_best_model(model_preference),
                generation_config=repetition_generation_config,
            )

            # --- Learning Mode Preamble ---
            if learning_mode == "exercise":
                mode_note = """KONZEPTLERNEN-MODUS:
Das Material enthält Übungs- oder Prüfungsaufgaben. Erkläre NICHT nur stichpunktartig, sondern UMFANGREICH die KONZEPTE, METHODEN und STANDARD-VERFAHREN,
die man braucht, um diese Aufgabentypen zu lösen.
- Beziehe dich auf ALLE erkennbaren Aufgabenbereiche/Themenblöcke aus dem Material (nicht nur ein paar Highlights). Pro größerem Abschnitt oder Aufgabentyp: eigener Unterabschnitt mit tiefer Erklärung, typischen Mustern und Merksätzen.
- Die Active-Recall-Fragen am Ende prüfen Verständnis der Methoden (nicht reines Auswendiglernen von Endzahlen einer konkreten Aufgabe)."""
                if custom_rules.strip():
                    mode_note += f"\n\nZusätzliche Wünsche des Nutzers:\n{custom_rules.strip()}"
            else:
                if custom_rules.strip():
                    mode_note = f"""Nutzer-Schwerpunkte (gewichte diese stärker, bleibe aber gründlich):
{custom_rules.strip()}
Decke nach Möglichkeit weiterhin das gesamte Material sinnvoll ab. Nur wenn der Nutzer explizit nur ein einzelnes Thema will (z. B. „nur Aufgabe 3“), darfst du den Rest weglassen."""
                else:
                    mode_note = """STANDARD (keine Schwerpunkte vom Nutzer):
- Gehe das gesamte Quellenmaterial systematisch durch. Keine willkürliche Kurzfassung und keine Auswahl „nur der wichtigsten“ Aufgaben, wenn das Material eine Prüfung, Klausur oder Aufgabensammlung ist.
- Bei Prüfungen/Klausuren: arbeite JEDE erkennbare Aufgabe und Teilaufgabe in der Reihenfolge des Dokuments ab (z. B. Teil A/B, 1a, 1b, 2.1, 2.2 …). Überspringe nichts.
- Pro Aufgabe/Teilaufgabe: (1) Aufgabenidee kurz in eigenen Worten, (2) vollständiger Lösungsweg mit Begründungen — Schritt für Schritt, so ausführlich wie eine gute Musterlösung, (3) optional: typische Fehler oder Alternativweg.
- Gesamtlänge: Die Wiederholung soll DEUTLICH ausführlich sein (mindestens Umfang mehrerer DIN-A4-Seiten Fließtext). Kurz-TL;DR allein reicht nicht."""

            prompt = f"""
Du bist ein erfahrener Mathe-/Naturwissenschafts-Tutor. Erstelle eine SEHR AUSFÜHRLICHE Wiederholung (Review / Prüfungsvorbereitung) auf Deutsch.
Ziel: Ein Lernender soll mit diesem Dokument die Inhalte und Lösungswege wirklich vertiefen können — nicht nur oberflächlich wiederholen.

Fokus / Modus:
{mode_note}

UMFANG (verbindlich):
- Schreibe lang und detailliert. Lieber zu ausführlich als zu kurz.
- Der mittlere Hauptteil (siehe Struktur unten) soll mindestens etwa 60–70 % der Gesamtlänge ausmachen.

WICHTIGE FORMATIERUNGSREGEL FÜR MATHEMATIK:
Verwende IMMER die LaTeX-Notation für mathematische Formeln und Ausdrücke.
- Inline: $E = mc^2$
- Block: $$E = mc^2$$
- Jede $$-Umgebung korrekt schließen. Keine einzelnen $ im normalen Text (Währung als Wort „Euro“ o. Ä.).

{MATH_ACCURACY_INSTRUCTIONS}
- In Musterlösungen zu Prüfungsaufgaben sind falsche Zahlen besonders schädlich: jede Teilaufgabe mindestens einmal logisch gegenprüfen (Zwischenwerte, Probe).

Struktur (sauberes Markdown, in dieser Reihenfolge):

## Kurzüberblick
8–12 prägnante, aber inhaltlich substanzielle Kernpunkte (nicht nur drei Sätze).

## Detaillierte Durcharbeitung
Hauptteil — hier die meiste Länge und Tiefe.
- **Normaler Modus:** Wie oben beschrieben: bei Klausur/Prüfung jede Teilaufgabe nacheinander; sonst das Material thematisch vollständig und tiefgehend durchgehen.
- **Konzepte-lernen-Modus:** Statt Einzelaufgaben „abzuschreiben“, pro Aufgabenblock/Thema die zugrundeliegenden Ideen, Standardalgorithmen und Beweis-/Rechenmuster sehr ausführlich erklären — aber so, dass alles Material abgedeckt ist.

## Active Recall
Mindestens 15 anspruchsvolle Fragen. Zu jeder Frage direkt darunter eine ausführliche Musterantwort (mit Rechenweg/LaTeX wo nötig).

## Häufige Stolpersteine
Konkrete typische Fehler, Missverständnisse und wie man sie vermeidet (ausführlich, nicht nur drei Bulletpoints).

## Lösungsstrategien & Formeln
Wo relevant (Mathe/NW): Sätze/Formeln und wann man welche Strategie wählt — als nachschlagbare Zusammenfassung zum obigen Hauptteil.

## Fazit & Themenzusammenhänge
Kurze Einordnung, wie die Teile zusammenhängen; optional Prüfungstipps.

Schreibe durchgehend verständlich, mit Absätzen und klarer Markdown-Gliederung (Zwischenüberschriften pro Aufgabe o. Ä.).

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

    @staticmethod
    def chat(content: List[Any], user_message: str, history: List[Dict[str, str]], model_preference: str = None, active_file: dict = None) -> str:
        """Handles a conversational chat with the AI, given the folder content context and optionally the currently active file."""
        try:
            system_instruction = (
                "Du bist der hilfreiche 'Blop KI-Assistent'. Deine Aufgabe ist es, Fragen des Schülers "
                "zu seinen Lernmaterialien (PDFs, Notizen, etc.) zu beantworten.\n"
                "Sei immer freundlich, ermutigend und hilfsbereit. Antworte in der Sprache des Nutzers (standardmäßig Deutsch).\n"
                "Fasse dich eher kurz und präzise, um den Chatverlauf lesbar zu halten.\n\n"
                "WICHTIGE FORMATIERUNGSREGEL FÜR MATHEMATIK:\n"
                "Verwende IMMER die LaTeX-Notation für mathematische Formeln und Ausdrücke. \n"
                "- Für Inline-Formeln verwende ein einzelnes Dollarzeichen: $E = mc^2$ \n"
                "- Für Block-Formeln verwende doppelte Dollarzeichen: $$E = mc^2$$ \n"
                "Verwende niemals andere Notationen für Mathematik.\n\n"
                + MATH_ACCURACY_INSTRUCTIONS
                + "\n"
            )

            if active_file:
                # Add context about the currently open file and instructions for editing
                file_name = active_file.get("name", "Unbekannt")
                file_type = active_file.get("type", "Unbekannt")
                file_content = active_file.get("content", "")
                
                # Make sure file content is a string if it's a JSON structure (like plans/flashcards)
                if isinstance(file_content, (dict, list)):
                    file_content_str = json.dumps(file_content, ensure_ascii=False, indent=2)
                else:
                    file_content_str = str(file_content)

                system_instruction += (
                    f"WICHTIGE OPERATION (DOKUMENTEN-BEARBEITUNG):\n"
                    f"Der Nutzer hat aktuell folgende Datei geöffnet:\n"
                    f"- Name: {file_name}\n"
                    f"- Typ: {file_type}\n"
                    f"- Inhalt:\n{file_content_str}\n\n"
                    f"ANWEISUNG ZUM BEARBEITEN DER DATEI:\n"
                    f"Wenn der Nutzer dich bittet, diese geöffnete Datei zu ändern (z.B. 'Kürze die Zusammenfassung', 'Füge X zum Plan hinzu', 'Übersetze es'), "
                    f"dann MUSST DU die GANZ genaue und komplette NEUE Version dieser Datei generieren.\n"
                    f"Wenn du das Dokument bearbeitest, antworte AUSSCHLIEßLICH mit einem JSON-Block im folgenden Format (KEIN anderer Text!):\n"
                    f"```json\n"
                    f"{{\n"
                    f"  \"blop_action\": \"update_file\",\n"
                    f"  \"new_content\": <HIER DER KOMPLETTE NEUE INHALT_STR ODER DAS JSON ARRAY WIE VORHER>\n"
                    f"}}\n"
                    f"```\n"
                    f"Beachte: Wenn die Originaldatei ein JSON Array war (wie bei Lernplänen, Karteikarten oder Quizzes), muss `new_content` wieder exakt so ein Array sein.\n"
                    f"Wenn es nur normaler Chat ist (keine Änderung gefordert), antworte ganz normal als Text."
                )

            # Initialize model with the system instruction
            model = genai.GenerativeModel(
                model_name=get_best_model(model_preference),
                system_instruction=system_instruction
            )

            # The context must be injected. Since we are stateless, we inject it into the VERY FIRST user message.
            context_parts = ["--- LERNMATERIAL KONTEXT ---\nBitte nutze dieses Material, um Anfragen darauf basierend zu beantworten.\n"]
            if isinstance(content, list):
                context_parts.extend(content)
            elif content:
                context_parts.append(content)
                
            formatted_history = []
            if history:
                 first_user_msg = True
                 for msg in history:
                     if not msg.get("content"):
                         continue
                     role = "user" if msg.get("role") == "user" else "model"
                     
                     parts = [msg.get("content")]
                     if role == "user" and first_user_msg:
                         # Inject the folder materials into the very first message of the conversation
                         parts = context_parts + parts
                         first_user_msg = False
                         
                     formatted_history.append({"role": role, "parts": parts})
                     
                 # Start the chat with reconstructed history
                 chat_session = model.start_chat(history=formatted_history)
                 # Send the new incoming message
                 response = chat_session.send_message(user_message, safety_settings=SAFETY_SETTINGS)
            else:
                 # No history, this is the very first message. Inject context directly into the new message.
                 message_parts = context_parts + [user_message]
                 chat_session = model.start_chat(history=[])
                 response = chat_session.send_message(message_parts, safety_settings=SAFETY_SETTINGS)
            
            if not response.text:
                raise Exception("Keine Antwort vom Modell erhalten.")
                
            return response.text
            
        except Exception as e:
            import traceback
            traceback.print_exc()
            print(f"Chat Error: {e}")
            raise Exception(f"Fehler im Chatbot: {str(e)}")

