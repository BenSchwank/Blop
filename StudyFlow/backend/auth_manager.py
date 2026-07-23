import base64
import hashlib
import hmac
import json
import os
import secrets
import time
from datetime import datetime
import uuid

import requests

SESSION_DB_FILE = "user_data/sessions.json"
# Prefix for HMAC-signed session tokens. These survive Render redeploys/cold starts
# (unlike sessions.json on the ephemeral container filesystem).
SIGNED_SESSION_PREFIX = "bs1."
# 7 days — file-based sessions used to be 24h sliding, but ephemeral disk made that
# unreliable in production; a longer fixed TTL is better UX for signed tokens.
SIGNED_SESSION_TTL_SEC = 7 * 24 * 3600

class AuthManager:
    _sessions_cache = None
    _sessions_mtime = 0
    # Best-effort logout denylist for signed tokens (process-local; cleared on restart).
    _revoked_jtis = set()

    @staticmethod
    def _get_db():
        from data_manager import DataManager
        return DataManager._init_supabase()

    @staticmethod
    def _b64url_encode(raw: bytes) -> str:
        return base64.urlsafe_b64encode(raw).rstrip(b"=").decode("ascii")

    @staticmethod
    def _b64url_decode(text: str) -> bytes:
        pad = "=" * (-len(text) % 4)
        return base64.urlsafe_b64decode((text + pad).encode("ascii"))

    @staticmethod
    def _session_secret() -> bytes:
        """Derive HMAC key from env. Prefer SESSION_SECRET; fall back to Supabase key."""
        explicit = (os.environ.get("SESSION_SECRET") or "").strip()
        if explicit:
            return explicit.encode("utf-8")
        for key in ("SUPABASE_SERVICE_ROLE_KEY", "SUPABASE_KEY", "BLOP_ADMIN_PASSWORD"):
            val = (os.environ.get(key) or "").strip()
            if val:
                # Bind purpose so a leaked admin password is not a raw session key.
                return hashlib.sha256(f"blop-session-v1:{val}".encode("utf-8")).digest()
        # Local-dev fallback — unstable across restarts; set SESSION_SECRET in prod.
        return hashlib.sha256(b"blop-session-dev-insecure").digest()

    @staticmethod
    def _make_signed_session(username: str) -> str:
        now = int(time.time())
        payload = {
            "u": username,
            "iat": now,
            "exp": now + SIGNED_SESSION_TTL_SEC,
            "jti": str(uuid.uuid4()),
        }
        body = AuthManager._b64url_encode(
            json.dumps(payload, separators=(",", ":"), sort_keys=True).encode("utf-8")
        )
        sig = AuthManager._b64url_encode(
            hmac.new(AuthManager._session_secret(), body.encode("ascii"), hashlib.sha256).digest()
        )
        return f"{SIGNED_SESSION_PREFIX}{body}.{sig}"

    @staticmethod
    def _parse_signed_session(session_id: str):
        """Return username if token is valid, else None."""
        if not session_id or not session_id.startswith(SIGNED_SESSION_PREFIX):
            return None
        rest = session_id[len(SIGNED_SESSION_PREFIX) :]
        parts = rest.split(".")
        if len(parts) != 2:
            return None
        body, sig = parts
        expected = AuthManager._b64url_encode(
            hmac.new(AuthManager._session_secret(), body.encode("ascii"), hashlib.sha256).digest()
        )
        if not hmac.compare_digest(sig, expected):
            return None
        try:
            payload = json.loads(AuthManager._b64url_decode(body).decode("utf-8"))
        except Exception:
            return None
        jti = payload.get("jti")
        if jti and jti in AuthManager._revoked_jtis:
            return None
        try:
            exp = int(payload.get("exp", 0))
        except (TypeError, ValueError):
            return None
        if exp < int(time.time()):
            return None
        username = payload.get("u")
        if not isinstance(username, str) or not username.strip():
            return None
        return username.strip()

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
        # Prefer signed tokens so Render restarts do not wipe active logins.
        return AuthManager._make_signed_session(username)

    @staticmethod
    def validate_session(session_id):
        if not session_id:
            return None

        signed_user = AuthManager._parse_signed_session(session_id)
        if signed_user:
            return signed_user

        # Legacy UUID sessions in local sessions.json (dev / pre-migration).
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
        if session_id and session_id.startswith(SIGNED_SESSION_PREFIX):
            rest = session_id[len(SIGNED_SESSION_PREFIX) :]
            parts = rest.split(".")
            if len(parts) == 2:
                try:
                    payload = json.loads(AuthManager._b64url_decode(parts[0]).decode("utf-8"))
                    jti = payload.get("jti")
                    if jti:
                        AuthManager._revoked_jtis.add(jti)
                except Exception:
                    pass
            return

        sessions = AuthManager._load_sessions()
        if session_id in sessions:
            del sessions[session_id]
            AuthManager._save_sessions(sessions)

    @staticmethod
    def _db_unreachable_message(exc: Exception = None) -> str:
        from urllib.parse import urlparse
        host = ""
        try:
            raw = (os.environ.get("SUPABASE_URL") or "").strip().strip('"').strip("'")
            host = urlparse(raw).hostname or ""
        except Exception:
            host = ""
        base = "Server kann die Datenbank nicht erreichen (DNS)."
        if host:
            base += f" Host: {host}."
        base += " Bitte SUPABASE_URL auf Render prüfen (keine Anführungszeichen, https://xxx.supabase.co)."
        if exc:
            print(f"AuthManager DB unreachable: {exc}")
        return base

    @staticmethod
    def _hash_password(password: str) -> str:
        """Returns pbkdf2_sha256$120000$<salt_hex>$<dk_hex>."""
        salt = secrets.token_hex(16)
        dk = hashlib.pbkdf2_hmac("sha256", password.encode(), salt.encode(), 120000)
        return f"pbkdf2_sha256$120000${salt}${dk.hex()}"

    @staticmethod
    def _verify_password(password: str, stored: str) -> bool:
        """Verify password against stored hash. Supports PBKDF2 and legacy unsalted SHA-256."""
        if not stored:
            return False
        if stored.startswith("pbkdf2_sha256$"):
            parts = stored.split("$")
            if len(parts) != 4:
                return False
            _, iters_str, salt, stored_dk = parts
            try:
                iters = int(iters_str)
            except ValueError:
                return False
            dk = hashlib.pbkdf2_hmac("sha256", password.encode(), salt.encode(), iters)
            return hmac.compare_digest(dk.hex(), stored_dk)
        # Legacy: unsalted SHA-256 (64-char hex)
        if len(stored) == 64:
            candidate = hashlib.sha256(password.encode()).hexdigest()
            return hmac.compare_digest(candidate, stored)
        return False

    @staticmethod
    def _email_from_jwt_unverified(jwt_token: str) -> str:
        try:
            parts = jwt_token.split(".")
            if len(parts) < 2:
                return ""
            payload_b64 = parts[1]
            pad = "=" * (-len(payload_b64) % 4)
            payload = json.loads(base64.urlsafe_b64decode(payload_b64 + pad))
            return (payload.get("email") or "").strip().lower()
        except Exception:
            return ""

    @staticmethod
    def get_user(username):
        db = AuthManager._get_db()
        if not db:
            return None
        try:
            res = db.table('users').select('*').eq('username', username).execute()
            if len(res.data) > 0:
                return res.data[0]
        except Exception as e:
            print(f"AuthManager.get_user failed: {e}")
            raise
        return None

    @staticmethod
    def get_user_by_email(email):
        db = AuthManager._get_db()
        if not db:
            return None
        try:
            res = db.table('users').select('*').eq('email', email).execute()
            if len(res.data) > 0:
                return res.data[0]
        except Exception as e:
            print(f"AuthManager.get_user_by_email failed: {e}")
            raise
        return None

    @staticmethod
    def login(email_or_username, password):
        try:
            try:
                AuthManager.ensure_admin()
            except Exception as admin_err:
                # Never block login because admin bootstrap failed.
                print(f"AuthManager.ensure_admin (non-fatal): {admin_err}")

            db = AuthManager._get_db()
            if not db:
                return False, "Datenbank nicht erreichbar. Bitte später erneut versuchen."
            
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
                    if "name or service not known" in err_msg or "errno -2" in err_msg or "nodename nor servname" in err_msg:
                        return False, AuthManager._db_unreachable_message(e)
                    if "timed out" in err_msg or "connection" in err_msg:
                        return False, "Verbindung zur Anmeldung fehlgeschlagen. Bitte später erneut versuchen."
                    return False, "E-Mail oder Passwort falsch."
            else:
                # Old Username-based fallback
                try:
                    user = AuthManager.get_user(email_or_username)
                except Exception as e:
                    err_msg = str(e).lower()
                    if "name or service not known" in err_msg or "errno -2" in err_msg:
                        return False, AuthManager._db_unreachable_message(e)
                    print(f"AuthManager.login get_user crash: {e}")
                    return False, "Datenbankfehler bei der Anmeldung. Bitte später erneut versuchen."

                if not user:
                    return False, "Benutzer nicht gefunden"

                stored = user.get("password_hash") or ""
                if not isinstance(stored, str):
                    stored = str(stored)
                if AuthManager._verify_password(password, stored):
                    # Rehash legacy unsalted SHA-256 to PBKDF2 on successful login
                    if stored and not stored.startswith("pbkdf2_sha256$"):
                        try:
                            new_hash = AuthManager._hash_password(password)
                            db = AuthManager._get_db()
                            if db:
                                db.table("users").update({"password_hash": new_hash}).eq(
                                    "username", user["username"]
                                ).execute()
                        except Exception as _rehash_err:
                            print(f"AuthManager: rehash failed (non-fatal): {_rehash_err}")
                    return True, user["username"]
                return False, "Passwort falsch"
        except Exception as e:
            print(f"AuthManager.login crash: {e}")
            err_msg = str(e).lower()
            if "name or service not known" in err_msg or "errno -2" in err_msg:
                return False, AuthManager._db_unreachable_message(e)
            return False, "Ein interner Fehler ist aufgetreten."

    @staticmethod
    def register(username, email, password):
        username = (username or "").strip()
        email = (email or "").strip().lower()
        password = password or ""

        if len(username) < 3:
            return False, "Benutzername muss mindestens 3 Zeichen haben"
        if "@" not in email or "." not in email.split("@")[-1]:
            return False, "Bitte gib eine gültige E-Mail-Adresse ein"
        if len(password) < 6:
            return False, "Passwort muss mindestens 6 Zeichen haben"

        try:
            user = AuthManager.get_user(username)
            if user:
                return False, "Benutzername bereits vergeben"

            db = AuthManager._get_db()
            if not db:
                return False, "Registrierung vorübergehend nicht möglich. Bitte später erneut versuchen."

            existing_email = db.table('users').select('*').eq('email', email).execute()
            if existing_email.data and len(existing_email.data) > 0:
                return False, "Diese E-Mail Adresse ist bereits registriert"

            # 1. Register with Supabase Auth
            try:
                auth_res = db.auth.sign_up({
                    "email": email,
                    "password": password
                })
                if not auth_res or not auth_res.user:
                    return False, "Registrierung fehlgeschlagen. Bitte erneut versuchen."
            except Exception as e:
                err = str(e).lower()
                if "rate limit" in err or "email rate" in err or "over_email_send_rate_limit" in err:
                    return False, "Zu viele E-Mails in kurzer Zeit. Bitte ein paar Minuten warten und erneut versuchen."
                if "already registered" in err or "already been registered" in err or "user already exists" in err:
                    return False, "Diese E-Mail Adresse ist bereits registriert"
                if "password" in err and ("weak" in err or "least" in err or "characters" in err):
                    return False, "Passwort ist zu schwach. Bitte mindestens 6 Zeichen verwenden."
                return False, "Anmeldung beim Auth-Dienst fehlgeschlagen. Bitte später erneut versuchen."

            # 2. Store in our custom users table
            new_user = {
                "username": username,
                "email": email,
                "auth_id": auth_res.user.id,
                "password_hash": AuthManager._hash_password(password),
                "tokens": 500, # Initial tokens
                "preferred_model": "",
                "xp": 0,
                "streak_days": 1,
                "is_admin": False,
                "created_at": datetime.now().isoformat()
            }

            try:
                db.table('users').insert(new_user).execute()
            except Exception as e:
                err = str(e).lower()
                if "duplicate" in err or "unique" in err or "already exists" in err:
                    return False, "Benutzername oder E-Mail ist bereits vergeben"
                # Auth user may already exist; surface a recoverable message.
                return False, "Konto konnte nicht vollständig angelegt werden. Bitte Login versuchen oder Support kontaktieren."

            return True, "Registrierung erfolgreich! Bitte überprüfe dein E-Mail Postfach, um deinen Account zu aktivieren."
        except Exception:
            return False, "Registrierung vorübergehend nicht möglich. Bitte später erneut versuchen."

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
    def get_preferred_model(username: str) -> str:
        user = AuthManager.get_user(username)
        if not user:
            return ""
        return str(user.get("preferred_model") or "").strip()

    @staticmethod
    def set_preferred_model(username: str, model_name: str) -> bool:
        db = AuthManager._get_db()
        if not db:
            return False
        cleaned = (model_name or "").strip()
        res = db.table("users").update({"preferred_model": cleaned}).eq("username", username).execute()
        return bool(res.data)

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

        stored = user.get("password_hash", "")
        if not AuthManager._verify_password(old_password, stored):
            return False, "Altes Passwort ist falsch."

        db = AuthManager._get_db()
        db.table("users").update({"password_hash": AuthManager._hash_password(new_password)}).eq(
            "username", username
        ).execute()
        return True, "Passwort erfolgreich geändert!"

    @staticmethod
    def _password_reset_redirect_url():
        explicit = os.environ.get("PASSWORD_RESET_REDIRECT_URL", "").strip()
        if explicit:
            return explicit.rstrip("/")
        base = os.environ.get("BLOP_APP_PUBLIC_URL", "").strip().rstrip("/")
        if base:
            return f"{base}/auth/reset-password"
        return "http://localhost:3000/auth/reset-password"

    @staticmethod
    def request_password_reset(email: str) -> None:
        """Triggers Supabase recovery email. Swallows errors (caller returns generic message)."""
        email = (email or "").strip().lower()
        if not email or "@" not in email:
            return
        db = AuthManager._get_db()
        if not db:
            return
        supabase_url = (os.environ.get("SUPABASE_URL") or "").rstrip("/")
        supabase_key = os.environ.get("SUPABASE_KEY") or ""
        if not supabase_url or not supabase_key:
            return
        redirect = AuthManager._password_reset_redirect_url()
        try:
            db.auth.reset_password_for_email(email, {"redirect_to": redirect})
        except Exception as e:
            # Fallback if method name/signature differs — raw GoTrue recover
            try:
                r = requests.post(
                    f"{supabase_url}/auth/v1/recover",
                    params={"redirect_to": redirect},
                    headers={
                        "apikey": supabase_key,
                        "Content-Type": "application/json",
                    },
                    json={"email": email},
                    timeout=30,
                )
                if r.status_code >= 400:
                    print(f"password reset recover HTTP {r.status_code}: {r.text[:200]}")
            except Exception as e2:
                print(f"password reset failed: {e} | fallback: {e2}")

    @staticmethod
    def complete_password_reset(access_token: str, new_password: str) -> tuple[bool, str]:
        """Sets new password via GoTrue using recovery JWT; syncs public.users.password_hash."""
        token = (access_token or "").strip()
        if not token:
            return False, "Ungültiger Link. Bitte fordere eine neue E-Mail an."
        if len(new_password) < 8:
            return False, "Passwort muss mindestens 8 Zeichen haben."
        supabase_url = (os.environ.get("SUPABASE_URL") or "").rstrip("/")
        supabase_key = os.environ.get("SUPABASE_KEY") or ""
        if not supabase_url or not supabase_key:
            return False, "Serverfehler: Supabase nicht konfiguriert."
        try:
            r = requests.put(
                f"{supabase_url}/auth/v1/user",
                headers={
                    "Authorization": f"Bearer {token}",
                    "apikey": supabase_key,
                    "Content-Type": "application/json",
                },
                json={"password": new_password},
                timeout=30,
            )
        except Exception as e:
            print(f"complete_password_reset request error: {e}")
            return False, "Verbindungsfehler. Bitte später erneut versuchen."
        if r.status_code not in (200, 201):
            detail = (r.text or "")[:300]
            print(f"complete_password_reset HTTP {r.status_code}: {detail}")
            return False, "Link abgelaufen oder ungültig. Bitte fordere eine neue E-Mail an."
        email = ""
        try:
            body = r.json()
            email = (body.get("email") or body.get("user", {}).get("email") or "").strip().lower()
        except Exception:
            pass
        if not email:
            email = AuthManager._email_from_jwt_unverified(token)
        if not email:
            return False, "Antwort der Anmeldung unvollständig."
        row = AuthManager.get_user_by_email(email)
        if row:
            db = AuthManager._get_db()
            if db:
                db.table("users").update(
                    {"password_hash": AuthManager._hash_password(new_password)}
                ).eq("username", row["username"]).execute()
        return True, "Passwort wurde geändert. Du kannst dich jetzt anmelden."

    @staticmethod
    def ensure_admin():
        """Ensure admin_ exists when BLOP_ADMIN_PASSWORD is configured.

        Never raises — login must not depend on admin bootstrap succeeding.
        Never force-resets an existing admin password.
        """
        try:
            admin_user = "admin_"
            admin_pw = os.environ.get("BLOP_ADMIN_PASSWORD", "").strip()

            db = AuthManager._get_db()
            if not db:
                return

            res = db.table("users").select("username").eq("username", admin_user).execute()
            if len(res.data) == 0:
                if not admin_pw:
                    print(
                        "WARNING: Admin user missing but BLOP_ADMIN_PASSWORD is not set "
                        "— skipping admin creation."
                    )
                    return
                row = {
                    "username": admin_user,
                    "password_hash": AuthManager._hash_password(admin_pw),
                    "tokens": 999999,
                    "is_admin": True,
                }
                # email is required in some schemas — use a stable placeholder
                row["email"] = os.environ.get(
                    "BLOP_ADMIN_EMAIL", "admin@localhost"
                ).strip() or "admin@localhost"
                db.table("users").insert(row).execute()
            # Never force-reset an existing admin's password
        except Exception as e:
            print(f"AuthManager.ensure_admin failed (non-fatal): {e}")

