from fastapi import FastAPI, HTTPException, Body, UploadFile, File
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
from typing import List, Optional
import uvicorn
import os

# Import local modules
from data_manager import DataManager
from auth_manager import AuthManager

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

# --- UPLOAD & AI ENDPOINTS ---

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

class PlanRequest(BaseModel):
    username: str
    folder_id: str
    duration_days: int

@app.post("/api/ai/plan")
def create_study_plan(request: PlanRequest):
    """Generates a study plan from folder contents."""
    from ai_service import AIService
    
    full_text = _get_folder_context(request.username, request.folder_id)

    if not full_text:
        raise HTTPException(status_code=400, detail="Kein Textmaterial im Ordner gefunden (Lade PDFs oder YouTube-Videos hoch).")

    # 2. Generate Plan
    plan = AIService.generate_study_plan(full_text, request.duration_days)
    
    # 3. Save Plan (Auto-Save + File)
    DataManager.save_plan(plan, request.username, request.folder_id)
    
    return {"status": "success", "plan": plan}

def _get_folder_context(username: str, folder_id: str) -> str:
    """Helper to aggregate text from all files in a folder."""
    files = DataManager.list_files(username, folder_id)
    full_text = ""
    
    for f in files:
        # 1. YouTube Transcripts
        if f.get("type") == "transcript":
             if "content" in f:
                 full_text += f"--- Transkript: {f.get('name')} ---\n{f['content']}\n\n"
        
        # 2. PDFs
        elif f.get("type") == "pdf":
            try:
                from pypdf import PdfReader
                import os
                # Get file path (handles local/cloud diffs)
                pdf_path = DataManager.get_pdf_path(f["name"], username, folder_id)
                
                if pdf_path and os.path.exists(pdf_path):
                    reader = PdfReader(pdf_path)
                    text = ""
                    for page in reader.pages:
                        text += page.extract_text() + "\n"
                    full_text += f"--- PDF: {f.get('name')} ---\n{text}\n\n"
            except Exception as e:
                print(f"Error reading PDF {f.get('name')}: {e}")
                
    return full_text

@app.post("/api/ai/quiz")
def create_quiz(request: PlanRequest):
    """Generates a quiz from folder contents."""
    from ai_service import AIService
    import json
    from datetime import datetime

    full_text = _get_folder_context(request.username, request.folder_id)
    
    if not full_text:
        raise HTTPException(status_code=400, detail="Kein Textmaterial gefunden.")
        
    quiz = AIService.generate_quiz(full_text)
    
    # Save as file
    file_id = f"quiz_{int(datetime.now().timestamp())}"
    DataManager.save_file_metadata({
        "id": file_id, "name": "AI Quiz", "type": "quiz", "content": quiz, "created_at": datetime.now().strftime("%Y-%m-%d")
    }, request.username, request.folder_id)
    
    return {"status": "success", "quiz": quiz}

@app.post("/api/ai/flashcards")
def create_flashcards(request: GenRequest):
    from ai_service import AIService
    full_text = _get_folder_context(request.username, request.folder_id)
    
    if not full_text: raise HTTPException(status_code=400, detail="Kein Textmaterial gefunden.")
        
    cards = AIService.generate_flashcards(full_text)
    
    DataManager.save_file_metadata({
        "id": f"cards_{int(datetime.now().timestamp())}",
        "name": "Generierte Karteikarten",
        "type": "flashcards",
        "content": cards,
        "created_at": datetime.now().strftime("%Y-%m-%d")
    }, request.username, request.folder_id)

    return cards

@app.post("/api/ai/summary")
def create_summary(request: GenRequest):
    from ai_service import AIService
    full_text = _get_folder_context(request.username, request.folder_id)
    
    if not full_text: raise HTTPException(status_code=400, detail="Kein Textmaterial gefunden.")
        
    summary = AIService.generate_summary(full_text)
    
    # Save summary 
    DataManager.save_generated_summary(summary, request.username, request.folder_id)
    
    return {"status": "success", "summary": summary}

if __name__ == "__main__":
    port = int(os.environ.get("PORT", 8000))
    uvicorn.run("main:app", host="0.0.0.0", port=port, reload=True)
