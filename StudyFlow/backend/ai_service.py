import os
import google.generativeai as genai
import json
from typing import List, Dict, Any

# Configure GenAI
api_key = os.environ.get("GOOGLE_API_KEY")
if api_key:
    genai.configure(api_key=api_key)

model_name = "gemini-2.0-flash-exp" # Or latest available

class AIService:
    @staticmethod
    def generate_summary(content: List[Any], detail_level: str = "Normal") -> str:
        """Generates a summary from text or multimodal content."""
        try:
            model = genai.GenerativeModel(model_name)
            
            # Prepare Prompt Part
            prompt = f"""
            Erstelle eine Zusammenfassung des folgenden Materials auf Deutsch.
            Detailgrad: {detail_level}
            
            Formatierung: Markdown.
            Verwende Überschriften, Listen und hebe wichtige Begriffe fett hervor.
            """
            
            # Combine Prompt + Content
            # content is expected to be a list that can contain strings or File objects
            input_parts = [prompt]
            if isinstance(content, list):
                input_parts.extend(content)
            else:
                input_parts.append(content)
            
            response = model.generate_content(input_parts)
            return response.text
        except Exception as e:
            return f"Fehler bei der Generierung: {str(e)}"

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
            
            response = model.generate_content(input_parts)
            return json.loads(response.text)
        except Exception as e:
            print(f"Quiz Error: {e}")
            return []

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
                
            response = model.generate_content(input_parts)
            return json.loads(response.text)
        except Exception as e:
            print(f"Flashcard Error: {e}")
            return []

    @staticmethod
    def generate_study_plan(content: List[Any], duration_days: int) -> List[Dict[str, Any]]:
        """Generates a structured study plan from multimodal content."""
        try:
            model = genai.GenerativeModel(model_name, generation_config={"response_mime_type": "application/json"})
            prompt = f"""
            Erstelle einen detaillierten Lernplan für {duration_days} Tage basierend auf dem Material.
            Der Plan soll den Stoff sinnvoll aufteilen.
            
            Ausgabe-Format: JSON Array
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

            response = model.generate_content(input_parts)
            return json.loads(response.text)
        except Exception as e:
            print(f"Plan Error: {e}")
            return []
