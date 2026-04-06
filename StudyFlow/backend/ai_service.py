import os
from genai_warnings import suppress_known_google_warnings

suppress_known_google_warnings()
import google.generativeai as genai
import json
from typing import List, Dict, Any, Optional, Tuple

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
    _REPETITION_CONTINUATION_PROMPT = """Die Ausgabe wurde wegen des Ausgabe-Tokenslimits abgebrochen.

Setze NAHTLOS im selben Dokument fort (keine Begrüßung; keine Wiederholung von „## Kurzüberblick“ oder bereits vollständig ausgearbeiteten Aufgaben).

Als Nächstes ausdrücklich den REST bearbeiten: fast immer fehlen dann noch Aufgaben, Teilaufgaben oder Projektphasen am **Ende** des Quellen-PDFs — diese jetzt vollständig und ausführlich (Konzepte/Lösungswege wie zuvor).

Wenn Hauptteil und Aufgaben komplett sind, aber die Struktur noch fehlt: ergänze nur die noch fehlenden Abschnitte „## Active Recall“, „## Häufige Stolpersteine“, „## Lösungsstrategien & Formeln“, „## Fazit & Themenzusammenhänge“ — jeweils nur soweit nach deiner bisherigen Ausgabe noch nicht vorhanden."""
    _ELABORATION_CONTINUATION_PROMPT = """Die Ausarbeitung wurde wegen Ausgabe-Tokenlimit abgeschnitten.

Setze NAHTLOS fort:
- keine neue Einleitung, keine Wiederholung bereits fertiger Abschnitte
- zuerst fehlende Themen/Teilaufgaben/Projektphasen am Dokumentende ergänzen
- danach ggf. noch fehlende Abschnitte (Praxisbeispiele, Transfer, Fazit) ergänzen
- bleibe in derselben Struktur und im gleichen Stil"""

    @staticmethod
    def _generate_repetition_with_continuation(model, input_parts: List[Any], max_followups: int = 6) -> str:
        """Chat-based generation so we can continue after MAX_TOKENS (long repetitions)."""
        chat = model.start_chat(history=[])
        response = chat.send_message(input_parts, safety_settings=SAFETY_SETTINGS)
        if not response.candidates:
            raise Exception("Leere Kandidaten-Antwort vom Modell.")
        chunks: List[str] = []
        if response.text:
            chunks.append(response.text)
        fr = response.candidates[0].finish_reason
        follow = 0
        while fr == genai.protos.Candidate.FinishReason.MAX_TOKENS and follow < max_followups:
            follow += 1
            print(f"Repetition: finish_reason=MAX_TOKENS, continuation {follow}/{max_followups}")
            response = chat.send_message(
                AIService._REPETITION_CONTINUATION_PROMPT,
                safety_settings=SAFETY_SETTINGS,
            )
            if not response.candidates:
                break
            if response.text:
                chunks.append(response.text)
            fr = response.candidates[0].finish_reason
        return "\n\n".join(chunks) if chunks else ""

    @staticmethod
    def _generate_with_continuation(
        model,
        input_parts: List[Any],
        continuation_prompt: str,
        max_followups: int = 4,
        log_prefix: str = "Generation",
        return_meta: bool = False,
    ) -> Any:
        """Generic chat-based generation that continues when MAX_TOKENS is hit."""
        def _usage_to_dict(usage_obj: Any) -> Dict[str, int]:
            if not usage_obj:
                return {"prompt_tokens": 0, "output_tokens": 0, "total_tokens": 0}
            return {
                "prompt_tokens": int(getattr(usage_obj, "prompt_token_count", 0) or 0),
                "output_tokens": int(getattr(usage_obj, "candidates_token_count", 0) or 0),
                "total_tokens": int(getattr(usage_obj, "total_token_count", 0) or 0),
            }

        usage_sum = {"prompt_tokens": 0, "output_tokens": 0, "total_tokens": 0}
        chat = model.start_chat(history=[])
        response = chat.send_message(input_parts, safety_settings=SAFETY_SETTINGS)
        if not response.candidates:
            raise Exception("Leere Kandidaten-Antwort vom Modell.")
        chunks: List[str] = []
        if response.text:
            chunks.append(response.text)
        first_usage = _usage_to_dict(getattr(response, "usage_metadata", None))
        usage_sum["prompt_tokens"] += first_usage["prompt_tokens"]
        usage_sum["output_tokens"] += first_usage["output_tokens"]
        usage_sum["total_tokens"] += first_usage["total_tokens"]
        finish_reason = response.candidates[0].finish_reason
        follow = 0
        while finish_reason == genai.protos.Candidate.FinishReason.MAX_TOKENS and follow < max_followups:
            follow += 1
            print(f"{log_prefix}: finish_reason=MAX_TOKENS, continuation {follow}/{max_followups}")
            response = chat.send_message(continuation_prompt, safety_settings=SAFETY_SETTINGS)
            if not response.candidates:
                break
            if response.text:
                chunks.append(response.text)
            step_usage = _usage_to_dict(getattr(response, "usage_metadata", None))
            usage_sum["prompt_tokens"] += step_usage["prompt_tokens"]
            usage_sum["output_tokens"] += step_usage["output_tokens"]
            usage_sum["total_tokens"] += step_usage["total_tokens"]
            finish_reason = response.candidates[0].finish_reason
        text = "\n\n".join(chunks).strip()
        if not return_meta:
            return text
        return {
            "text": text,
            "usage": usage_sum,
            "used_model": str(getattr(model, "model_name", "") or ""),
        }

    @staticmethod
    def _extract_usage(response: Any) -> Dict[str, int]:
        usage_obj = getattr(response, "usage_metadata", None)
        return {
            "prompt_tokens": int(getattr(usage_obj, "prompt_token_count", 0) or 0),
            "output_tokens": int(getattr(usage_obj, "candidates_token_count", 0) or 0),
            "total_tokens": int(getattr(usage_obj, "total_token_count", 0) or 0),
        }

    @staticmethod
    def _pick_available_model(preferred: Optional[str] = None, fast_only: bool = False) -> str:
        """Return a model that exists for this API key; fallback safely."""
        if preferred:
            try:
                available = []
                for m in genai.list_models():
                    name = m.name.replace("models/", "")
                    if hasattr(m, "supported_generation_methods") and "generateContent" in m.supported_generation_methods:
                        available.append(name)
                if preferred in available:
                    return preferred
                print(f"Preferred model '{preferred}' not available. Available: {available}")
            except Exception as e:
                print(f"Could not validate preferred model '{preferred}': {e}")

        if fast_only:
            for candidate in ("gemini-2.0-flash", "gemini-2.0-flash-lite", "gemini-1.5-flash", "gemini-1.5-flash-8b"):
                picked = get_best_model(candidate)
                if picked == candidate:
                    return candidate
        return get_best_model(None)

    @staticmethod
    def _pick_strong_model(preferred: Optional[str] = None) -> str:
        """Prefer high-quality models first, then fallback to fast models."""
        if preferred:
            picked = AIService._pick_available_model(preferred=preferred, fast_only=False)
            if picked == preferred:
                return picked
        strong_candidates = (
            "gemini-2.5-pro",
            "gemini-2.0-pro-exp",
            "gemini-1.5-pro",
        )
        for candidate in strong_candidates:
            picked = AIService._pick_available_model(preferred=candidate, fast_only=False)
            if picked == candidate:
                return candidate
        return AIService._pick_available_model(preferred=None, fast_only=True)

    @staticmethod
    def generate_summary(content: List[Any], detail_level: str = "Normal", model_preference: str = None, learning_mode: str = "normal", return_meta: bool = False) -> Any:
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
            if not return_meta:
                return response.text
            return {
                "text": response.text,
                "usage": AIService._extract_usage(response),
                "used_model": str(getattr(model, "model_name", "") or ""),
            }
        except Exception as e:
            print(f"Summary Error: {e}")
            raise Exception(f"Fehler bei der Zusammenfassung: {str(e)}")

    @staticmethod
    def generate_quiz(
        content: List[Any],
        model_preference: str = None,
        question_count: int = 10,
        difficulty: str = "gemischt",
        return_meta: bool = False
    ) -> Any:
        """Generates a comprehensive quiz from multimodal content."""
        try:
            model = genai.GenerativeModel(get_best_model(model_preference), generation_config={"response_mime_type": "application/json"})
            prompt = f"""
Du bist ein erfahrener Lehrer. Erstelle ein anspruchsvolles Quiz mit {question_count} Fragen basierend auf dem Lernmaterial.

WICHTIGE FORMATIERUNGSREGEL FÜR MATHEMATIK:
Verwende IMMER die LaTeX-Notation für mathematische Formeln und Ausdrücke in den Fragen und Antworten. 
- Für Inline-Formeln (im Textfluss) verwende ein einzelnes Dollarzeichen: $E = mc^2$
- Für Block-Formeln (eigene Zeile) verwende doppelte Dollarzeichen: $$E = mc^2$$

""" + MATH_ACCURACY_INSTRUCTIONS + """
- Die als richtig markierte Antwort und die Erklärung müssen rechnerisch stimmig sein; falsche Zahlen in Erklärungen sind inakzeptabel.

Anforderungen:
- Decke ALLE wichtigen Themen des Materials ab
- Schwierigkeitsprofil: {difficulty}
- Die Antwortoptionen sollen plausibel aber unterschiedlich sein
- Achte auf klare, eindeutige Fragestellungen
- Formuliere auf Deutsch

Ausgabe-Format: JSON Array mit genau {question_count} Objekten:
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
            parsed = json.loads(response.text)
            if not return_meta:
                return parsed
            return {
                "data": parsed,
                "usage": AIService._extract_usage(response),
                "used_model": str(getattr(model, "model_name", "") or ""),
            }
        except Exception as e:
            print(f"Quiz Error: {e}")
            raise Exception(f"Fehler beim Quiz-Generieren: {str(e)}")

    @staticmethod
    def generate_flashcards(
        content: List[Any],
        model_preference: str = None,
        cards_count: int = 20,
        max_text_per_card: int = 320,
        return_meta: bool = False
    ) -> Any:
        """Generates comprehensive flashcards from multimodal content."""
        try:
            model = genai.GenerativeModel(get_best_model(model_preference), generation_config={"response_mime_type": "application/json"})
            prompt = f"""
Du bist ein erfahrener Tutor. Erstelle {cards_count} hochwertige Karteikarten aus dem Lernmaterial.

WICHTIGE FORMATIERUNGSREGEL FÜR MATHEMATIK:
Verwende IMMER die LaTeX-Notation für mathematische Formeln und Ausdrücke. 
- Für Inline-Formeln (im Textfluss) verwende ein einzelnes Dollarzeichen: $E = mc^2$
- Für Block-Formeln (eigene Zeile) verwende doppelte Dollarzeichen: $$E = mc^2$$

""" + MATH_ACCURACY_INSTRUCTIONS + """
- Auf der Rückseite bei Rechenaufgaben: nur Ergebnisse mit nachvollziehbarer Kette und Probe angeben.

Anforderungen:
- Decke ALLE wichtigen Konzepte, Begriffe und Fakten ab
- Vorderseite: präzise Frage oder Begriff
- Rückseite: ausführliche, vollständige Antwort/Erklärung (maximal ca. {max_text_per_card} Zeichen)
- Variiere zwischen Definitionsfragen, Erklärungsfragen und Anwendungsfragen
- Deutsch

Format: JSON Array mit genau {cards_count} Objekten:
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
            parsed = json.loads(response.text)
            if not return_meta:
                return parsed
            return {
                "data": parsed,
                "usage": AIService._extract_usage(response),
                "used_model": str(getattr(model, "model_name", "") or ""),
            }
        except Exception as e:
            print(f"Flashcard Error: {e}")
            raise Exception(f"Fehler beim Karteikarten-Generieren: {str(e)}")

    @staticmethod
    def generate_study_plan(content: List[Any], duration_days: int, hours_per_day: float = 2.0, model_preference: str = None, active_days: list[int] = None, learning_mode: str = "normal", return_meta: bool = False) -> Any:
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
            if not return_meta:
                return result
            return {
                "data": result,
                "usage": AIService._extract_usage(response),
                "used_model": str(getattr(model, "model_name", "") or ""),
            }
        except Exception as e:
            print(f"Plan Error: {e}")
            raise Exception(f"Fehler beim Lernplan-Generieren: {str(e)}")

    @staticmethod
    def _validate_refined_quiz(obj: Any) -> None:
        if not isinstance(obj, list) or len(obj) == 0:
            raise ValueError("Quiz muss ein nicht-leeres JSON-Array sein.")
        for i, row in enumerate(obj):
            if not isinstance(row, dict):
                raise ValueError(f"Quiz-Eintrag {i} ist kein Objekt.")
            for k in ("question", "options", "answer", "explanation"):
                if k not in row:
                    raise ValueError(f"Quiz-Eintrag {i} fehlt Feld '{k}'.")
            if not isinstance(row.get("options"), list):
                raise ValueError(f"Quiz-Eintrag {i}: options muss eine Liste sein.")

    @staticmethod
    def _validate_refined_flashcards(obj: Any) -> None:
        if not isinstance(obj, list) or len(obj) == 0:
            raise ValueError("Karteikarten müssen ein nicht-leeres JSON-Array sein.")
        for i, row in enumerate(obj):
            if not isinstance(row, dict):
                raise ValueError(f"Karteikarte {i} ist kein Objekt.")
            if "front" not in row or "back" not in row:
                raise ValueError(f"Karteikarte {i} braucht front und back.")

    @staticmethod
    def _validate_refined_plan(obj: Any) -> None:
        if not isinstance(obj, list) or len(obj) == 0:
            raise ValueError("Lernplan muss ein nicht-leeres JSON-Array sein.")
        for i, day in enumerate(obj):
            if not isinstance(day, dict):
                raise ValueError(f"Lernplan-Tag {i} ist kein Objekt.")
            for k in ("day", "topic", "goal", "summary", "tasks", "focus", "tip"):
                if k not in day:
                    raise ValueError(f"Lernplan-Tag {i} fehlt Feld '{k}'.")
            if not isinstance(day.get("tasks"), list):
                raise ValueError(f"Lernplan-Tag {i}: tasks muss eine Liste sein.")

    @staticmethod
    def refine_json_artifact(
        artifact_type: str,
        base_content: Any,
        user_prompt: str,
        model_preference: str = None,
        return_meta: bool = False,
    ) -> Any:
        """Refine quiz / flashcards / plan JSON using the same shapes as generate_*."""
        try:
            if isinstance(base_content, str):
                try:
                    data = json.loads(base_content)
                except json.JSONDecodeError as e:
                    raise Exception(f"Ungültiger JSON-Inhalt: {e}") from e
            else:
                data = base_content

            at = (artifact_type or "").lower()
            if at == "quiz":
                schema = """
Ausgabe: JSON-Array von Objekten (gleiche Struktur wie beim Erzeugen):
[
  {
    "question": "...",
    "options": ["A","B","C","D"],
    "answer": "exakte Option",
    "explanation": "..."
  }
]
"""
                AIService._validate_refined_quiz(data)
            elif at == "flashcards":
                schema = """
Ausgabe: JSON-Array von Objekten:
[
  { "front": "...", "back": "..." }
]
"""
                AIService._validate_refined_flashcards(data)
            elif at == "plan":
                schema = """
Ausgabe: JSON-Array von Tag-Objekten (gleiche Struktur wie generate_study_plan):
[
  {
    "day": 1,
    "topic": "...",
    "goal": "...",
    "summary": "...",
    "tasks": [ { "description": "...", "completed": false } ],
    "focus": "...",
    "tip": "..."
  }
]
"""
                AIService._validate_refined_plan(data)
            else:
                raise Exception(f"Unbekannter Artefakt-Typ für JSON-Anpassung: {artifact_type}")

            existing = json.dumps(data, ensure_ascii=False)
            model = genai.GenerativeModel(
                get_best_model(model_preference),
                generation_config={"response_mime_type": "application/json"},
            )
            prompt = f"""Du passt ein bestehendes Lern-Artefakt (Typ: {at}) an.

Nutzerwunsch (konkret umsetzen):
{user_prompt.strip()}

Regeln:
- Antworte NUR mit gültigem JSON (kein Markdown, kein Kommentar).
- Behalte die gleiche JSON-Struktur wie unten; Inhalte dürfen sich stark ändern.
- Sprache: Deutsch.
- Mathematik: LaTeX wie gewohnt ($inline$, $$block$$).

Bestehendes JSON:
{existing}

{schema}
"""
            response = model.generate_content(prompt, safety_settings=SAFETY_SETTINGS)
            if not response.text:
                raise Exception("Leere Antwort vom Modell erhalten.")
            parsed = json.loads(response.text)
            if at == "quiz":
                AIService._validate_refined_quiz(parsed)
            elif at == "flashcards":
                AIService._validate_refined_flashcards(parsed)
            elif at == "plan":
                AIService._validate_refined_plan(parsed)

            if not return_meta:
                return parsed
            return {
                "data": parsed,
                "usage": AIService._extract_usage(response),
                "used_model": str(getattr(model, "model_name", "") or ""),
            }
        except Exception as e:
            print(f"Refine JSON artifact error ({artifact_type}): {e}")
            raise Exception(f"JSON-Anpassung fehlgeschlagen: {str(e)}")

    @staticmethod
    def generate_task_help(content: List[Any], task_description: str, model_preference: str = None, return_meta: bool = False) -> Any:
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
                
            if not return_meta:
                return response.text
            return {
                "text": response.text,
                "usage": AIService._extract_usage(response),
                "used_model": str(getattr(model, "model_name", "") or ""),
            }
        except Exception as e:
            print(f"Task Help Error: {e}")
            raise Exception(f"Fehler bei der Aufgaben-Hilfe: {str(e)}")

    @staticmethod
    def generate_podcast_script(content: List[Any], model_preference: str = None, return_meta: bool = False) -> Any:
        """Plain German narration text for TTS (no markdown headings)."""
        try:
            model = genai.GenerativeModel(
                get_best_model(model_preference),
                generation_config={"temperature": 0.45, "max_output_tokens": 8192},
            )
            prompt = """
Du bist Redakteur für einen Lern-Podcast auf Deutsch.
Erstelle aus dem folgenden Material einen zusammenhängenden, gut verständlichen **reinen Vorlesetext** (ein Sprecher / Monolog) für die Sprachausgabe.

Regeln:
- Keine Sprecherlabels wie „Moderator:“ oder „Host:“.
- Keine Markdown-Überschriften mit #; höchstens einfache Absätze.
- Struktur: kurze Einleitung, dann thematische Abschnitte in Fließtext.
- Länge: grob 800 bis 3500 Wörter — kürzer wenn das Material dünn ist, länger wenn sehr viel Stoff da ist.
- Fokus: Kernkonzepte, Definitionen, Zusammenhänge; keine Meta-Kommentare („im Folgenden…“ sparsam).

Antworte NUR mit dem Vorlesetext, ohne Titelzeile oder Einleitungssatz der Art „Hier ist der Podcast“.
"""
            input_parts = [prompt]
            if isinstance(content, list):
                input_parts.extend(content)
            else:
                input_parts.append(content)
            response = model.generate_content(input_parts, safety_settings=SAFETY_SETTINGS)
            if not response.text:
                raise Exception("Leere Antwort vom Modell erhalten.")
            text = response.text.strip()
            if not return_meta:
                return text
            return {
                "text": text,
                "usage": AIService._extract_usage(response),
                "used_model": str(getattr(model, "model_name", "") or ""),
            }
        except Exception as e:
            print(f"Podcast script error: {e}")
            raise Exception(f"Podcast-Skript fehlgeschlagen: {str(e)}")

    @staticmethod
    def _merge_usage_dict(a: Dict[str, int], b: Dict[str, int]) -> Dict[str, int]:
        return {
            "prompt_tokens": int(a.get("prompt_tokens", 0)) + int(b.get("prompt_tokens", 0)),
            "output_tokens": int(a.get("output_tokens", 0)) + int(b.get("output_tokens", 0)),
            "total_tokens": int(a.get("total_tokens", 0)) + int(b.get("total_tokens", 0)),
        }

    @staticmethod
    def _parse_json_storyboard(raw: str) -> dict:
        """Parse model JSON; repair common syntax issues (quotes, truncation artifacts)."""
        raw = (raw or "").strip()
        if not raw:
            raise ValueError(
                "Leere Modellantwort für das Storyboard. Bitte erneut versuchen oder weniger Szenen wählen."
            )
        try:
            data = json.loads(raw)
        except json.JSONDecodeError:
            start = raw.find("{")
            end = raw.rfind("}") + 1
            slice_ = raw[start:end] if start >= 0 and end > start else raw
            try:
                data = json.loads(slice_)
            except json.JSONDecodeError:
                try:
                    from json_repair import repair_json

                    fixed = repair_json(slice_)
                    data = json.loads(fixed)
                except Exception as inner:
                    raise ValueError(
                        "Das Storyboard-JSON ist ungültig oder wurde abgeschnitten. "
                        "Bitte weniger Szenen oder eine kürzere Erzähltiefe wählen und erneut versuchen. "
                        f"Technischer Hinweis: {str(inner)}"
                    ) from inner
        if not isinstance(data, dict) or not isinstance(data.get("scenes"), list):
            raise ValueError("Ungültiges Storyboard-Format (title/scenes).")
        return data

    @staticmethod
    def _generate_learning_video_storyboard_raw(
        model: Any,
        input_parts: List[Any],
        max_followups: int = 4,
    ) -> Tuple[str, Dict[str, int], Any]:
        """Chat-based generation; join chunks with '' so JSON stays one document when MAX_TOKENS hits."""
        chat = model.start_chat(history=[])
        response = chat.send_message(input_parts, safety_settings=SAFETY_SETTINGS)
        if not response.candidates:
            raise Exception("Leere Kandidaten-Antwort vom Modell.")
        chunks: List[str] = []
        if response.text:
            chunks.append(response.text)
        usage_sum = AIService._extract_usage(response)
        last_response = response
        fr = response.candidates[0].finish_reason
        follow = 0
        cont = (
            "Das JSON wurde am Ende abgeschnitten. Vervollständige NUR den fehlenden Rest des JSON-Objekts, "
            "exakt an der Abbruchstelle. Keine Wiederholung des bereits Ausgegebenen, kein Markdown, "
            "keine Erklärung — nur gültiges JSON-Fragment, das sich nahtlos an den bisherigen Text anschließt."
        )
        while fr == genai.protos.Candidate.FinishReason.MAX_TOKENS and follow < max_followups:
            follow += 1
            print(f"Learning video storyboard: finish_reason=MAX_TOKENS, continuation {follow}/{max_followups}")
            response = chat.send_message(cont, safety_settings=SAFETY_SETTINGS)
            last_response = response
            if not response.candidates:
                break
            if response.text:
                chunks.append(response.text)
            usage_sum = AIService._merge_usage_dict(usage_sum, AIService._extract_usage(response))
            fr = response.candidates[0].finish_reason
        raw = "".join(chunks).strip()
        if not raw:
            raise Exception("Kein Storyboard-Text in der Modellantwort.")
        return raw, usage_sum, last_response

    @staticmethod
    def generate_learning_video_storyboard(
        content: List[Any],
        model_preference: str = None,
        return_meta: bool = False,
        storyboard_options: Optional[dict] = None,
    ) -> Any:
        """JSON storyboard with scenes for slideshow + narration."""
        try:
            opts = storyboard_options or {}
            lv_max = 22
            target = int(opts.get("target_scenes") or 6)
            target = max(4, min(lv_max, target))
            lo = max(4, target - 1)
            hi = min(lv_max, target + 1)
            depth = (opts.get("narration_depth") or "standard").strip().lower()
            if depth not in ("compact", "standard", "detailed", "deep"):
                depth = "standard"
            want_images = bool(opts.get("include_image_queries"))
            material_note = (opts.get("material_summary") or "").strip()

            if depth == "compact":
                narr_hint = "narration: 2-3 kurze Sätze auf Deutsch, klar und didaktisch"
                body_max = "max 400 Zeichen"
            elif depth == "deep":
                narr_hint = (
                    "narration: 12-18 ausführliche Sätze auf Deutsch, didaktisch, mit Definitionen, "
                    "Beispielen und Zusammenhängen; nicht nur Stichpunkte vorlesen"
                )
                body_max = "max 950 Zeichen"
            elif depth == "detailed":
                narr_hint = "narration: 8-14 ausführliche Sätze auf Deutsch, klar und didaktisch, mit Beispielen wo sinnvoll"
                body_max = "max 800 Zeichen"
            else:
                narr_hint = "narration: 4-8 Sätze auf Deutsch, klar und didaktisch"
                body_max = "max 600 Zeichen"

            scene_fields = '''      "title": "Kurzer Szenentitel",
      "body": "Stichpunkte oder Fließtext auf Deutsch (%s), für Folien",
      "narration": "%s"''' % (body_max, narr_hint.replace('"', "'"))
            if want_images:
                scene_fields += ',\n      "image_query": "2-4 English keywords for a stock photo (no people faces if possible), e.g. ancient rome map"'

            schema_hint = """
Erzeuge EIN JSON-Objekt (kein Markdown) mit diesem Schema:
{
  "title": "Kurztitel des Videos",
  "opening_narration": "Genau 3-6 vollständige Sätze auf Deutsch: kurze Begrüßung, worum es geht, wie das Video aufgebaut ist (Roadmap). Wird als Erstes vorgelesen.",
  "scenes": [
    {
%s
    }
  ]
}
Erzeuge zwischen %d und %d Szenen (Ziel: etwa %d). Jede Szene deckt einen inhaltlichen Abschnitt des Materials ab.
Die gesprochene Gesamtfassung (opening_narration plus alle narration-Felder) soll zusammenhängend wirken.
""" % (
                scene_fields,
                lo,
                hi,
                target,
            )
            if material_note:
                schema_hint += "\nHinweis zum Umfang des Materials:\n" + material_note + "\n"
            vis = (opts.get("visual_style") or "").strip().lower()
            if vis == "whiteboard":
                schema_hint += (
                    "\nVisueller Stil Whiteboard: body als kurze Liste — ein Hauptgedanke pro Zeile (Strich oder "
                    "Nummer), Zeilen etwa 60–100 Zeichen; lieber mehr Zeilen als ein langer Absatz. "
                    "Struktur wie am Brett (z. B. Begriff — Kurzerklärung; Schritt 1 / Schritt 2).\n"
                )
            schema_hint += (
                "\nWichtig für gültiges JSON: In allen Textfeldern (title, opening_narration, body, narration) "
                "darf im Fließtext kein rohes doppeltes Anführungszeichen (ASCII 34) vorkommen — nutze typografische "
                "‚…‘ oder Umformulierung ohne Zitate, damit die JSON-Strings syntaktisch gültig bleiben.\n"
            )

            model = genai.GenerativeModel(
                get_best_model(model_preference),
                generation_config={
                    "response_mime_type": "application/json",
                    "temperature": 0.4,
                    "max_output_tokens": 16384,
                },
            )
            input_parts = [schema_hint + "\n\nAnalysiere das Material und fülle das JSON sinnvoll."]
            if isinstance(content, list):
                input_parts.extend(content)
            else:
                input_parts.append(content)
            raw, usage_sum, _last_resp = AIService._generate_learning_video_storyboard_raw(
                model, input_parts, max_followups=4
            )
            try:
                data = AIService._parse_json_storyboard(raw)
            except ValueError as ve:
                raise Exception(str(ve)) from ve
            if not return_meta:
                return data
            return {
                "data": data,
                "usage": usage_sum,
                "used_model": str(getattr(model, "model_name", "") or ""),
            }
        except Exception as e:
            print(f"Learning video storyboard error: {e}")
            raise Exception(f"Lernvideo-Storyboard fehlgeschlagen: {str(e)}")

    @staticmethod
    def transcribe_audio(audio_file_path: str, model_preference: str = None, return_meta: bool = False) -> Any:
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
            if not return_meta:
                return result
            return {
                "data": result,
                "usage": AIService._extract_usage(response),
                "used_model": str(getattr(model, "model_name", "") or ""),
            }
        except Exception as e:
            print(f"Audio Transcription Error: {e}")
            raise Exception(f"Fehler bei der Audio-Transkription: {str(e)}")

    @staticmethod
    def generate_elaboration(content: List[Any], detail_level: str = "Normal", custom_rules: str = "", model_preference: str = None, return_meta: bool = False) -> Any:
        """Generates a detailed elaboration/essay based on the material and user instructions."""
        try:
            elaboration_generation_config = {
                "max_output_tokens": 16384,
                "temperature": 0.4,
            }
            model = genai.GenerativeModel(
                get_best_model(model_preference),
                generation_config=elaboration_generation_config,
            )
            detail_hint = {
                "Kurz": "Kompakt, aber immer noch substanziell.",
                "Normal": "Ausführlich und tiefgehend.",
                "Detailliert": "Sehr detailliert mit vielen Erklärungen, Beispielen und Transfer.",
                "Sehr detailliert": "Maximal ausführlich, mehrseitig und didaktisch aufgebaut.",
            }.get(detail_level, f"Vom Nutzer gewünscht: {detail_level}. Interpretiere dies als hohe Detailtiefe.")

            prompt = f"""
Du bist ein akademischer Autor und Tutor. Erstelle eine LANGFORM-AUSARBEITUNG (Essay/Hausarbeit-Entwurf) auf Deutsch, die als vollständiges Lern- und Arbeitsdokument taugt.

Detailgrad-Vorgabe: {detail_level}
Interpretation: {detail_hint}

{"Spezifische Anweisungen des Nutzers:" if custom_rules else "Generelle Anweisung:"}
{custom_rules if custom_rules else "Fasse die wichtigsten Aspekte des Materials in einem gut strukturierten Text zusammen."}

VOLLSTÄNDIGKEIT (kritisch):
- Arbeite das Material vollständig durch, nicht nur den Anfang.
- Typischer Fehler: letzte Aufgaben/Projektteile werden ausgelassen. Das ist hier NICHT erlaubt.
- Decke ausdrücklich auch Schlussabschnitte, hohe Aufgabennummern und Anhänge ab, sofern relevant.

UMFANG (verbindlich):
- Schreibe eine lange, substanzielle Ausarbeitung (mindestens Umfang mehrerer DIN-A4-Seiten, wenn das Material groß ist).
- Hauptteil soll den größten Anteil haben und in mehrere Unterkapitel gegliedert sein.

WICHTIGE FORMATIERUNGSREGEL FÜR MATHEMATIK:
Verwende IMMER die LaTeX-Notation für mathematische Formeln und Ausdrücke. 
- Für Inline-Formeln (im Textfluss) verwende ein einzelnes Dollarzeichen: $E = mc^2$
- Für Block-Formeln (eigene Zeile) verwende doppelte Dollarzeichen: $$E = mc^2$$

{MATH_ACCURACY_INSTRUCTIONS}

Erforderliche Struktur (Markdown):
## Einleitung
- Kontext, Problemstellung, Ziel der Ausarbeitung.

## Systematische Durcharbeitung
- Pro Thema/Aufgabenblock eigener Unterabschnitt.
- Bei aufgabenlastigem Material: Aufgabenlogik, Lösungsstrategie, Begründungen, typische Fehler.
- Bei Theorie-Material: Definitionen, Zusammenhänge, Argumentationslinien, Beispiele.

## Vertiefung & Transfer
- Vertiefte Beispiele, alternative Lösungsansätze, Transfer auf ähnliche Aufgaben.

## Fazit
- Kernergebnisse, Zusammenhänge, Lern-/Prüfungshinweise.

Schreibe im Fließtext auf Deutsch, verwende Absätze zur besseren Lesbarkeit und formatiere Zwischenüberschriften mit Markdown.

Hier ist das Quellenmaterial:
"""

            input_parts = [prompt]
            if isinstance(content, list):
                input_parts.extend(content)
            else:
                input_parts.append(content)
            try:
                full_result = AIService._generate_with_continuation(
                    model=model,
                    input_parts=input_parts,
                    continuation_prompt=AIService._ELABORATION_CONTINUATION_PROMPT,
                    max_followups=6,
                    log_prefix="Elaboration",
                    return_meta=return_meta,
                )
            except Exception as mt_err:
                print(f"Elaboration multiturn failed ({mt_err}), falling back to single-shot generate_content")
                response = model.generate_content(input_parts, safety_settings=SAFETY_SETTINGS)
                if not response.text:
                    raise Exception("Leere Antwort vom Modell erhalten.")
                if not return_meta:
                    full_text = response.text
                    full_result = full_text
                else:
                    full_result = {
                        "text": response.text,
                        "usage": AIService._extract_usage(response),
                        "used_model": str(getattr(model, "model_name", "") or ""),
                    }
            full_text = full_result["text"] if return_meta else full_result
            if not full_text:
                raise Exception("Leere Antwort vom Modell erhalten.")
            return full_result
        except Exception as e:
            print(f"Elaboration Error: {e}")
            raise Exception(f"Fehler bei der Ausarbeitung: {str(e)}")

    @staticmethod
    def refine_document_with_chat(
        *,
        document_content: str,
        user_message: str,
        history: List[Dict[str, str]],
        model_preference: str = None,
        return_meta: bool = False,
    ) -> Dict[str, Any]:
        """Return a patch proposal for interactive document editing."""
        try:
            selected_model = AIService._pick_strong_model(model_preference)
            model = genai.GenerativeModel(
                model_name=selected_model,
                generation_config={
                    "response_mime_type": "application/json",
                    "max_output_tokens": 2500,
                    "temperature": 0.25,
                },
                system_instruction=(
                    "Du bist ein Dokument-Editor für Blop. "
                    "Erzeuge KEINEN Komplett-Text als Ersatz, sondern einen klaren Änderungsvorschlag als JSON.\n"
                    "Du antwortest IMMER als JSON.\n"
                    "Nutze vorzugsweise apply_mode 'append' oder 'replace_section'.\n"
                    "Für replace_section setze section_hint auf eine vorhandene Markdown-Überschrift (z. B. '## Fazit')."
                ),
            )
            compact_history = []
            for msg in history[-8:]:
                role = "user" if msg.get("role") == "user" else "model"
                compact_history.append({"role": role, "parts": [msg.get("content", "")]})
            chat = model.start_chat(history=compact_history)
            prompt = f"""
Aktuelles Dokument (Ausschnitt):
{document_content[:12000]}

Nutzerwunsch:
{user_message}

Antworte nur als JSON im Format:
{{
  "patch_description": "kurze Beschreibung",
  "apply_mode": "append" | "replace_section",
  "section_hint": "## Abschnittstitel oder leer",
  "new_text": "neuer Markdown-Text",
  "confidence": 0.0
}}
"""
            try:
                response = chat.send_message(prompt, safety_settings=SAFETY_SETTINGS)
            except Exception as first_err:
                err_msg = str(first_err)
                if "model is not found" in err_msg.lower() or "404" in err_msg:
                    fallback_model = AIService._pick_strong_model(None)
                    print(f"Document chat model fallback: '{selected_model}' -> '{fallback_model}'")
                    model = genai.GenerativeModel(
                        model_name=fallback_model,
                        generation_config={
                            "response_mime_type": "application/json",
                            "max_output_tokens": 2500,
                            "temperature": 0.25,
                        },
                        system_instruction=(
                            "Du bist ein Dokument-Editor für Blop. "
                            "Erzeuge KEINEN Komplett-Text als Ersatz, sondern einen klaren Änderungsvorschlag als JSON.\n"
                            "Du antwortest IMMER als JSON.\n"
                            "Nutze vorzugsweise apply_mode 'append' oder 'replace_section'.\n"
                            "Für replace_section setze section_hint auf eine vorhandene Markdown-Überschrift (z. B. '## Fazit')."
                        ),
                    )
                    chat = model.start_chat(history=compact_history)
                    response = chat.send_message(prompt, safety_settings=SAFETY_SETTINGS)
                else:
                    raise
            if not response.text:
                raise Exception("Leere Antwort vom Modell erhalten.")
            parsed = json.loads(response.text)
            if not isinstance(parsed, dict):
                raise Exception("Patch-Antwort ist kein JSON-Objekt.")
            apply_mode = parsed.get("apply_mode")
            if apply_mode not in ("append", "replace_section"):
                parsed["apply_mode"] = "append"
            parsed["patch_description"] = str(parsed.get("patch_description", "Dokumentänderung"))
            parsed["section_hint"] = str(parsed.get("section_hint", "") or "")
            parsed["new_text"] = str(parsed.get("new_text", "") or "")
            if not parsed["new_text"].strip():
                raise Exception("Patch enthält keinen neuen Text.")
            if not return_meta:
                return parsed
            return {
                "patch": parsed,
                "usage": AIService._extract_usage(response),
                "used_model": str(getattr(model, "model_name", "") or ""),
            }
        except Exception as e:
            print(f"Document Chat Patch Error: {e}")
            raise Exception(f"Fehler bei der interaktiven Dokument-Bearbeitung: {str(e)}")

    @staticmethod
    def generate_repetition(content: List[Any], custom_rules: str = "", model_preference: str = None, learning_mode: str = "normal", return_meta: bool = False) -> Any:
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
- Pflicht: Auch die **letzten** Aufgabenblöcke, Projektphasen oder Teilaufgaben am **Dokumentende** müssen in eigenen Unterabschnitten vorkommen. Viele Modelle lassen den Schluss weg — das ist hier unzulässig.
- Wenn es nummerierte Aufgaben oder „Teil A/B/C“ gibt: stelle sicher, dass die **höchsten Nummern** und der **Schlussteil** inhaltlich abgedeckt sind, nicht nur der Anfang.
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
- Besonders oft fehlen die **letzten drei bis fünf** Aufgaben oder **Projekt-/Pflichtaufgaben am Ende** — diese sind genauso verpflichtend wie der Anfang. Arbeite das PDF/Heft **von der ersten bis zur letzten Seite** inhaltlich ab.
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

VOLLSTÄNDIGKEIT (kritisch, häufiger Modellfehler):
- Nicht nur den Anfang des Materials ausarbeiten und dann „fertig“ wirken: **Dokumentende und letzte Aufgaben/Projektphasen** müssen inhaltlich behandelt sein.
- Wenn du merkst, dass der Platz knapp wird: kürze lieber Zwischenwiederholungen in der Mitte, aber **nicht** den Schluss des Quelldokuments.

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

            try:
                if return_meta:
                    full_result = AIService._generate_with_continuation(
                        model=model,
                        input_parts=input_parts,
                        continuation_prompt=AIService._REPETITION_CONTINUATION_PROMPT,
                        max_followups=6,
                        log_prefix="Repetition",
                        return_meta=True,
                    )
                else:
                    full_text = AIService._generate_repetition_with_continuation(model, input_parts)
                    full_result = full_text
            except Exception as mt_err:
                print(f"Repetition multiturn failed ({mt_err}), falling back to single-shot generate_content")
                response = model.generate_content(input_parts, safety_settings=SAFETY_SETTINGS)
                if not response.text:
                    raise Exception("Leere Antwort vom Modell erhalten.")
                if return_meta:
                    full_result = {
                        "text": response.text,
                        "usage": AIService._extract_usage(response),
                        "used_model": str(getattr(model, "model_name", "") or ""),
                    }
                else:
                    full_result = response.text
            full_text = full_result["text"] if return_meta else full_result
            if not (full_text or "").strip():
                raise Exception("Leere Antwort vom Modell erhalten.")
            return full_result
        except Exception as e:
            print(f"Repetition Error: {e}")
            raise Exception(f"Fehler bei der Wiederholung: {str(e)}")

    @staticmethod
    def chat(content: List[Any], user_message: str, history: List[Dict[str, str]], model_preference: str = None, active_file: dict = None, return_meta: bool = False) -> Any:
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
            if not return_meta:
                return response.text
            return {
                "text": response.text,
                "usage": AIService._extract_usage(response),
                "used_model": str(getattr(model, "model_name", "") or ""),
            }
            
        except Exception as e:
            import traceback
            traceback.print_exc()
            print(f"Chat Error: {e}")
            raise Exception(f"Fehler im Chatbot: {str(e)}")

