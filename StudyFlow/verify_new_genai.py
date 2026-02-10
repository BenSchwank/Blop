
import sys
import os

# Add path
sys.path.append(r"c:\Users\NaqsZ\OneDrive\Arbeit_Schule\coding\Blop\Blop-3.3.3\StudyFlow")

from unittest.mock import MagicMock
sys.modules["streamlit"] = MagicMock()
sys.modules["streamlit"].secrets = {"google": {"client_id": "", "client_secret": "", "redirect_uri": ""}}

try:
    from google import genai
    from google.genai import types
    
    print(f"Google GenAI SDK Imported Successfully.")
    
    # Check if we can instantiate client (mock key since we don't have real one here easily without env)
    # But ideally we check if Image Generation is supported in types
    
    if hasattr(genai, "Client"):
        print("[OK] genai.Client exists.")
    else:
        print("[FAIL] genai.Client MISSING.")
        
    # Check for image generation types/methods
    # Usually it's client.models.generate_image
    
    print("Verification script finished import checks.")

except ImportError as e:
    print(f"[ERROR] Import failed: {e}")
except Exception as e:
    print(f"[ERROR] Logic error: {e}")
