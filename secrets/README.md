# Secrets — wo was hingehört

**Niemals** Clientgeheimnisse in Git committen oder im Chat an den Agenten schicken.

## Desktop Google Kalender (Clientgeheimnis)

| Ort | Zweck |
|-----|--------|
| `%APPDATA%\Blop\BlopApp\google_desktop_client_secret.txt` | Lokal auf deinem PC (funktioniert schon) |
| Render Env: `GOOGLE_DESKTOP_CLIENT_SECRET` oder `GOOGLE_CLIENT_SECRET` | Wenn Backend `/desktop/exchange` live ist |

Anlegen: Cloud Console → Client **„blop notes“ (Desktop)** → neues Secret → in die Textdatei (eine Zeile).

Skript: `scripts/setup-google-desktop-secret.ps1` (wenn du eine JSON hast).

## Android

Android-OAuth-Clients haben **kein** Clientgeheimnis. Stattdessen:

- Package `com.benschwank.blop`
- SHA-1 vom **Play App Signing** am Client **„Blop Handy notes“**
- SHA-1 vom **Debug-Keystore** am Client **„Blop Handy notes (debug)“**
- OAuth-Testnutzer bzw. Zustimmungsbildschirm „In Produktion“

## Supabase?

Supabase ist für User-Daten/DB — **nicht** der richtige Ort für Google OAuth Client Secrets. Die gehören in **Render Environment Variables** (Server) bzw. die lokale AppData-Datei (nur dein PC).

## Web-Client „Blop“

Bleibt für GIS / Website-Login; Secret nur auf dem Backend (Render), falls nötig.
