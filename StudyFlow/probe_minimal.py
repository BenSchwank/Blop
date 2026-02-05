import sys
try:
    import google.generativeai as genai
    print("Google SDK works")
except ImportError as e:
    print(f"Google SDK failed: {e}")
except Exception as e:
    print(f"Google SDK crashed: {e}")

try:
    import faiss
    print("FAISS works")
except ImportError as e:
    print(f"FAISS failed: {e}")
except Exception as e:
    print(f"FAISS crashed: {e}")

try:
    import numpy as np
    print("Numpy works")
except ImportError as e:
    print(f"Numpy failed: {e}")
except Exception as e:
    print(f"Numpy crashed: {e}")
