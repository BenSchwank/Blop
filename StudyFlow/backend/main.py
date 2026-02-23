from fastapi import FastAPI, HTTPException, Body, UploadFile, File
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
from typing import List, Optional
import uvicorn
import os

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
    password: str

@app.get("/")
def read_root():
    return {"message": "Blop Study Backend Running", "version": "1.0.0"}

@app.get("/api/health")
def health_check():
    return {"status": "ok", "database": "firebase" if DataManager._init_firestore() else "local"}

@app.get("/api/ping")
def ping():
    return {"status": "pong", "message": "Backend is online!"}

# --- AUTH ENDPOINTS ---
@app.post("/api/auth/login")
def login(request: LoginRequest):
    """Login endpoint - returns session token"""
    if AuthManager.login(request.username, request.password):
        session_id = AuthManager.create_session(request.username)
        return {
            "success": True,
            "session_id": session_id,
            "username": request.username
        }
    raise HTTPException(status_code=401, detail="Ungültige Anmeldedaten")

@app.post("/api/auth/register")
def register(request: RegisterRequest):
    """Register new user"""
    if AuthManager.register(request.username, request.password):
        return {"success": True, "message": "Registrierung erfolgreich"}
    raise HTTPException(status_code=400, detail="Benutzername bereits vergeben")

@app.post("/api/auth/logout")
def logout(session_id: str):
    """Logout - invalidate session"""
    AuthManager.logout_session(session_id)
    return {"success": True}

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
    """Returns list of folders for a user."""
    data = DataManager.load(username)
    return data.get("folders", [])

@app.delete("/api/folders/{folder_id}")
def delete_folder(folder_id: str, username: str):
    """Deletes a folder."""
    if DataManager.delete_folder(folder_id, username):
        return {"status": "success", "message": "Ordner gelöscht"}
    raise HTTPException(status_code=404, detail="Ordner nicht gefunden")

@app.post("/api/folders")
def create_folder(folder: FolderCreate):
    """Creates a new folder."""
    try:
        DataManager.create_folder(folder.name, folder.username)
        return {"status": "success", "folder": folder.name}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

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

# --- UPLOAD & AI ENDPOINTS ---

class APIKeyRequest(BaseModel):
    username: str
    api_key: str

@app.post("/api/auth/apikey")
def save_api_key(request: APIKeyRequest):
    """Saves the user's Google API Key."""
    DataManager.save_api_key(request.username, request.api_key)
    return {"status": "success", "message": "API Key gespeichert"}

@app.delete("/api/auth/apikey/{username}")
def remove_api_key(username: str):
    """Removes the user's Google API Key."""
    if DataManager.delete_api_key(username):
        return {"status": "success", "message": "API Key entfernt"}
    raise HTTPException(status_code=404, detail="API Key nicht gefunden")

def _configure_genai(username: str):
    """Configures GenAI with User Key (priority) or Env Key."""
    user_key = DataManager.get_api_key(username)
    env_key = os.environ.get("GOOGLE_API_KEY")
    
    active_key = user_key if user_key else env_key
    
    if not active_key:
        raise HTTPException(status_code=400, detail="NO_API_KEY_FOUND")
        
    genai.configure(api_key=active_key)
    return active_key

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
        from youtube_transcript_api import YouTubeTranscriptApi
        
        # Extract Video ID
        video_id = ""
        if "v=" in url:
            video_id = url.split("v=")[1].split("&")[0]
        elif "youtu.be/" in url:
            video_id = url.split("youtu.be/")[1].split("?")[0]
            
        if not video_id:
            raise HTTPException(status_code=400, detail="Ungültige YouTube URL")
            
        # Get Transcript
        transcript_list = YouTubeTranscriptApi.get_transcript(video_id, languages=['de', 'en'])
        transcript_text = " ".join([t['text'] for t in transcript_list])
        
        # Get Video Title (Mock or requires API Key, using generic for now)
        video_title = f"YouTube: {video_id}"
        
        # Save as text file/transcript
        DataManager.save_transcript(video_title, transcript_text, username, folder_id)
        
        return {"status": "success", "message": "Transkript importiert"}
        
    except Exception as e:
        print(f"YouTube Error: {e}")
        raise HTTPException(status_code=500, detail=f"Fehler beim Import: {str(e)}")

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
             
        _configure_genai(username)
        
        # Save audio to temp file for Gemini upload
        with tempfile.NamedTemporaryFile(delete=False, suffix=".webm") as temp_audio:
            content = await file.read()
            temp_audio.write(content)
            temp_audio_path = temp_audio.name
            
        try:
            # Transcribe via Gemini
            transcript_text = AIService.transcribe_audio(temp_audio_path)
            
            # Save the transcript as if it were a Youtube video
            audio_title = f"Sprachmemo {datetime.now().strftime('%d.%m.%Y %H:%M')}"
            DataManager.save_transcript(audio_title, transcript_text, username, folder_id)
            
            return {"status": "success", "message": "Audio transkribiert und gespeichert"}
        finally:
             if os.path.exists(temp_audio_path):
                 os.remove(temp_audio_path)
                 
    except HTTPException:
        raise
    except Exception as e:
        print(f"Audio Upload Error: {e}")
        raise HTTPException(status_code=500, detail=f"Fehler bei der Audio-Verarbeitung: {str(e)}")

class GenRequest(BaseModel):
    username: str
    folder_id: str
    model_preference: str = None

class SummaryRequest(BaseModel):
    username: str
    folder_id: str
    detail_level: str = "Normal"
    model_preference: str = None

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

class PlanRequest(BaseModel):
    username: str
    folder_id: str
    duration_days: int
    hours_per_day: float = 2.0  # Default 2 hours per day
    active_days: list[int] = None # List of active weekdays (0=Mo, 6=Su). If None, all days are active.
    model_preference: str = None

@app.post("/api/ai/plan")
def create_study_plan(request: PlanRequest):
    """Generates a study plan from folder contents."""
    from ai_service import AIService
    
    try:
        # Configure API Key
        _configure_genai(request.username)
        
        context, debug_log = _get_folder_context(request.username, request.folder_id)

        if not context:
            error_msg = f"Kein Material gefunden. Debug: {'; '.join(debug_log)}"
            print(error_msg)
            raise HTTPException(status_code=400, detail=error_msg)

        # Generate Plan
        plan = AIService.generate_study_plan(context, request.duration_days, request.hours_per_day, model_preference=request.model_preference, active_days=request.active_days)
        
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
        _configure_genai(request.username)
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
        _configure_genai(request.username)
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
        _configure_genai(request.username)
        context, debug_log = _get_folder_context(request.username, request.folder_id)
        if not context:
            raise HTTPException(status_code=400, detail=f"Kein Material gefunden. Debug: {'; '.join(debug_log)}")
        summary = AIService.generate_summary(context, detail_level=request.detail_level, model_preference=request.model_preference)
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
        _configure_genai(request.username)
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
        _configure_genai(request.username)
        context, debug_log = _get_folder_context(request.username, request.folder_id)
        if not context:
            raise HTTPException(status_code=400, detail=f"Kein Material gefunden. Debug: {'; '.join(debug_log)}")
        
        repetition_text = AIService.generate_repetition(context, custom_rules=request.custom_rules, model_preference=request.model_preference)
        
        DataManager.save_file_metadata({
            "id": f"repetition_{int(datetime.now().timestamp())}",
            "name": "Wiederholungsbogen",
            "type": "summary", # Reusing 'summary' rich text editor styling
            "content": repetition_text,
            "created_at": datetime.now().strftime("%Y-%m-%d")
        }, request.username, request.folder_id)
        
        return {"status": "success", "repetition": repetition_text}
    except HTTPException:
        raise
    except Exception as e:
        print(f"Repetition error: {e}")
        raise HTTPException(status_code=500, detail=f"Wiederholungs-Fehler: {str(e)}")


if __name__ == "__main__":
    port = int(os.environ.get("PORT", 8000))
    uvicorn.run("main:app", host="0.0.0.0", port=port, reload=True)
