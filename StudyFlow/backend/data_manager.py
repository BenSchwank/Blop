import json
import os
import shutil
from datetime import datetime
import uuid
import tempfile
from typing import Optional, List

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
    def get_ai_context_file_ids(username: str, folder_id: str) -> Optional[List[str]]:
        """Returns None or empty to mean 'all files'. Non-empty list restricts AI context to those file ids."""
        db = DataManager._init_supabase()
        if not db:
            return None
        try:
            res = (
                db.table("folders")
                .select("ai_context_file_ids")
                .eq("id", str(folder_id))
                .eq("username", username)
                .execute()
            )
            if not res.data:
                return None
            val = res.data[0].get("ai_context_file_ids")
            if val is None:
                return None
            if isinstance(val, str):
                try:
                    val = json.loads(val)
                except Exception:
                    return None
            if not isinstance(val, list):
                return None
            out = [str(x) for x in val if x is not None and str(x).strip()]
            return out if len(out) > 0 else None
        except Exception as e:
            print(f"get_ai_context_file_ids: {e}")
            return None

    @staticmethod
    def set_ai_context_file_ids(username: str, folder_id: str, file_ids: Optional[List[str]]) -> bool:
        """Pass None or [] to clear filter (all materials)."""
        db = DataManager._init_supabase()
        if not db:
            return False
        payload = None
        if file_ids is not None and len(file_ids) > 0:
            payload = [str(x) for x in file_ids]
        try:
            res = (
                db.table("folders")
                .update({"ai_context_file_ids": payload})
                .eq("id", str(folder_id))
                .eq("username", username)
                .execute()
            )
            return len(res.data) > 0
        except Exception as e:
            print(f"set_ai_context_file_ids: {e}")
            return False

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
            "created_at": file_data.get("created_at", datetime.now().isoformat()),
        }
        if file_data.get("file_url") is not None:
            record["file_url"] = file_data["file_url"]
        
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
    def get_folder_name(username, folder_id) -> Optional[str]:
        """Ordner-Anzeigename für E-Mails / UI; None wenn nicht gefunden."""
        db = DataManager._init_supabase()
        if not db:
            return None
        try:
            res = (
                db.table("folders")
                .select("name")
                .eq("id", str(folder_id))
                .eq("username", username)
                .execute()
            )
            if res.data and len(res.data) > 0:
                return res.data[0].get("name") or None
        except Exception:
            pass
        return None

    @staticmethod
    def list_files(username, folder_id):
        db = DataManager._init_supabase()
        if not db: return []
        
        res = db.table('files').select('*').eq('folder_id', str(folder_id)).eq('username', username).execute()
        files = []
        for f in res.data:
            # Parse content back if json
            content = f.get("content")
            if content and (content.startswith("[") or content.startswith("{")):
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
    def get_pdf_path(filename, username, folder_id, file_id=None):
        db = DataManager._init_supabase()
        if not db: return None

        path = None
        if file_id:
            try:
                meta = (
                    db.table('files')
                    .select('file_url,name')
                    .eq('id', file_id)
                    .eq('folder_id', str(folder_id))
                    .eq('username', username)
                    .eq('type', 'pdf')
                    .limit(1)
                    .execute()
                )
                if meta.data and len(meta.data) > 0:
                    path = meta.data[0].get('file_url')
                    if not filename:
                        filename = meta.data[0].get('name') or "dokument.pdf"
            except Exception:
                path = None

        if not path and filename:
            path = f"{username}/{folder_id}/{filename}"
        if not path:
            return None

        try:
            res = db.storage.from_("blop_documents").download(path)
            
            tmp_dir = os.path.join(DATA_DIR, "temp")
            if not os.path.exists(tmp_dir): os.makedirs(tmp_dir)
            safe_filename = os.path.basename(filename or "dokument.pdf")
            tmp_path = os.path.join(tmp_dir, safe_filename)
            
            with open(tmp_path, "wb") as f:
                f.write(res)
            return tmp_path
        except:
            return None

    @staticmethod
    def get_pdf_bytes(filename, username, folder_id, file_id=None):
        db = DataManager._init_supabase()
        if not db:
            return None, None

        path = None
        resolved_name = filename or None
        if file_id:
            try:
                meta = (
                    db.table('files')
                    .select('file_url,name')
                    .eq('id', file_id)
                    .eq('folder_id', str(folder_id))
                    .eq('username', username)
                    .eq('type', 'pdf')
                    .limit(1)
                    .execute()
                )
                if meta.data and len(meta.data) > 0:
                    path = meta.data[0].get('file_url')
                    if not resolved_name:
                        resolved_name = meta.data[0].get('name') or "dokument.pdf"
            except Exception:
                path = None

        if not path and filename:
            path = f"{username}/{folder_id}/{filename}"
        if not path:
            return None, None

        try:
            payload = db.storage.from_("blop_documents").download(path)
            # Supabase SDK versions may return raw bytes or a response-like object.
            if hasattr(payload, "content"):
                payload = payload.content
            if hasattr(payload, "read"):
                payload = payload.read()
            if isinstance(payload, bytearray):
                payload = bytes(payload)
            if not isinstance(payload, (bytes, memoryview)):
                return None, None
            return bytes(payload), (resolved_name or os.path.basename(path) or "dokument.pdf")
        except Exception:
            return None, None

    @staticmethod
    def save_audio(file_data, filename, username, folder_id):
        db = DataManager._init_supabase()
        if not db: raise Exception("Supabase DB nicht initialisiert")
        
        path = f"{username}/{folder_id}/audio/{filename}"
        db.storage.from_("blop_documents").upload(path, file_data, {"upsert": "true", "content-type": "audio/mpeg"})
        
        file_id = f"audio_{int(datetime.now().timestamp())}_{uuid.uuid4().hex[:8]}"
        DataManager.save_file_metadata({
            "id": file_id,
            "name": filename,
            "type": "audio",
            "file_url": path
        }, username, folder_id)
        
        return file_id

    @staticmethod
    def save_video(file_data: bytes, filename: str, username: str, folder_id: str, display_name: Optional[str] = None) -> str:
        db = DataManager._init_supabase()
        if not db:
            raise Exception("Supabase DB nicht initialisiert")
        path = f"{username}/{folder_id}/video/{filename}"
        db.storage.from_("blop_documents").upload(
            path, file_data, {"upsert": "true", "content-type": "video/mp4"}
        )
        file_id = f"video_{int(datetime.now().timestamp())}_{uuid.uuid4().hex[:8]}"
        DataManager.save_file_metadata(
            {
                "id": file_id,
                "name": display_name or filename,
                "type": "video",
                "file_url": path,
            },
            username,
            folder_id,
        )
        return file_id

    @staticmethod
    def get_video_bytes(filename: str, username: str, folder_id: str, file_id: Optional[str] = None):
        db = DataManager._init_supabase()
        if not db:
            return None, None
        path = None
        resolved_name = filename or None
        if file_id:
            try:
                meta = (
                    db.table("files")
                    .select("file_url,name")
                    .eq("id", file_id)
                    .eq("folder_id", str(folder_id))
                    .eq("username", username)
                    .eq("type", "video")
                    .limit(1)
                    .execute()
                )
                if meta.data and len(meta.data) > 0:
                    path = meta.data[0].get("file_url")
                    if not resolved_name:
                        resolved_name = meta.data[0].get("name") or "video.mp4"
            except Exception:
                path = None
        if not path and filename:
            path = f"{username}/{folder_id}/video/{filename}"
        if not path:
            return None, None
        try:
            payload = db.storage.from_("blop_documents").download(path)
            if hasattr(payload, "content"):
                payload = payload.content
            if hasattr(payload, "read"):
                payload = payload.read()
            if isinstance(payload, bytearray):
                payload = bytes(payload)
            if not isinstance(payload, (bytes, memoryview)):
                return None, None
            return bytes(payload), (resolved_name or os.path.basename(path) or "video.mp4")
        except Exception:
            return None, None

    @staticmethod
    def get_audio_bytes(
        filename: str, username: str, folder_id: str, file_id: Optional[str] = None
    ):
        """Load audio from storage; prefer file_id so renames (DB name vs object key) stay valid."""
        db = DataManager._init_supabase()
        if not db:
            return None, None
        path = None
        resolved_name = filename or None
        if file_id:
            try:
                meta = (
                    db.table("files")
                    .select("file_url,name")
                    .eq("id", file_id)
                    .eq("folder_id", str(folder_id))
                    .eq("username", username)
                    .eq("type", "audio")
                    .limit(1)
                    .execute()
                )
                if meta.data and len(meta.data) > 0:
                    path = meta.data[0].get("file_url")
                    if not resolved_name:
                        resolved_name = meta.data[0].get("name") or "audio.mp3"
            except Exception:
                path = None
        if not path and filename:
            path = f"{username}/{folder_id}/audio/{filename}"
        if not path:
            return None, None
        try:
            payload = db.storage.from_("blop_documents").download(path)
            if hasattr(payload, "content"):
                payload = payload.content
            if hasattr(payload, "read"):
                payload = payload.read()
            if isinstance(payload, bytearray):
                payload = bytes(payload)
            if not isinstance(payload, (bytes, memoryview)):
                return None, None
            base = os.path.basename(path) or "audio.mp3"
            return bytes(payload), (resolved_name or base)
        except Exception:
            return None, None

    @staticmethod
    def get_audio_path(filename, username, folder_id):
        db = DataManager._init_supabase()
        if not db:
            return None

        path = f"{username}/{folder_id}/audio/{filename}"
        try:
            res = db.storage.from_("blop_documents").download(path)
            tmp_dir = os.path.join(DATA_DIR, "temp")
            if not os.path.exists(tmp_dir):
                os.makedirs(tmp_dir)
            tmp_path = os.path.join(tmp_dir, filename)

            with open(tmp_path, "wb") as f:
                f.write(res)
            return tmp_path
        except Exception:
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
