import json
import os
import streamlit as st
from datetime import datetime

DATA_DIR = "user_data"

class DataManager:
    @staticmethod
    def _get_file(username):
        if not os.path.exists(DATA_DIR):
            os.makedirs(DATA_DIR)
        # Sanitize username just in case
        safe_name = "".join([c for c in username if c.isalnum() or c in "-_"])
        return os.path.join(DATA_DIR, f"{safe_name}.json")

    @staticmethod
    def load(username):
        filepath = DataManager._get_file(username)
        if not os.path.exists(filepath):
            return {"folders": [], "files": []}
        try:
            with open(filepath, "r", encoding="utf-8") as f:
                return json.load(f)
        except:
            return {"folders": [], "files": []}

    @staticmethod
    def save(data, username):
        filepath = DataManager._get_file(username)
        with open(filepath, "w", encoding="utf-8") as f:
            json.dump(data, f, ensure_ascii=False, indent=4)

    @staticmethod
    def create_folder(name, username):
        data = DataManager.load(username)
        folder = {
            "id": f"folder_{int(datetime.now().timestamp())}",
            "name": name,
            "created_at": datetime.now().strftime("%Y-%m-%d")
        }
        data["folders"].append(folder)
        DataManager.save(data, username)
        return folder

    @staticmethod
    def save_summary(title, content, username, folder_id=None):
        data = DataManager.load(username)
        file_entry = {
            "id": f"file_{int(datetime.now().timestamp())}",
            "title": title,
            "content": content,
            "folder_id": folder_id,
            "created_at": datetime.now().strftime("%d.%m.%Y")
        }
        data["files"].append(file_entry)
        DataManager.save(data, username)

    # --- PDF File Management ---
    @staticmethod
    def _get_folder_path(username, folder_id):
        safe_user = "".join([c for c in username if c.isalnum() or c in "-_"])
        path = os.path.join(DATA_DIR, safe_user, str(folder_id))
        if not os.path.exists(path):
            os.makedirs(path)
        return path

    @staticmethod
    def save_pdf(uploaded_file, username, folder_id):
        folder_path = DataManager._get_folder_path(username, folder_id)
        file_path = os.path.join(folder_path, uploaded_file.name)
        with open(file_path, "wb") as f:
            f.write(uploaded_file.getbuffer())
        return file_path

    @staticmethod
    def list_pdfs(username, folder_id):
        folder_path = DataManager._get_folder_path(username, folder_id)
        if not os.path.exists(folder_path):
            return []
        return [f for f in os.listdir(folder_path) if f.lower().endswith('.pdf')]

    @staticmethod
    def get_pdf_path(filename, username, folder_id):
        return os.path.join(DataManager._get_folder_path(username, folder_id), filename)
