import streamlit
import requests
import json
import os
try:
    import numpy as np
    print("Numpy works")
except ImportError as e:
    print(f"Numpy failed: {e}")

try:
    import faiss
    print("FAISS works")
except ImportError as e:
    print(f"FAISS failed: {e}")
except Exception as e:
    print(f"FAISS crashed: {e}")
