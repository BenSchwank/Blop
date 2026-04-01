from fastapi import FastAPI, HTTPException, Body, UploadFile, File, BackgroundTasks
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
from typing import Any, List, Optional
import uvicorn
import os
from datetime import datetime
from pathlib import Path

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
import google.generativeai as genai

# Configure GenAI (Global for main.py usage like upload_file)
api_key = os.environ.get("GOOGLE_API_KEY")
if api_key:
    genai.configure(api_key=api_key)

app = FastAPI(title="Blop Study Backend")

# CORS (Allow Frontend to connect)
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],  # Allow all origins for now (fix CORS errors)
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# --- MODELS ---
class FolderCreate(BaseModel):
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
@app.get("/api/files/{folder_id}")
def get_files(folder_id: str, username: str):
    """Returns files in a specific folder."""
    files = DataManager.list_files(username, folder_id)
    return files

class FileUpdateRequest(BaseModel):
    username: str
    folder_id: str
    file_id: str
    content: Any


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
    "gemini-2.0-flash": {"in": 1.4, "out": 4.0},
    "gemini-1.5-flash": {"in": 1.0, "out": 3.0},
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

@app.get("/api/files/download_audio")
def download_audio(username: str, folder_id: str, filename: str):
    """Downloads an audio file."""
    from fastapi.responses import FileResponse
    path = DataManager.get_audio_path(filename, username, folder_id)
    if not path or not os.path.exists(path):
        raise HTTPException(status_code=404, detail="Audiodatei nicht gefunden")
    return FileResponse(path, media_type="audio/mpeg", filename=filename)

@app.get("/api/files/download_pdf")
def download_pdf(username: str, folder_id: str, filename: str = "", file_id: Optional[str] = None):
    """Downloads or displays a PDF file."""
    from fastapi.responses import Response
    pdf_bytes, download_name = DataManager.get_pdf_bytes(filename, username, folder_id, file_id=file_id)
    if not pdf_bytes:
        raise HTTPException(status_code=404, detail="PDF-Datei nicht gefunden")
    safe_name = (download_name or "dokument.pdf").replace('"', '')
    return Response(
        content=pdf_bytes,
        media_type="application/pdf",
        headers={
            "Content-Disposition": f'inline; filename="{safe_name}"',
            "Cache-Control": "no-store",
        },
    )

@app.delete("/api/files/{file_id}")
def delete_file(file_id: str, username: str, folder_id: str):
    """Deletes a file from the server and metadata."""
    if DataManager.delete_file(username, folder_id, file_id):
        return {"status": "success", "message": "Datei gelöscht"}
    raise HTTPException(status_code=404, detail="Datei nicht gefunden oder Fehler beim Löschen")

class UpdateFileRequest(BaseModel):
    username: str
    folder_id: str
    file_id: str
    content: Any
    
@app.put("/api/files/update")
def update_file(request: UpdateFileRequest):
    """Updates the content of an existing file/material."""
    try:
        if isinstance(request.content, list) and not _is_valid_flashcards_payload(request.content):
            raise HTTPException(status_code=400, detail="Ungültiges Flashcard-Format: erwartet Liste mit {front, back}.")
        # We need a method in DataManager to update content
        if DataManager.update_file_content(request.username, request.folder_id, request.file_id, request.content):
            return {"status": "success", "message": "Gespeichert"}
        raise HTTPException(status_code=404, detail="Datei nicht gefunden")
    except Exception as e:
        print(f"Update File Error: {e}")
        raise HTTPException(status_code=500, detail=f"Fehler: {str(e)}")

class RenameFileRequest(BaseModel):
    username: str
    name: str

@app.put("/api/files/{folder_id}/{file_id}/rename")
def rename_file(folder_id: str, file_id: str, request: RenameFileRequest):
    try:
        if DataManager.rename_file_metadata(request.username, folder_id, file_id, request.name):
            return {"status": "success", "message": "Umbenannt"}
        raise HTTPException(status_code=404, detail="Datei nicht gefunden")
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Fehler beim Umbenennen: {str(e)}")

class MoveFileRequest(BaseModel):
    username: str
    target_folder_id: str

@app.put("/api/files/{folder_id}/{file_id}/move")
def move_file(folder_id: str, file_id: str, request: MoveFileRequest):
    try:
        if DataManager.move_file(request.username, folder_id, file_id, request.target_folder_id):
            return {"status": "success", "message": "Verschoben"}
        raise HTTPException(status_code=404, detail="Fehler beim Verschieben")
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Fehler: {str(e)}")

class GenRequest(BaseModel):
    username: str
    folder_id: str
    model_preference: str = None

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

@app.post("/api/ai/plan")
def create_study_plan(request: PlanRequest):
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


def _get_folder_context(username: str, folder_id: str):
    """
    Aggregates material from the folder.
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
        quiz_result = AIService.generate_quiz(context, model_preference=model_pref, return_meta=True)
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
def create_flashcards(request: GenRequest):
    from ai_service import AIService
    from datetime import datetime
    try:
        ensure_minimum_tokens(request.username, 1)
        _configure_genai()
        model_pref = resolve_model_preference(request.username, request.model_preference)
        context, debug_log = _get_folder_context(request.username, request.folder_id)
        if not context:
            raise HTTPException(status_code=400, detail=f"Kein Material gefunden. Debug: {'; '.join(debug_log)}")
        cards_result = AIService.generate_flashcards(context, model_preference=model_pref, return_meta=True)
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
        base_content = target.get("content")
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


if __name__ == "__main__":
    port = int(os.environ.get("PORT", 8000))
    uvicorn.run("main:app", host="0.0.0.0", port=port, reload=True)
