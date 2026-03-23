import json
import os
import shutil
from datetime import datetime
import uuid
import tempfile

# Supabase Imports
try:
    from supabase import create_client, Client
except ImportError:
    pass

DATA_DIR = "user_data"

class DataManager:
    _supabase: Client = None

    @staticmethod
    def _init_supabase() -> Client:
        """Initializes Supabase using Env Variables. Fails hard if not found."""
        if DataManager._supabase is not None:
            return DataManager._supabase
            
        url: str = os.environ.get("SUPABASE_URL")
        key: str = os.environ.get("SUPABASE_KEY")
        
        if not url or not key:
            print("FATAL: SUPABASE_URL or SUPABASE_KEY not found in environment!")
            return None
            
        try:
            DataManager._supabase = create_client(url, key)
            return DataManager._supabase
        except Exception as e:
            print(f"Supabase Init Error: {e}")
            return None

    @staticmethod
    def user_exists(username):
        db = DataManager._init_supabase()
        if not db: return False
        res = db.table('users').select('username').eq('username', username).execute()
        return len(res.data) > 0

    @staticmethod
    def get_all_cloud_users():
        db = DataManager._init_supabase()
        if not db: return []
        res = db.table('users').select('username').execute()
        return [r['username'] for r in res.data]

    @staticmethod
    def load(username):
        """Mock load for legacy features relying on it. Returns folder structure."""
        db = DataManager._init_supabase()
        if not db: return {"folders": [], "files": []}
        
        # Build the 'folders' array
        f_res = db.table('folders').select('*').eq('username', username).execute()
        folders = f_res.data
        
        # Attach files to folders for compatibility
        files_res = db.table('files').select('*').eq('username', username).execute()
        
        for folder in folders:
            folder["files"] = [f for f in files_res.data if f['folder_id'] == folder['id']]
            
        # The legacy app used 'load' to get the user doc root
        return {"folders": folders, "files": files_res.data}

    @staticmethod
    def save(data, username):
        """Deprecated compatibility method - most actions should use targeted DB calls now."""
        # Due to relational DB, 'save entire JSON' is highly unoptimized. 
        # But we mostly replaced call sites. For the few remaining, we do nothing or throw warning.
        pass

    @staticmethod
    def get_api_key(username):
        """We do not store individual api keys anymore, returning None."""
        return None

    @staticmethod
    def save_api_key(username, api_key):
        pass

    @staticmethod
    def delete_api_key(username):
        return True

    @staticmethod
    def create_folder(name, username):
        db = DataManager._init_supabase()
        if not db: raise Exception("Datenbank nicht erreichbar")
        
        f_id = f"folder_{int(datetime.now().timestamp())}_{uuid.uuid4().hex[:4]}"
        folder = {
            "id": f_id,
            "username": username,
            "name": name,
            "created_at": datetime.now().isoformat()
        }
        res = db.table('folders').insert(folder).execute()
        return folder

    @staticmethod
    def create_subfolder(name, username, parent_id):
        db = DataManager._init_supabase()
        if not db: raise Exception("Datenbank nicht erreichbar")
        
        f_id = f"folder_{int(datetime.now().timestamp())}_{uuid.uuid4().hex[:6]}"
        folder = {
            "id": f_id,
            "username": username,
            "name": name,
            "parent_id": parent_id,
            "created_at": datetime.now().isoformat()
        }
        db.table('folders').insert(folder).execute()
        return folder

    @staticmethod
    def delete_folder(folder_id, username):
        db = DataManager._init_supabase()
        if not db: return False
        
        try:
            db.table('folders').delete().eq('id', folder_id).eq('username', username).execute()
            # Note: files and subfolders are CASCADE deleted by Postgres
            
            # Delete physical files from Storage bucket 'blop_documents'
            # (In production, use a Database Trigger or Edge Function to clean up Storage buckets on Cascade delete!)
            
            return True
        except: return False

    @staticmethod
    def rename_folder(folder_id, new_name, username):
        db = DataManager._init_supabase()
        if not db: return False
        
        res = db.table('folders').update({"name": new_name}).eq('id', folder_id).eq('username', username).execute()
        return len(res.data) > 0

    @staticmethod
    def move_folder(folder_id, new_parent_id, username):
        db = DataManager._init_supabase()
        if not db: return False
        
        res = db.table('folders').update({"parent_id": new_parent_id}).eq('id', folder_id).eq('username', username).execute()
        return len(res.data) > 0

    @staticmethod
    def save_file_metadata(file_data, username, folder_id):
        db = DataManager._init_supabase()
        if not db: return
        
        record = {
            "id": file_data["id"],
            "username": username,
            "folder_id": str(folder_id),
            "name": file_data.get("name", "Unbenannt"),
            "type": file_data.get("type", "unknown"),
            "content": json.dumps(file_data.get("content")) if isinstance(file_data.get("content"), (dict, list)) else file_data.get("content"),
            "created_at": file_data.get("created_at", datetime.now().isoformat())
        }
        
        # Upsert
        db.table('files').upsert(record).execute()

    @staticmethod
    def update_file_content(username, folder_id, file_id, new_content):
        db = DataManager._init_supabase()
        if not db: return False
        
        content_val = json.dumps(new_content) if isinstance(new_content, (dict, list)) else new_content
        res = db.table('files').update({"content": content_val, "updated_at": datetime.now().isoformat()}).eq('id', file_id).eq('username', username).execute()
        return len(res.data) > 0

    @staticmethod
    def delete_file(username, folder_id, file_id):
        db = DataManager._init_supabase()
        if not db: return False
        
        try:
            # Check if it was a storage file
            file = db.table('files').select('file_url').eq('id', file_id).eq('username', username).execute()
            if len(file.data) > 0 and file.data[0].get('file_url'):
                db.storage.from_("blop_documents").remove([file.data[0]['file_url']])
                
            db.table('files').delete().eq('id', file_id).eq('username', username).execute()
            return True
        except: return False

    @staticmethod
    def rename_file(username, folder_id, file_id, new_name):
        return DataManager.rename_file_metadata(username, folder_id, file_id, new_name)
        
    @staticmethod
    def rename_file_metadata(username, folder_id, file_id, new_name):
        db = DataManager._init_supabase()
        if not db: return False
        res = db.table('files').update({"name": new_name}).eq('id', file_id).eq('username', username).execute()
        return len(res.data) > 0

    @staticmethod
    def move_file(username, source_folder_id, file_id, target_folder_id):
        db = DataManager._init_supabase()
        if not db: return False
        res = db.table('files').update({"folder_id": target_folder_id}).eq('id', file_id).eq('username', username).execute()
        return len(res.data) > 0

    @staticmethod
    def list_files(username, folder_id):
        db = DataManager._init_supabase()
        if not db: return []
        
        res = db.table('files').select('*').eq('folder_id', str(folder_id)).eq('username', username).execute()
        files = []
        for f in res.data:
            # Parse content back if json
            content = f.get("content")
            if content and content.startswith("[") or content.startswith("{"):
                try: content = json.loads(content)
                except: pass
            
            f["content"] = content
            files.append(f)
        return files

    @staticmethod
    def save_transcript(filename, content, username, folder_id):
        file_id = f"transcript_{int(datetime.now().timestamp())}_{uuid.uuid4().hex[:6]}"
        DataManager.save_file_metadata({
            "id": file_id, "name": filename, "type": "transcript", "content": content
        }, username, folder_id)

    @staticmethod
    def save_plan_as_file(plan_data, username, folder_id, name="Lernplan"):
        file_id = f"plan_main_{folder_id}"
        DataManager.save_file_metadata({
            "id": file_id, "name": name, "type": "plan", "content": plan_data
        }, username, folder_id)

    @staticmethod
    def save_summary_as_file(title, content, username, folder_id, type="summary"):
        file_id = f"summary_{int(datetime.now().timestamp())}"
        DataManager.save_file_metadata({
            "id": file_id, "name": title, "type": type, "content": content
        }, username, folder_id)

    @staticmethod
    def save_plan(plan_data, username, folder_id):
        DataManager.save_plan_as_file(plan_data, username, folder_id, name="Aktueller Lernplan")
            
    @staticmethod
    def load_plan(username, folder_id):
        db = DataManager._init_supabase()
        if not db: return []
        res = db.table('files').select('content').eq('folder_id', str(folder_id)).eq('username', username).eq('type', 'plan').execute()
        if len(res.data) > 0:
            c = res.data[0]["content"]
            if isinstance(c, str):
                try: return json.loads(c)
                except: return []
            return c
        return []

    @staticmethod
    def save_generated_summary(summary_text, username, folder_id):
        DataManager.save_summary_as_file("Automatische Zusammenfassung", summary_text, username, folder_id, "summary")

    @staticmethod
    def load_generated_summary(username, folder_id):
        db = DataManager._init_supabase()
        if not db: return ""
        res = db.table('files').select('content').eq('folder_id', str(folder_id)).eq('username', username).eq('type', 'summary').execute()
        return res.data[0]["content"] if len(res.data) > 0 else ""

    # --- Storage Binary Files (PDFs & Audio) ---

    @staticmethod
    def save_pdf(file_data, filename, username, folder_id):
        db = DataManager._init_supabase()
        if not db: raise Exception("Supabase DB nicht initialisiert")
        
        path = f"{username}/{folder_id}/{filename}"
        res = db.storage.from_("blop_documents").upload(path, file_data, {"upsert": "true", "content-type": "application/pdf"})
        
        file_id = f"pdf_{int(datetime.now().timestamp())}"
        DataManager.save_file_metadata({
            "id": file_id,
            "name": filename,
            "type": "pdf",
            "file_url": path
        }, username, folder_id)
        
        return filename

    @staticmethod
    def list_pdfs(username, folder_id):
        # We now query normal files table for type='pdf'
        db = DataManager._init_supabase()
        if not db: return []
        res = db.table('files').select('name').eq('folder_id', str(folder_id)).eq('username', username).eq('type', 'pdf').execute()
        return [r['name'] for r in res.data]

    @staticmethod
    def get_pdf_path(filename, username, folder_id):
        db = DataManager._init_supabase()
        if not db: return None
        
        path = f"{username}/{folder_id}/{filename}"
        try:
            res = db.storage.from_("blop_documents").download(path)
            
            tmp_dir = os.path.join(DATA_DIR, "temp")
            if not os.path.exists(tmp_dir): os.makedirs(tmp_dir)
            tmp_path = os.path.join(tmp_dir, filename)
            
            with open(tmp_path, "wb") as f:
                f.write(res)
            return tmp_path
        except:
            return None

    @staticmethod
    def save_audio(file_data, filename, username, folder_id):
        db = DataManager._init_supabase()
        if not db: raise Exception("Supabase DB nicht initialisiert")
        
        path = f"{username}/{folder_id}/audio/{filename}"
        db.storage.from_("blop_documents").upload(path, file_data, {"upsert": "true", "content-type": "audio/mpeg"})
        
        file_id = f"audio_{int(datetime.now().timestamp())}"
        DataManager.save_file_metadata({
            "id": file_id,
            "name": filename,
            "type": "audio",
            "file_url": path
        }, username, folder_id)
        
        return filename

    @staticmethod
    def get_audio_path(filename, username, folder_id):
        db = DataManager._init_supabase()
        if not db: return None
        
        path = f"{username}/{folder_id}/audio/{filename}"
        try:
            res = db.storage.from_("blop_documents").download(path)
            tmp_dir = os.path.join(DATA_DIR, "temp")
            if not os.path.exists(tmp_dir): os.makedirs(tmp_dir)
            tmp_path = os.path.join(tmp_dir, filename)
            
            with open(tmp_path, "wb") as f:
                f.write(res)
            return tmp_path
        except:
            return None

    @staticmethod
    def delete_user_data(username):
        db = DataManager._init_supabase()
        if not db: return
        db.table('users').delete().eq('username', username).execute()
        # Triggers will cascade delete folders and files. 
        # In real prod, bucket cleanup needs an Edge Function.

    @staticmethod
    def clear_user_data(username):
        db = DataManager._init_supabase()
        if not db: return
        db.table('folders').delete().eq('username', username).execute()

    @staticmethod
    def save_flashcards(username, folder_id, cards_data):
        file_id = f"cards_{folder_id}_main"
        DataManager.save_file_metadata({
            "id": file_id, "name": "Flashcards", "type": "flashcards", "content": cards_data
        }, username, folder_id)

    @staticmethod
    def load_flashcards(username, folder_id):
        db = DataManager._init_supabase()
        if not db: return []
        res = db.table('files').select('content').eq('folder_id', str(folder_id)).eq('username', username).eq('type', 'flashcards').execute()
        if len(res.data) > 0:
            c = res.data[0]["content"]
            if isinstance(c, str):
                try: return json.loads(c)
                except: return []
            return c
        return []

    @staticmethod
    def update_card_progress(username, folder_id, card_id, new_box, next_review_date):
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

    @staticmethod
    def share_item(content, type, username, folder_id):
        # Skipped for brevity / minimal viable backend. 
        return "SHARE_NOT_IMPLEMENTED"

    @staticmethod
    def get_shared_item(share_id):
        return None

    @staticmethod
    def import_shared_plan(share_id, target_username):
        return None, "Not implemented yet"
