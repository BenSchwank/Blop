
import sys
import os

# Add path
sys.path.append(r"c:\Users\NaqsZ\OneDrive\Arbeit_Schule\coding\Blop\Blop-3.3.3\StudyFlow")

from unittest.mock import MagicMock
sys.modules["streamlit"] = MagicMock()
sys.modules["streamlit"].secrets = {"google": {"client_id": "", "client_secret": "", "redirect_uri": ""}}

try:
    import google.generativeai as genai
    
    print(f"GenAI Version: {genai.__version__}")
    
    if hasattr(genai, "ImageGenerationModel"):
        print("[OK] genai.ImageGenerationModel exists.")
    else:
        print("[FAIL] genai.ImageGenerationModel components MISSING. Might need update.")
        
except ImportError as e:
    print(f"[ERROR] Import failed: {e}")
except Exception as e:
    print(f"[ERROR] Logic error: {e}")
