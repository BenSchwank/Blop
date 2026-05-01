import json
import os
import shutil
import hashlib
from datetime import datetime, timedelta
import uuid
import tempfile
from typing import Optional, List, Dict, Any

# Supabase Imports
try:
    from supabase import create_client, Client
except ImportError:
    pass

DATA_DIR = "user_data"

class DataManager:
    _supabase: Client = None
    _supabase_signing: Client = None

    @staticmethod
    def _init_supabase_signing() -> Optional[Client]:
        """Client für Storage-Signierung; bevorzugt SUPABASE_SERVICE_ROLE_KEY (Sign-API braucht oft Service-Role)."""
        if DataManager._supabase_signing is not None:
            return DataManager._supabase_signing
        url: str = os.environ.get("SUPABASE_URL") or ""
        key: str = (
            os.environ.get("SUPABASE_SERVICE_ROLE_KEY")
            or os.environ.get("SUPABASE_KEY")
            or ""
        ).strip()
        if not url or not key:
            return None
        try:
            DataManager._supabase_signing = create_client(url, key)
            return DataManager._supabase_signing
        except Exception as e:
            print(f"Supabase signing client init: {e}")
            return None

    @staticmethod
    def _sanitize_filename(filename: str) -> str:
        """Replace special characters in filenames so Supabase Storage accepts the key."""
        import re as _re
        unicode_map = {
            'ä': 'ae', 'ö': 'oe', 'ü': 'ue',
            'Ä': 'Ae', 'Ö': 'Oe', 'Ü': 'Ue',
            'ß': 'ss',
        }
        for char, repl in unicode_map.items():
            filename = filename.replace(char, repl)
        filename = filename.replace(' ', '_')
        filename = _re.sub(r'[^\w.\-]', '', filename)
        return filename

    @staticmethod
    def _normalize_bucket_object_path(path: str, bucket_id: str) -> str:
        p = (path or "").strip().lstrip("/")
        prefix = f"{bucket_id}/"
        if p.startswith(prefix):
            p = p[len(prefix) :]
        return p.strip().lstrip("/")

    @staticmethod
    def _parse_iso_datetime(value: Optional[str]) -> Optional[datetime]:
        if not value:
            return None
        try:
            return datetime.fromisoformat(str(value).replace("Z", "+00:00"))
        except Exception:
            return None

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
    def verify_share_tables_postgrest() -> Dict[str, Any]:
        """
        Lightweight PostgREST/Supabase table presence check for share tables.

        This intentionally does a tiny `select` with `count=exact` and `limit=1` so we hit PostgREST
        without pulling row payloads. Missing tables surface as PGRST205 in the client error string.
        """
        db = DataManager._init_supabase()
        if not db:
            return {"ok": False, "error": "Supabase nicht konfiguriert (SUPABASE_URL/SUPABASE_KEY fehlen)."}

        def _probe(table: str) -> Dict[str, Any]:
            try:
                db.table(table).select("id", count="exact").limit(1).execute()
                return {"ok": True, "table": table}
            except Exception as e:
                return {"ok": False, "table": table, "error": str(e)}

        share_requests = _probe("share_requests")
        share_links = _probe("share_links")
        ok = bool(share_requests.get("ok")) and bool(share_links.get("ok"))
        return {"ok": ok, "share_requests": share_requests, "share_links": share_links}

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

        safe_filename = DataManager._sanitize_filename(filename)
        path = f"{username}/{folder_id}/{safe_filename}"
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

        safe_filename = DataManager._sanitize_filename(filename)
        path = f"{username}/{folder_id}/audio/{safe_filename}"
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
        safe_filename = DataManager._sanitize_filename(filename)
        path = f"{username}/{folder_id}/video/{safe_filename}"
        try:
            db.storage.from_("blop_documents").upload(
                path, file_data, {"upsert": "true", "content-type": "video/mp4"}
            )
        except Exception as e:
            blob = (repr(e) + " " + str(e)).lower()
            if (
                "413" in blob
                or "payload too large" in blob
                or "too large" in blob
                or "maximum allowed" in blob
                or "exceeded" in blob
            ):
                raise RuntimeError(
                    "Die Videodatei ist zu groß für den Speicher (Supabase-Uploadlimit, z. B. 50 MB auf Free). "
                    "Wähle ein kürzeres Lernvideo (weniger Szenen oder kürzere Erzähltiefe), "
                    "oder setze auf dem Server stärkere Kompression: "
                    "LEARNING_VIDEO_CRF=30–32, LEARNING_VIDEO_MAXRATE=800k, optional "
                    "LEARNING_VIDEO_WIDTH=640 LEARNING_VIDEO_HEIGHT=360. "
                    "Alternativ: Supabase Pro oder anderes Object Storage (S3/R2) ohne 50-MB-Grenze."
                ) from e
            raise
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
    def create_signed_media_url(
        username: str,
        folder_id: str,
        file_id: str,
        kind: str,
        expires_in: int = 3600,
    ) -> Optional[str]:
        """Time-limited HTTPS URL to blop_documents object; bypasses app proxy for <video>/<audio>."""
        db = DataManager._init_supabase()
        if not db or not file_id:
            return None
        media_type = (kind or "").strip().lower()
        if media_type not in ("video", "audio"):
            return None
        bucket_id = "blop_documents"
        try:
            path = None
            for fid in (str(folder_id), folder_id):
                meta = (
                    db.table("files")
                    .select("file_url")
                    .eq("id", file_id)
                    .eq("folder_id", fid)
                    .eq("username", username)
                    .eq("type", media_type)
                    .limit(1)
                    .execute()
                )
                if meta.data:
                    path = meta.data[0].get("file_url")
                    break
            if not path or not isinstance(path, str):
                print("create_signed_media_url: keine file_url in DB")
                return None
            object_path = DataManager._normalize_bucket_object_path(path, bucket_id)
            if not object_path:
                return None
            ttl = max(60, min(int(expires_in or 3600), 7200))
            sign_db = DataManager._init_supabase_signing() or db
            res = sign_db.storage.from_(bucket_id).create_signed_url(object_path, ttl)
            url = None
            if isinstance(res, dict):
                url = res.get("signedURL") or res.get("signed_url")
                inner = res.get("data")
                if not url and isinstance(inner, dict):
                    url = inner.get("signedUrl") or inner.get("signedURL")
            else:
                url = getattr(res, "signed_url", None) or getattr(res, "signedURL", None)
            if isinstance(url, str) and url.startswith("http"):
                return url
            print(f"create_signed_media_url: unerwartete Signatur-Antwort: {type(res)!r}")
        except Exception as e:
            print(f"create_signed_media_url: {e}")
        return None

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
    def _copy_shared_file_into_folder(file_row: Dict[str, Any], target_username: str, target_folder_id: str) -> Dict[str, Any]:
        db = DataManager._init_supabase()
        if not db:
            raise RuntimeError("Supabase DB nicht initialisiert")

        source_name = str(file_row.get("name") or "Freigegebene Datei")
        source_type = str(file_row.get("type") or "unknown")
        file_content = file_row.get("content")
        source_file_url = file_row.get("file_url")
        now = datetime.now().isoformat()
        new_id = f"shared_{int(datetime.now().timestamp())}_{uuid.uuid4().hex[:8]}"

        payload: Dict[str, Any] = {
            "id": new_id,
            "username": target_username,
            "folder_id": str(target_folder_id),
            "name": source_name,
            "type": source_type,
            "content": file_content,
            "created_at": now,
            "updated_at": now,
        }

        if source_file_url:
            source_object_path = DataManager._normalize_bucket_object_path(source_file_url, "blop_documents")
            if source_object_path:
                try:
                    binary = db.storage.from_("blop_documents").download(source_object_path)
                    if hasattr(binary, "content"):
                        binary = binary.content
                    if hasattr(binary, "read"):
                        binary = binary.read()
                    if isinstance(binary, bytearray):
                        binary = bytes(binary)
                    if isinstance(binary, (bytes, memoryview)):
                        ext = os.path.splitext(source_name)[1]
                        safe_basename = DataManager._sanitize_filename(f"{uuid.uuid4().hex}{ext}")
                        target_object = f"{target_username}/{target_folder_id}/shared/{safe_basename}"
                        db.storage.from_("blop_documents").upload(
                            target_object, bytes(binary), {"upsert": "true"}
                        )
                        payload["file_url"] = target_object
                except Exception as e:
                    print(f"_copy_shared_file_into_folder storage copy: {e}")

        db.table("files").insert(payload).execute()
        return payload

    @staticmethod
    def create_share_request(source_username: str, target_username: str, file_id: str, message: str = "") -> Dict[str, Any]:
        db = DataManager._init_supabase()
        if not db:
            raise RuntimeError("Supabase DB nicht initialisiert")
        if not DataManager.user_exists(target_username):
            raise ValueError("Zielnutzer nicht gefunden")

        file_res = (
            db.table("files")
            .select("id,name,type,folder_id,username")
            .eq("id", str(file_id))
            .eq("username", source_username)
            .limit(1)
            .execute()
        )
        if not file_res.data:
            raise ValueError("Datei wurde nicht gefunden")

        request_row = {
            "id": f"share_req_{int(datetime.now().timestamp())}_{uuid.uuid4().hex[:8]}",
            "sender_username": source_username,
            "target_username": target_username,
            "file_id": str(file_id),
            "status": "pending",
            "message": (message or "").strip(),
            "created_at": datetime.now().isoformat(),
        }
        db.table("share_requests").insert(request_row).execute()
        return request_row

    @staticmethod
    def create_share_link(source_username: str, file_id: str, expires_in_days: int = 7, max_uses: int = 1) -> Dict[str, Any]:
        db = DataManager._init_supabase()
        if not db:
            raise RuntimeError("Supabase DB nicht initialisiert")

        file_res = (
            db.table("files")
            .select("id,name,type,folder_id,username")
            .eq("id", str(file_id))
            .eq("username", source_username)
            .limit(1)
            .execute()
        )
        if not file_res.data:
            raise ValueError("Datei wurde nicht gefunden")

        token = f"{uuid.uuid4().hex}{uuid.uuid4().hex}"
        token_hash = hashlib.sha256(token.encode("utf-8")).hexdigest()
        ttl_days = max(1, min(int(expires_in_days or 7), 30))
        allowed_uses = max(1, min(int(max_uses or 1), 100))

        link_row = {
            "id": f"share_link_{int(datetime.now().timestamp())}_{uuid.uuid4().hex[:8]}",
            "token_hash": token_hash,
            "sender_username": source_username,
            "file_id": str(file_id),
            "created_at": datetime.now().isoformat(),
            "expires_at": (datetime.now() + timedelta(days=ttl_days)).isoformat(),
            "max_uses": allowed_uses,
            "use_count": 0,
            "is_active": True,
        }
        db.table("share_links").insert(link_row).execute()
        return {**link_row, "token": token}

    @staticmethod
    def get_share_link_info(token: str) -> Optional[Dict[str, Any]]:
        db = DataManager._init_supabase()
        if not db or not token:
            return None
        token_hash = hashlib.sha256(token.encode("utf-8")).hexdigest()
        res = (
            db.table("share_links")
            .select("*")
            .eq("token_hash", token_hash)
            .eq("is_active", True)
            .limit(1)
            .execute()
        )
        if not res.data:
            return None
        row = res.data[0]
        expires_at = DataManager._parse_iso_datetime(row.get("expires_at"))
        if expires_at and expires_at < datetime.now(expires_at.tzinfo):
            return None
        if int(row.get("use_count") or 0) >= int(row.get("max_uses") or 1):
            return None

        file_res = (
            db.table("files")
            .select("id,name,type,folder_id,username")
            .eq("id", str(row.get("file_id")))
            .eq("username", str(row.get("sender_username")))
            .limit(1)
            .execute()
        )
        if not file_res.data:
            return None
        return {**row, "file": file_res.data[0]}

    @staticmethod
    def import_share_link(token: str, target_username: str, target_folder_id: str) -> Dict[str, Any]:
        db = DataManager._init_supabase()
        if not db:
            raise RuntimeError("Supabase DB nicht initialisiert")

        share = DataManager.get_share_link_info(token)
        if not share:
            raise ValueError("Link ist ungueltig oder abgelaufen")

        file_res = (
            db.table("files")
            .select("*")
            .eq("id", str(share.get("file_id")))
            .eq("username", str(share.get("sender_username")))
            .limit(1)
            .execute()
        )
        if not file_res.data:
            raise ValueError("Freigegebene Datei existiert nicht mehr")

        imported = DataManager._copy_shared_file_into_folder(file_res.data[0], target_username, target_folder_id)
        db.table("share_links").update(
            {
                "use_count": int(share.get("use_count") or 0) + 1,
                "last_used_at": datetime.now().isoformat(),
            }
        ).eq("id", share.get("id")).execute()
        return imported

    @staticmethod
    def accept_share_request(share_request_id: str, target_username: str, target_folder_id: str) -> Dict[str, Any]:
        db = DataManager._init_supabase()
        if not db:
            raise RuntimeError("Supabase DB nicht initialisiert")

        req_res = (
            db.table("share_requests")
            .select("*")
            .eq("id", share_request_id)
            .eq("target_username", target_username)
            .eq("status", "pending")
            .limit(1)
            .execute()
        )
        if not req_res.data:
            raise ValueError("Share-Request nicht gefunden")
        req = req_res.data[0]

        file_res = (
            db.table("files")
            .select("*")
            .eq("id", str(req.get("file_id")))
            .eq("username", str(req.get("sender_username")))
            .limit(1)
            .execute()
        )
        if not file_res.data:
            raise ValueError("Freigegebene Datei existiert nicht mehr")

        imported = DataManager._copy_shared_file_into_folder(file_res.data[0], target_username, target_folder_id)
        db.table("share_requests").update(
            {"status": "accepted", "accepted_at": datetime.now().isoformat()}
        ).eq("id", share_request_id).execute()
        return imported

    @staticmethod
    def list_incoming_share_requests(target_username: str) -> List[Dict[str, Any]]:
        db = DataManager._init_supabase()
        if not db:
            return []
        req_res = (
            db.table("share_requests")
            .select("*")
            .eq("target_username", target_username)
            .eq("status", "pending")
            .execute()
        )
        out: List[Dict[str, Any]] = []
        for row in req_res.data or []:
            file_res = (
                db.table("files")
                .select("id,name,type,folder_id,username")
                .eq("id", str(row.get("file_id")))
                .eq("username", str(row.get("sender_username")))
                .limit(1)
                .execute()
            )
            if not file_res.data:
                continue
            out.append({**row, "file": file_res.data[0]})
        return out

    @staticmethod
    def save_background_job(
        job_id: str,
        job_type: str,
        username: str,
        status: str,
        detail: Optional[str] = None,
        result: Optional[Dict[str, Any]] = None,
    ) -> bool:
        """Persistiert Lernvideo-/Podcast-Jobs (Multi-Instance / Restart). Tabelle ai_background_jobs anlegen (siehe supabase/migrations)."""
        db = DataManager._init_supabase()
        if not db:
            return False
        try:
            row: Dict[str, Any] = {
                "job_id": job_id,
                "job_type": job_type,
                "username": username,
                "status": status,
                "detail": detail,
                "result": result,
            }
            db.table("ai_background_jobs").upsert(row).execute()
            return True
        except Exception as e:
            print(f"save_background_job: {e}")
            return False

    @staticmethod
    def get_background_job(job_id: str, username: str) -> Optional[Dict[str, Any]]:
        db = DataManager._init_supabase()
        if not db:
            return None
        try:
            res = (
                db.table("ai_background_jobs")
                .select("*")
                .eq("job_id", job_id)
                .eq("username", username)
                .limit(1)
                .execute()
            )
            if res.data:
                return res.data[0]
        except Exception as e:
            print(f"get_background_job: {e}")
        return None
