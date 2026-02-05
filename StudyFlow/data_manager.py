import json
import os
import streamlit as st
from datetime import datetime

DATA_FILE = "blop_data.json"

class DataManager:
    @staticmethod
    def load():
        if not os.path.exists(DATA_FILE):
            return {"folders": [], "files": []}
        try:
            with open(DATA_FILE, "r", encoding="utf-8") as f:
                return json.load(f)
        except:
            return {"folders": [], "files": []}

    @staticmethod
    def save(data):
        with open(DATA_FILE, "w", encoding="utf-8") as f:
            json.dump(data, f, ensure_ascii=False, indent=4)

    @staticmethod
    def create_folder(name):
        data = DataManager.load()
        folder = {
            "id": f"folder_{int(datetime.now().timestamp())}",
            "name": name,
            "created_at": datetime.now().strftime("%Y-%m-%d")
        }
        data["folders"].append(folder)
        DataManager.save(data)
        return folder

    @staticmethod
    def save_summary(title, content, folder_id=None):
        data = DataManager.load()
        file_entry = {
            "id": f"file_{int(datetime.now().timestamp())}",
            "title": title,
            "content": content,
            "folder_id": folder_id,
            "created_at": datetime.now().strftime("%d.%m.%Y")
        }
        data["files"].append(file_entry)
        DataManager.save(data)
