import json
import os
import streamlit as st
import shutil
from datetime import datetime

# Optional Firebase Imports
try:
    import firebase_admin
    from firebase_admin import credentials, firestore
except ImportError:
    pass

DATA_DIR = "user_data"

class DataManager:
    _db = None

    @staticmethod
    def _init_firestore():
        """Initializes Firestore if secrets are present and not already initialized."""
        if DataManager._db is not None:
            return DataManager._db
            
        if "firebase" in st.secrets:
            try:
                if not firebase_admin._apps:
                    # Parse secrets to dict
                    cred_dict = dict(st.secrets["firebase"])
                    cred = credentials.Certificate(cred_dict)
                    firebase_admin.initialize_app(cred)
                
                DataManager._db = firestore.client()
                return DataManager._db
            except Exception as e:
                print(f"Firestore Init Error: {e}")
                st.session_state.db_error = str(e)
                return None
        else:
            st.session_state.db_error = "Secret '[firebase]' not found in secrets.toml"
        return None

    @staticmethod
    def _get_file(username):
        if not os.path.exists(DATA_DIR):
            os.makedirs(DATA_DIR)
        safe_name = "".join([c for c in username if c.isalnum() or c in "-_"])
        return os.path.join(DATA_DIR, f"{safe_name}.json")

    @staticmethod
    def load(username):
        db = DataManager._init_firestore()
        if db:
            # CLOUD MODE
            doc_ref = db.collection("users").document(username)
            doc = doc_ref.get()
            if doc.exists:
                return doc.to_dict()
            else:
                return {"folders": [], "files": []}
        else:
            # LOCAL MODE
            filepath = DataManager._get_file(username)
            if not os.path.exists(filepath):
                return {"folders": [], "files": []}
            try:
                with open(filepath, "r", encoding="utf-8") as f:
                    return json.load(f)
            except:
                return {"folders": [], "files": []}

    @staticmethod
    def save(data, username):
        db = DataManager._init_firestore()
        if db:
            # CLOUD MODE
            db.collection("users").document(username).set(data)
        else:
            # LOCAL MODE
            filepath = DataManager._get_file(username)
            with open(filepath, "w", encoding="utf-8") as f:
                json.dump(data, f, ensure_ascii=False, indent=4)

    @staticmethod
    def create_folder(name, username):
        data = DataManager.load(username)
        folder = {
            "id": f"folder_{int(datetime.now().timestamp())}",
            "name": name,
            "created_at": datetime.now().strftime("%Y-%m-%d")
        }
        if "folders" not in data: data["folders"] = []
        data["folders"].append(folder)
        DataManager.save(data, username)
        return folder

    @staticmethod
    def save_summary(title, content, username, folder_id=None):
        data = DataManager.load(username)
        file_entry = {
            "id": f"file_{int(datetime.now().timestamp())}",
            "title": title,
            "content": content,
            "folder_id": folder_id,
            "created_at": datetime.now().strftime("%d.%m.%Y")
        }
        if "files" not in data: data["files"] = []
        data["files"].append(file_entry)
        DataManager.save(data, username)

    # --- NEW: Auto-Save Methods ---

    @staticmethod
    def save_plan(plan_data, username, folder_id):
        """Updates the 'plan_data' for a specific folder/project."""
        data = DataManager.load(username)
        
        # Find folder and update its plan
        found = False
        for f in data.get("folders", []):
            if f["id"] == folder_id:
                f["plan_data"] = plan_data
                f["last_updated"] = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
                found = True
                break
        
        if found:
            DataManager.save(data, username)
            
    @staticmethod
    def load_plan(username, folder_id):
        """Loads (auto-saved) plan data for a folder."""
        data = DataManager.load(username)
        for f in data.get("folders", []):
            if f["id"] == folder_id:
                return f.get("plan_data", [])
        return []

    @staticmethod
    def save_generated_summary(summary_text, username, folder_id):
        """Updates the 'summary_text' (Full Summary) for a specific folder."""
        data = DataManager.load(username)
        found = False
        for f in data.get("folders", []):
            if f["id"] == folder_id:
                f["summary_text"] = summary_text
                found = True
                break
        if found:
            DataManager.save(data, username)

    @staticmethod
    def load_generated_summary(username, folder_id):
        data = DataManager.load(username)
        for f in data.get("folders", []):
            if f["id"] == folder_id:
                return f.get("summary_text", "")
        return ""

    # --- PDF File Management ---
    @staticmethod
    def _get_folder_path(username, folder_id):
        safe_user = "".join([c for c in username if c.isalnum() or c in "-_"])
        path = os.path.join(DATA_DIR, safe_user, str(folder_id))
        if not os.path.exists(path):
            os.makedirs(path)
        return path

    @staticmethod
    def save_pdf(uploaded_file, username, folder_id):
        # Hybrid Approach:
        # If Cloud: Try to save to Firestore (Base64) if small, else fail/warn.
        # Ideally we use Google Cloud Storage, but sticking to Firestore for now (MVP).
        
        db = DataManager._init_firestore()
        if db:
            # CLOUD MODE (Firestore Blobs - NOT RECOMMENDED for large files but requested)
            # Limit check: 1MB
            if uploaded_file.size > 900 * 1024:
                print("Warning: File too large for Firestore Document!")
                return None
            
            import base64
            file_data = uploaded_file.getvalue()
            b64 = base64.b64encode(file_data).decode('utf-8')
            
            doc_ref = db.collection("users").document(username).collection("pdfs").document(f"{folder_id}_{uploaded_file.name}")
            doc_ref.set({
                "name": uploaded_file.name,
                "folder_id": folder_id,
                "content_b64": b64,
                "created_at": datetime.now().strftime("%Y-%m-%d")
            })
            return uploaded_file.name # Return name as reference
        else:
            # LOCAL MODE
            folder_path = DataManager._get_folder_path(username, folder_id)
            file_path = os.path.join(folder_path, uploaded_file.name)
            with open(file_path, "wb") as f:
                f.write(uploaded_file.getbuffer())
            return file_path

    @staticmethod
    def list_pdfs(username, folder_id):
        db = DataManager._init_firestore()
        if db:
            # CLOUD MODE
            docs = db.collection("users").document(username).collection("pdfs").where("folder_id", "==", folder_id).stream()
            return [d.to_dict()["name"] for d in docs]
        else:
            # LOCAL MODE
            folder_path = DataManager._get_folder_path(username, folder_id)
            if not os.path.exists(folder_path):
                return []
            return [f for f in os.listdir(folder_path) if f.lower().endswith('.pdf')]

    @staticmethod
    def get_pdf_path(filename, username, folder_id):
        # In cloud mode, this "path" concept is tricky.
        # We might need to download it to a temp file for processing.
        
        db = DataManager._init_firestore()
        if db:
            # CLOUD MODE: Download to temp
            doc_ref = db.collection("users").document(username).collection("pdfs").document(f"{folder_id}_{filename}")
            doc = doc_ref.get()
            if doc.exists:
                import base64
                import tempfile
                data = doc.to_dict()
                
                # Create temp file
                tmp_dir = os.path.join(DATA_DIR, "temp")
                if not os.path.exists(tmp_dir): os.makedirs(tmp_dir)
                
                tmp_path = os.path.join(tmp_dir, filename)
                with open(tmp_path, "wb") as f:
                    f.write(base64.b64decode(data["content_b64"]))
                return tmp_path
            return None
        else:
            # LOCAL MODE
            return os.path.join(DataManager._get_folder_path(username, folder_id), filename)

    @staticmethod
    def delete_user_data(username):
        """Deletes all data associated with a user."""
        db = DataManager._init_firestore()
        
        if db:
             # CLOUD MODE: Delete Doc & Subcollections
             # Firestore recursive delete is complex, we do simple best effort here
             try:
                 db.collection("users").document(username).delete()
                 # Note: Subcollections (pdfs) are NOT automatically deleted!
                 # We would need to list and delete them.
                 pdfs = db.collection("users").document(username).collection("pdfs").stream()
                 for p in pdfs:
                     p.reference.delete()
             except: pass
        else:
            # LOCAL MODE
            # 1. Delete Main Data JSON
            json_path = DataManager._get_file(username)
            if os.path.exists(json_path):
                os.remove(json_path)
                
            # 2. Delete User Directory (PDFs etc.)
            safe_user = "".join([c for c in username if c.isalnum() or c in "-_"])
            user_dir = os.path.join(DATA_DIR, safe_user)
            if os.path.exists(user_dir):
                shutil.rmtree(user_dir)

    @staticmethod
    def clear_user_data(username):
        """Deletes all data but keeps the user account."""
        DataManager.delete_user_data(username)
        DataManager.save({"folders": [], "files": []}, username)
