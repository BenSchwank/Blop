import os
import shutil

USER_DATA_DIR = os.path.join(os.path.dirname(__file__), "user_data")

def cleanup_local_streamlit_data():
    """Löscht alle Ordner und Dateien im Verzeichnis user_data, da Blop Study nun Firebase nutzt."""
    if not os.path.exists(USER_DATA_DIR):
        print(f"Das Verzeichnis {USER_DATA_DIR} existiert nicht. Nichts zum Aufräumen.")
        return

    print(f"Starte Aufräumen von: {USER_DATA_DIR}")
    deleted_count = 0
    for item in os.listdir(USER_DATA_DIR):
        item_path = os.path.join(USER_DATA_DIR, item)
        try:
            if os.path.isfile(item_path) or os.path.islink(item_path):
                os.unlink(item_path)
                print(f"Gelöscht: Datei {item}")
                deleted_count += 1
            elif os.path.isdir(item_path):
                shutil.rmtree(item_path)
                print(f"Gelöscht: Ordner {item}")
                deleted_count += 1
        except Exception as e:
            print(f"Fehler beim Löschen von {item}: {e}")
    
    if deleted_count > 0:
        print(f"Erfolgreich {deleted_count} alte Streamlit-Objekte gelöscht!")
    else:
        print("Der Ordner war bereits leer.")

if __name__ == "__main__":
    print("--- Blop Study Lokaler Datenbank-Cleanup ---")
    print("Dieses Skript löscht alle alten lokalen JSON-Daten und Dateiordner,")
    print("die noch aus der Streamlit-Version in 'backend/user_data' liegen.")
    print("Die neuen Daten in Firebase bleiben davon völlig unberührt.\n")
    
    confirm = input("Möchtest du die alten lokalen Daten restlos löschen? (j/n): ")
    if confirm.lower() == 'j':
        cleanup_local_streamlit_data()
    else:
        print("Abbruch.")
