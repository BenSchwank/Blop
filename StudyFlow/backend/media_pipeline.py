"""
TTS (OpenAI), slide images (Pillow), ffmpeg slideshow video. Optional OPENAI_API_KEY.
"""
import os
import re
import shutil
import subprocess
import tempfile
from typing import List

import requests

# OpenAI TTS max input length per request
TTS_CHUNK_CHARS = 3800


def _openai_tts_failure_message(response: requests.Response) -> str:
    """German user-facing hint for common OpenAI Speech API failures (Podcast / Lernvideo)."""
    code = response.status_code
    snippet = (response.text or "").strip()[:600]
    if code == 401:
        return (
            "OpenAI TTS (401): API-Schlüssel ungültig oder widerrufen. "
            "Bitte auf dem Server OPENAI_API_KEY prüfen und unter https://platform.openai.com/api-keys "
            "einen gültigen Secret Key setzen."
        )
    if code == 429:
        return (
            "OpenAI TTS (429): Kontingent aufgebraucht oder Rate-Limit. "
            "Unter https://platform.openai.com → Billing Zahlungsmethode und Guthaben prüfen; "
            "Lernvideos senden mehrere TTS-Anfragen (langer Text) und treffen schneller an Limits. "
            f"API-Antwort: {snippet}"
        )
    return f"OpenAI TTS API Fehler {code}: {snippet}"


def openai_tts_speech_mp3(text: str, voice: str = "alloy") -> bytes:
    """Returns MP3 bytes. Requires OPENAI_API_KEY."""
    key = os.environ.get("OPENAI_API_KEY", "").strip()
    if not key:
        raise RuntimeError("OPENAI_API_KEY ist nicht gesetzt. Bitte in den Server-Umgebungsvariablen hinterlegen.")
    text = (text or "").strip()
    if not text:
        raise ValueError("Leerer Text für TTS.")
    chunks: List[str] = []
    remaining = text
    while remaining:
        if len(remaining) <= TTS_CHUNK_CHARS:
            chunks.append(remaining)
            break
        cut = remaining.rfind(" ", 0, TTS_CHUNK_CHARS)
        if cut < TTS_CHUNK_CHARS // 2:
            cut = TTS_CHUNK_CHARS
        chunks.append(remaining[:cut].strip())
        remaining = remaining[cut:].strip()
    mp3_parts: List[bytes] = []
    for ch in chunks:
        r = requests.post(
            "https://api.openai.com/v1/audio/speech",
            headers={"Authorization": f"Bearer {key}", "Content-Type": "application/json"},
            json={"model": "tts-1", "input": ch, "voice": voice, "response_format": "mp3"},
            timeout=120,
        )
        if not r.ok:
            raise RuntimeError(_openai_tts_failure_message(r))
        mp3_parts.append(r.content)
    if len(mp3_parts) == 1:
        return mp3_parts[0]
    return _concat_mp3_ffmpeg(mp3_parts)


def _concat_mp3_ffmpeg(parts: List[bytes]) -> bytes:
    ffmpeg = shutil.which("ffmpeg")
    if not ffmpeg:
        # naive concat of mp3 streams often glitches; still better than failing if single chunk
        return b"".join(parts)
    with tempfile.TemporaryDirectory() as tmp:
        paths = []
        for i, p in enumerate(parts):
            path = os.path.join(tmp, f"p{i}.mp3")
            with open(path, "wb") as f:
                f.write(p)
            paths.append(path)
        lst = os.path.join(tmp, "list.txt")
        with open(lst, "w", encoding="utf-8") as f:
            for p in paths:
                f.write(f"file '{p.replace(chr(92), '/')}'\n")
        out = os.path.join(tmp, "out.mp3")
        try:
            subprocess.run(
                [ffmpeg, "-y", "-f", "concat", "-safe", "0", "-i", lst, "-c", "copy", out],
                check=True,
                capture_output=True,
            )
            with open(out, "rb") as f:
                return f.read()
        except Exception:
            return b"".join(parts)


def _wrap_text(draw, text: str, font, max_width: int) -> List[str]:
    words = re.split(r"(\s+)", text)
    lines: List[str] = []
    cur = ""
    for w in words:
        if not w.strip():
            continue
        test = (cur + " " + w).strip() if cur else w
        bbox = draw.textbbox((0, 0), test, font=font)
        if bbox[2] - bbox[0] <= max_width:
            cur = test
        else:
            if cur:
                lines.append(cur)
            cur = w
    if cur:
        lines.append(cur)
    return lines if lines else [text[:200]]


def render_scene_slides(
    scenes: List[dict],
    width: int = 1280,
    height: int = 720,
) -> List[str]:
    """Writes PNG files per scene; returns paths."""
    try:
        from PIL import Image, ImageDraw, ImageFont
    except ImportError as e:
        raise RuntimeError("Pillow (PIL) wird für Lernvideos benötigt.") from e

    paths: List[str] = []
    tmp_root = tempfile.mkdtemp(prefix="blop_video_")
    try:
        font_title = ImageFont.load_default()
        font_body = ImageFont.load_default()
    except Exception:
        font_title = font_body = None

    for i, sc in enumerate(scenes):
        title = str(sc.get("title") or f"Szene {i+1}")[:120]
        body = str(sc.get("body") or sc.get("bullets") or "")[:2000]
        img = Image.new("RGB", (width, height), color=(15, 18, 35))
        draw = ImageDraw.Draw(img)
        margin = 60
        y = margin
        for line in _wrap_text(draw, title, font_title, width - 2 * margin):
            draw.text((margin, y), line, fill=(230, 235, 255), font=font_title)
            y += 28
        y += 20
        for line in _wrap_text(draw, body, font_body, width - 2 * margin):
            draw.text((margin, y), line, fill=(180, 190, 220), font=font_body)
            y += 22
            if y > height - 40:
                break
        p = os.path.join(tmp_root, f"slide_{i:03d}.png")
        img.save(p)
        paths.append(p)
    return paths


def _audio_duration_sec(path: str) -> float:
    ffprobe = shutil.which("ffprobe")
    if not ffprobe:
        return 30.0
    try:
        out = subprocess.run(
            [
                ffprobe,
                "-v",
                "error",
                "-show_entries",
                "format=duration",
                "-of",
                "default=noprint_wrappers=1:nokey=1",
                path,
            ],
            capture_output=True,
            text=True,
            check=True,
        )
        return max(3.0, float((out.stdout or "").strip() or 30))
    except Exception:
        return 30.0


def ffmpeg_slideshow_with_audio(
    image_paths: List[str],
    audio_path: str,
    output_mp4_path: str,
) -> None:
    ffmpeg = shutil.which("ffmpeg")
    if not ffmpeg:
        raise RuntimeError(
            "ffmpeg ist nicht installiert oder nicht im PATH. Lernvideo-Erstellung auf dem Server nicht möglich."
        )
    if not image_paths:
        raise ValueError("Keine Bilder für das Video.")
    dur = _audio_duration_sec(audio_path)
    seg_secs = max(2.5, dur / len(image_paths))
    with tempfile.TemporaryDirectory() as tmp:
        parts: List[str] = []
        for i, img in enumerate(image_paths):
            seg = os.path.join(tmp, f"seg_{i:03d}.mp4")
            subprocess.run(
                [
                    ffmpeg,
                    "-y",
                    "-loop",
                    "1",
                    "-i",
                    img,
                    "-t",
                    str(seg_secs),
                    "-vf",
                    f"scale={1280}:{720}:force_original_aspect_ratio=decrease,pad=1280:720:(ow-iw)/2:(oh-ih)/2",
                    "-r",
                    "24",
                    "-pix_fmt",
                    "yuv420p",
                    "-an",
                    seg,
                ],
                check=True,
                capture_output=True,
            )
            parts.append(seg)
        lst = os.path.join(tmp, "vlist.txt")
        with open(lst, "w", encoding="utf-8") as f:
            for p in parts:
                f.write(f"file '{p.replace(chr(92), '/')}'\n")
        merged_v = os.path.join(tmp, "merged.mp4")
        subprocess.run(
            [ffmpeg, "-y", "-f", "concat", "-safe", "0", "-i", lst, "-c", "copy", merged_v],
            check=True,
            capture_output=True,
        )
        subprocess.run(
            [
                ffmpeg,
                "-y",
                "-i",
                merged_v,
                "-i",
                audio_path,
                "-c:v",
                "copy",
                "-c:a",
                "aac",
                "-shortest",
                output_mp4_path,
            ],
            check=True,
            capture_output=True,
        )
