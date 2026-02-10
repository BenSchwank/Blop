import json
import os
import streamlit as st
import shutil
from datetime import datetime
import uuid

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
                    
                    # Fix formatting of private_key (often broken in TOML/Streamlit Secrets)
                    if "private_key" in cred_dict:
                        pk = cred_dict["private_key"]
                        # 1. Replace literal \n with actual newline
                        pk = pk.replace("\\n", "\n")
                        # 2. Handle case where it might be space-separated entries if copy-paste went wrong? (Unlikely but safe)
                        # pk = pk.replace("-----BEGIN PRIVATE KEY----- ", "-----BEGIN PRIVATE KEY-----\n")
                        # pk = pk.replace(" -----END PRIVATE KEY-----", "\n-----END PRIVATE KEY-----")
                        cred_dict["private_key"] = pk
                    
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
        """LEGACY: Saves a manual summary note."""
        DataManager.save_summary_as_file(title, content, username, folder_id, "summary")

    # --- NEW: File-Based Management ---
    
    @staticmethod
    def save_file_metadata(file_data, username, folder_id):
        """
        Generic method to save file metadata (Plan, Summary, PDF) 
        to a 'files' list in the folder or subcollection.
        file_data: {id, name, type, created_at, ...}
        """
        db = DataManager._init_firestore()
        if db:
            # CLOUD MODE: Subcollection 'files'
            doc_ref = db.collection("users").document(username).collection("folders").document(str(folder_id)).collection("files").document(file_data["id"])
            doc_ref.set(file_data)
        else:
            # LOCAL MODE: Update folder's 'files' list
            data = DataManager.load(username)
            found_folder = None
            for f in data.get("folders", []):
                if f["id"] == folder_id:
                    found_folder = f
                    break
            
            if found_folder:
                if "files" not in found_folder: found_folder["files"] = []
                # Update or Append
                existing_idx = next((i for i, x in enumerate(found_folder["files"]) if x["id"] == file_data["id"]), -1)
                if existing_idx >= 0:
                    found_folder["files"][existing_idx] = file_data
                else:
                    found_folder["files"].append(file_data)
                DataManager.save(data, username)

    @staticmethod
    def list_files(username, folder_id):
        """Returns all files (PDFs, Plans, Summaries) for a workspace."""
        files = []
        
        # 1. Get PDFs -> Convert to generic file format
        pdfs = DataManager.list_pdfs(username, folder_id) # Returns [] of names
        # We need more metadata for PDFs if possible, but list_pdfs only returns names in local mode.
        # Let's standardize this.
        
        # 2. Get Plans/Summaries from Storage
        db = DataManager._init_firestore()
        if db:
            # CLOUD
            # a) PDFs (from pdfs collection - distinct from files collection for now to avoid breaking existing pdf logic)
            # Actually, let's just query the 'files' subcollection we created for new types
            docs = db.collection("users").document(username).collection("folders").document(str(folder_id)).collection("files").stream()
            files.extend([d.to_dict() for d in docs])
            
            # b) PDFs are stored in 'pdfs' collection. We should manually list them too or migrate them?
            # For now, let's fetch them and wrap as File objects
            pdf_docs = db.collection("users").document(username).collection("pdfs").where("folder_id", "==", folder_id).stream()
            for p in pdf_docs:
                p_data = p.to_dict()
                files.append({
                    "id": p.id,
                    "name": p_data.get("name"),
                    "type": "pdf",
                    "created_at": p_data.get("created_at")
                })
        else:
            # LOCAL
            data = DataManager.load(username)
            folder = next((f for f in data.get("folders", []) if f["id"] == folder_id), None)
            if folder:
                # a) New generic files
                files.extend(folder.get("files", []))
                
                # b) Local PDFs are just files on disk
                folder_path = DataManager._get_folder_path(username, folder_id)
                if os.path.exists(folder_path):
                    for f in os.listdir(folder_path):
                        if f.lower().endswith(".pdf"):
                            files.append({
                                "id": f, # filename as id for local
                                "name": f,
                                "type": "pdf",
                                "created_at": datetime.fromtimestamp(os.path.getctime(os.path.join(folder_path, f))).strftime("%Y-%m-%d")
                            })
                            
        return files

    @staticmethod
    def save_plan_as_file(plan_data, username, folder_id, name="Lernplan"):
        file_id = f"plan_{int(datetime.now().timestamp())}"
        file_meta = {
            "id": file_id,
            "name": name,
            "type": "plan",
            "content": plan_data, # For Plans, content IS the JSON
            "created_at": datetime.now().strftime("%Y-%m-%d")
        }
        DataManager.save_file_metadata(file_meta, username, folder_id)

    @staticmethod
    def save_summary_as_file(title, content, username, folder_id, type="summary"):
        file_id = f"summary_{int(datetime.now().timestamp())}"
        file_meta = {
            "id": file_id,
            "name": title,
            "type": type,
            "content": content, # For Summaries, content is HTML/Markdown string
            "created_at": datetime.now().strftime("%Y-%m-%d")
        }
        DataManager.save_file_metadata(file_meta, username, folder_id)


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
        
        # ALSO save as a distinct file for the new UI
        # We overwrite proper "Auto-Save" file or create new?
        # For simplicity: One "Main Plan" per folder for now?
        # Let's save/overwrite a file named "Aktueller Lernplan"
        DataManager.save_plan_as_file(plan_data, username, folder_id, name="Aktueller Lernplan")
            
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
            
        # ALSO save as a distinct file
        DataManager.save_summary_as_file("Automatische Zusammenfassung", summary_text, username, folder_id, "summary")

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
            # CLOUD MODE (Firestore Chunking)
            # Firestore has 1MB limit. We chunk files > 900KB.
            
            import base64
            file_data = uploaded_file.getvalue()
            total_size = len(file_data)
            chunk_size = 900 * 1024 # 900KB chunks
            
            # Create a main document for metadata
            doc_ref = db.collection("users").document(username).collection("pdfs").document(f"{folder_id}_{uploaded_file.name}")
            
            # Prepare metadata
            metadata = {
                "name": uploaded_file.name,
                "folder_id": folder_id,
                "created_at": datetime.now().strftime("%Y-%m-%d"),
                "total_size": total_size,
                "is_chunked": True,
                "chunk_count": (total_size + chunk_size - 1) // chunk_size
            }
            doc_ref.set(metadata)
            
            # Upload chunks
            for i in range(0, total_size, chunk_size):
                chunk_data = file_data[i:i + chunk_size]
                b64_chunk = base64.b64encode(chunk_data).decode('utf-8')
                
                # Save chunk in sub-coll
                chunk_ref = doc_ref.collection("chunks").document(f"chunk_{i // chunk_size}")
                chunk_ref.set({
                    "index": i // chunk_size,
                    "data": b64_chunk
                })
            
            return uploaded_file.name
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
                meta = doc.to_dict()
                
                import base64
                
                # Check if chunked
                if meta.get("is_chunked"):
                    # Reassemble
                    full_b64 = ""
                    chunks = doc_ref.collection("chunks").order_by("index").stream()
                    # We might need to write binary chunks directly to file to save memory
                    
                    tmp_dir = os.path.join(DATA_DIR, "temp")
                    if not os.path.exists(tmp_dir): os.makedirs(tmp_dir)
                    tmp_path = os.path.join(tmp_dir, filename)
                    
                    with open(tmp_path, "wb") as f:
                        for chunk in chunks:
                            c_data = chunk.to_dict()
                            f.write(base64.b64decode(c_data["data"]))
                    return tmp_path
                    
                else:
                    # Legacy or small file (if we had specific field)
                    # But we are rewriting everything to be robust. 
                    # If old format exists (content_b64 in main doc):
                    if "content_b64" in meta:
                        tmp_dir = os.path.join(DATA_DIR, "temp")
                        if not os.path.exists(tmp_dir): os.makedirs(tmp_dir)
                        tmp_path = os.path.join(tmp_dir, filename)
                        with open(tmp_path, "wb") as f:
                            f.write(base64.b64decode(meta["content_b64"]))
                        return tmp_path
                return None
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
