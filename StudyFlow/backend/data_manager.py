import json
import os
import shutil
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
    _user_cache = {} # Map username -> {data, mtime}

    @staticmethod
    def _init_firestore():
        """Initializes Firestore using Env Variable OR backend/firebase_credentials.json."""
        if DataManager._db is not None:
            return DataManager._db
            
        # 1. Try Environment Variable (Render/Cloud)
        env_creds = os.environ.get("FIREBASE_CREDENTIALS")
        if env_creds:
            try:
                if not firebase_admin._apps:
                    cred_dict = json.loads(env_creds)
                    cred = credentials.Certificate(cred_dict)
                    firebase_admin.initialize_app(cred)
                
                DataManager._db = firestore.client()
                return DataManager._db
            except Exception as e:
                print(f"Firestore Env Init Error: {e}")
                # Fallthrough to local file check
        
        # 2. Try Local File (Dev)
        cred_path = os.path.join(os.path.dirname(__file__), "firebase_credentials.json")
        if os.path.exists(cred_path):
            try:
                if not firebase_admin._apps:
                    cred = credentials.Certificate(cred_path)
                    firebase_admin.initialize_app(cred)
                
                DataManager._db = firestore.client()
                return DataManager._db
            except Exception as e:
                print(f"Firestore File Init Error: {e}")
                return None
        else:
            # Only print if also failed env var
            if not env_creds:
                 print(f"Firebase credentials not found (Env or File).")
        return None

    @staticmethod
    def _get_file(username):
        if not os.path.exists(DATA_DIR):
            os.makedirs(DATA_DIR)
        safe_name = "".join([c for c in username if c.isalnum() or c in "-_"])
        return os.path.join(DATA_DIR, f"{safe_name}.json")



    @staticmethod
    def _get_folder_path(username, folder_id):
        safe_name = "".join([c for c in username if c.isalnum() or c in "-_"])
        user_dir = os.path.join(DATA_DIR, f"{safe_name}_files")
        if not os.path.exists(user_dir):
            os.makedirs(user_dir)
        return os.path.join(user_dir, str(folder_id))

    @staticmethod
    def user_exists(username):
        """Checks if a user exists in Firestore (to prevent duplicate registration)."""
        db = DataManager._init_firestore()
        if db:
            doc_ref = db.collection("users").document(username)
            doc = doc_ref.get()
            return doc.exists
        return False
        
    @staticmethod
    def get_all_cloud_users():
        """Fetches all user documents from Firestore."""
        db = DataManager._init_firestore()
        if db:
            try:
                users_ref = db.collection("users").stream()
                return [u.id for u in users_ref]
            except:
                return []
        return []

    @staticmethod
    def load(username):
        db = DataManager._init_firestore()
        if db:
            # CLOUD MODE: Single Source of Truth
            try:
                doc_ref = db.collection("users").document(username)
                doc = doc_ref.get()
                if doc.exists:
                    return doc.to_dict()
                else:
                    return {"folders": [], "files": []}
            except Exception as e:
                print(f"Firestore Load Error: {e}")
                return {"folders": [], "files": []}
        else:
            # LOCAL MODE
            filepath = DataManager._get_file(username)
            if not os.path.exists(filepath):
                return {"folders": [], "files": []}
            
            # Cache Check
            try:
                mtime = os.path.getmtime(filepath)
                if username in DataManager._user_cache:
                    uc = DataManager._user_cache[username]
                    if uc["mtime"] == mtime:
                        return uc["data"]
            except: pass

            try:
                with open(filepath, "r", encoding="utf-8") as f:
                    data = json.load(f)
                    # Update Cache
                    DataManager._user_cache[username] = {"data": data, "mtime": mtime}
                    return data
            except:
                return {"folders": [], "files": []}

    @staticmethod
    def save(data, username):
        db = DataManager._init_firestore()
        if db:
            # CLOUD MODE
            try:
                db.collection("users").document(username).set(data)
            except Exception as e:
                print(f"Firestore Save Error: {e}")
        else:
            # LOCAL MODE
            filepath = DataManager._get_file(username)
            with open(filepath, "w", encoding="utf-8") as f:
                json.dump(data, f, ensure_ascii=False, indent=4)

    @staticmethod
    def get_api_key(username):
        data = DataManager.load(username)
        return data.get("api_key")

    @staticmethod
    def save_api_key(username, api_key):
        data = DataManager.load(username)
        data["api_key"] = api_key
        DataManager.save(data, username)

    @staticmethod
    def delete_api_key(username):
        data = DataManager.load(username)
        if "api_key" in data:
            del data["api_key"]
            DataManager.save(data, username)
            return True
        return False

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
    def create_subfolder(name, username, parent_id):
        """Creates a subfolder inside an existing folder."""
        data = DataManager.load(username)
        subfolder = {
            "id": f"folder_{int(datetime.now().timestamp())}_{uuid.uuid4().hex[:6]}",
            "name": name,
            "parent_id": parent_id,
            "created_at": datetime.now().strftime("%Y-%m-%d")
        }
        if "folders" not in data: data["folders"] = []
        data["folders"].append(subfolder)
        DataManager.save(data, username)
        return subfolder

    @staticmethod
    def delete_folder(folder_id, username):
        """Deletes a folder (Project) and its contents."""
        db = DataManager._init_firestore()
        
        if db:
            # CLOUD MODE
            # Note: Firestore requires recursive delete for subcollections. 
            # For this MVP, we just delete the folder doc pointer in 'folders'
            # But wait, folders are stored inside the 'users' doc in a list?
            # Or as subcollections?
            # looking at create_folder: it appends to 'folders' list in the USER doc (both locally and cloud if using save).
            # Wait, `save` method:
            # db.collection("users").document(username).set(data)
            # So folders are just a list in the user document.
            
            data = DataManager.load(username)
            if "folders" in data:
                original_len = len(data["folders"])
                data["folders"] = [f for f in data["folders"] if f["id"] != folder_id]
                if len(data["folders"]) < original_len:
                    DataManager.save(data, username)
                    return True
            return False
        else:
            # LOCAL MODE
            data = DataManager.load(username)
            if "folders" in data:
                original_len = len(data["folders"])
                data["folders"] = [f for f in data["folders"] if f["id"] != folder_id]
                
                if len(data["folders"]) < original_len:
                    # Save metadata
                    DataManager.save(data, username)
                    
                    # Delete physical folder
                    folder_path = DataManager._get_folder_path(username, folder_id)
                    if os.path.exists(folder_path):
                        try:
                            shutil.rmtree(folder_path)
                        except Exception as e:
                            print(f"Error deleting folder on disk: {e}")
                    return True
            return False

    @staticmethod
    def rename_folder(folder_id, new_name, username):
        """Renames an existing folder."""
        data = DataManager.load(username)
        if "folders" in data:
            for f in data["folders"]:
                if f["id"] == folder_id:
                    f["name"] = new_name
                    DataManager.save(data, username)
                    return True
        return False

    @staticmethod
    def move_folder(folder_id, new_parent_id, username):
        """Moves a folder by updating its parent_id."""
        data = DataManager.load(username)
        if "folders" in data:
            for f in data["folders"]:
                if f["id"] == folder_id:
                    if new_parent_id:
                        f["parent_id"] = new_parent_id
                    else:
                        f.pop("parent_id", None)
                    DataManager.save(data, username)
                    return True
        return False

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
                DataManager.save(data, username)
                
    @staticmethod
    def update_file_content(username, folder_id, file_id, new_content):
        """Updates the content of an existing file in the folder."""
        db = DataManager._init_firestore()
        if db:
            # CLOUD MODE
            doc_ref = db.collection("users").document(username).collection("folders").document(str(folder_id)).collection("files").document(file_id)
            doc_ref.set({"content": new_content}, merge=True)
            return True
        else:
            # LOCAL MODE
            data = DataManager.load(username)
            found_file = False
            for f in data.get("folders", []):
                if f["id"] == folder_id:
                    if "files" in f:
                        for idx, file_item in enumerate(f["files"]):
                            if file_item["id"] == file_id:
                                f["files"][idx]["content"] = new_content
                                f["files"][idx]["updated_at"] = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
                                found_file = True
                                break
                    break
            
            if found_file:
                DataManager.save(data, username)
                return True
            return False

    @staticmethod
    def delete_file(username, folder_id, file_id):
        """Deletes a file (PDF, Plan, Summary) from the workspace."""
        db = DataManager._init_firestore()
        
        # Helper to identify type
        is_pdf = not (file_id.startswith("plan_") or file_id.startswith("summary_"))
        
        if db:
             # CLOUD MODE
             try:
                 if is_pdf:
                     # Delete from 'pdfs' collection
                     db.collection("users").document(username).collection("pdfs").document(file_id).delete()
                     # Note: Chunks remain orphaned in Firestore (Client SDK requires recursive delete). 
                     # For MVP we leave them, or we'd need to list and delete 'chunks' subcollection.
                 else:
                     # Delete from 'files' subcollection
                     db.collection("users").document(username).collection("folders").document(str(folder_id)).collection("files").document(file_id).delete()
                 return True
             except Exception as e:
                 print(f"Delete Error: {e}")
                 return False
        else:
             # LOCAL MODE
             if is_pdf:
                 # Delete generic PDF from disk
                 folder_path = DataManager._get_folder_path(username, folder_id)
                 file_path = os.path.join(folder_path, file_id) # file_id is filename for local PDFs
                 if os.path.exists(file_path):
                     try:
                         os.remove(file_path)
                         return True
                     except:
                         return False
                 return False
             else:
                 # Delete Plan/Summary from JSON metadata
                 data = DataManager.load(username)
                 found_folder = None
                 for f in data.get("folders", []):
                     if f["id"] == folder_id:
                         found_folder = f
                         break
                 
                 if found_folder and "files" in found_folder:
                     original_len = len(found_folder["files"])
                     found_folder["files"] = [f for f in found_folder["files"] if f["id"] != file_id]
                     if len(found_folder["files"]) < original_len:
                         DataManager.save(data, username)
                         return True
        return False

    @staticmethod
    def rename_file(username, folder_id, file_id, new_name):
        """Renames a file (Plan, Summary, Repetition, etc) by updating its metadata."""
        db = DataManager._init_firestore()
        
        # PDFs are stored differently and renaming them requires moving files/docs,
        # so for this MVP we only support renaming generic files in the "files" array/collection.
        is_pdf = file_id.endswith(".pdf") or not (
            file_id.startswith("plan_") or 
            file_id.startswith("summary_") or 
            file_id.startswith("rep_") or 
            file_id.startswith("elab_") or 
            file_id.startswith("quiz_") or 
            file_id.startswith("flash_")
        )
        
        if db:
             try:
                 if not is_pdf:
                     doc_ref = db.collection("users").document(username).collection("folders").document(str(folder_id)).collection("files").document(file_id)
                     doc_ref.set({"name": new_name}, merge=True)
                     return True
             except Exception as e:
                 print(f"Rename Error: {e}")
                 return False
        else:
             if not is_pdf:
                 data = DataManager.load(username)
                 for f in data.get("folders", []):
                     if f["id"] == folder_id and "files" in f:
                         for file_item in f["files"]:
                             if file_item["id"] == file_id:
                                 file_item["name"] = new_name
                                 DataManager.save(data, username)
                                 return True
        return False

    @staticmethod
    def move_file(username, source_folder_id, file_id, target_folder_id):
        """Moves a file from one folder to another. Complex due to Cloud/Local diffs."""
        db = DataManager._init_firestore()
        is_pdf = file_id.endswith(".pdf") or not (
            file_id.startswith("plan_") or 
            file_id.startswith("summary_") or 
            file_id.startswith("rep_") or 
            file_id.startswith("elab_") or 
            file_id.startswith("quiz_") or 
            file_id.startswith("flash_")
        )

        if db:
            try:
                if is_pdf:
                    # Move PDF Doc
                    source_ref = db.collection("users").document(username).collection("pdfs").document(f"{source_folder_id}_{file_id}")
                    target_ref = db.collection("users").document(username).collection("pdfs").document(f"{target_folder_id}_{file_id}")
                    
                    doc = source_ref.get()
                    if doc.exists:
                        data = doc.to_dict()
                        data["folder_id"] = target_folder_id
                        target_ref.set(data)
                        
                        # Move chunks (this is slow, but works for MVP)
                        chunks = source_ref.collection("chunks").stream()
                        for chunk in chunks:
                            target_ref.collection("chunks").document(chunk.id).set(chunk.to_dict())
                            chunk.reference.delete()
                            
                        source_ref.delete()
                        return True
                else:
                    # Move standard file doc
                    source_ref = db.collection("users").document(username).collection("folders").document(str(source_folder_id)).collection("files").document(file_id)
                    target_ref = db.collection("users").document(username).collection("folders").document(str(target_folder_id)).collection("files").document(file_id)
                    
                    doc = source_ref.get()
                    if doc.exists:
                        target_ref.set(doc.to_dict())
                        source_ref.delete()
                        return True
            except Exception as e:
                print(f"Move Error: {e}")
                return False
        else:
            if is_pdf:
                # Local FS move
                source_folder_path = DataManager._get_folder_path(username, source_folder_id)
                target_folder_path = DataManager._get_folder_path(username, target_folder_id)
                source_path = os.path.join(source_folder_path, file_id)
                target_path = os.path.join(target_folder_path, file_id)
                
                if os.path.exists(source_path):
                    if not os.path.exists(target_folder_path):
                        os.makedirs(target_folder_path)
                    shutil.move(source_path, target_path)
                    return True
            else:
                # Update JSON Metadata
                data = DataManager.load(username)
                file_obj = None
                
                # Remove from source
                for f in data.get("folders", []):
                    if f["id"] == source_folder_id and "files" in f:
                        for i, file_item in enumerate(f["files"]):
                            if file_item["id"] == file_id:
                                file_obj = f["files"].pop(i)
                                break
                        break
                        
                # Add to target
                if file_obj:
                    for f in data.get("folders", []):
                        if f["id"] == target_folder_id:
                            if "files" not in f:
                                f["files"] = []
                            f["files"].append(file_obj)
                            DataManager.save(data, username)
                            return True
        return False

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
                        if f.lower().endswith(".pdf") or f.lower().endswith(".txt"):
                            files.append({
                                "id": f, # filename as id for local
                                "name": f,
                                "type": "pdf" if f.lower().endswith(".pdf") else "transcript",
                                "created_at": datetime.fromtimestamp(os.path.getctime(os.path.join(folder_path, f))).strftime("%Y-%m-%d")
                            })
                            
        return files

    @staticmethod
    def save_transcript(filename, content, username, folder_id):
        """Saves a transcript as a file metadata object with content."""
        file_id = f"transcript_{int(datetime.now().timestamp())}_{uuid.uuid4().hex[:6]}"
        file_meta = {
            "id": file_id,
            "name": filename,
            "type": "transcript",
            "content": content,
            "created_at": datetime.now().strftime("%Y-%m-%d")
        }
        DataManager.save_file_metadata(file_meta, username, folder_id)

    @staticmethod
    def save_plan_as_file(plan_data, username, folder_id, name="Lernplan"):
        # Use a fixed ID so we always overwrite (upsert) the plan, not accumulate duplicates
        file_id = f"plan_main_{folder_id}"
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
    def save_pdf(file_data, filename, username, folder_id):
        # Hybrid Approach:
        # If Cloud: Try to save to Firestore (Base64) if small, else fail/warn.
        # Ideally we use Google Cloud Storage, but sticking to Firestore for now (MVP).
        
        db = DataManager._init_firestore()
        if db:
            # CLOUD MODE (Firestore Chunking)
            # Firestore has 1MB limit. We chunk files > 900KB.
            
            import base64
            # file_data is already bytes
            total_size = len(file_data)
            chunk_size = 500 * 1024 # 500KB chunks (Base64 grows to ~666KB, safe for 1MB limit)
            
            # Create a main document for metadata
            doc_ref = db.collection("users").document(username).collection("pdfs").document(f"{folder_id}_{filename}")
            
            # Prepare metadata
            metadata = {
                "name": filename,
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
            
            return filename
        else:
            # LOCAL MODE
            folder_path = DataManager._get_folder_path(username, folder_id)
            file_path = os.path.join(folder_path, filename)
            with open(file_path, "wb") as f:
                f.write(file_data)
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
                            try:
                                f.write(base64.b64decode(c_data["data"]))
                            except:
                                pass # Skip bad chunks
                    
                    if os.path.getsize(tmp_path) == 0:
                        return None
                    return tmp_path
                    
                else:
                    # Legacy or small file (if we had specific field)
                    # But we are rewriting everything to be robust. 
                    # If old format exists (content_b64 in main doc):
                    if "content_b64" in meta:
                        tmp_dir = os.path.join(DATA_DIR, "temp")
                        if not os.path.exists(tmp_dir): os.makedirs(tmp_dir)
                        tmp_path = os.path.join(tmp_dir, filename)
                        
                        try:
                            with open(tmp_path, "wb") as f:
                                f.write(base64.b64decode(meta["content_b64"]))
                                
                            if os.path.getsize(tmp_path) == 0: return None
                            return tmp_path
                        except:
                            return None
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
    @staticmethod
    def save_flashcards(username, folder_id, cards_data):
        """
        Saves a list of flashcards.
        cards_data: List of dicts {id, front, back, box, next_review, last_reviewed}
        """
        # Save as a special file type "flashcards.json" in the folder
        file_id = f"cards_{folder_id}.json"
        
        # We wrap it in a file metadata structure compatible with list_files
        file_meta = {
            "id": file_id,
            "name": "Flashcards",
            "type": "flashcards",
            "created_at": datetime.now().isoformat(),
            "content": cards_data
        }
        
        DataManager.save_file_metadata(file_meta, username, folder_id)

    @staticmethod
    def load_flashcards(username, folder_id):
        """Loads flashcards for the given folder."""
        # We iterate over files to find type="flashcards"
        files = DataManager.list_files(username, folder_id)
        for f in files:
            if f.get("type") == "flashcards":
                return f.get("content", [])
        return []

    @staticmethod
    def update_card_progress(username, folder_id, card_id, new_box, next_review_date):
        """Updates a single card's progress (Leitner System)."""
        cards = DataManager.load_flashcards(username, folder_id)
        updated = False
        
        for card in cards:
            if card["id"] == card_id:
                card["box"] = new_box
                card["next_review"] = next_review_date
                card["last_reviewed"] = datetime.now().isoformat()
                updated = True
                break
        
        if updated:
            DataManager.save_flashcards(username, folder_id, cards)
            return True
        return False

    # --- NEW: Sharing Features (Community/Collab) ---
    
    @staticmethod
    def share_item(content, type, username, folder_id):
        """Creates a shareable link/code for a Plan or Summary."""
        share_id = f"PLAN-{uuid.uuid4().hex[:6].upper()}"
        
        item_data = {
            "id": share_id,
            "type": type,
            "content": content,
            "author": username,
            "created_at": datetime.now().isoformat(),
            "folder_id": folder_id
        }
        
        db = DataManager._init_firestore()
        if db:
            # CLOUD MODE
            db.collection("shared_items").document(share_id).set(item_data)
        else:
            # LOCAL MODE (Simulated Central Store)
            shared_file = os.path.join(DATA_DIR, "shared_items.json")
            all_shared = {}
            if os.path.exists(shared_file):
                try:
                    with open(shared_file, "r", encoding="utf-8") as f:
                        all_shared = json.load(f)
                except: pass
            
            all_shared[share_id] = item_data
            
            with open(shared_file, "w", encoding="utf-8") as f:
                json.dump(all_shared, f, ensure_ascii=False, indent=4)
                
        return share_id

    @staticmethod
    def get_shared_item(share_id):
        """Retrieves a shared item by ID."""
        db = DataManager._init_firestore()
        if db:
            # CLOUD MODE
            doc = db.collection("shared_items").document(share_id).get()
            if doc.exists:
                return doc.to_dict()
            return None
        else:
            # LOCAL MODE
            shared_file = os.path.join(DATA_DIR, "shared_items.json")
            if os.path.exists(shared_file):
                try:
                    with open(shared_file, "r", encoding="utf-8") as f:
                        all_shared = json.load(f)
                        return all_shared.get(share_id)
                except: return None
            return None

    @staticmethod
    def import_shared_plan(share_id, target_username):
        """Imports a shared Plan as a new project/folder."""
        item = DataManager.get_shared_item(share_id)
        if not item or item.get("type") != "plan":
            return None, "Ungültiger Code oder kein Lernplan."
            
        # Create Folder
        folder_name = f"Import: {item.get('content', [{}])[0].get('topic', 'Lernplan')}"
        folder = DataManager.create_folder(folder_name, target_username)
        
        # Save Plan
        DataManager.save_plan(item["content"], target_username, folder["id"])
        
        return folder, "Erfolgreich importiert!"
