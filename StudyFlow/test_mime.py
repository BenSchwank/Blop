import os
import google.generativeai as genai
import time
from backend.data_manager import DataManager

# Mock configuration
from dotenv import load_dotenv
load_dotenv()
if "GOOGLE_API_KEY" in os.environ:
    genai.configure(api_key=os.environ["GOOGLE_API_KEY"])

def test_upload():
    # Let's see how Gemini reacts to audio/webm vs default
    import mimetypes
    print(mimetypes.guess_type("test.webm"))
    
if __name__ == "__main__":
    test_upload()
