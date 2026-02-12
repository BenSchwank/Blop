import os
import hashlib
from gtts import gTTS
import streamlit as st

AUDIO_CACHE_DIR = "user_data/audio_cache"

class AudioManager:
    @staticmethod
    def _get_cache_path(text, lang):
        """Generates a unique filename based on text content hash."""
        if not os.path.exists(AUDIO_CACHE_DIR):
            os.makedirs(AUDIO_CACHE_DIR)
            
        # Hash the text to create a stable ID
        text_hash = hashlib.md5(text.encode('utf-8')).hexdigest()
        filename = f"{text_hash}_{lang}.mp3"
        return os.path.join(AUDIO_CACHE_DIR, filename)

    @staticmethod
    def generate_audio(text, lang='de'):
        """
        Converts text to speech and returns the file path.
        Uses caching to avoid re-generating the same text.
        """
        try:
            # simple check for empty text
            if not text or len(text.strip()) < 5:
                return None
                
            path = AudioManager._get_cache_path(text, lang)
            
            # Return existing if cached
            if os.path.exists(path):
                return path
            
            # Generate new
            tts = gTTS(text=text, lang=lang)
            tts.save(path)
            return path
            
        except Exception as e:
            print(f"TTS Error: {e}")
            return None

    @staticmethod
    def clear_cache():
        """Removes all cached audio files."""
        if os.path.exists(AUDIO_CACHE_DIR):
            for f in os.listdir(AUDIO_CACHE_DIR):
                if f.endswith(".mp3"):
                    try:
                        os.remove(os.path.join(AUDIO_CACHE_DIR, f))
                    except: pass
