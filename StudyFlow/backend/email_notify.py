"""
Optional E-Mail wenn KI-Dokumente fertig sind.

Empfänger (TO): immer die E-Mail des jeweiligen Nutzers aus der Datenbank
(Supabase-Tabelle users, Spalte email) — abhängig vom username der KI-Anfrage.
Nicht aus Umgebungsvariablen; jeder Account bekommt die Mail an seine registrierte Adresse.

Absender / SMTP-Server (ein technisches Postfach des Betreibers) über Env:

  BLOP_SMTP_HOST       z. B. smtp.gmail.com
  BLOP_SMTP_PORT       Standard 587 (STARTTLS)
  BLOP_SMTP_USER       SMTP-Login (Versand-Postfach)
  BLOP_SMTP_PASSWORD   SMTP-Passwort / App-Passwort
  BLOP_SMTP_FROM       Absender-Adresse (falls leer: BLOP_SMTP_USER)
  BLOP_APP_PUBLIC_URL  z. B. https://deine-domain.de — für Link zum Ordner
"""

import html
import os
import smtplib
import ssl
from email.mime.multipart import MIMEMultipart
from email.mime.text import MIMEText
from typing import Optional


def smtp_configured() -> bool:
    return bool(os.environ.get("BLOP_SMTP_HOST", "").strip() and os.environ.get("BLOP_SMTP_USER", "").strip())


def send_document_ready_email(
    to_addr: str,
    username: str,
    document_label: str,
    folder_name: Optional[str],
    folder_id: str,
) -> None:
    """Sendet eine kurze Info-Mail. Bei Konfigurations-/SMTP-Fehlern nur loggen."""
    to_addr = (to_addr or "").strip()
    if not to_addr or "@" not in to_addr:
        return

    host = os.environ.get("BLOP_SMTP_HOST", "").strip()
    port = int(os.environ.get("BLOP_SMTP_PORT", "587") or "587")
    smtp_user = os.environ.get("BLOP_SMTP_USER", "").strip()
    password = os.environ.get("BLOP_SMTP_PASSWORD", "").strip()
    from_addr = (os.environ.get("BLOP_SMTP_FROM", "") or smtp_user).strip()
    base_url = os.environ.get("BLOP_APP_PUBLIC_URL", "").rstrip("/")

    if not host or not smtp_user:
        return

    folder_line = f"Ordner: {folder_name}\n" if folder_name else ""
    link_line = ""
    if base_url and folder_id:
        link_line = f"\nDirekt öffnen: {base_url}/folder/{folder_id}\n"

    text_body = (
        f"Hallo,\n\n"
        f"dein Blop Study Dokument ist fertig.\n\n"
        f"Was: {document_label}\n"
        f"{folder_line}"
        f"Account: {username}\n"
        f"{link_line}\n"
        f"— Blop Study\n"
    )

    html_body = f"""\
<html><body style="font-family:system-ui,sans-serif;line-height:1.5;color:#111">
  <p>Hallo,</p>
  <p>dein <strong>Blop Study</strong>-Dokument ist fertig.</p>
  <p><strong>{document_label}</strong></p>
  {f"<p>Ordner: <strong>{folder_name}</strong></p>" if folder_name else ""}
  <p>Account: <code>{username}</code></p>
  {f'<p><a href="{base_url}/folder/{folder_id}">Zum Ordner in Blop Study</a></p>' if base_url and folder_id else ""}
  <p style="color:#666;font-size:12px;margin-top:2rem">Automatische Benachrichtigung — bitte nicht antworten.</p>
</body></html>"""

    msg = MIMEMultipart("alternative")
    msg["Subject"] = f"Blop Study: {document_label} ist fertig"
    msg["From"] = from_addr
    msg["To"] = to_addr
    msg.attach(MIMEText(text_body, "plain", "utf-8"))
    msg.attach(MIMEText(html_body, "html", "utf-8"))

    try:
        context = ssl.create_default_context()
        if port == 465:
            with smtplib.SMTP_SSL(host, port, timeout=30, context=context) as server:
                if password:
                    server.login(smtp_user, password)
                server.sendmail(from_addr, [to_addr], msg.as_string())
        else:
            with smtplib.SMTP(host, port, timeout=30) as server:
                server.ehlo()
                server.starttls(context=context)
                server.ehlo()
                if password:
                    server.login(smtp_user, password)
                server.sendmail(from_addr, [to_addr], msg.as_string())
    except Exception as e:
        print(f"[email_notify] SMTP send failed: {e}")


def try_notify_subscription_started(username: str, tier: str, interval: str, provider: str) -> None:
    if not smtp_configured():
        return
    try:
        from auth_manager import AuthManager

        user = AuthManager.get_user(username)
        to_addr = ((user or {}).get("email") or "").strip()
        if not to_addr or "@" not in to_addr:
            return

        host = os.environ.get("BLOP_SMTP_HOST", "").strip()
        port = int(os.environ.get("BLOP_SMTP_PORT", "587") or "587")
        smtp_user = os.environ.get("BLOP_SMTP_USER", "").strip()
        password = os.environ.get("BLOP_SMTP_PASSWORD", "").strip()
        from_addr = (os.environ.get("BLOP_SMTP_FROM", "") or smtp_user).strip()
        base_url = os.environ.get("BLOP_APP_PUBLIC_URL", "").rstrip("/")
        tier_label = tier.capitalize()
        interval_label = "jährlich" if interval == "year" else "monatlich"
        settings_url = f"{base_url}/settings" if base_url else ""
        safe_user = html.escape(username)
        safe_tier = html.escape(tier_label)
        safe_provider = html.escape(provider.capitalize())

        text_body = (
            f"Hallo {username},\n\n"
            f"dein Blop Study {tier_label}-Abo ist jetzt aktiv.\n\n"
            f"Abrechnung: {interval_label}\n"
            f"Zahlungsanbieter: {provider.capitalize()}\n\n"
            f"Du kannst dein Abo jederzeit in den Einstellungen verwalten oder kündigen.\n"
            f"{settings_url}\n\n— Blop Study\n"
        )
        html_body = f"""<html><body style="font-family:system-ui,sans-serif;line-height:1.5;color:#111">
  <h2>Dein Blop Study {safe_tier}-Abo ist aktiv</h2>
  <p>Hallo {safe_user},</p>
  <p>vielen Dank für dein Vertrauen. Dein <strong>{safe_tier}-Abo</strong> wurde erfolgreich aktiviert.</p>
  <p><strong>Abrechnung:</strong> {interval_label}<br><strong>Zahlungsanbieter:</strong> {safe_provider}</p>
  <p>Du kannst dein Abo jederzeit in den Einstellungen verwalten oder kündigen.</p>
  {f'<p><a href="{settings_url}">Abo verwalten</a></p>' if settings_url else ''}
  <p style="color:#666;font-size:12px;margin-top:2rem">Automatische Bestätigung — bitte nicht antworten.</p>
</body></html>"""

        msg = MIMEMultipart("alternative")
        msg["Subject"] = f"Blop Study: Dein {tier_label}-Abo ist aktiv"
        msg["From"] = from_addr
        msg["To"] = to_addr
        msg.attach(MIMEText(text_body, "plain", "utf-8"))
        msg.attach(MIMEText(html_body, "html", "utf-8"))

        context = ssl.create_default_context()
        if port == 465:
            with smtplib.SMTP_SSL(host, port, timeout=30, context=context) as server:
                if password:
                    server.login(smtp_user, password)
                server.sendmail(from_addr, [to_addr], msg.as_string())
        else:
            with smtplib.SMTP(host, port, timeout=30) as server:
                server.ehlo()
                server.starttls(context=context)
                server.ehlo()
                if password:
                    server.login(smtp_user, password)
                server.sendmail(from_addr, [to_addr], msg.as_string())
    except Exception as e:
        print(f"[email_notify] subscription confirmation failed: {e}")


def try_notify_document_ready(username: str, folder_id: str, document_label: str) -> None:
    """BackgroundTask: Mail an users.email des Accounts `username` (pro Nutzer unterschiedlich)."""
    if not smtp_configured():
        return
    try:
        from auth_manager import AuthManager
        from data_manager import DataManager

        user = AuthManager.get_user(username)
        if not user:
            return
        # Empfänger: nur aus DB — Registrierung / Google-Login hat email gesetzt
        email = (user.get("email") or "").strip()
        if not email or "@" not in email:
            return
        folder_name = DataManager.get_folder_name(username, folder_id)
        send_document_ready_email(email, username, document_label, folder_name, folder_id)
    except Exception as e:
        print(f"[email_notify] try_notify_document_ready: {e}")
