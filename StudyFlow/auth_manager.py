import hashlib
import json
import os
import streamlit as st
from datetime import datetime
import uuid

USER_DB_FILE = "user_data/users.json"
SESSION_DB_FILE = "user_data/sessions.json"

class AuthManager:
    _db_cache = None
    _sessions_cache = None
    _sessions_mtime = 0
    @staticmethod
    def _load_sessions():
        # Check mtime
        if os.path.exists(SESSION_DB_FILE):
             mtime = os.path.getmtime(SESSION_DB_FILE)
             if AuthManager._sessions_cache is not None and mtime == AuthManager._sessions_mtime:
                 return AuthManager._sessions_cache
             
             try:
                 with open(SESSION_DB_FILE, "r", encoding="utf-8") as f:
                     AuthManager._sessions_cache = json.load(f)
                     AuthManager._sessions_mtime = mtime
                     return AuthManager._sessions_cache
             except: pass
        return {}

    @staticmethod
    def _save_sessions(sessions):
        if not os.path.exists("user_data"):
            os.makedirs("user_data")
        with open(SESSION_DB_FILE, "w", encoding="utf-8") as f:
            json.dump(sessions, f, indent=4)

    @staticmethod
    def create_session(username):
        session_id = str(uuid.uuid4())
        sessions = AuthManager._load_sessions()
        
        # Clean up old sessions first (simple housekeeping)
        # AuthManager._cleanup_sessions(sessions) 
        
        sessions[session_id] = {
            "username": username,
            "created_at": datetime.now().isoformat(),
            "last_active": datetime.now().isoformat()
        }
        AuthManager._save_sessions(sessions)
        return session_id

    @staticmethod
    def validate_session(session_id):
        sessions = AuthManager._load_sessions()
        if session_id not in sessions:
            return None
            
        session = sessions[session_id]
        
        # Check Timestamp (4 mins = 240 seconds)
        try:
            last_active = datetime.fromisoformat(session["last_active"])
            if (datetime.now() - last_active).total_seconds() > 240:
                # Expired
                del sessions[session_id]
                AuthManager._save_sessions(sessions)
                return None
        except:
             return None # Invalid format
            
        # Update activity
        session["last_active"] = datetime.now().isoformat()
        AuthManager._save_sessions(sessions)
        return session["username"]

    @staticmethod
    def logout_session(session_id):
        sessions = AuthManager._load_sessions()
        if session_id in sessions:
            del sessions[session_id]
            AuthManager._save_sessions(sessions)

    @staticmethod
    def _load_users():
        # 1. Load Local
        local_users = {}
        if os.path.exists(USER_DB_FILE):
             try:
                 with open(USER_DB_FILE, "r", encoding="utf-8") as f:
                     local_users = json.load(f)
             except: pass
        
        # 2. Sync with Cloud (Auth Sync)
        # We try to fetch from a special 'auth_store' document in Firestore
        from data_manager import DataManager
        db = DataManager._init_firestore()
        if db:
            try:
                # We store all auth data in ONE document for simplicity (like users.json)
                # Collection: config, Doc: auth_users
                doc = db.collection("config").document("auth_users").get()
                if doc.exists:
                    cloud_users = doc.to_dict()
                    # Merge: Cloud wins? Or Local wins?
                    # Let's say Cloud is source of truth if we are "syncing".
                    # But if we just registered locally, we want to keep it.
                    # Best: Merge, preferring Cloud for existing keys?
                    # Actually, if we rebooted, local is empty. Cloud restores it.
                    for u, data in cloud_users.items():
                        if u not in local_users:
                            local_users[u] = data
                        # If in both, we trust Cloud (maybe user changed PW on another device?)
                        # local_users[u] = data 
            except Exception as e:
                print(f"Auth Sync Error: {e}")
                
        return local_users

    @staticmethod
    def _save_users(users):
        # 1. Save Local
        if not os.path.exists("user_data"):
            os.makedirs("user_data")
        with open(USER_DB_FILE, "w", encoding="utf-8") as f:
            json.dump(users, f, indent=4)
            
        # 2. Save Cloud (Auth Sync)
        from data_manager import DataManager
        db = DataManager._init_firestore()
        if db:
            try:
                db.collection("config").document("auth_users").set(users)
            except Exception as e:
                print(f"Auth Sync Write Error: {e}")

    @staticmethod
    def _hash_password(password):
        return hashlib.sha256(password.encode()).hexdigest()

    @staticmethod
    def login(username, password):
        AuthManager.ensure_admin() # Ensure admin exists before login check
        users = AuthManager._load_users()
        if username not in users:
            return False
        
        hashed_pw = AuthManager._hash_password(password)
        if users[username]["password"] == hashed_pw:
            return True
        return False

    @staticmethod
    def register(username, password):
        users = AuthManager._load_users()
        if username in users:
            return False # User exists locally
            
        # Check Cloud (Firestore) to prevent claiming existing cloud user
        from data_manager import DataManager
        if DataManager.user_exists(username):
            return False # User exists in cloud
        
        users[username] = {
            "password": AuthManager._hash_password(password),
            "created_at": datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        }
        AuthManager._save_users(users)
        return True

    @staticmethod
    def get_all_users():
        # Get Local/Synced Users
        users = AuthManager._load_users()
        
        # Get Orphaned Cloud Users (Have Data but no Auth)
        from data_manager import DataManager
        cloud_user_ids = DataManager.get_all_cloud_users()
        
        for uid in cloud_user_ids:
            if uid not in users and uid != "config": # Exclude config collection
                # Add placeholder for Admin Panel visibility
                users[uid] = {
                    "password": "CLOUD_ONLY_NO_PASSWORD",
                    "created_at": "Unknown (Cloud Data)",
                    "is_cloud_only": True
                }
        return users

    @staticmethod
    def set_user_api_key(username, api_key):
        users = AuthManager._load_users()
        if username in users:
            users[username]["api_key"] = api_key
            AuthManager._save_users(users)
            return True
        return False

    @staticmethod
    def get_user_api_key(username):
        users = AuthManager._load_users()
        if username in users:
            return users[username].get("api_key")
        return None

    @staticmethod
    def delete_user(username):
        users = AuthManager._load_users()
        if username in users:
            del users[username]
            AuthManager._save_users(users)
            # Cleanup Data
            from data_manager import DataManager
            DataManager.delete_user_data(username)
            return True
        return False

    @staticmethod
    def reset_password_force(username, new_password):
        """Resets a user's password to a specific value immediately."""
        try:
            users = AuthManager._load_users()
            
            # If user is not in local/auth store, we CREATE them (Adopting cloud user)
            if username not in users:
                users[username] = {
                    "created_at": datetime.now().strftime("%Y-%m-%d %H:%M:%S")
                }
                
            users[username]["password"] = AuthManager._hash_password(new_password)
            users[username]["is_cloud_only"] = False # Remove flag if present
            
            AuthManager._save_users(users)
            return True
        except Exception as e:
            print(f"Password Reset Error: {e}")
            return False

    @staticmethod
    def change_password(username, old_password, new_password):
        """Allows a user to change their own password verify old password first."""
        try:
            users = AuthManager._load_users()
            if username not in users:
                return False, "Benutzer nicht gefunden."
            
            # Verify Old Password
            user_data = users[username]
            hashed_old = AuthManager._hash_password(old_password)
            
            # Handle cases where password might be missing (cloud-only adopted users)
            current_stored_pw = user_data.get("password")
            
            if current_stored_pw and current_stored_pw != hashed_old:
                return False, "Altes Passwort ist falsch."
            
            # Update Password
            users[username]["password"] = AuthManager._hash_password(new_password)
            # Ensure "is_cloud_only" is removed if present
            if "is_cloud_only" in users[username]:
                del users[username]["is_cloud_only"]
                
            AuthManager._save_users(users)
            return True, "Passwort erfolgreich geändert!"
            
        except Exception as e:
            return False, f"Fehler: {str(e)}"

    @staticmethod
    def ensure_admin():
        users = AuthManager._load_users()
        admin_user = "admin_"
        target_pw = "Martin400!"
        target_hash = AuthManager._hash_password(target_pw)
        
        # Create or Reset Admin if missing or wrong password
        if admin_user not in users or users[admin_user]["password"] != target_hash:
            users[admin_user] = {
                "password": target_hash,
                "created_at": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
                "is_admin": True
            }
            AuthManager._save_users(users)

    @staticmethod
    def check_login():
        # 1. Already Authenticated in Session State?
        if st.session_state.get("authenticated"):
            # Update session activity if token exists
            try:
                params = st.query_params
                if "session" in params:
                    AuthManager.validate_session(params["session"]) # Just touch timestamp
            except: pass
            return True
            
        # 2. Check for Session Token in URL (Persistence)
        try:
            params = st.query_params
            if "session" in params:
                 token = params["session"]
                 username = AuthManager.validate_session(token)
                 
                 if username:
                     st.session_state.authenticated = True
                     st.session_state.username = username
                     st.success(f"Willkommen zurück, {username}!")
                     return True
                 else:
                     # Invalid Token (Expired or Bad)
                     # We can't clear params easily without rerun, but UI will show login
                     pass
        except: pass
                 
        if "authenticated" not in st.session_state:
            st.session_state.authenticated = False
        return False

    @staticmethod
    def logout():
        # Clear Session Token from URL/DB
        params = st.query_params
        if "session" in params:
            AuthManager.logout_session(params["session"])
            try:
                # Clear all params
                st.query_params.clear()
            except: pass
            
        st.session_state.authenticated = False
        st.session_state.username = None
        st.rerun()
