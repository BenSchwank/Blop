from fastapi import FastAPI, HTTPException, Body, UploadFile, File, BackgroundTasks, Request
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse
from pydantic import BaseModel
from typing import Any, List, Optional
import uvicorn
import os
import json
from datetime import datetime
from pathlib import Path
import shutil
import tempfile
import threading
import time
import uuid

try:
    from dotenv import load_dotenv
    load_dotenv(Path(__file__).resolve().parent / ".env")
except ImportError:
    pass

# Import local modules
# Import local modules
from data_manager import DataManager
from auth_manager import AuthManager
from email_notify import try_notify_document_ready
from genai_warnings import suppress_known_google_warnings

suppress_known_google_warnings()
import google.generativeai as genai

# Configure GenAI (Global for main.py usage like upload_file)
api_key = os.environ.get("GOOGLE_API_KEY")
if api_key:
    genai.configure(api_key=api_key)

app = FastAPI(title="Blop Study Backend")

# CORS (Allow Frontend to connect)
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    # True + "*" ist im Browser ungültig; Medien von blop-study.com → Render würden ohne korrektes ACAO scheitern
    allow_credentials=False,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Lernvideo: lange ffmpeg/TTS-Pipeline — synchron würde Render/Proxy-Timeouts (502) auslösen
_LEARNING_VIDEO_JOBS: dict = {}
_LEARNING_VIDEO_JOBS_LOCK = threading.Lock()
_LEARNING_VIDEO_JOB_MAX = 256


def _learning_video_prune_jobs_locked() -> None:
    if len(_LEARNING_VIDEO_JOBS) < _LEARNING_VIDEO_JOB_MAX:
        return
    finished = [
        (jid, j.get("created_at", 0))
        for jid, j in _LEARNING_VIDEO_JOBS.items()
        if j.get("status") in ("done", "error")
    ]
    finished.sort(key=lambda x: x[1])
    for jid, _ in finished[: max(32, len(finished) // 2)]:
        _LEARNING_VIDEO_JOBS.pop(jid, None)


def _learning_video_job_fail(job_id: str, detail: str) -> None:
    with _LEARNING_VIDEO_JOBS_LOCK:
        if job_id in _LEARNING_VIDEO_JOBS:
            _LEARNING_VIDEO_JOBS[job_id]["status"] = "error"
            _LEARNING_VIDEO_JOBS[job_id]["detail"] = detail
            _LEARNING_VIDEO_JOBS[job_id]["finished_at"] = time.time()


def _normalize_learning_video_options(req: "LearningVideoRequest") -> dict:
    """Defaults: rich + stock; optional Ken-Burns motion and xfade (see slide_motion / slide_crossfade)."""
    ts = req.target_scenes
    if ts is None:
        ts = 6
    try:
        ts = int(ts)
    except (TypeError, ValueError):
        ts = 6
    ts = max(4, min(22, ts))
    depth = (req.narration_depth or "standard").strip().lower()
    if depth not in ("compact", "standard", "detailed", "deep"):
        depth = "standard"
    visual = (req.visual_style or "rich").strip().lower()
    if visual not in ("clean", "rich", "whiteboard"):
        visual = "rich"
    if req.use_stock_images is None:
        use_stock = True
    else:
        use_stock = bool(req.use_stock_images)
    if use_stock and not os.environ.get("PEXELS_API_KEY", "").strip():
        print("Lernvideo: Stock-Bilder gewünscht, aber PEXELS_API_KEY fehlt — Folien ohne Fotos.")
        use_stock = False
    voice = (req.tts_voice or "alloy").strip().lower()
    if req.slide_motion is not None:
        slide_motion = bool(req.slide_motion)
    elif os.environ.get("LEARNING_VIDEO_KEN_BURNS", "").strip() == "0":
        slide_motion = False
    else:
        slide_motion = True
    if req.slide_crossfade is not None:
        slide_crossfade = bool(req.slide_crossfade)
    else:
        slide_crossfade = os.environ.get("LEARNING_VIDEO_XFADE", "0").strip() == "1"
    return {
        "target_scenes": ts,
        "narration_depth": depth,
        "visual_style": visual,
        "use_stock_images": use_stock,
        "tts_voice": voice,
        "slide_motion": slide_motion,
        "slide_crossfade": slide_crossfade,
    }


def _learning_video_job_worker(
    job_id: str,
    username: str,
    folder_id: str,
    model_preference: Optional[str],
    lv_opts: Optional[dict] = None,
) -> None:
    from ai_service import AIService
    from media_pipeline import (
        VIDEO_H,
        VIDEO_W,
        ffmpeg_slideshow_with_audio,
        openai_tts_speech_mp3,
        render_scene_slides_weighted,
    )

    opts = lv_opts or {}
    slide_root: Optional[str] = None
    work_tmp: Optional[str] = None
    try:
        _configure_genai()
        model_pref = resolve_model_preference(username, model_preference)
        context, debug_log = _get_folder_context(username, folder_id)
        if not context:
            _learning_video_job_fail(
                job_id,
                f"Kein Material gefunden. Debug: {'; '.join(debug_log)}",
            )
            return
        context = _truncate_for_learning_video_context(context)
        try:
            folder_files = DataManager.list_files(username, folder_id)
        except Exception:
            folder_files = []
        pdf_n = sum(1 for f in folder_files if f.get("type") == "pdf")
        text_n = sum(
            1
            for f in folder_files
            if f.get("type")
            in ("transcript", "summary", "repetition", "plan", "quiz", "flashcards")
        )
        other_n = max(0, len(folder_files) - pdf_n - text_n)
        material_summary = (
            f"Der Ordner enthält {pdf_n} PDF-Datei(en), {text_n} Text-/Lernquelle(n) und {other_n} weitere Einträge. "
            "Nutze die Szenen so, dass der Stoff breit und ausführlich abgedeckt wird — nicht nur oberflächliche Stichworte."
        )
        sb_result = AIService.generate_learning_video_storyboard(
            context,
            model_preference=model_pref,
            return_meta=True,
            storyboard_options={
                "target_scenes": opts.get("target_scenes", 6),
                "narration_depth": opts.get("narration_depth", "standard"),
                "include_image_queries": bool(opts.get("use_stock_images")),
                "material_summary": material_summary,
                "visual_style": opts.get("visual_style", "clean"),
            },
        )
        data = sb_result["data"]
        scenes = data.get("scenes") or []
        if not scenes:
            _learning_video_job_fail(job_id, "Storyboard ohne Szenen.")
            return
        opening = str(data.get("opening_narration") or "").strip()
        narr_parts: List[str] = []
        if opening:
            narr_parts.append(opening)
        narr_parts.extend(
            str(s.get("narration") or "").strip()
            for s in scenes
            if str(s.get("narration") or "").strip()
        )
        full_narration = "\n\n".join(narr_parts)
        if not full_narration.strip():
            full_narration = str(data.get("title") or "Kurzes Lernvideo zu deinem Material.")
        mp3_bytes = openai_tts_speech_mp3(full_narration, voice=opts.get("tts_voice") or "alloy")
        use_stock = bool(opts.get("use_stock_images"))
        vtitle = str(data.get("title") or "KI-Lernvideo")
        slide_scenes: List[dict] = [
            {
                "slide_kind": "title",
                "title": vtitle,
                "body": "KI-Lernvideo aus deinem Ordner",
            }
        ]
        n_content = len(scenes)
        for idx, s in enumerate(scenes):
            title = s.get("title") or ""
            body = s.get("body") or ""
            if isinstance(body, list):
                body = "\n".join(str(x) for x in body)
            item = {
                "title": str(title),
                "body": str(body),
                "chapter": f"{idx + 1}/{n_content}",
            }
            if use_stock:
                iq = str(s.get("image_query") or "").strip()
                if iq:
                    item["image_query"] = iq
            slide_scenes.append(item)
        img_weighted = render_scene_slides_weighted(
            slide_scenes,
            width=VIDEO_W,
            height=VIDEO_H,
            visual_style=opts.get("visual_style", "clean"),
            use_stock_images=use_stock,
        )
        if img_weighted:
            slide_root = os.path.dirname(img_weighted[0][0])
        work_tmp = tempfile.mkdtemp(prefix="blop_lv_")
        audio_path = os.path.join(work_tmp, "narration.mp3")
        with open(audio_path, "wb") as f:
            f.write(mp3_bytes)
        out_mp4 = os.path.join(work_tmp, "out.mp4")
        ffmpeg_slideshow_with_audio(
            img_weighted,
            audio_path,
            out_mp4,
            ken_burns=bool(opts.get("slide_motion", True)),
            crossfade=bool(opts.get("slide_crossfade", False)),
        )
        with open(out_mp4, "rb") as f:
            video_bytes = f.read()
        if not video_bytes:
            _learning_video_job_fail(job_id, "Lernvideo-Datei ist leer (ffmpeg/Encoding fehlgeschlagen).")
            return
        vfname = f"learning_video_{int(datetime.now().timestamp())}.mp4"
        file_id = DataManager.save_video(
            video_bytes, vfname, username, folder_id, display_name=vtitle
        )
        charge = deduct_tokens_by_usage(
            username,
            "learning_video",
            sb_result.get("used_model", model_pref or ""),
            sb_result.get("usage"),
        )
        try_notify_document_ready(username, folder_id, "Lernvideo")
        with _LEARNING_VIDEO_JOBS_LOCK:
            if job_id in _LEARNING_VIDEO_JOBS:
                _LEARNING_VIDEO_JOBS[job_id].update(
                    {
                        "status": "done",
                        "finished_at": time.time(),
                        "file_id": file_id,
                        "filename": vfname,
                        "title": vtitle,
                        "used_model": sb_result.get("used_model", model_pref or ""),
                        "usage": sb_result.get("usage", {}),
                        **charge,
                    }
                )
    except HTTPException as e:
        detail = e.detail
        if not isinstance(detail, str):
            detail = str(detail)
        _learning_video_job_fail(job_id, detail)
    except RuntimeError as e:
        _learning_video_job_fail(job_id, str(e))
    except Exception as e:
        print(f"Learning video error: {e}")
        _learning_video_job_fail(job_id, f"Lernvideo-Fehler: {str(e)}")
    finally:
        if slide_root:
            shutil.rmtree(slide_root, ignore_errors=True)
        if work_tmp:
            shutil.rmtree(work_tmp, ignore_errors=True)


# --- MODELS ---
class FolderCreate(BaseModel):
    name: str
    username: str


class RenameFolderRequest(BaseModel):
    name: str
    username: str


class FileModel(BaseModel):
    id: str
    name: str
    type: str # pdf, video, plan
    created_at: str

class LoginRequest(BaseModel):
    username: str
    password: str

class RegisterRequest(BaseModel):
    username: str
    email: str
    password: str

class ForgotPasswordRequest(BaseModel):
    email: str

class ResetPasswordConfirmRequest(BaseModel):
    access_token: str
    new_password: str

class ChatMessage(BaseModel):
    role: str
    content: str

class ChatRequest(BaseModel):
    username: str
    folder_id: str
    message: str
    history: List[ChatMessage]
    model_preference: Optional[str] = None
    active_file: Optional[dict] = None  # {id, name, type, content}

class GoogleVerifyRequest(BaseModel):
    token: str
    client_id: Optional[str] = None

@app.get("/api/search")
def global_search(username: str, q: str):
    """Searches through all folders and files for the given user"""
    if not username or not q:
        return []

    q_lower = q.lower()
    results = []
    
    # 1. Search Folders
    folders = DataManager.get_folders(username)
    for folder in folders:
        # Match folder name
        if q_lower in folder.get("name", "").lower():
            results.append({
                "type": "folder",
                "id": folder["id"],
                "name": folder["name"],
                "folder_id": folder["id"] # To navigate to it
            })
            
        # 2. Search Files inside this folder
        files = DataManager.get_files(username, folder["id"])
        for file in files:
            # We search by name or type
            file_name = file.get("name", "")
            file_type = file.get("type", "")
            if q_lower in file_name.lower() or q_lower in file_type.lower():
                results.append({
                    "type": "file",
                    "file_type": file_type,
                    "id": file["id"],
                    "name": file_name,
                    "folder_id": folder["id"],
                    "folder_name": folder.get("name", "Ordner")
                })
                
    return results

@app.get("/")
def read_root():
    return {"message": "Blop Study Backend Running", "version": "1.0.0"}

@app.get("/api/health")
@app.head("/api/health")
def health_check():
    """Schneller Liveness-Check für UptimeRobot/Render — ohne Netzwerk zu Supabase (vermeidet Timeouts/502 durch langsamen Ping)."""
    return {"status": "ok"}


@app.get("/api/health/ready")
def health_ready():
    """Optional: prüft Supabase-Konfiguration (kann langsamer sein; nicht als einziger Uptime-Ping nutzen)."""
    try:
        db = DataManager._init_supabase()
        return {"status": "ok", "database": "supabase" if db else "unconfigured"}
    except Exception as e:
        return {"status": "ok", "database": "error", "detail": str(e)[:200]}

@app.get("/api/ping")
def ping():
    return {"status": "pong", "message": "Backend is online!"}

# --- AUTH ENDPOINTS ---
@app.post("/api/auth/login")
def login(request: LoginRequest):
    """Login endpoint - returns session token"""
    success, result_or_error = AuthManager.login(request.username, request.password)
    if success:
        session_id = AuthManager.create_session(result_or_error)
        return {
            "success": True,
            "session_id": session_id,
            "username": result_or_error
        }
    raise HTTPException(status_code=401, detail=result_or_error)

@app.post("/api/auth/register")
def register(request: RegisterRequest):
    """Register new user via Email and Supabase Auth"""
    success, message = AuthManager.register(request.username, request.email, request.password)
    if success:
        return {"success": True, "message": message}
    raise HTTPException(status_code=400, detail=message)

@app.post("/api/auth/forgot-password")
def forgot_password(request: ForgotPasswordRequest):
    """Sends Supabase recovery email. Same response whether the address exists (no enumeration)."""
    AuthManager.request_password_reset(request.email)
    return {
        "success": True,
        "message": "Wenn diese E-Mail bei uns registriert ist, erhältst du gleich eine Nachricht mit einem Link zum Zurücksetzen.",
    }

@app.post("/api/auth/reset-password-confirm")
def reset_password_confirm(request: ResetPasswordConfirmRequest):
    """Completes recovery: GoTrue updates password; local users.password_hash is synced."""
    ok, msg = AuthManager.complete_password_reset(request.access_token, request.new_password)
    if ok:
        return {"success": True, "message": msg}
    raise HTTPException(status_code=400, detail=msg)

@app.post("/api/auth/logout")
def logout(session_id: str = Body(..., embed=True)):
    if not session_id:
         raise HTTPException(status_code=400, detail="Keine Session ID")
    AuthManager.logout_session(session_id)
    return {"message": "Logged out successfully"}

@app.post("/api/auth/google/verify")
def verify_google_oauth(req: GoogleVerifyRequest):
    try:
        from google.oauth2 import id_token
        from google.auth.transport import requests as google_requests
        import requests as py_requests
        import string
        import random
        import errno
        import time

        if not req.token or not req.token.strip():
            raise HTTPException(status_code=400, detail="Leerer Google Token")

        token = req.token.strip()

        def _fetch_userinfo_with_retry(access_token: str):
            last_exc = None
            for attempt in range(3):
                try:
                    resp = py_requests.get(
                        "https://www.googleapis.com/oauth2/v3/userinfo",
                        headers={"Authorization": f"Bearer {access_token}"},
                        timeout=10
                    )
                    return resp
                except OSError as oe:
                    # Transient socket/resource issue on host side (e.g. Errno 11).
                    if getattr(oe, "errno", None) == errno.EAGAIN and attempt < 2:
                        time.sleep(0.25 * (attempt + 1))
                        last_exc = oe
                        continue
                    raise
                except Exception as ex:
                    if attempt < 2:
                        time.sleep(0.25 * (attempt + 1))
                        last_exc = ex
                        continue
                    raise
            if last_exc:
                raise last_exc

        # Check if the token is a JWT (id_token) or a plain access_token.
        # JWTs start with 'eyJ'
        if token.startswith("eyJ") or token.startswith("ey"):
            idinfo = id_token.verify_oauth2_token(
                token,
                google_requests.Request()
            )
        else:
            # Verifying an OAuth Access Token by fetching the user profile
            resp = _fetch_userinfo_with_retry(token)
            if resp.status_code != 200:
                raise ValueError(f"Invalid Google Access Token. Status: {resp.status_code}, Response: {resp.text}")
            idinfo = resp.json()
        
        email = idinfo.get('email')
        if not email:
            raise HTTPException(status_code=400, detail="Keine Email in Google Token gefunden")

        # Generate a username from the email or use the email
        username = email.split('@')[0]
        name = idinfo.get('name', username)
        
        # Ensure user exists in our DB (lookup by e-mail first)
        user = AuthManager.get_user_by_email(email)
        if not user:
            # Maybe the username from email is taken?
            existing = AuthManager.get_user(username)
            if existing: # Append numeric suffix if username exists
                username = f"{username}_{random.randint(100,999)}"
            
            # Register automatically with generated password + real e-mail
            random_pw = ''.join(random.choices(string.ascii_letters + string.digits, k=32))
            ok, msg = AuthManager.register(username, email, random_pw)
            if not ok:
                # If the email already exists, resolve canonical user and continue.
                if "bereits registriert" in str(msg).lower():
                    existing_email_user = AuthManager.get_user_by_email(email)
                    if existing_email_user:
                        username = existing_email_user["username"]
                    else:
                        raise HTTPException(status_code=409, detail=f"Google-Login Konflikt: {msg}")
                else:
                    raise HTTPException(status_code=400, detail=f"Google-Registrierung fehlgeschlagen: {msg}")
        else:
            username = user['username']

        # Ensure we don't accidentally log into admin_ via Google unless they somehow have the admin email
        if username == "admin_":
            raise HTTPException(status_code=403, detail="Google Login für admin_ gesperrt")

        session_id = AuthManager.create_session(username)
        return {"session_id": session_id, "username": username}

    except ValueError as ve:
        raise HTTPException(status_code=400, detail=f"Ungültiges Google Auth Token: {str(ve)}")
    except HTTPException:
        raise
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Server Fehler: {str(e)}")

@app.get("/api/auth/validate")
def validate_session(session_id: str):
    """Validate session token"""
    username = AuthManager.validate_session(session_id)
    if username:
        return {"valid": True, "username": username}
    raise HTTPException(status_code=401, detail="Session ungültig oder abgelaufen")

class DeleteAccountRequest(BaseModel):
    username: str
    password: str

@app.delete("/api/auth/user")
def delete_account(request: DeleteAccountRequest):
    """Permanently deletes a user account."""
    # 1. Verify Password first (Security)
    if not AuthManager.login(request.username, request.password):
        raise HTTPException(status_code=401, detail="Falsches Passwort.")
        
    # 2. Delete
    if AuthManager.delete_user(request.username):
        return {"success": True, "message": "Account gelöscht."}
    raise HTTPException(status_code=500, detail="Fehler beim Löschen.")

# --- ADMIN ENDPOINTS ---
@app.get("/api/admin/users")
def get_all_users(admin_username: str):
    """Get all users (admin only)"""
    if admin_username != "admin_":
        raise HTTPException(status_code=403, detail="Nur für Admins")
    
    users = AuthManager.get_all_users()
    user_list = []
    
    for username, data in users.items():
        if username == "config":
            continue
            
        user_list.append({
            "username": username,
            "xp": data.get("xp", 0),
            "streak": data.get("streak_days", 0),
            "created_at": data.get("created_at", "Unknown"),
            "is_admin": data.get("is_admin", False)
        })
    
    return user_list

@app.get("/api/admin/leaderboard")
def get_leaderboard(limit: int = 10):
    """Get top users by XP"""
    return AuthManager.get_leaderboard_data(limit)

# --- FOLDER ENDPOINTS ---
@app.get("/api/folders")
def get_folders(username: str):
    """Returns list of root folders for a user."""
    data = DataManager.load(username)
    all_folders = data.get("folders", [])
    # Only return root folders (those without a parent_id)
    return [f for f in all_folders if not f.get("parent_id")]

@app.delete("/api/folders/{folder_id}")
def delete_folder(folder_id: str, username: str):
    """Deletes a folder."""
    if DataManager.delete_folder(folder_id, username):
        return {"status": "success", "message": "Ordner gelöscht"}
    raise HTTPException(status_code=404, detail="Ordner nicht gefunden")

@app.put("/api/folders/{folder_id}")
def rename_folder(folder_id: str, body: RenameFolderRequest):
    """Renames a folder."""
    if DataManager.rename_folder(folder_id, body.name, body.username):
        return {"status": "success", "folder": body.name}
    raise HTTPException(status_code=404, detail="Ordner nicht gefunden")

class MoveFolderRequest(BaseModel):
    new_parent_id: Optional[str] = None
    username: str

@app.put("/api/folders/{folder_id}/move")
def move_folder(folder_id: str, body: MoveFolderRequest):
    """Moves a folder to a new parent."""
    if DataManager.move_folder(folder_id, body.new_parent_id, body.username):
        return {"status": "success", "message": "Ordner verschoben"}
    raise HTTPException(status_code=404, detail="Ordner nicht gefunden")


class FolderAiContextRequest(BaseModel):
    username: str
    included_file_ids: Optional[List[str]] = None


@app.get("/api/folders/{folder_id}/ai-context")
def get_folder_ai_context(folder_id: str, username: str):
    """Returns which file IDs are included for AI; null means all files."""
    ids = DataManager.get_ai_context_file_ids(username, folder_id)
    return {"included_file_ids": ids}


@app.put("/api/folders/{folder_id}/ai-context")
def put_folder_ai_context(folder_id: str, body: FolderAiContextRequest):
    """Sets included file IDs for AI context; null, omitted, or [] clears to 'all materials'."""
    all_files = DataManager.list_files(body.username, folder_id)
    valid_ids = {f.get("id") for f in all_files if f.get("id")}
    if body.included_file_ids is None or len(body.included_file_ids) == 0:
        if not DataManager.set_ai_context_file_ids(body.username, folder_id, None):
            raise HTTPException(status_code=404, detail="Ordner nicht gefunden")
        return {"status": "success", "included_file_ids": None}
    for fid in body.included_file_ids:
        if fid not in valid_ids:
            raise HTTPException(status_code=400, detail=f"Unbekannte Datei-ID: {fid}")
    if not DataManager.set_ai_context_file_ids(body.username, folder_id, body.included_file_ids):
        raise HTTPException(status_code=404, detail="Ordner nicht gefunden")
    return {"status": "success", "included_file_ids": body.included_file_ids}


@app.post("/api/folders")
def create_folder(folder: FolderCreate):
    """Creates a new folder."""
    try:
        DataManager.create_folder(folder.name, folder.username)
        return {"status": "success", "folder": folder.name}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

class SubfolderCreate(BaseModel):
    name: str
    username: str

@app.post("/api/folders/{parent_id}/subfolders")
def create_subfolder(parent_id: str, body: SubfolderCreate):
    """Creates a subfolder inside a parent folder."""
    try:
        subfolder = DataManager.create_subfolder(body.name, body.username, parent_id)
        return {"status": "success", "subfolder": subfolder}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.get("/api/folders/{parent_id}/subfolders")
def get_subfolders(parent_id: str, username: str):
    """Returns subfolders inside a parent folder."""
    data = DataManager.load(username)
    folders = data.get("folders", [])
    return [f for f in folders if f.get("parent_id") == parent_id]

# --- FILE ENDPOINTS ---
class FileUpdateRequest(BaseModel):
    username: str
    folder_id: str
    file_id: str
    content: Any


class RenameFileRequest(BaseModel):
    username: str
    name: str


def _is_valid_flashcards_payload(content: Any) -> bool:
    if not isinstance(content, list):
        return False
    for row in content:
        if not isinstance(row, dict):
            return False
        if "front" not in row or "back" not in row:
            return False
        if row.get("front") is None or row.get("back") is None:
            return False
        if not isinstance(row.get("front"), str) or not isinstance(row.get("back"), str):
            return False
    return True

@app.put("/api/files/update")
def update_file(request: FileUpdateRequest):
    """Updates the content of an existing file."""
    if isinstance(request.content, list) and not _is_valid_flashcards_payload(request.content):
        raise HTTPException(status_code=400, detail="Ungültiges Flashcard-Format: erwartet Liste mit {front, back}.")
    if DataManager.update_file_content(request.username, request.folder_id, request.file_id, request.content):
        return {"status": "success", "message": "Datei gespeichert"}
    raise HTTPException(status_code=500, detail="Fehler beim Speichern der Datei")

@app.put("/api/files/{folder_id}/{file_id}/rename")
def rename_file(folder_id: str, file_id: str, body: RenameFileRequest):
    """Renames a generic file."""
    if DataManager.rename_file(body.username, folder_id, file_id, body.name):
        return {"status": "success", "file": body.name}
    raise HTTPException(status_code=404, detail="Datei nicht gefunden oder Format (z.B. PDF) nicht unterstützt.")

class MoveFileRequest(BaseModel):
    target_folder_id: str
    username: str

@app.put("/api/files/{folder_id}/{file_id}/move")
def move_file(folder_id: str, file_id: str, body: MoveFileRequest):
    """Moves a file to a different folder."""
    if DataManager.move_file(body.username, folder_id, file_id, body.target_folder_id):
        return {"status": "success", "message": "Datei verschoben"}
    raise HTTPException(status_code=404, detail="Fehler beim Verschieben der Datei")

@app.delete("/api/files/{file_id}")
def delete_file(file_id: str, username: str, folder_id: str):
    """Deletes a file from a folder."""
    if DataManager.delete_file(username, folder_id, file_id):
        return {"status": "success", "message": "Datei gelöscht"}
    raise HTTPException(status_code=404, detail="Fehler beim Löschen der Datei")

# --- UPLOAD & AI ENDPOINTS ---

class SubscriptionUpgradeRequest(BaseModel):
    username: str
    tier: str

class UserModelPreferenceRequest(BaseModel):
    username: str
    preferred_model: str = ""

@app.get("/api/user/{username}")
def get_user_info(username: str):
    """Returns user profile info including tokens and subscription tier."""
    user = AuthManager.get_user(username)
    if not user:
        raise HTTPException(status_code=404, detail="User not found")
        
    # Standardize output, default to "free" and set admin tokens to infinity visually
    tokens = user.get("tokens", 0)
    if user.get("is_admin", False):
         tokens = 999999
         
    return {
        "username": user.get("username"),
        "email": user.get("email", ""),
        "tokens": tokens,
        "subscription_tier": user.get("subscription_tier", "free"),
        "preferred_model": user.get("preferred_model", ""),
        "xp": user.get("xp", 0),
        "is_admin": user.get("is_admin", False)
    }

@app.put("/api/user/model-preference")
def update_user_model_preference(body: UserModelPreferenceRequest):
    user = AuthManager.get_user(body.username)
    if not user:
        raise HTTPException(status_code=404, detail="User not found")
    ok = AuthManager.set_preferred_model(body.username, body.preferred_model)
    if not ok:
        raise HTTPException(status_code=500, detail="Model preference could not be saved")
    return {"status": "success", "preferred_model": (body.preferred_model or "").strip()}

@app.post("/api/subscription/upgrade")
def upgrade_subscription(request: SubscriptionUpgradeRequest):
    """Mock endpoint to upgrade subscription and grant tokens."""
    tier_lower = request.tier.lower()
    
    # Define token grants per tier
    tier_tokens = {
        "basic": 1000,
        "pro": 5000,
        "premium": 15000
    }
    
    if tier_lower not in tier_tokens:
        raise HTTPException(status_code=400, detail="Ungültiges Abo-Modell (Verfügbar: basic, pro, premium)")
        
    db = AuthManager._get_db()
    if not db:
        raise HTTPException(status_code=500, detail="DB Error")
        
    user = AuthManager.get_user(request.username)
    if not user:
        raise HTTPException(status_code=404, detail="User not found")
        
    new_tokens = user.get("tokens", 0) + tier_tokens[tier_lower]
    
    # Update DB
    db.table('users').update({
        "subscription_tier": tier_lower,
        "tokens": new_tokens
    }).eq("username", request.username).execute()
    
    return {
        "status": "success", 
        "message": f"Erfolgreich auf {tier_lower.capitalize()} hochgestuft. {tier_tokens[tier_lower]} Tokens hinzugefügt!",
        "new_tokens": new_tokens,
        "subscription_tier": tier_lower
    }

MODEL_TOKEN_RATES_PER_1K = {
    "gemini-2.5-pro": {"in": 7.0, "out": 21.0},
    "gemini-2.0-pro-exp": {"in": 5.0, "out": 12.0},
    "gemini-1.5-pro": {"in": 3.5, "out": 10.0},
    "gemini-2.5-flash": {"in": 1.4, "out": 4.0},
    "gemini-2.0-flash": {"in": 1.4, "out": 4.0},
    "gemini-1.5-flash": {"in": 1.0, "out": 3.0},
    "gemini-2.5-flash-lite": {"in": 0.7, "out": 2.0},
    "gemini-2.0-flash-lite": {"in": 0.7, "out": 2.0},
    "gemini-1.5-flash-8b": {"in": 0.6, "out": 1.5},
    "gemini-pro": {"in": 2.0, "out": 5.0},
}

FEATURE_MULTIPLIER = {
    "plan": 1.3,
    "quiz": 1.0,
    "flashcards": 1.1,
    "summary": 1.0,
    "elaboration": 1.4,
    "elaboration_refine": 1.2,
    "repetition": 1.5,
    "task_help": 0.7,
    "chat": 0.6,
    "document_chat": 0.8,
    "audio_transcribe": 1.0,
    "podcast": 1.2,
    "learning_video": 1.4,
}

def ensure_minimum_tokens(username: str, reserve: int = 1):
    tokens = AuthManager.get_tokens(username)
    if tokens < reserve:
        raise HTTPException(status_code=402, detail=f"Nicht genügend Tokens. Du hast {tokens}, mindestens {reserve} werden benötigt.")
    return tokens

def resolve_model_preference(username: str, request_model: Optional[str]) -> Optional[str]:
    requested = (request_model or "").strip()
    if requested:
        return requested
    stored = (AuthManager.get_preferred_model(username) or "").strip()
    return stored or None

def _rates_for_model(model_name: str):
    model_name = (model_name or "").strip()
    if model_name in MODEL_TOKEN_RATES_PER_1K:
        return MODEL_TOKEN_RATES_PER_1K[model_name]
    # Prefix fallback for versioned model ids
    for key, rates in MODEL_TOKEN_RATES_PER_1K.items():
        if model_name.startswith(key):
            return rates
    return {"in": 1.0, "out": 3.0}

def deduct_tokens_by_usage(username: str, feature_key: str, used_model: str, usage: dict):
    usage = usage or {}
    prompt_tokens = int(usage.get("prompt_tokens", 0) or 0)
    output_tokens = int(usage.get("output_tokens", 0) or 0)
    total_tokens = int(usage.get("total_tokens", 0) or 0)
    usage_estimated = False
    if prompt_tokens <= 0 and output_tokens <= 0:
        usage_estimated = True
        # conservative fallback when provider usage is absent
        total_tokens = total_tokens or 700
        prompt_tokens = int(total_tokens * 0.5)
        output_tokens = total_tokens - prompt_tokens
    rates = _rates_for_model(used_model)
    multiplier = float(FEATURE_MULTIPLIER.get(feature_key, 1.0))
    raw_cost = ((prompt_tokens / 1000.0) * rates["in"] + (output_tokens / 1000.0) * rates["out"]) * multiplier
    tokens_charged = max(1, int(round(raw_cost)))
    before = AuthManager.get_tokens(username)
    if before < tokens_charged:
        raise HTTPException(status_code=402, detail=f"Nicht genügend Tokens. Benötigt: {tokens_charged}, verfügbar: {before}.")
    ok = AuthManager.deduct_tokens(username, tokens_charged)
    if not ok:
        raise HTTPException(status_code=500, detail="Token-Abzug fehlgeschlagen.")
    remaining = AuthManager.get_tokens(username)
    return {
        "tokens_charged": tokens_charged,
        "remaining_tokens": remaining,
        "usage_estimated": usage_estimated,
    }

def _configure_genai(username: str = None):
    """Configures GenAI with Central Env Key."""
    env_key = os.environ.get("GOOGLE_API_KEY")
    
    if not env_key:
        raise HTTPException(status_code=500, detail="NO_API_KEY_FOUND")
        
    genai.configure(api_key=env_key)
    return env_key

@app.post("/api/files/upload")
async def upload_file(
    username: str, 
    folder_id: str, 
    file: UploadFile = File(...)
):
    """Uploads a file (PDF) to the folder."""
    try:
        # Determine file type
        if file.filename.lower().endswith('.pdf'):
            content = await file.read()
            saved_name = DataManager.save_pdf(content, file.filename, username, folder_id)
            return {"status": "success", "filename": saved_name}
        else:
            raise HTTPException(status_code=400, detail="Nur PDFs werden aktuell unterstützt.")
    except Exception as e:
        print(f"Upload Error: {e}")
        raise HTTPException(status_code=500, detail=str(e))

@app.post("/api/files/youtube")
def import_youtube(username: str, folder_id: str, url: str = Body(..., embed=True)):
    """Imports a YouTube video transcript."""
    try:
        import youtube_transcript_api
        
        # Extract Video ID
        video_id = ""
        if "v=" in url:
            video_id = url.split("v=")[1].split("&")[0]
        elif "youtu.be/" in url:
            video_id = url.split("youtu.be/")[1].split("?")[0]
            
        if not video_id:
            raise HTTPException(status_code=400, detail="Ungültige YouTube URL")
            
        # Get Transcript
        transcript_list = youtube_transcript_api.YouTubeTranscriptApi.get_transcript(video_id, languages=['de', 'en'])
        transcript_text = " ".join([t['text'] for t in transcript_list])
        
        # Get Video Title (Mock or requires API Key, using generic for now)
        video_title = f"YouTube: {video_id}"
        
        # Save as text file/transcript
        DataManager.save_transcript(video_title, transcript_text, username, folder_id)
        
        return {"status": "success", "message": "Transkript importiert"}
        
    except Exception as e:
        print(f"YouTube Error: {e}")
        raise HTTPException(status_code=500, detail=f"Fehler beim Import: {str(e)}")

class DocumentRequest(BaseModel):
    title: str
    content: str
    
@app.post("/api/files/document")
def create_document(username: str, folder_id: str, request: DocumentRequest):
    """Creates a new custom text document."""
    try:
        if not request.title or not request.content:
             raise HTTPException(status_code=400, detail="Titel und Inhalt dürfen nicht leer sein.")
             
        # Re-use save_transcript as it saves purely text-based content
        DataManager.save_transcript(request.title, request.content, username, folder_id)
        
        return {"status": "success", "message": "Dokument erstellt"}
        
    except Exception as e:
        print(f"Create Document Error: {e}")
        raise HTTPException(status_code=500, detail=f"Fehler beim Erstellen: {str(e)}")


@app.post("/api/files/audio")
async def upload_audio(
    username: str, 
    folder_id: str, 
    file: UploadFile = File(...)
):
    """Uploads an audio file, transcribes it via Gemini, and saves the text."""
    from ai_service import AIService
    import tempfile
    try:
        if not file.filename.lower().endswith(('.webm', '.wav', '.mp3', '.m4a')):
             raise HTTPException(status_code=400, detail="Nur unterstützte Audioformate (.webm, .wav, .mp3, .m4a)")
             
        ensure_minimum_tokens(username, 1)
        _configure_genai(username)
        model_pref = resolve_model_preference(username, None)
        
        # Read the file content
        content = await file.read()
        
        # Save audio temporarily for Gemini
        with tempfile.NamedTemporaryFile(delete=False, suffix=Path(file.filename).suffix) as temp_audio:
            temp_audio.write(content)
            temp_audio_path = temp_audio.name
            
        try:
            # Transcribe via Gemini
            transcribe_result = AIService.transcribe_audio(temp_audio_path, model_preference=model_pref, return_meta=True)
            result_dict = transcribe_result["data"]
            
            transcript_text = result_dict.get("content", "")
            audio_title = result_dict.get("title", f"Audio Notiz {datetime.now().strftime('%d.%m.%Y %H:%M')}")
            
            # Persist original audio securely for downloading
            safe_filename = f"audio_{int(datetime.now().timestamp())}{Path(file.filename).suffix}"
            DataManager.save_audio(content, safe_filename, username, folder_id)

            # Save the transcript, linking it to the audio file if needed
            # We add a hidden markdown link or metadata so the frontend knows there's a file
            transcript_text = f"<!-- AUDIO_FILE:{safe_filename} -->\n{transcript_text}"
            
            DataManager.save_transcript(audio_title, transcript_text, username, folder_id)
            
            charge = deduct_tokens_by_usage(
                username,
                "audio_transcribe",
                transcribe_result.get("used_model", model_pref or ""),
                transcribe_result.get("usage"),
            )
            return {
                "status": "success",
                "message": "Audio transkribiert und als Notiz gespeichert",
                "used_model": transcribe_result.get("used_model", model_pref or ""),
                "usage": transcribe_result.get("usage", {}),
                **charge,
            }
        finally:
             if os.path.exists(temp_audio_path):
                 os.remove(temp_audio_path)
                 
    except HTTPException:
        raise
    except Exception as e:
        print(f"Audio Upload Error: {e}")
        raise HTTPException(status_code=500, detail=f"Fehler bei der Audio-Verarbeitung: {str(e)}")

@app.post("/api/files/image")
async def upload_image(
    username: str, 
    folder_id: str, 
    file: UploadFile = File(...)
):
    """Uploads an image, extracts text/context via Gemini, and saves."""
    import tempfile
    try:
        if not file.filename.lower().endswith(('.jpg', '.jpeg', '.png', '.webp')):
             raise HTTPException(status_code=400, detail="Nur unterstützte Bildformate (.jpg, .jpeg, .png, .webp)")
             
        ensure_minimum_tokens(username, 1)
        _configure_genai(username)
        
        content = await file.read()
        
        with tempfile.NamedTemporaryFile(delete=False, suffix=Path(file.filename).suffix) as temp_img:
            temp_img.write(content)
            temp_img_path = temp_img.name
            
        try:
            import google.generativeai as genai
            from PIL import Image
            
            img = Image.open(temp_img_path)
            model = genai.GenerativeModel('gemini-1.5-pro')
            response = model.generate_content([
                "Beschreibe dieses Bild detailliert für meine Lernunterlagen. Extrahiere jeglichen relevanten Text und erkläre Diagramme oder Konzepte.",
                img
            ])
            
            extracted_text = response.text
            image_title = f"Bild: {file.filename}"
            
            DataManager.save_transcript(image_title, extracted_text, username, folder_id)
            
            charge = deduct_tokens_by_usage(
                username,
                "summary",
                "gemini-1.5-pro",
                {
                    "prompt_tokens": int(getattr(getattr(response, "usage_metadata", None), "prompt_token_count", 0) or 0),
                    "output_tokens": int(getattr(getattr(response, "usage_metadata", None), "candidates_token_count", 0) or 0),
                    "total_tokens": int(getattr(getattr(response, "usage_metadata", None), "total_token_count", 0) or 0),
                },
            )
            return {"status": "success", "message": "Bild verarbeitet und als Text notiert", **charge}
        finally:
             if os.path.exists(temp_img_path):
                 os.remove(temp_img_path)
                 
    except Exception as e:
        print(f"Image Upload Error: {e}")
        raise HTTPException(status_code=500, detail=f"Fehler bei der Bildverarbeitung: {str(e)}")

@app.get("/api/files/signed-media-url")
def signed_media_url(
    username: str,
    folder_id: str,
    file_id: str,
    kind: str,
    expires_in: int = 3600,
):
    """Kurzlebige Supabase-URL für Video/Audio — Wiedergabe umgeht Vercel-Rewrite (große MP4)."""
    ttl = max(60, min(int(expires_in), 7200))
    url = DataManager.create_signed_media_url(
        username, folder_id, file_id, kind, expires_in=ttl
    )
    if not url:
        raise HTTPException(
            status_code=404,
            detail=(
                "Signierte Medien-URL konnte nicht erstellt werden. "
                "Auf Render optional SUPABASE_SERVICE_ROLE_KEY setzen (Storage Sign-API)."
            ),
        )
    return {"url": url, "expires_in": ttl}


@app.get("/api/files/download_audio")
def download_audio(
    username: str,
    folder_id: str,
    filename: str = "",
    file_id: Optional[str] = None,
):
    """Streams an audio file (MP3); use file_id after renames — DB file_url stays correct."""
    from fastapi.responses import Response
    try:
        if not file_id and not (filename or "").strip():
            raise HTTPException(status_code=400, detail="file_id oder filename erforderlich")
        audio_bytes, download_name = DataManager.get_audio_bytes(
            filename or "", username, folder_id, file_id=file_id
        )
        if not audio_bytes:
            raise HTTPException(status_code=404, detail="Audiodatei nicht gefunden")
        safe_name = (download_name or "audio.mp3").replace('"', "")
        return Response(
            content=audio_bytes,
            media_type="audio/mpeg",
            headers={
                "Content-Disposition": f'inline; filename="{safe_name}"',
                "Cache-Control": "no-store",
            },
        )
    except HTTPException:
        raise
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Audio-Download fehlgeschlagen: {str(e)}")


@app.get("/api/files/download_video")
def download_video(
    request: Request,
    username: str,
    folder_id: str,
    filename: str = "",
    file_id: Optional[str] = None,
):
    """Streams an MP4 learning video from storage (Range requests for browser players)."""
    from fastapi.responses import Response
    try:
        video_bytes, download_name = DataManager.get_video_bytes(filename, username, folder_id, file_id=file_id)
        if not video_bytes:
            raise HTTPException(status_code=404, detail="Video nicht gefunden")
        safe_name = (download_name or "video.mp4").replace('"', "")
        total = len(video_bytes)
        if total == 0:
            raise HTTPException(status_code=404, detail="Video-Datei ist leer")
        headers_base = {
            "Content-Disposition": f'inline; filename="{safe_name}"',
            "Accept-Ranges": "bytes",
            "Cache-Control": "no-store",
        }
        range_header = (request.headers.get("range") or request.headers.get("Range") or "").strip()
        if range_header.startswith("bytes="):
            try:
                spec = range_header[6:].strip()
                start_s, end_s = spec.split("-", 1)
                if not start_s and end_s:
                    n = int(end_s)
                    start = max(0, total - n)
                    end = total - 1
                else:
                    start = int(start_s) if start_s else 0
                    end = int(end_s) if end_s else total - 1
                start = max(0, min(start, total - 1))
                end = max(start, min(end, total - 1))
                chunk = video_bytes[start : end + 1]
                return Response(
                    content=chunk,
                    status_code=206,
                    media_type="video/mp4",
                    headers={
                        **headers_base,
                        "Content-Range": f"bytes {start}-{end}/{total}",
                        "Content-Length": str(len(chunk)),
                    },
                )
            except (ValueError, TypeError):
                pass
        return Response(
            content=video_bytes,
            media_type="video/mp4",
            headers={**headers_base, "Content-Length": str(total)},
        )
    except HTTPException:
        raise
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Video-Download fehlgeschlagen: {str(e)}")


@app.get("/api/files/download_pdf")
def download_pdf(username: str, folder_id: str, filename: str = "", file_id: Optional[str] = None):
    """Downloads or displays a PDF file."""
    from fastapi.responses import Response
    try:
        pdf_bytes, download_name = DataManager.get_pdf_bytes(filename, username, folder_id, file_id=file_id)
        if not pdf_bytes:
            raise HTTPException(status_code=404, detail="PDF-Datei nicht gefunden oder Storage-Pfad ungültig")
        safe_name = (download_name or "dokument.pdf").replace('"', '')
        return Response(
            content=pdf_bytes,
            media_type="application/pdf",
            headers={
                "Content-Disposition": f'inline; filename="{safe_name}"',
                "Cache-Control": "no-store",
            },
        )
    except HTTPException:
        raise
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"PDF-Download fehlgeschlagen: {str(e)}")


@app.get("/api/files/{folder_id}")
def get_files(folder_id: str, username: str):
    """Returns files in a specific folder."""
    files = DataManager.list_files(username, folder_id)
    return files


class GenRequest(BaseModel):
    username: str
    folder_id: str
    model_preference: str = None
    quiz_question_count: Optional[int] = None
    quiz_difficulty: Optional[str] = None
    flashcards_count: Optional[int] = None
    flashcards_max_text: Optional[int] = None

class SummaryRequest(BaseModel):
    username: str
    folder_id: str
    detail_level: str = "Normal"
    model_preference: str = None
    learning_mode: str = "normal"

class ElaborationRequest(BaseModel):
    username: str
    folder_id: str
    detail_level: str = "Normal"
    custom_rules: str = ""
    model_preference: str = None

class ElaborationRefineRequest(BaseModel):
    username: str
    folder_id: str
    file_id: str
    prompt: str
    detail_level: str = "Sehr detailliert"
    save_mode: str = "new_file"  # new_file | replace
    model_preference: str = None

class RepetitionRequest(BaseModel):
    username: str
    folder_id: str
    custom_rules: str = ""
    model_preference: str = None
    learning_mode: str = "normal"

class PlanRequest(BaseModel):
    username: str
    folder_id: str
    duration_days: int
    hours_per_day: float = 2.0  # Default 2 hours per day
    active_days: list[int] = None # List of active weekdays (0=Mo, 6=Su). If None, all days are active.
    model_preference: str = None
    learning_mode: str = "normal"

class TaskHelpRequest(BaseModel):
    username: str
    folder_id: str
    task_description: str
    model_preference: str = None

class DocumentChatPatchRequest(BaseModel):
    username: str
    folder_id: str
    file_id: str
    message: str
    history: List[ChatMessage]
    model_preference: Optional[str] = None


class PodcastRequest(BaseModel):
    username: str
    folder_id: str
    model_preference: Optional[str] = None


class LearningVideoRequest(BaseModel):
    username: str
    folder_id: str
    model_preference: Optional[str] = None
    target_scenes: Optional[int] = None
    narration_depth: Optional[str] = None
    visual_style: Optional[str] = None
    motion: Optional[str] = None
    use_stock_images: Optional[bool] = None
    tts_voice: Optional[str] = None
    slide_motion: Optional[bool] = None
    slide_crossfade: Optional[bool] = None


@app.post("/api/ai/plan")
def create_study_plan(request: PlanRequest, background_tasks: BackgroundTasks):
    """Generates a study plan from folder contents."""
    from ai_service import AIService
    
    try:
        ensure_minimum_tokens(request.username, 2)
        _configure_genai()
        model_pref = resolve_model_preference(request.username, request.model_preference)
        
        context, debug_log = _get_folder_context(request.username, request.folder_id)

        if not context:
            error_msg = f"Kein Material gefunden. Debug: {'; '.join(debug_log)}"
            print(error_msg)
            raise HTTPException(status_code=400, detail=error_msg)

        # Generate Plan
        plan_result = AIService.generate_study_plan(
            context,
            request.duration_days,
            request.hours_per_day,
            model_preference=model_pref,
            active_days=request.active_days,
            learning_mode=request.learning_mode,
            return_meta=True,
        )
        plan = plan_result["data"]
        
        # Save Plan
        DataManager.save_plan(plan, request.username, request.folder_id)
        background_tasks.add_task(try_notify_document_ready, request.username, request.folder_id, "Lernplan")
        charge = deduct_tokens_by_usage(
            request.username,
            "plan",
            plan_result.get("used_model", model_pref or ""),
            plan_result.get("usage"),
        )
        return {
            "status": "success",
            "plan": plan,
            "used_model": plan_result.get("used_model", model_pref or ""),
            "usage": plan_result.get("usage", {}),
            **charge,
        }
    
    except HTTPException:
        raise  # Re-raise HTTP exceptions as-is
    except Exception as e:
        print(f"Plan generation error: {e}")
        raise HTTPException(status_code=500, detail=f"Lernplan-Fehler: {str(e)}")


def _truncate_for_learning_video_context(parts: List[Any]) -> List[Any]:
    """Kürzt Text + begrenzt PDF-Anhänge, damit Gemini-Storyboard nicht bei großen Ordnern timeoutet."""
    MAX_TEXT_CHARS = 100_000
    MAX_PDF_FILES = 3
    if not parts:
        return parts
    texts: List[str] = []
    files: List[Any] = []
    for p in parts:
        if isinstance(p, str):
            texts.append(p)
        else:
            files.append(p)
    out: List[Any] = []
    if texts:
        merged = "\n\n".join(texts)
        if len(merged) > MAX_TEXT_CHARS:
            merged = (
                merged[:MAX_TEXT_CHARS]
                + "\n\n[… Material wurde automatisch gekürzt (Lernvideo-Storyboard, Timeout-Schutz). …]"
            )
        out.append(merged)
    out.extend(files[:MAX_PDF_FILES])
    if len(files) > MAX_PDF_FILES:
        print(
            f"Lernvideo: {len(files) - MAX_PDF_FILES} PDF(s) nicht mitgesendet (Limit {MAX_PDF_FILES} für Storyboard)."
        )
    return out if out else parts


def _get_folder_context(username: str, folder_id: str, included_file_ids: Optional[List[str]] = None):
    """
    Aggregates material from the folder.
    If included_file_ids is None, loads saved folder filter from DB; empty/null filter uses all files.
    Returns (content_parts, debug_log)
    """
    # Ensure GenAI is configured (redundant but safe for helper usage)
    _configure_genai(username)
    
    debug_log = []
    try:
        files = DataManager.list_files(username, folder_id)
        debug_log.append(f"Found {len(files)} files")
    except Exception as e:
        debug_log.append(f"ListFiles Error: {str(e)}")
        return [], debug_log

    ids_filter = included_file_ids
    if ids_filter is None:
        ids_filter = DataManager.get_ai_context_file_ids(username, folder_id)
    if ids_filter is not None and len(ids_filter) > 0:
        idset = set(ids_filter)
        files = [f for f in files if f.get("id") in idset]
        debug_log.append(f"Filtered to {len(files)} files by ai_context_file_ids")
    else:
        debug_log.append("Using all files (no ai_context filter)")
        
    content_parts = []
    has_content = False
    
    for f in files:
        f_type = f.get("type", "unknown")
        f_name = f.get("name", "unnamed")
        debug_log.append(f"Processing {f_name} ({f_type})")
        
        # 1. YouTube Transcripts (Text)
        if f_type == "transcript":
             if "content" in f:
                 content_parts.append(f"--- Transkript: {f_name} ---\n{f['content']}\n\n")
                 has_content = True
                 debug_log.append(f"Added transcript {f_name}")
             else:
                 debug_log.append(f"Transcript {f_name} has no content")

        # 1b. Text-based generated / stored documents (summary, plans, quiz JSON, etc.)
        elif f_type in ("summary", "repetition", "plan", "quiz", "flashcards"):
            raw = f.get("content")
            if raw is None:
                debug_log.append(f"{f_name} ({f_type}) has no content field")
                continue
            if isinstance(raw, (dict, list)):
                text = json.dumps(raw, ensure_ascii=False, indent=2)
            else:
                text = str(raw)
            if not text.strip():
                debug_log.append(f"{f_name} ({f_type}) empty content")
                continue
            content_parts.append(f"--- {f_name} ({f_type}) ---\n{text}\n\n")
            has_content = True
            debug_log.append(f"Added text context from {f_type} {f_name}")
        
        # 2. PDFs (Upload to Gemini for OCR/Vision)
        elif f_type == "pdf":
            try:
                # Get local path
                debug_log.append(f"Fetching PDF path for {f_name}")
                pdf_path = DataManager.get_pdf_path(f_name, username, folder_id)
                
                if pdf_path:
                    if os.path.exists(pdf_path):
                        try:
                            import time
                            uploaded_file = genai.upload_file(pdf_path, mime_type="application/pdf")
                            
                            while uploaded_file.state.name == "PROCESSING":
                                debug_log.append(f"Waiting for PDF {f_name} processing...")
                                time.sleep(2)
                                uploaded_file = genai.get_file(uploaded_file.name)
                                
                            if uploaded_file.state.name == "FAILED":
                                debug_log.append(f"PDF {f_name} processing failed on Gemini.")
                                continue

                            content_parts.append(uploaded_file)
                            has_content = True
                            debug_log.append(f"Successfully uploaded and processed {f_name}")
                        except Exception as upload_err:
                             debug_log.append(f"GenAI Upload Error: {str(upload_err)}")
                    else:
                        debug_log.append(f"Path returned but file missing: {pdf_path}")
                else:
                    debug_log.append(f"DataManager returned None path for {f_name}")
            except Exception as e:
                debug_log.append(f"Error processing PDF {f_name}: {str(e)}")
                
    if not has_content:
        return [], debug_log
        
    return content_parts, debug_log

@app.post("/api/ai/quiz")
def create_quiz(request: GenRequest, background_tasks: BackgroundTasks):
    from ai_service import AIService
    from datetime import datetime
    try:
        ensure_minimum_tokens(request.username, 1)
        _configure_genai()
        model_pref = resolve_model_preference(request.username, request.model_preference)
        context, debug_log = _get_folder_context(request.username, request.folder_id)
        if not context:
            raise HTTPException(status_code=400, detail=f"Kein Material gefunden. Debug: {'; '.join(debug_log)}")
        quiz_count = max(3, min(30, int(request.quiz_question_count or 10)))
        quiz_difficulty = (request.quiz_difficulty or "gemischt").strip().lower()
        if quiz_difficulty not in {"leicht", "mittel", "schwer", "gemischt"}:
            quiz_difficulty = "gemischt"
        quiz_result = AIService.generate_quiz(
            context,
            model_preference=model_pref,
            question_count=quiz_count,
            difficulty=quiz_difficulty,
            return_meta=True,
        )
        quiz = quiz_result["data"]
        file_id = f"quiz_main_{request.folder_id}"
        DataManager.save_file_metadata({
            "id": file_id, "name": "AI Quiz", "type": "quiz", "content": quiz, "created_at": datetime.now().strftime("%Y-%m-%d")
        }, request.username, request.folder_id)
        charge = deduct_tokens_by_usage(
            request.username,
            "quiz",
            quiz_result.get("used_model", model_pref or ""),
            quiz_result.get("usage"),
        )
        return {
            "status": "success",
            "quiz": quiz,
            "used_model": quiz_result.get("used_model", model_pref or ""),
            "usage": quiz_result.get("usage", {}),
            **charge,
        }
    except HTTPException:
        raise
    except Exception as e:
        print(f"Quiz error: {e}")
        raise HTTPException(status_code=500, detail=f"Quiz-Fehler: {str(e)}")

@app.post("/api/ai/flashcards")
def create_flashcards(request: GenRequest, background_tasks: BackgroundTasks):
    from ai_service import AIService
    from datetime import datetime
    try:
        ensure_minimum_tokens(request.username, 1)
        _configure_genai()
        model_pref = resolve_model_preference(request.username, request.model_preference)
        context, debug_log = _get_folder_context(request.username, request.folder_id)
        if not context:
            raise HTTPException(status_code=400, detail=f"Kein Material gefunden. Debug: {'; '.join(debug_log)}")
        cards_count = max(5, min(60, int(request.flashcards_count or 20)))
        max_text = max(120, min(1200, int(request.flashcards_max_text or 320)))
        cards_result = AIService.generate_flashcards(
            context,
            model_preference=model_pref,
            cards_count=cards_count,
            max_text_per_card=max_text,
            return_meta=True,
        )
        cards = cards_result["data"]
        DataManager.save_file_metadata({
            "id": f"cards_main_{request.folder_id}",
            "name": "Generierte Karteikarten",
            "type": "flashcards",
            "content": cards,
            "created_at": datetime.now().strftime("%Y-%m-%d")
        }, request.username, request.folder_id)
        background_tasks.add_task(try_notify_document_ready, request.username, request.folder_id, "Karteikarten")
        charge = deduct_tokens_by_usage(
            request.username,
            "flashcards",
            cards_result.get("used_model", model_pref or ""),
            cards_result.get("usage"),
        )
        return {
            "status": "success",
            "flashcards": cards,
            "used_model": cards_result.get("used_model", model_pref or ""),
            "usage": cards_result.get("usage", {}),
            **charge,
        }
    except HTTPException:
        raise
    except Exception as e:
        print(f"Flashcards error: {e}")
        raise HTTPException(status_code=500, detail=f"Karteikarten-Fehler: {str(e)}")

@app.post("/api/ai/summary")
def create_summary(request: SummaryRequest, background_tasks: BackgroundTasks):
    from ai_service import AIService
    try:
        ensure_minimum_tokens(request.username, 1)
        _configure_genai()
        model_pref = resolve_model_preference(request.username, request.model_preference)
        context, debug_log = _get_folder_context(request.username, request.folder_id)
        if not context:
            raise HTTPException(status_code=400, detail=f"Kein Material gefunden. Debug: {'; '.join(debug_log)}")
        summary_result = AIService.generate_summary(
            context,
            detail_level=request.detail_level,
            model_preference=model_pref,
            learning_mode=request.learning_mode,
            return_meta=True,
        )
        summary = summary_result["text"]
        DataManager.save_generated_summary(summary, request.username, request.folder_id)
        charge = deduct_tokens_by_usage(
            request.username,
            "summary",
            summary_result.get("used_model", model_pref or ""),
            summary_result.get("usage"),
        )
        return {
            "status": "success",
            "summary": summary,
            "used_model": summary_result.get("used_model", model_pref or ""),
            "usage": summary_result.get("usage", {}),
            **charge,
        }
    except HTTPException:
        raise
    except Exception as e:
        print(f"Summary error: {e}")
        raise HTTPException(status_code=500, detail=f"Zusammenfassung-Fehler: {str(e)}")

@app.post("/api/ai/elaboration")
def create_elaboration(request: ElaborationRequest):
    from ai_service import AIService
    from datetime import datetime
    try:
        ensure_minimum_tokens(request.username, 2)
        _configure_genai()
        model_pref = resolve_model_preference(request.username, request.model_preference)
        context, debug_log = _get_folder_context(request.username, request.folder_id)
        if not context:
            raise HTTPException(status_code=400, detail=f"Kein Material gefunden. Debug: {'; '.join(debug_log)}")
        
        elaboration_result = AIService.generate_elaboration(
            context,
            detail_level=request.detail_level,
            custom_rules=request.custom_rules,
            model_preference=model_pref,
            return_meta=True,
        )
        elaboration_text = elaboration_result["text"]
        
        DataManager.save_file_metadata({
            "id": f"elaboration_{int(datetime.now().timestamp())}",
            "name": "Ausarbeitung",
            "type": "summary", # Reusing 'summary' rich text editor styling for now
            "content": elaboration_text,
            "created_at": datetime.now().strftime("%Y-%m-%d")
        }, request.username, request.folder_id)
        background_tasks.add_task(try_notify_document_ready, request.username, request.folder_id, "Ausarbeitung")
        charge = deduct_tokens_by_usage(
            request.username,
            "elaboration",
            elaboration_result.get("used_model", model_pref or ""),
            elaboration_result.get("usage"),
        )
        return {
            "status": "success",
            "elaboration": elaboration_text,
            "used_model": elaboration_result.get("used_model", model_pref or ""),
            "usage": elaboration_result.get("usage", {}),
            **charge,
        }
    except HTTPException:
        raise
    except Exception as e:
        print(f"Elaboration error: {e}")
        raise HTTPException(status_code=500, detail=f"Ausarbeitungs-Fehler: {str(e)}")

@app.post("/api/ai/elaboration/refine")
def refine_elaboration(request: ElaborationRefineRequest):
    from ai_service import AIService
    from datetime import datetime
    try:
        ensure_minimum_tokens(request.username, 1)
        _configure_genai()
        model_pref = resolve_model_preference(request.username, request.model_preference)
        all_files = DataManager.list_files(request.username, request.folder_id)
        target = next((f for f in all_files if f.get("id") == request.file_id), None)
        if not target:
            raise HTTPException(status_code=404, detail="Datei für Anpassung nicht gefunden.")
        ftype = (target.get("type") or "summary").lower()
        json_types = ("quiz", "flashcards", "plan")
        base_content = target.get("content")
        if ftype in json_types:
            if base_content is None:
                raise HTTPException(status_code=400, detail="Die ausgewählte Datei hat keinen Inhalt.")
            refined_result = AIService.refine_json_artifact(
                ftype,
                base_content,
                request.prompt.strip(),
                model_pref,
                return_meta=True,
            )
            refined_payload = refined_result["data"]
            save_mode = (request.save_mode or "new_file").lower()
            if save_mode == "replace":
                if not DataManager.update_file_content(
                    request.username,
                    request.folder_id,
                    request.file_id,
                    refined_payload,
                ):
                    raise HTTPException(status_code=404, detail="Datei konnte nicht überschrieben werden.")
                charge = deduct_tokens_by_usage(
                    request.username,
                    "elaboration_refine",
                    refined_result.get("used_model", model_pref or ""),
                    refined_result.get("usage"),
                )
                return {
                    "status": "success",
                    "saved_as": "replace",
                    "content": refined_payload,
                    "used_model": refined_result.get("used_model", model_pref or ""),
                    "usage": refined_result.get("usage", {}),
                    **charge,
                }
            new_id = f"{ftype}_refined_{int(datetime.now().timestamp())}"
            base_name = target.get("name") or ftype
            DataManager.save_file_metadata(
                {
                    "id": new_id,
                    "name": f"{base_name} (überarbeitet)",
                    "type": ftype,
                    "content": refined_payload,
                    "created_at": datetime.now().strftime("%Y-%m-%d"),
                },
                request.username,
                request.folder_id,
            )
            charge = deduct_tokens_by_usage(
                request.username,
                "elaboration_refine",
                refined_result.get("used_model", model_pref or ""),
                refined_result.get("usage"),
            )
            return {
                "status": "success",
                "saved_as": "new_file",
                "new_file_id": new_id,
                "content": refined_payload,
                "used_model": refined_result.get("used_model", model_pref or ""),
                "usage": refined_result.get("usage", {}),
                **charge,
            }

        if isinstance(base_content, (dict, list)):
            base_content = json.dumps(base_content, ensure_ascii=False, indent=2)
        base_content = str(base_content or "")
        if not base_content.strip():
            raise HTTPException(status_code=400, detail="Die ausgewählte Datei hat keinen Inhalt.")
        combined_rules = (
            "Passe das bestehende Dokument gezielt an.\n"
            "Behalte den vorhandenen Stil bei, verbessere Struktur/Tiefe, "
            "und setze den Nutzerwunsch konkret um.\n\n"
            f"Nutzerwunsch:\n{request.prompt.strip()}\n\n"
            "Bestehendes Dokument:\n"
            f"{base_content}"
        )
        refined_result = AIService.generate_elaboration(
            content=[base_content],
            detail_level=request.detail_level,
            custom_rules=combined_rules,
            model_preference=model_pref,
            return_meta=True,
        )
        refined_text = refined_result["text"]
        save_mode = (request.save_mode or "new_file").lower()
        if save_mode == "replace":
            if not DataManager.update_file_content(
                request.username,
                request.folder_id,
                request.file_id,
                refined_text,
            ):
                raise HTTPException(status_code=404, detail="Datei konnte nicht überschrieben werden.")
            charge = deduct_tokens_by_usage(
                request.username,
                "elaboration_refine",
                refined_result.get("used_model", model_pref or ""),
                refined_result.get("usage"),
            )
            return {
                "status": "success",
                "saved_as": "replace",
                "content": refined_text,
                "used_model": refined_result.get("used_model", model_pref or ""),
                "usage": refined_result.get("usage", {}),
                **charge,
            }
        new_id = f"elaboration_refined_{int(datetime.now().timestamp())}"
        base_name = target.get("name") or "Ausarbeitung"
        DataManager.save_file_metadata(
            {
                "id": new_id,
                "name": f"{base_name} (überarbeitet)",
                "type": target.get("type", "summary"),
                "content": refined_text,
                "created_at": datetime.now().strftime("%Y-%m-%d"),
            },
            request.username,
            request.folder_id,
        )
        charge = deduct_tokens_by_usage(
            request.username,
            "elaboration_refine",
            refined_result.get("used_model", model_pref or ""),
            refined_result.get("usage"),
        )
        return {
            "status": "success",
            "saved_as": "new_file",
            "new_file_id": new_id,
            "content": refined_text,
            "used_model": refined_result.get("used_model", model_pref or ""),
            "usage": refined_result.get("usage", {}),
            **charge,
        }
    except HTTPException:
        raise
    except Exception as e:
        print(f"Elaboration refine error: {e}")
        raise HTTPException(status_code=500, detail=f"Ausarbeitung-Anpassung fehlgeschlagen: {str(e)}")

@app.post("/api/ai/repetition")
def create_repetition(request: RepetitionRequest, background_tasks: BackgroundTasks):
    from ai_service import AIService
    from datetime import datetime
    try:
        ensure_minimum_tokens(request.username, 2)
        _configure_genai()
        model_pref = resolve_model_preference(request.username, request.model_preference)
        context, debug_log = _get_folder_context(request.username, request.folder_id)
        if not context:
            raise HTTPException(status_code=400, detail=f"Kein Material gefunden. Debug: {'; '.join(debug_log)}")
        
        repetition_result = AIService.generate_repetition(
            context,
            custom_rules=request.custom_rules,
            model_preference=model_pref,
            learning_mode=request.learning_mode,
            return_meta=True,
        )
        repetition_text = repetition_result["text"]
        
        DataManager.save_file_metadata({
            "id": f"repetition_{int(datetime.now().timestamp())}",
            "name": "Wiederholungsbogen",
            "type": "repetition", 
            "content": repetition_text,
            "created_at": datetime.now().strftime("%Y-%m-%d")
        }, request.username, request.folder_id)
        background_tasks.add_task(try_notify_document_ready, request.username, request.folder_id, "Wiederholungsbogen")
        charge = deduct_tokens_by_usage(
            request.username,
            "repetition",
            repetition_result.get("used_model", model_pref or ""),
            repetition_result.get("usage"),
        )
        return {
            "status": "success",
            "repetition": repetition_text,
            "used_model": repetition_result.get("used_model", model_pref or ""),
            "usage": repetition_result.get("usage", {}),
            **charge,
        }
    except HTTPException:
        raise
    except Exception as e:
        print(f"Repetition error: {e}")
        raise HTTPException(status_code=500, detail=f"Wiederholungs-Fehler: {str(e)}")

@app.post("/api/ai/task-help")
def get_task_help(request: TaskHelpRequest):
    from ai_service import AIService
    try:
        ensure_minimum_tokens(request.username, 1)
        _configure_genai()
        model_pref = resolve_model_preference(request.username, request.model_preference)
        context, debug_log = _get_folder_context(request.username, request.folder_id)
        if not context:
            raise HTTPException(status_code=400, detail=f"Kein Material gefunden. Debug: {'; '.join(debug_log)}")
        
        help_result = AIService.generate_task_help(context, request.task_description, model_pref, return_meta=True)
        charge = deduct_tokens_by_usage(
            request.username,
            "task_help",
            help_result.get("used_model", model_pref or ""),
            help_result.get("usage"),
        )
        return {
            "status": "success",
            "help_text": help_result["text"],
            "used_model": help_result.get("used_model", model_pref or ""),
            "usage": help_result.get("usage", {}),
            **charge,
        }
    except HTTPException:
        raise
    except Exception as e:
        print(f"Task Help error: {e}")
        raise HTTPException(status_code=500, detail=f"Aufgabenhilfe-Fehler: {str(e)}")

@app.post("/api/ai/chat")
def chat_endpoint(request: ChatRequest):
    from ai_service import AIService
    try:
        ensure_minimum_tokens(request.username, 1)
        _configure_genai()
        model_pref = resolve_model_preference(request.username, request.model_preference)
        context, debug_log = _get_folder_context(request.username, request.folder_id)
        if not context:
            # Fallback to general chat if no context is found, but pass empty list
            context = []
        
        # Convert history
        history_dicts = [{"role": msg.role, "content": msg.content} for msg in request.history]
        
        reply_result = AIService.chat(
            content=context, 
            user_message=request.message, 
            history=history_dicts, 
            model_preference=model_pref,
            active_file=request.active_file,
            return_meta=True,
        )
        charge = deduct_tokens_by_usage(
            request.username,
            "chat",
            reply_result.get("used_model", model_pref or ""),
            reply_result.get("usage"),
        )
        return {
            "status": "success",
            "reply": reply_result["text"],
            "used_model": reply_result.get("used_model", model_pref or ""),
            "usage": reply_result.get("usage", {}),
            **charge,
        }
    except HTTPException:
        raise
    except Exception as e:
        print(f"Chat error: {e}")
        raise HTTPException(status_code=500, detail=f"Chatbot-Fehler: {str(e)}")

@app.post("/api/ai/document-chat")
def document_chat_patch(request: DocumentChatPatchRequest):
    from ai_service import AIService
    try:
        ensure_minimum_tokens(request.username, 1)
        _configure_genai()
        model_pref = resolve_model_preference(request.username, request.model_preference)
        all_files = DataManager.list_files(request.username, request.folder_id)
        target = next((f for f in all_files if f.get("id") == request.file_id), None)
        if not target:
            raise HTTPException(status_code=404, detail="Datei nicht gefunden.")
        content = target.get("content")
        if isinstance(content, (dict, list)):
            content = json.dumps(content, ensure_ascii=False, indent=2)
        content = str(content or "")
        history_dicts = [{"role": msg.role, "content": msg.content} for msg in request.history]
        patch_result = AIService.refine_document_with_chat(
            document_content=content,
            user_message=request.message,
            history=history_dicts,
            model_preference=model_pref,
            return_meta=True,
        )
        charge = deduct_tokens_by_usage(
            request.username,
            "document_chat",
            patch_result.get("used_model", model_pref or ""),
            patch_result.get("usage"),
        )
        return {
            "status": "success",
            "patch": patch_result["patch"],
            "used_model": patch_result.get("used_model", model_pref or ""),
            "usage": patch_result.get("usage", {}),
            **charge,
        }
    except HTTPException:
        raise
    except Exception as e:
        print(f"Document chat error: {e}")
        raise HTTPException(status_code=500, detail=f"Dokument-Chat-Fehler: {str(e)}")


@app.post("/api/ai/podcast")
def create_podcast(request: PodcastRequest, background_tasks: BackgroundTasks):
    from ai_service import AIService
    from media_pipeline import openai_tts_speech_mp3

    try:
        ensure_minimum_tokens(request.username, 2)
        _configure_genai()
        model_pref = resolve_model_preference(request.username, request.model_preference)
        context, debug_log = _get_folder_context(request.username, request.folder_id)
        if not context:
            raise HTTPException(
                status_code=400,
                detail=f"Kein Material gefunden. Debug: {'; '.join(debug_log)}",
            )
        script_result = AIService.generate_podcast_script(
            context, model_preference=model_pref, return_meta=True
        )
        text = script_result["text"]
        mp3_bytes = openai_tts_speech_mp3(text)
        fname = f"podcast_{int(datetime.now().timestamp())}.mp3"
        file_id = DataManager.save_audio(mp3_bytes, fname, request.username, request.folder_id)
        charge = deduct_tokens_by_usage(
            request.username,
            "podcast",
            script_result.get("used_model", model_pref or ""),
            script_result.get("usage"),
        )
        background_tasks.add_task(
            try_notify_document_ready, request.username, request.folder_id, "Podcast"
        )
        return {
            "status": "success",
            "file_id": file_id,
            "filename": fname,
            "used_model": script_result.get("used_model", model_pref or ""),
            "usage": script_result.get("usage", {}),
            **charge,
        }
    except HTTPException:
        raise
    except RuntimeError as e:
        raise HTTPException(status_code=503, detail=str(e))
    except Exception as e:
        print(f"Podcast error: {e}")
        raise HTTPException(status_code=500, detail=f"Podcast-Fehler: {str(e)}")


@app.post("/api/ai/learning-video")
def create_learning_video(request: LearningVideoRequest):
    """Startet Lernvideo-Erstellung im Hintergrund (vermeidet HTTP-502 durch Proxy-Timeouts)."""
    try:
        ensure_minimum_tokens(request.username, 3)
    except HTTPException:
        raise
    job_id = str(uuid.uuid4())
    lv_opts = _normalize_learning_video_options(request)
    with _LEARNING_VIDEO_JOBS_LOCK:
        _learning_video_prune_jobs_locked()
        _LEARNING_VIDEO_JOBS[job_id] = {
            "status": "pending",
            "username": request.username,
            "created_at": time.time(),
        }
    thread = threading.Thread(
        target=_learning_video_job_worker,
        args=(job_id, request.username, request.folder_id, request.model_preference, lv_opts),
        daemon=True,
    )
    thread.start()
    return JSONResponse(
        status_code=202,
        content={
            "status": "accepted",
            "job_id": job_id,
        },
    )


@app.get("/api/ai/learning-video/status/{job_id}")
def learning_video_job_status(job_id: str, username: str):
    with _LEARNING_VIDEO_JOBS_LOCK:
        job = _LEARNING_VIDEO_JOBS.get(job_id)
    if not job or job.get("username") != username:
        raise HTTPException(status_code=404, detail="Unbekannter oder abgelaufener Auftrag.")
    status = job.get("status")
    if status == "pending":
        return {"status": "pending"}
    if status == "error":
        return {"status": "error", "detail": job.get("detail") or "Unbekannter Fehler."}
    if status == "done":
        return {
            "status": "done",
            "file_id": job["file_id"],
            "filename": job["filename"],
            "title": job["title"],
            "used_model": job.get("used_model", ""),
            "usage": job.get("usage", {}),
            "tokens_charged": job.get("tokens_charged"),
            "remaining_tokens": job.get("remaining_tokens"),
            "usage_estimated": job.get("usage_estimated"),
        }
    return {"status": "pending"}


if __name__ == "__main__":
    port = int(os.environ.get("PORT", 8000))
    uvicorn.run("main:app", host="0.0.0.0", port=port, reload=True)
