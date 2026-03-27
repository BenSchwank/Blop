from fastapi import FastAPI, HTTPException, Body, UploadFile, File
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
from typing import List, Optional
import uvicorn
import os
from datetime import datetime
from pathlib import Path

# Import local modules
# Import local modules
from data_manager import DataManager
from auth_manager import AuthManager
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
    """Lightweight check for load balancers / UptimeRobot; must not fail if DB is misconfigured."""
    db = DataManager._init_supabase()
    return {"status": "ok", "database": "supabase" if db else "unconfigured"}

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
    content: str

@app.put("/api/files/update")
def update_file(request: FileUpdateRequest):
    """Updates the content of an existing file."""
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
        "xp": user.get("xp", 0),
        "is_admin": user.get("is_admin", False)
    }

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

def check_and_deduct_tokens(username: str, cost: int):
    tokens = AuthManager.get_tokens(username)
    if tokens < cost:
        raise HTTPException(status_code=402, detail=f"Nicht genügend Tokens. Du hast {tokens}, {cost} werden benötigt.")
    AuthManager.deduct_tokens(username, cost)
    return True

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
             
        check_and_deduct_tokens(username, 5)
        _configure_genai(username)
        
        # Read the file content
        content = await file.read()
        
        # Save audio temporarily for Gemini
        with tempfile.NamedTemporaryFile(delete=False, suffix=Path(file.filename).suffix) as temp_audio:
            temp_audio.write(content)
            temp_audio_path = temp_audio.name
            
        try:
            # Transcribe via Gemini
            result_dict = AIService.transcribe_audio(temp_audio_path)
            
            transcript_text = result_dict.get("content", "")
            audio_title = result_dict.get("title", f"Audio Notiz {datetime.now().strftime('%d.%m.%Y %H:%M')}")
            
            # Persist original audio securely for downloading
            safe_filename = f"audio_{int(datetime.now().timestamp())}{Path(file.filename).suffix}"
            DataManager.save_audio(content, safe_filename, username, folder_id)

            # Save the transcript, linking it to the audio file if needed
            # We add a hidden markdown link or metadata so the frontend knows there's a file
            transcript_text = f"<!-- AUDIO_FILE:{safe_filename} -->\n{transcript_text}"
            
            DataManager.save_transcript(audio_title, transcript_text, username, folder_id)
            
            return {"status": "success", "message": "Audio transkribiert und als Notiz gespeichert"}
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
             
        check_and_deduct_tokens(username, 5)
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
            
            return {"status": "success", "message": "Bild verarbeitet und als Text notiert"}
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
    content: str
    
@app.put("/api/files/update")
def update_file(request: UpdateFileRequest):
    """Updates the content of an existing file/material."""
    try:
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

@app.post("/api/ai/plan")
def create_study_plan(request: PlanRequest):
    """Generates a study plan from folder contents."""
    from ai_service import AIService
    
    try:
        # Check Tokens & Configure API Key
        check_and_deduct_tokens(request.username, 20)
        _configure_genai()
        
        context, debug_log = _get_folder_context(request.username, request.folder_id)

        if not context:
            error_msg = f"Kein Material gefunden. Debug: {'; '.join(debug_log)}"
            print(error_msg)
            raise HTTPException(status_code=400, detail=error_msg)

        # Generate Plan
        plan = AIService.generate_study_plan(context, request.duration_days, request.hours_per_day, model_preference=request.model_preference, active_days=request.active_days, learning_mode=request.learning_mode)
        
        # Save Plan
        DataManager.save_plan(plan, request.username, request.folder_id)
        
        return {"status": "success", "plan": plan}
    
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
def create_quiz(request: GenRequest):
    from ai_service import AIService
    from datetime import datetime
    try:
        check_and_deduct_tokens(request.username, 10)
        _configure_genai()
        context, debug_log = _get_folder_context(request.username, request.folder_id)
        if not context:
            raise HTTPException(status_code=400, detail=f"Kein Material gefunden. Debug: {'; '.join(debug_log)}")
        quiz = AIService.generate_quiz(context, model_preference=request.model_preference)
        file_id = f"quiz_main_{request.folder_id}"
        DataManager.save_file_metadata({
            "id": file_id, "name": "AI Quiz", "type": "quiz", "content": quiz, "created_at": datetime.now().strftime("%Y-%m-%d")
        }, request.username, request.folder_id)
        return {"status": "success", "quiz": quiz}
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
        check_and_deduct_tokens(request.username, 15)
        _configure_genai()
        context, debug_log = _get_folder_context(request.username, request.folder_id)
        if not context:
            raise HTTPException(status_code=400, detail=f"Kein Material gefunden. Debug: {'; '.join(debug_log)}")
        cards = AIService.generate_flashcards(context, model_preference=request.model_preference)
        DataManager.save_file_metadata({
            "id": f"cards_main_{request.folder_id}",
            "name": "Generierte Karteikarten",
            "type": "flashcards",
            "content": cards,
            "created_at": datetime.now().strftime("%Y-%m-%d")
        }, request.username, request.folder_id)
        return {"status": "success", "flashcards": cards}
    except HTTPException:
        raise
    except Exception as e:
        print(f"Flashcards error: {e}")
        raise HTTPException(status_code=500, detail=f"Karteikarten-Fehler: {str(e)}")

@app.post("/api/ai/summary")
def create_summary(request: SummaryRequest):
    from ai_service import AIService
    try:
        check_and_deduct_tokens(request.username, 10)
        _configure_genai()
        context, debug_log = _get_folder_context(request.username, request.folder_id)
        if not context:
            raise HTTPException(status_code=400, detail=f"Kein Material gefunden. Debug: {'; '.join(debug_log)}")
        summary = AIService.generate_summary(context, detail_level=request.detail_level, model_preference=request.model_preference, learning_mode=request.learning_mode)
        DataManager.save_generated_summary(summary, request.username, request.folder_id)
        return {"status": "success", "summary": summary}
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
        check_and_deduct_tokens(request.username, 10)
        _configure_genai()
        context, debug_log = _get_folder_context(request.username, request.folder_id)
        if not context:
            raise HTTPException(status_code=400, detail=f"Kein Material gefunden. Debug: {'; '.join(debug_log)}")
        
        elaboration_text = AIService.generate_elaboration(context, detail_level=request.detail_level, custom_rules=request.custom_rules, model_preference=request.model_preference)
        
        DataManager.save_file_metadata({
            "id": f"elaboration_{int(datetime.now().timestamp())}",
            "name": "Ausarbeitung",
            "type": "summary", # Reusing 'summary' rich text editor styling for now
            "content": elaboration_text,
            "created_at": datetime.now().strftime("%Y-%m-%d")
        }, request.username, request.folder_id)
        
        return {"status": "success", "elaboration": elaboration_text}
    except HTTPException:
        raise
    except Exception as e:
        print(f"Elaboration error: {e}")
        raise HTTPException(status_code=500, detail=f"Ausarbeitungs-Fehler: {str(e)}")

@app.post("/api/ai/repetition")
def create_repetition(request: RepetitionRequest):
    from ai_service import AIService
    from datetime import datetime
    try:
        check_and_deduct_tokens(request.username, 15)
        _configure_genai()
        context, debug_log = _get_folder_context(request.username, request.folder_id)
        if not context:
            raise HTTPException(status_code=400, detail=f"Kein Material gefunden. Debug: {'; '.join(debug_log)}")
        
        repetition_text = AIService.generate_repetition(context, custom_rules=request.custom_rules, model_preference=request.model_preference, learning_mode=request.learning_mode)
        
        DataManager.save_file_metadata({
            "id": f"repetition_{int(datetime.now().timestamp())}",
            "name": "Wiederholungsbogen",
            "type": "repetition", 
            "content": repetition_text,
            "created_at": datetime.now().strftime("%Y-%m-%d")
        }, request.username, request.folder_id)
        
        return {"status": "success", "repetition": repetition_text}
    except HTTPException:
        raise
    except Exception as e:
        print(f"Repetition error: {e}")
        raise HTTPException(status_code=500, detail=f"Wiederholungs-Fehler: {str(e)}")

@app.post("/api/ai/task-help")
def get_task_help(request: TaskHelpRequest):
    from ai_service import AIService
    try:
        check_and_deduct_tokens(request.username, 3)
        _configure_genai()
        context, debug_log = _get_folder_context(request.username, request.folder_id)
        if not context:
            raise HTTPException(status_code=400, detail=f"Kein Material gefunden. Debug: {'; '.join(debug_log)}")
        
        help_text = AIService.generate_task_help(context, request.task_description, request.model_preference)
        return {"status": "success", "help_text": help_text}
    except HTTPException:
        raise
    except Exception as e:
        print(f"Task Help error: {e}")
        raise HTTPException(status_code=500, detail=f"Aufgabenhilfe-Fehler: {str(e)}")

@app.post("/api/ai/chat")
def chat_endpoint(request: ChatRequest):
    from ai_service import AIService
    try:
        check_and_deduct_tokens(request.username, 2)
        _configure_genai()
        context, debug_log = _get_folder_context(request.username, request.folder_id)
        if not context:
            # Fallback to general chat if no context is found, but pass empty list
            context = []
        
        # Convert history
        history_dicts = [{"role": msg.role, "content": msg.content} for msg in request.history]
        
        reply = AIService.chat(
            content=context, 
            user_message=request.message, 
            history=history_dicts, 
            model_preference=request.model_preference,
            active_file=request.active_file
        )
        return {"status": "success", "reply": reply}
    except HTTPException:
        raise
    except Exception as e:
        print(f"Chat error: {e}")
        raise HTTPException(status_code=500, detail=f"Chatbot-Fehler: {str(e)}")


if __name__ == "__main__":
    port = int(os.environ.get("PORT", 8000))
    uvicorn.run("main:app", host="0.0.0.0", port=port, reload=True)
