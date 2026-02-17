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
    def generate_summary(text: str, detail_level: str = "Normal") -> str:
        """Generates a summary of the provided text."""
        try:
            model = genai.GenerativeModel(model_name)
            prompt = f"""
            Erstelle eine Zusammenfassung des folgenden Textes auf Deutsch.
            Detailgrad: {detail_level}
            
            Formatierung: Markdown.
            Verwende Überschriften, Listen und hebe wichtige Begriffe fett hervor.
            
            Text:
            {text[:30000]} 
            """ # Truncate to avoid context limit issues blindly
            
            response = model.generate_content(prompt)
            return response.text
        except Exception as e:
            return f"Fehler bei der Generierung: {str(e)}"

    @staticmethod
    def generate_quiz(text: str) -> List[Dict[str, Any]]:
        """Generates a quiz from the text."""
        try:
            model = genai.GenerativeModel(model_name, generation_config={"response_mime_type": "application/json"})
            prompt = f"""
            Erstelle ein Quiz mit 5 Fragen basierend auf dem folgenden Text.
            Ausgabe-Format: JSON Array.
            Beispiel:
            [
                {{
                    "question": "Frage?",
                    "options": ["A", "B", "C", "D"],
                    "answer": "A"
                }}
            ]
            
            Text:
            {text[:20000]}
            """
            
            response = model.generate_content(prompt)
            return json.loads(response.text)
        except Exception as e:
            print(f"Quiz Error: {e}")
            return []

    @staticmethod
    def generate_flashcards(text: str) -> List[Dict[str, str]]:
        """Generates flashcards."""
        try:
            model = genai.GenerativeModel(model_name, generation_config={"response_mime_type": "application/json"})
            prompt = f"""
            Erstelle 10 Karteikarten aus dem Text.
            Format: JSON Array
            [
                {{"front": "Frage/Begriff", "back": "Antwort/Erklärung"}}
            ]
            
            Text:
            {text[:20000]}
            """
            response = model.generate_content(prompt)
            return json.loads(response.text)
        except Exception as e:
            print(f"Flashcard Error: {e}")
            return []
