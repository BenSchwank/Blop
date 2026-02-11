import hashlib
import json
import os
import streamlit as st
from datetime import datetime

USER_DB_FILE = "user_data/users.json"

class AuthManager:
    @staticmethod
    def _load_users():
        if not os.path.exists(USER_DB_FILE):
            return {}
        try:
            with open(USER_DB_FILE, "r", encoding="utf-8") as f:
                return json.load(f)
        except:
            return {}

    @staticmethod
    def _save_users(users):
        if not os.path.exists("user_data"):
            os.makedirs("user_data")
        with open(USER_DB_FILE, "w", encoding="utf-8") as f:
            json.dump(users, f, indent=4)

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
        return AuthManager._load_users()

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
            if username in users:
                users[username]["password"] = AuthManager._hash_password(new_password)
                AuthManager._save_users(users)
                return True
            return False
        except Exception as e:
            print(f"Password Reset Error: {e}")
            return False

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
        if "authenticated" not in st.session_state:
            st.session_state.authenticated = False
        return st.session_state.authenticated

    @staticmethod
    def logout():
        st.session_state.authenticated = False
        st.session_state.username = None
        st.rerun()
