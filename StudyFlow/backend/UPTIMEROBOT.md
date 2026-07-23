# UptimeRobot (Docker-Backend auf Render)

Auf dem **Free-Tier** schläft die Web-Instanz nach Inaktivität ein; der erste Request nach dem Aufwachen kann **langsam** sein oder kurz **502** liefern. Ein regelmäßiger **HTTP-Monitor** hält den Dienst wacher (und zeigt Ausfälle an).

## Monitor anlegen (uptimerobot.com)

1. **Add New Monitor** → Typ **HTTP(s)**.
2. **URL:** `https://DEIN-RENDER-HOST.onrender.com/api/health`  
   (exakt dieser Pfad — schneller Liveness-Check **ohne** Supabase-Aufruf.)
3. **Monitoring Interval:** z. B. **5 Minuten** (Free bei UptimeRobot oft max. 5 min).
4. **Optional:** Alert-Kontakt (E-Mail) bei Down.

## Einstellungen

| Feld | Empfehlung |
|------|------------|
| Methode | **GET** (Standard) oder **HEAD** — beide sind unterstützt (`GET` und `HEAD` auf `/api/health`). |
| Erwarteter Status | **200** |
| Keyword (optional) | nicht nötig; Antwort ist `{"status":"ok"}` |

**Nicht** `/api/health/ready` als einzigen Ping nutzen — der kann Supabase anfragen und bei Netzproblemen träge wirken.

## Render-URL finden

Im Render-Dashboard: Service **blop-study-backend** → oben die **URL** (z. B. `https://blop-study-backend.onrender.com`). Daraus wird die Monitor-URL:  
`https://…/api/health`

## Hinweis zu 502

502 kann auch bei **langen KI-/ffmpeg-Jobs** oder **Proxy-Timeouts** auftreten — UptimeRobot verhindert nur das **Einschlafen**, nicht alle 502-Ursachen. Siehe auch [`LERNVIDEO_OPS.md`](LERNVIDEO_OPS.md).

## Hinweis zu 401 auf blop-study.com

**401** bedeutet nicht „Render-URL falsch“. Das Frontend proxied `/api/*` nach Render; wenn Health `200` liefert, ist die URL in Ordnung.

Häufiger Fall: der Browser hat noch `username`/`session_id` in `localStorage`, aber die Session ist auf dem Backend ungültig.

Früher lagen Sessions in `user_data/sessions.json` auf dem **ephemeren** Render-Dateisystem — nach Redeploy/Cold-Start waren alle Logins weg, während das Dashboard noch „eingeloggt“ wirkte. Sessions sind jetzt **HMAC-signierte Tokens** (überleben Restarts). Bestehende alte UUID-Sessions werden dadurch ungültig → einmal neu einloggen.
