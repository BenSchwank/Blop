import os
import json
import firebase_admin
from firebase_admin import credentials, firestore

def connect_firebase():
    secrets_path = os.path.join(".streamlit", "secrets.toml")
    if not os.path.exists(secrets_path):
        print(f"ERROR: {secrets_path} not found.")
        return None
        
    try:
        import toml
        with open(secrets_path, "r", encoding="utf-8") as f:
            secrets = toml.load(f)
    except ImportError:
        print("ERROR: Please 'pip install toml' to read secrets.")
        return None
        
    if "firebase" not in secrets:
        print("ERROR: [firebase] block missing in secrets.toml")
        return None
        
    cred_dict = dict(secrets["firebase"])
    if "private_key" in cred_dict:
        cred_dict["private_key"] = cred_dict["private_key"].replace("\\n", "\n")
        
    if not firebase_admin._apps:
        cred = credentials.Certificate(cred_dict)
        firebase_admin.initialize_app(cred)
        
    return firestore.client()

def main():
    print("=== Start Local to Firestore Migration ===")
    db = connect_firebase()
    if not db:
        print("Migration aborted.")
        return
        
    user_data_dir = "user_data"
    if not os.path.exists(user_data_dir):
        print("No user_data directory found. Nothing to migrate.")
        return
        
    # 1. Migrate Auth (users.json)
    users_file = os.path.join(user_data_dir, "users.json")
    if os.path.exists(users_file):
        with open(users_file, "r", encoding="utf-8") as f:
            auth_users = json.load(f)
        db.collection("config").document("auth_users").set(auth_users)
        print(f"Migrated {len(auth_users)} auth accounts.")
        
    # 2. Migrate User Workspace (folders, plans, summaries)
    for file in os.listdir(user_data_dir):
        if file.endswith(".json") and file not in ["users.json", "sessions.json"]:
            username = file.replace(".json", "")
            with open(os.path.join(user_data_dir, file), "r", encoding="utf-8") as f:
                data = json.load(f)
                
            # Set the root document for the user
            db.collection("users").document(username).set(data)
            print(f"Migrated workspace root for user: {username}")
            
            # Set the Generic Files within the Folders
            for folder in data.get("folders", []):
                folder_id = str(folder.get("id"))
                for gen_file in folder.get("files", []):
                    file_id = gen_file.get("id")
                    db.collection("users").document(username)\
                      .collection("folders").document(folder_id)\
                      .collection("files").document(file_id).set(gen_file)
                    print(f"  -> Migrated file metadata '{gen_file.get('name')}'")

    print("=== Migration Completed Successfully ===")
    print("Note: Local raw physical texts (PDFs) that were placed directly inside folder directories may need re-uploading if they were entirely offline, but their metadata is preserved.")

if __name__ == "__main__":
    main()
