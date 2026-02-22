import os
import google.generativeai as genai
import json
from typing import List, Dict, Any

# Configure GenAI
api_key = os.environ.get("GOOGLE_API_KEY")
if api_key:
    genai.configure(api_key=api_key)

model_name = "gemini-2.0-flash-exp" # Or latest available

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
        """Generates a summary from text or multimodal content."""
        try:
            model = genai.GenerativeModel(model_name)
            
            prompt = f"""
            Erstelle eine Zusammenfassung des folgenden Materials auf Deutsch.
            Detailgrad: {detail_level}
            
            Formatierung: Markdown.
            Verwende Überschriften, Listen und hebe wichtige Begriffe fett hervor.
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
        """Generates a quiz from multimodal content."""
        try:
            model = genai.GenerativeModel(model_name, generation_config={"response_mime_type": "application/json"})
            prompt = """
            Erstelle ein Quiz mit 5 Fragen basierend auf dem folgenden Material.
            Ausgabe-Format: JSON Array.
            Beispiel:
            [
                {
                    "question": "Frage?",
                    "options": ["A", "B", "C", "D"],
                    "answer": "A"
                }
            ]
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
        """Generates flashcards from multimodal content."""
        try:
            model = genai.GenerativeModel(model_name, generation_config={"response_mime_type": "application/json"})
            prompt = """
            Erstelle 10 Karteikarten aus dem Material.
            Format: JSON Array
            [
                {"front": "Frage/Begriff", "back": "Antwort/Erklärung"}
            ]
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
    def generate_study_plan(content: List[Any], duration_days: int) -> List[Dict[str, Any]]:
        """Generates a structured study plan from multimodal content."""
        try:
            model = genai.GenerativeModel(model_name, generation_config={"response_mime_type": "application/json"})
            prompt = f"""
            Erstelle einen detaillierten Lernplan für {duration_days} Tage basierend auf dem Material.
            Der Plan soll den Stoff sinnvoll aufteilen. Verteile das Material gleichmäßig auf genau {duration_days} Tage.
            
            Ausgabe-Format: JSON Array mit genau {duration_days} Einträgen:
            [
                {{
                    "day": 1,
                    "topic": "Thema des Tages",
                    "tasks": ["Lese Abschnitt X", "Mache Aufgabe Y"],
                    "goal": "Ziel des Tages"
                }}
            ]
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
