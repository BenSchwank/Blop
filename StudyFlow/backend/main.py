from fastapi import FastAPI, HTTPException, Body
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

class File(BaseModel):
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

if __name__ == "__main__":
    port = int(os.environ.get("PORT", 8000))
    uvicorn.run("main:app", host="0.0.0.0", port=port, reload=True)
