import hashlib
import json
import os
from datetime import datetime
import uuid

SESSION_DB_FILE = "user_data/sessions.json"

class AuthManager:
    _sessions_cache = None
    _sessions_mtime = 0

    @staticmethod
    def _get_db():
        from data_manager import DataManager
        return DataManager._init_supabase()

    @staticmethod
    def _load_sessions():
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
        AuthManager._sessions_cache = sessions
        AuthManager._sessions_mtime = os.path.getmtime(SESSION_DB_FILE)

    @staticmethod
    def create_session(username):
        session_id = str(uuid.uuid4())
        sessions = AuthManager._load_sessions()
        
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
        
        try:
            last_active = datetime.fromisoformat(session["last_active"])
            # Extended session timeout for production (e.g. 24 hours)
            if (datetime.now() - last_active).total_seconds() > 86400:
                del sessions[session_id]
                AuthManager._save_sessions(sessions)
                return None
        except:
             return None 
            
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
    def _hash_password(password):
        return hashlib.sha256(password.encode()).hexdigest()

    @staticmethod
    def get_user(username):
        db = AuthManager._get_db()
        if not db: return None
        res = db.table('users').select('*').eq('username', username).execute()
        if len(res.data) > 0:
            return res.data[0]
        return None

    @staticmethod
    def get_user_by_email(email):
        db = AuthManager._get_db()
        if not db: return None
        res = db.table('users').select('*').eq('email', email).execute()
        if len(res.data) > 0:
            return res.data[0]
        return None

    @staticmethod
    def login(email_or_username, password):
        try:
            AuthManager.ensure_admin()
            db = AuthManager._get_db()
            
            if '@' in email_or_username:
                # New Email-based flow via Supabase Auth
                try:
                    auth_res = db.auth.sign_in_with_password({
                        "email": email_or_username,
                        "password": password
                    })
                    if not auth_res.user:
                        return False, "Login fehlgeschlagen"
                    
                    # Found user, return associated username
                    user = AuthManager.get_user_by_email(email_or_username)
                    if user:
                        return True, user["username"]
                    return False, "Datenbank-Eintrag für diese E-Mail fehlt."
                except Exception as e:
                    err_msg = str(e).lower()
                    if "email not confirmed" in err_msg:
                        return False, "Bitte bestätige zuerst deine E-Mail Adresse über den Link in deinem Postfach."
                    return False, "E-Mail oder Passwort falsch."
            else:
                # Old Username-based fallback
                user = AuthManager.get_user(email_or_username)
                if not user:
                    return False, "Benutzer nicht gefunden"
                
                hashed_pw = AuthManager._hash_password(password)
                if user["password_hash"] == hashed_pw:
                    return True, user["username"]
                return False, "Passwort falsch"
        except Exception as e:
            print(f"AuthManager.login crash: {e}")
            return False, "Ein interner Fehler ist aufgetreten."

    @staticmethod
    def register(username, email, password):
        user = AuthManager.get_user(username)
        if user:
            return False, "Benutzername bereits vergeben"
            
        db = AuthManager._get_db()
        if not db: raise Exception("Supabase DB nicht initialisiert")
        
        existing_email = db.table('users').select('*').eq('email', email).execute()
        if len(existing_email.data) > 0:
            return False, "Diese E-Mail Adresse ist bereits registriert"

        # 1. Register with Supabase Auth
        try:
            auth_res = db.auth.sign_up({
                "email": email,
                "password": password
            })
            if not auth_res.user:
                return False, "Fehler bei der Registrierung"
        except Exception as e:
            return False, f"Auth Fehler: {str(e)}"
        
        # 2. Store in our custom users table
        new_user = {
            "username": username,
            "email": email,
            "auth_id": auth_res.user.id,
            "password_hash": AuthManager._hash_password(password),
            "tokens": 500, # Initial tokens
            "xp": 0,
            "streak_days": 1,
            "is_admin": False,
            "created_at": datetime.now().isoformat()
        }
        
        db.table('users').insert(new_user).execute()
        return True, "Registrierung erfolgreich! Bitte überprüfe dein E-Mail Postfach, um deinen Account zu aktivieren."

    @staticmethod
    def get_tokens(username):
        user = AuthManager.get_user(username)
        if not user: return 0
        
        # Admins have infinite tokens implicitly
        if user.get("is_admin", False):
            return 999999
            
        return user.get("tokens", 0)

    @staticmethod
    def deduct_tokens(username, amount):
        user = AuthManager.get_user(username)
        if not user: return False
        
        if user.get("is_admin", False):
            return True # Admins don't pay
            
        current_tokens = user.get("tokens", 0)
        if current_tokens < amount:
            return False
            
        db = AuthManager._get_db()
        new_balance = current_tokens - amount
        db.table('users').update({"tokens": new_balance}).eq('username', username).execute()
        return True

    @staticmethod
    def add_xp(username, amount):
        user = AuthManager.get_user(username)
        if not user: return 0
        
        new_xp = user.get("xp", 0) + amount
        db = AuthManager._get_db()
        db.table('users').update({"xp": new_xp}).eq('username', username).execute()
        return new_xp

    @staticmethod
    def update_activity(username):
        user = AuthManager.get_user(username)
        if not user: return
        # A simple placeholder. To implement proper streaks, we need last_active_date in db schema.
        # Since it's not in schema MVP, we just bump streak if needed, but skip for now to keep it simple.
        pass

    @staticmethod
    def get_leaderboard_data(limit=5):
        db = AuthManager._get_db()
        if not db: return []
        res = db.table('users').select('username', 'xp', 'streak_days').eq('is_admin', False).order('xp', desc=True).limit(limit).execute()
        return res.data

    @staticmethod
    def get_all_users():
        db = AuthManager._get_db()
        if not db: return {}
        res = db.table('users').select('*').execute()
        
        # Convert to dictionary format for legacy API expectations
        users_dict = {}
        for r in res.data:
            users_dict[r['username']] = r
        return users_dict

    @staticmethod
    def delete_user(username):
        db = AuthManager._get_db()
        if not db: return False
        res = db.table('users').delete().eq('username', username).execute()
        return True

    @staticmethod
    def change_password(username, old_password, new_password):
        user = AuthManager.get_user(username)
        if not user:
            return False, "Benutzer nicht gefunden."
        
        hashed_old = AuthManager._hash_password(old_password)
        if user["password_hash"] != hashed_old:
            return False, "Altes Passwort ist falsch."
        
        db = AuthManager._get_db()
        db.table('users').update({"password_hash": AuthManager._hash_password(new_password)}).eq('username', username).execute()
        return True, "Passwort erfolgreich geändert!"

    @staticmethod
    def ensure_admin():
        admin_user = "admin_"
        target_pw = "Martin400!"
        target_hash = AuthManager._hash_password(target_pw)
        
        db = AuthManager._get_db()
        if not db: return
        
        res = db.table('users').select('password_hash').eq('username', admin_user).execute()
        if len(res.data) == 0:
            # Admin doesn't exist — create
            db.table('users').insert({
                "username": admin_user,
                "password_hash": target_hash,
                "tokens": 999999,
                "is_admin": True
            }).execute()
        elif res.data[0]['password_hash'] != target_hash:
            # Admin exists but hash is wrong (e.g. changed from another device) — force reset
            db.table('users').update({
                "password_hash": target_hash,
                "is_admin": True,
                "tokens": 999999
            }).eq('username', admin_user).execute()

