"""
TTS (OpenAI), slide images (Pillow), ffmpeg slideshow video. Optional OPENAI_API_KEY, PEXELS_API_KEY.
"""
import os
import re
import shutil
import subprocess
import tempfile
from typing import List, Optional, Tuple

import requests

from study_slide_render import render_scene_slides_weighted

# OpenAI TTS max input length per request
TTS_CHUNK_CHARS = 3800

OPENAI_TTS_VOICES = frozenset({"alloy", "echo", "fable", "onyx", "nova", "shimmer"})

def _lv_int(name: str, default: int, lo: int, hi: int) -> int:
    raw = os.environ.get(name, "").strip()
    if not raw:
        return default
    try:
        return max(lo, min(hi, int(raw)))
    except ValueError:
        return default


# Balanced Low-Memory Defaults für kleine Instanzen (z. B. 512MB Render Free).
# Für höhere Qualität (mehr RAM/CPU nötig): LEARNING_VIDEO_WIDTH=854 LEARNING_VIDEO_HEIGHT=480 LEARNING_VIDEO_FPS=15
# LEARNING_VIDEO_CRF=29 LEARNING_VIDEO_MAXRATE=1M LEARNING_VIDEO_BUFSIZE=2M
VIDEO_W = _lv_int("LEARNING_VIDEO_WIDTH", 640, 640, 1920)
VIDEO_H = _lv_int("LEARNING_VIDEO_HEIGHT", 360, 360, 1080)
VIDEO_FPS = _lv_int("LEARNING_VIDEO_FPS", 10, 10, 30)
VIDEO_CRF = _lv_int("LEARNING_VIDEO_CRF", 32, 18, 35)
VIDEO_MAXRATE = (os.environ.get("LEARNING_VIDEO_MAXRATE", "600k").strip() or "600k")
VIDEO_BUFSIZE = (os.environ.get("LEARNING_VIDEO_BUFSIZE", "").strip() or "1200k")


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


def openai_tts_speech_mp3(text: str, voice: str = "alloy", instructions: Optional[str] = None) -> bytes:
    """Returns MP3 bytes. Requires OPENAI_API_KEY."""
    key = os.environ.get("OPENAI_API_KEY", "").strip()
    if not key:
        raise RuntimeError("OPENAI_API_KEY ist nicht gesetzt. Bitte in den Server-Umgebungsvariablen hinterlegen.")
    text = (text or "").strip()
    if not text:
        raise ValueError("Leerer Text für TTS.")
    v = (voice or "alloy").strip().lower()
    if v not in OPENAI_TTS_VOICES:
        v = "alloy"
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
        payload = {
            "model": "gpt-4o-mini-tts",
            "input": ch,
            "voice": v,
            "response_format": "mp3",
        }
        if (instructions or "").strip():
            payload["instructions"] = instructions.strip()
        r = requests.post(
            "https://api.openai.com/v1/audio/speech",
            headers={"Authorization": f"Bearer {key}", "Content-Type": "application/json"},
            json=payload,
            timeout=120,
        )
        if not r.ok:
            fallback = requests.post(
                "https://api.openai.com/v1/audio/speech",
                headers={"Authorization": f"Bearer {key}", "Content-Type": "application/json"},
                json={"model": "tts-1", "input": ch, "voice": v, "response_format": "mp3"},
                timeout=120,
            )
            if not fallback.ok:
                raise RuntimeError(_openai_tts_failure_message(fallback))
            mp3_parts.append(fallback.content)
            continue
        mp3_parts.append(r.content)
    if len(mp3_parts) == 1:
        return mp3_parts[0]
    return _concat_mp3_ffmpeg(mp3_parts)


def openai_tts_speech_mp3_concat_segments(
    segments: List[Tuple[str, str]],
    voice: str = "alloy",
) -> bytes:
    """Concatenate multiple TTS segments (text, instructions) into one MP3."""
    parts: List[bytes] = []
    for text, instructions in segments:
        if not (text or "").strip():
            continue
        parts.append(openai_tts_speech_mp3(text, voice=voice, instructions=instructions))
    if not parts:
        raise ValueError("Keine TTS-Segmente erzeugt.")
    if len(parts) == 1:
        return parts[0]
    return _concat_mp3_ffmpeg(parts)


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


def _slide_font_paths() -> List[str]:
    candidates = [
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        r"C:\Windows\Fonts\arialbd.ttf",
        r"C:\Windows\Fonts\arial.ttf",
        r"C:\Windows\Fonts\segoeuib.ttf",
        r"C:\Windows\Fonts\segoeui.ttf",
    ]
    return [p for p in candidates if os.path.isfile(p)]


def _resolve_ttf_paths() -> Tuple[Optional[str], Optional[str]]:
    paths = _slide_font_paths()
    bold_path = None
    regular_path = None
    for p in paths:
        pl = p.lower()
        if "bold" in pl or "bd" in pl or "euib" in pl:
            bold_path = p
        elif regular_path is None and "bold" not in pl and "bd" not in pl:
            regular_path = p
    if not regular_path and paths:
        regular_path = paths[-1]
    if not bold_path:
        bold_path = regular_path
    return bold_path, regular_path


def _load_slide_fonts():
    from PIL import ImageFont

    bold_path, regular_path = _resolve_ttf_paths()
    title_font = body_font = None
    try:
        if bold_path:
            title_font = ImageFont.truetype(bold_path, 44)
        if regular_path:
            body_font = ImageFont.truetype(regular_path, 28)
        if title_font is None and regular_path:
            title_font = ImageFont.truetype(regular_path, 40)
        if body_font is None and regular_path:
            body_font = ImageFont.truetype(regular_path, 26)
    except OSError:
        title_font = body_font = None
    if title_font is None:
        try:
            title_font = ImageFont.load_default()
            body_font = ImageFont.load_default()
        except Exception:
            title_font = body_font = None
    return title_font, body_font


def pexels_fetch_image_bytes(query: str) -> Optional[bytes]:
    key = os.environ.get("PEXELS_API_KEY", "").strip()
    if not key or not (query or "").strip():
        return None
    try:
        r = requests.get(
            "https://api.pexels.com/v1/search",
            params={"query": query.strip()[:120], "per_page": 1, "orientation": "landscape", "size": "large"},
            headers={"Authorization": key},
            timeout=25,
        )
        if not r.ok:
            return None
        data = r.json()
        photos = data.get("photos") or []
        if not photos:
            return None
        src = photos[0].get("src") or {}
        url = src.get("large2x") or src.get("large") or src.get("original")
        if not url:
            return None
        ir = requests.get(url, timeout=45)
        if not ir.ok:
            return None
        return ir.content
    except Exception:
        return None


def _draw_gradient_background(
    draw, width: int, height: int, top: Tuple[int, int, int], bottom: Tuple[int, int, int]
) -> None:
    for y in range(height):
        t = y / max(height - 1, 1)
        r = int(top[0] * (1 - t) + bottom[0] * t)
        g = int(top[1] * (1 - t) + bottom[1] * t)
        b = int(top[2] * (1 - t) + bottom[2] * t)
        draw.line([(0, y), (width, y)], fill=(r, g, b))


def _draw_footer_bar(draw, width: int, height: int, font_small, accent: Tuple[int, int, int]) -> None:
    fh = 46
    y0 = height - fh
    draw.rectangle([0, y0, width, height], fill=(16, 18, 36))
    draw.rectangle([0, y0, width, y0 + 3], fill=accent)
    msg = "Blop Study · Lernvideo"
    if font_small:
        bbox = draw.textbbox((0, 0), msg, font=font_small)
        tw = bbox[2] - bbox[0]
        draw.text(((width - tw) // 2, y0 + 14), msg, fill=(170, 180, 210), font=font_small)


def _draw_whiteboard_doodles(
    draw,
    width: int,
    canvas_h: int,
    scene_idx: int,
    accent: Tuple[int, int, int],
) -> None:
    """Marker-like lines and shapes — varied per scene (second accent, layout presets)."""
    import random

    rng = random.Random(scene_idx * 9973 + 31)
    alt = (
        min(255, accent[0] + 55),
        max(0, accent[1] - 25),
        min(255, accent[2] + 40),
    )
    soft = (
        accent[0] // 2 + 70,
        accent[1] // 2 + 75,
        accent[2] // 2 + 70,
    )
    variant = scene_idx % 3
    # Top corner brackets only (avoids layout clashes with footer)
    br = 20
    m = 14
    for ox, oy in ((m, m), (width - m - br, m)):
        draw.line([(ox, oy + br), (ox, oy), (ox + br, oy)], fill=soft, width=2)

    if variant == 0:
        pts = []
        x0, y0 = 36 + rng.randint(0, 24), 44 + rng.randint(0, 20)
        for k in range(10):
            pts.append((x0 + k * 16 + rng.randint(-5, 5), y0 + rng.randint(-8, 8)))
        if len(pts) >= 2:
            draw.line(pts, fill=accent, width=3)
        cx = width - 140 - rng.randint(0, 40)
        cy = 70 + rng.randint(0, 30)
        r = 22 + rng.randint(0, 8)
        draw.ellipse([cx - r, cy - r, cx + r, cy + r], outline=alt, width=2)
        draw.line([(cx, cy + r), (cx, cy + r + 18)], fill=alt, width=2)
    elif variant == 1:
        for j in range(5):
            px = 40 + j * 38 + rng.randint(-4, 4)
            py = 55 + rng.randint(-6, 6)
            draw.ellipse([px - 4, py - 4, px + 4, py + 4], outline=accent, width=2)
        draw.arc([width - 220, 50, width - 80, 140], start=200, end=340, fill=alt, width=2)
    else:
        hx = width // 2 - 40
        hy = 60
        draw.line([(hx, hy), (hx + 20, hy - 12), (hx + 40, hy)], fill=accent, width=3)
        draw.line([(hx + 20, hy - 12), (hx + 20, hy + 8)], fill=accent, width=3)

    ax1, ay1 = width // 2 + 80 + rng.randint(-20, 20), canvas_h // 3 + rng.randint(-15, 15)
    ax2, ay2 = ax1 + 55 + rng.randint(0, 15), ay1 + 35 + rng.randint(0, 12)
    draw.line([(ax1, ay1), (ax2, ay2)], fill=accent, width=2)
    draw.polygon([(ax2, ay2), (ax2 - 12, ay2 - 6), (ax2 - 8, ay2 + 4)], fill=accent)

    wx = 48
    wy = min(canvas_h - 120, 155 + rng.randint(0, 45))
    wpts = [(wx + j * 28, wy + (j % 3 - 1) * 4) for j in range(18)]
    draw.line(wpts, fill=soft, width=2)

    for k in range(3):
        cx = 85 + k * 56 + rng.randint(-3, 3)
        cy = canvas_h - 92 + rng.randint(-4, 4)
        draw.line([(cx - 6, cy), (cx + 6, cy)], fill=alt, width=2)
        draw.line([(cx, cy - 6), (cx, cy + 6)], fill=alt, width=2)
    chk_x, chk_y = width - 195, canvas_h - 88
    draw.line([(chk_x, chk_y), (chk_x + 8, chk_y + 12), (chk_x + 22, chk_y - 6)], fill=accent, width=3)


def _draw_chapter_badge(
    draw,
    width: int,
    chapter: str,
    font_badge,
    accent: Tuple[int, int, int],
    margin: int,
) -> None:
    if not chapter or not font_badge:
        return
    label = f"Teil {chapter}"
    pad_x, pad_y = 14, 8
    bbox = draw.textbbox((0, 0), label, font=font_badge)
    tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
    x1, y1 = width - margin - tw - 2 * pad_x, margin
    x2, y2 = x1 + tw + 2 * pad_x, y1 + th + 2 * pad_y
    draw.rounded_rectangle([x1, y1, x2, y2], radius=10, fill=(22, 24, 44), outline=accent, width=2)
    draw.text((x1 + pad_x, y1 + pad_y), label, fill=(230, 235, 255), font=font_badge)


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


def _vf_static() -> str:
    return (
        f"scale={VIDEO_W}:{VIDEO_H}:force_original_aspect_ratio=decrease,"
        f"pad={VIDEO_W}:{VIDEO_H}:(ow-iw)/2:(oh-ih)/2,setsar=1"
    )


def _vf_ken_burns(seg_secs: float) -> str:
    """Subtle slow zoom on still image; d = frame count for zoompan."""
    frames = max(2, int(round(seg_secs * float(VIDEO_FPS))))
    # Overscale input so zoompan can crop inward; increment zoom slightly each frame (max ~6%)
    return (
        f"scale=4000:-2,"
        f"zoompan=z='min(zoom+0.0014,1.08)':d={frames}:"
        f"x='iw/2-(iw/zoom/2)':y='ih/2-(ih/zoom/2)':"
        f"s={VIDEO_W}x{VIDEO_H}:fps={VIDEO_FPS}"
    )


def _probe_video_duration_sec(path: str) -> float:
    ffprobe = shutil.which("ffprobe")
    if not ffprobe:
        return 1.0
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
        return max(0.05, float((out.stdout or "").strip() or 1.0))
    except Exception:
        return 1.0


def _encode_segment(
    ffmpeg: str,
    img: str,
    seg_secs: float,
    out_path: str,
    *,
    ken_burns: bool = False,
) -> None:
    vf = _vf_ken_burns(seg_secs) if ken_burns else _vf_static()
    cmd = [
        ffmpeg,
        "-y",
        "-threads",
        "1",
        "-loop",
        "1",
        "-i",
        img,
        "-t",
        str(seg_secs),
        "-vf",
        vf,
        "-r",
        str(VIDEO_FPS),
        "-c:v",
        "libx264",
        "-preset",
        "ultrafast",
        "-crf",
        str(VIDEO_CRF),
        "-maxrate",
        VIDEO_MAXRATE,
        "-bufsize",
        VIDEO_BUFSIZE,
        "-pix_fmt",
        "yuv420p",
        "-threads",
        "1",
        "-an",
        out_path,
    ]
    try:
        subprocess.run(cmd, check=True, capture_output=True)
    except subprocess.CalledProcessError:
        if not ken_burns:
            raise
        print("Lernvideo: Ken-Burns-Zoompan fehlgeschlagen, nutze statische Skalierung.")
        subprocess.run(
            [
                ffmpeg,
                "-y",
                "-threads",
                "1",
                "-loop",
                "1",
                "-i",
                img,
                "-t",
                str(seg_secs),
                "-vf",
                _vf_static(),
                "-r",
                str(VIDEO_FPS),
                "-c:v",
                "libx264",
                "-preset",
                "ultrafast",
                "-crf",
                str(VIDEO_CRF),
                "-maxrate",
                VIDEO_MAXRATE,
                "-bufsize",
                VIDEO_BUFSIZE,
                "-pix_fmt",
                "yuv420p",
                "-threads",
                "1",
                "-an",
                out_path,
            ],
            check=True,
            capture_output=True,
        )


def _escape_drawtext(value: str) -> str:
    return (
        value.replace("\\", "\\\\")
        .replace(":", "\\:")
        .replace("'", "\\'")
        .replace("%", "\\%")
        .replace("[", "\\[")
        .replace("]", "\\]")
        .replace(",", "\\,")
    )


def _ffmpeg_stderr_snippet(raw: str, *, limit: int = 4000) -> str:
    """Skip the huge single-line 'configuration:' banner; prefer lines that look like real errors."""
    if not raw:
        return ""
    keys = ("error", "invalid", "failed", "undefined", "no such", "unable", "trailing", "matches no streams")
    lines = [ln for ln in raw.splitlines() if any(k in ln.lower() for k in keys)]
    if lines:
        joined = "\n".join(lines[-40:])
        return joined[:limit] if len(joined) > limit else joined
    tail = raw[-limit:] if len(raw) > limit else raw
    return tail


def _drawtext_ascii_safe(value: str) -> str:
    """drawtext in filtergraphs is picky; avoid non-ASCII and argv blowups."""
    v = (value or "").strip()
    for a, b in (
        ("ä", "ae"),
        ("ö", "oe"),
        ("ü", "ue"),
        ("Ä", "Ae"),
        ("Ö", "Oe"),
        ("Ü", "Ue"),
        ("ß", "ss"),
        ("é", "e"),
        ("è", "e"),
        ("ê", "e"),
        ("à", "a"),
        ("â", "a"),
        ("ô", "o"),
        ("î", "i"),
        ("ù", "u"),
        ("ç", "c"),
        ("ñ", "n"),
        ("–", "-"),
        ("—", "-"),
        ("'", " "),
        ('"', " "),
        ("\n", " "),
    ):
        v = v.replace(a, b)
    v = re.sub(r"[^\x20-\x7E]+", " ", v)
    v = re.sub(r"\s+", " ", v).strip()
    return _escape_drawtext(v)


_FONTFILE_DRAWTEXT_PREFIX: Optional[str] = None


def _drawtext_fontfile_prefix() -> str:
    """Headless Debian/Render: drawtext needs a real font path when fontconfig has no default."""
    global _FONTFILE_DRAWTEXT_PREFIX
    if _FONTFILE_DRAWTEXT_PREFIX is not None:
        return _FONTFILE_DRAWTEXT_PREFIX
    for raw in (
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    ):
        if os.path.isfile(raw):
            esc = raw.replace("\\", "/").replace(":", "\\:").replace("'", "\\'")
            _FONTFILE_DRAWTEXT_PREFIX = f"fontfile={esc}:"
            return _FONTFILE_DRAWTEXT_PREFIX
    _FONTFILE_DRAWTEXT_PREFIX = ""
    return _FONTFILE_DRAWTEXT_PREFIX


def _subtitle_chunks(
    script_text: str,
    max_words_per_chunk: int = 4,
    max_chunks: int = 26,
    max_chars_per_chunk: int = 0,
) -> List[str]:
    tokens = [t.strip() for t in re.split(r"\s+", (script_text or "").strip()) if t.strip()]
    if not tokens:
        return ["BLOP DEVLOG"]
    chunks: List[str] = []
    for i in range(0, len(tokens), max_words_per_chunk):
        piece = " ".join(tokens[i : i + max_words_per_chunk]).upper()
        if max_chars_per_chunk and len(piece) > max_chars_per_chunk:
            piece = piece[:max_chars_per_chunk].rstrip()
        chunks.append(piece)
    return chunks[:max_chunks]


def combine_media_with_audio_and_hormozi_subtitles(
    media_path: str,
    audio_path: str,
    script_text: str,
    output_mp4_path: str,
) -> None:
    ffmpeg = shutil.which("ffmpeg")
    ffprobe = shutil.which("ffprobe")
    if not ffmpeg:
        raise RuntimeError("ffmpeg nicht gefunden.")
    if not ffprobe:
        raise RuntimeError("ffprobe nicht gefunden.")
    if not os.path.exists(media_path):
        raise ValueError("Input-Media-Datei fehlt.")
    if not os.path.exists(audio_path):
        raise ValueError("Input-Audio-Datei fehlt.")

    probe = subprocess.run(
        [
            ffprobe,
            "-v",
            "error",
            "-select_streams",
            "v:0",
            "-show_entries",
            "stream=codec_type",
            "-of",
            "default=nw=1:nk=1",
            media_path,
        ],
        capture_output=True,
        text=True,
    )
    is_video = "video" in (probe.stdout or "").strip().lower()

    script_lines = [line.strip() for line in (script_text or "").splitlines() if line.strip()]
    script_tokens = " ".join(script_lines).split()
    if not script_tokens:
        script_tokens = ["Blop", "Devlog"]
    subtitle_word = _escape_drawtext(" ".join(script_tokens[:4]).upper())

    drawtext = (
        "drawtext="
        f"text='{subtitle_word}':"
        "fontcolor=white:"
        "fontsize=h*0.1:"
        "box=1:"
        "boxcolor=black@0.5:"
        "boxborderw=18:"
        "x=(w-text_w)/2:"
        "y=h*0.78:"
        "enable='between(t,0,20)'"
    )
    base_vf = (
        f"scale={VIDEO_W}:{VIDEO_H}:force_original_aspect_ratio=decrease,"
        f"pad={VIDEO_W}:{VIDEO_H}:(ow-iw)/2:(oh-ih)/2,setsar=1,{drawtext}"
    )

    if is_video:
        cmd = [
            ffmpeg,
            "-y",
            "-i",
            media_path,
            "-i",
            audio_path,
            "-vf",
            base_vf,
            "-c:v",
            "libx264",
            "-preset",
            "veryfast",
            "-crf",
            "23",
            "-c:a",
            "aac",
            "-b:a",
            "192k",
            "-shortest",
            "-movflags",
            "+faststart",
            output_mp4_path,
        ]
    else:
        cmd = [
            ffmpeg,
            "-y",
            "-loop",
            "1",
            "-i",
            media_path,
            "-i",
            audio_path,
            "-vf",
            base_vf,
            "-c:v",
            "libx264",
            "-preset",
            "veryfast",
            "-crf",
            "23",
            "-c:a",
            "aac",
            "-b:a",
            "192k",
            "-shortest",
            "-movflags",
            "+faststart",
            output_mp4_path,
        ]

    subprocess.run(cmd, check=True, capture_output=True)


def _has_video_stream(path: str) -> bool:
    ffprobe = shutil.which("ffprobe")
    if not ffprobe:
        return False
    probe = subprocess.run(
        [
            ffprobe,
            "-v",
            "error",
            "-select_streams",
            "v:0",
            "-show_entries",
            "stream=codec_type",
            "-of",
            "default=nw=1:nk=1",
            path,
        ],
        capture_output=True,
        text=True,
    )
    return "video" in (probe.stdout or "").strip().lower()


def _env_flag(name: str, default: bool = False) -> bool:
    raw = (os.environ.get(name, "") or "").strip().lower()
    if not raw:
        return default
    return raw in {"1", "true", "yes", "on"}


def _marketing_emotion_tempo(emotion: str) -> str:
    # Gentler atempo keeps speech more natural (aggressive values feel "robotic").
    return {
        "energetic": "1.08",
        "confident": "1.05",
        "calm": "1.0",
        "dramatic": "1.04",
    }.get((emotion or "energetic").strip().lower(), "1.08")


def _marketing_audio_filter_complex_from(speech_in: str, emotion: str, render_seconds: float) -> str:
    """Return an ffmpeg filter_complex fragment ending at labeled output [aout]."""
    tempo = _marketing_emotion_tempo(emotion)
    base = f"atempo={tempo},loudnorm=I=-16.5:TP=-1.2:LRA=11,apad"
    if not _env_flag("MARKETING_VOICE_HUMANIZE", False):
        return f"{speech_in}{base}[aout]"
    d = max(2.0, min(120.0, float(render_seconds or 20.0) + 2.0))
    # Very subtle bed; if anoisesrc is unavailable, ffmpeg will fail — keep behind env flag.
    return (
        f"anoisesrc=d={d:.1f}:c=pink:r=48000:a=0.00012[bed];"
        f"{speech_in}{base}[vo];"
        f"[bed][vo]amix=inputs=2:duration=first:dropout_transition=0[aout]"
    )


def _marketing_merge_audio_video_fc(video_fc: str, speech_in: str, emotion: str, render_seconds: float) -> str:
    afc = _marketing_audio_filter_complex_from(speech_in, emotion, render_seconds)
    return f"{video_fc};{afc}"


def _brainrot_phase_times(duration_sec: float) -> dict:
    """Duration-aware Hook / List / CTA windows for 5–20s clips (seconds)."""
    d = max(5.0, min(20.0, float(duration_sec)))
    hook_end = max(2.0, min(5.0, d * 0.26))
    list_start = hook_end + 0.18
    list_span = max(3.0, min(9.5, d * 0.48))
    list_end = min(d - 1.55, list_start + list_span)
    if list_end < list_start + 2.0:
        list_end = min(d - 1.2, list_start + 2.0)
    cta_start = max(list_end + 0.12, d * 0.68)
    cta_start = min(cta_start, d - 1.05)
    cta_end = d - 0.04
    subtitle_end = max(list_start + 0.5, min(cta_start - 0.22, d * 0.64))
    return {
        "d": d,
        "hook_end": hook_end,
        "list_start": list_start,
        "list_end": list_end,
        "cta_start": cta_start,
        "cta_end": cta_end,
        "subtitle_end": subtitle_end,
    }


def _drawtext_alpha_fade_expr(t0: float, t1: float, fade_in: float, fade_out: float) -> str:
    """Piecewise linear alpha for drawtext (commas escaped for filtergraph)."""
    t0i, t1i = max(0.0, t0), max(t0 + 0.01, t1)
    fi = max(0.08, min(fade_in, (t1i - t0i) * 0.45))
    fo = max(0.08, min(fade_out, (t1i - t0i) * 0.45))
    a = t0i + fi
    b = t1i - fo
    if b < a + 0.06:
        b = min(t1i - 0.02, a + 0.06)
    # if(t<t0,0) if(t<t0+fi, (t-t0)/fi, if(t<b,1, if(t<t1, (t1-t)/fo,0)))
    return (
        f"if(lt(t\\,{t0i:.3f})\\,0\\,"
        f"if(lt(t\\,{a:.3f})\\,clip(0\\,1\\,(t-{t0i:.3f})/{fi:.3f})\\,"
        f"if(lt(t\\,{b:.3f})\\,1\\,"
        f"if(lt(t\\,{t1i:.3f})\\,clip(0\\,1\\,({t1i:.3f}-t)/{fo:.3f})\\,0))))"
    )


def _marketing_subtitle_chain_for_window(
    script_text: str,
    window_start: float,
    window_end: float,
    *,
    max_chunks: int = 14,
    max_words_per_chunk: int = 4,
    max_chars_per_chunk: int = 0,
    fontsize_expr: str = "h*0.08",
    text_x_expr: str = "(w-text_w)/2",
    text_y_expr: str = "h*0.70",
) -> str:
    subtitle_chunks = _subtitle_chunks(
        script_text,
        max_words_per_chunk=max_words_per_chunk,
        max_chunks=max_chunks,
        max_chars_per_chunk=max_chars_per_chunk,
    )
    if not subtitle_chunks:
        return ""
    span = max(0.35, float(window_end) - float(window_start))
    chunk_duration = max(0.45, span / float(len(subtitle_chunks)))
    subtitle_filters: List[str] = []
    for idx, chunk in enumerate(subtitle_chunks):
        start = float(window_start) + idx * chunk_duration
        end = min(float(window_end), start + chunk_duration + 0.18)
        if start >= window_end:
            break
        subtitle_filters.append(
            "drawtext="
            f"{_drawtext_fontfile_prefix()}"
            f"text='{_drawtext_ascii_safe(chunk)}':"
            "fontcolor=white:"
            f"fontsize={fontsize_expr}:"
            "borderw=2:"
            "bordercolor=black@0.88:"
            "box=1:"
            "boxcolor=black@0.42:"
            "boxborderw=12:"
            f"x={text_x_expr}:"
            f"y={text_y_expr}:"
            f"enable='between(t,{start:.2f},{end:.2f})'"
        )
    return ",".join(subtitle_filters) if subtitle_filters else ""


def _whiteboard_summary_lines(bulletpoints: str, script_text: str, max_lines: int = 6) -> List[str]:
    raw_lines = [ln.strip(" \t-*•\u2022") for ln in (bulletpoints or "").splitlines()]
    lines = [ln for ln in raw_lines if ln]
    if len(lines) < 3:
        fallback = [s.strip() for s in re.split(r"(?<=[.!?])\s+", (script_text or "").strip()) if s.strip()]
        for s in fallback:
            if s not in lines:
                lines.append(s)
            if len(lines) >= max_lines:
                break
    if not lines:
        lines = ["Blop Devlog", "Update", "Community"]
    return lines[:max_lines]


def render_marketing_whiteboard_short(
    image_paths: List[str],
    bulletpoints: str,
    script_text: str,
    audio_path: str,
    output_mp4_path: str,
    target_duration_sec: int = 30,
    emotion: str = "energetic",
) -> None:
    """Image-only marketing short: dark gradient + grain + Ken Burns + kinetic summary text."""
    ffmpeg = shutil.which("ffmpeg")
    if not ffmpeg:
        raise RuntimeError("ffmpeg nicht gefunden.")
    if not image_paths:
        raise ValueError("Keine Bilder fuer Whiteboard-Render.")
    if not os.path.exists(audio_path):
        raise ValueError("Input-Audio-Datei fehlt.")

    target_w = 540
    target_h = 960
    target_fps = 12
    target_duration = max(10, min(45, int(target_duration_sec or 20)))
    audio_dur = max(0.1, _audio_duration_sec(audio_path))
    render_dur = min(float(target_duration), audio_dur + 0.25)

    hero_path = image_paths[0]
    if not os.path.exists(hero_path):
        raise ValueError("Hero-Bild fehlt.")

    summary_lines = _whiteboard_summary_lines(bulletpoints, script_text, max_lines=6)
    phase = max(1.2, render_dur / float(len(summary_lines)))

    kinetic_filters: List[str] = []
    for i, line in enumerate(summary_lines):
        t0 = i * phase
        t1 = min(render_dur, t0 + phase)
        esc = _escape_drawtext(line[:120])
        y_base = 0.18 + 0.02 * float(i)
        kinetic_filters.append(
            "drawtext="
            f"text='{esc}':"
            "fontcolor=white:"
            "fontsize='h*(0.065+0.01*sin(2*PI*t/3))':"
            "borderw=3:"
            "bordercolor=black@0.9:"
            "box=1:"
            "boxcolor=black@0.45:"
            "boxborderw=14:"
            "x=(w-text_w)/2:"
            f"y='h*({y_base:.3f}+0.01*sin(2*PI*t/2.2))'"
            + f":enable='between(t,{t0:.2f},{t1:.2f})'"
        )

    frames = max(3, int(round(render_dur * float(target_fps))))
    sub_chain = _marketing_subtitle_chain_for_window(script_text, 0.0, float(render_dur))
    sub_part = f",{sub_chain}" if sub_chain else ""
    kin_part = f",{','.join(kinetic_filters)}" if kinetic_filters else ""
    vf_graph = (
        f"[0:v]scale={target_w * 2}:-2,"
        f"zoompan=z='min(zoom+0.0016,1.10)':d={frames}:"
        f"x='iw/2-(iw/zoom/2)':y='ih/2-(ih/zoom/2)':"
        f"s={target_w}x{target_h}:fps={target_fps},format=yuv420p[fg];"
        f"color=c=#070b12:s={target_w}x{target_h}:d={render_dur},format=yuv420p[bg0];"
        f"[bg0]noise=alls=6:allf=t+u,format=yuv420p[bg];"
        f"[bg][fg]overlay=(W-w)/2:(H-h)/2:format=auto"
        f"{sub_part}"
        f"{kin_part},"
        f"format=yuv420p[vout]"
    )

    fc = _marketing_merge_audio_video_fc(vf_graph, "[1:a]", emotion, render_dur)
    cmd = [
        ffmpeg,
        "-y",
        "-loop",
        "1",
        "-i",
        hero_path,
        "-i",
        audio_path,
        "-filter_complex",
        fc,
        "-map",
        "[vout]",
        "-map",
        "[aout]",
        "-t",
        f"{render_dur:.3f}",
        "-r",
        str(target_fps),
        "-c:v",
        "libx264",
        "-preset",
        "ultrafast",
        "-crf",
        "30",
        "-threads",
        "1",
        "-c:a",
        "aac",
        "-b:a",
        "96k",
        "-movflags",
        "+faststart",
        output_mp4_path,
    ]

    subprocess.run(cmd, check=True, capture_output=True)


def combine_media_sequence_with_audio_and_hormozi_subtitles(
    media_paths: List[str],
    audio_path: str,
    script_text: str,
    output_mp4_path: str,
    target_duration_sec: int = 30,
    emotion: str = "energetic",
) -> None:
    ffmpeg = shutil.which("ffmpeg")
    if not ffmpeg:
        raise RuntimeError("ffmpeg nicht gefunden.")
    if not media_paths:
        raise ValueError("Keine Input-Medien uebergeben.")
    if not os.path.exists(audio_path):
        raise ValueError("Input-Audio-Datei fehlt.")

    # Low-memory short-video profile for small Render instances (512MB).
    # Keep 9:16 but reduce pixel count and bitrate pressure significantly.
    target_w = 540
    target_h = 960
    target_fps = 12
    target_duration = max(10, min(45, int(target_duration_sec or 20)))
    max_inputs = 6
    media_paths = media_paths[:max_inputs]
    video_inputs = [p for p in media_paths if _has_video_stream(p)]
    image_inputs = [p for p in media_paths if not _has_video_stream(p)]
    prioritized_paths = video_inputs + image_inputs
    if not prioritized_paths:
        raise ValueError("Keine renderbaren Input-Medien gefunden.")
    media_paths = prioritized_paths

    mixed = bool(video_inputs and image_inputs)
    if video_inputs:
        video_budget = float(target_duration) * (0.70 if mixed else 1.0)
        image_budget = float(target_duration) * (0.30 if mixed else 0.0)
    else:
        # Images-only longer reels: still use motion slides for the full duration.
        video_budget = 0.0
        image_budget = float(target_duration)

    timeline: List[Tuple[str, str, float, float]] = []
    # tuple: (kind, path, duration, start_offset_for_video)

    if video_inputs:
        n_slots = min(8, max(3, int(round(video_budget / max(2.0, target_duration / 6.0)))))
        per = max(1.2, video_budget / float(n_slots))
        v_cycle = list(video_inputs)
        for s in range(n_slots):
            path = v_cycle[s % len(v_cycle)]
            dur = max(1.0, min(per, max(1.0, _probe_video_duration_sec(path) * 0.45)))
            media_duration = max(1.0, _probe_video_duration_sec(path))
            dur = min(dur, media_duration)
            span = max(0.0, media_duration - dur)
            start = 0.0
            if span > 0.05:
                start = min(span, (s * 0.6180339887 * max(1.0, media_duration / max(1, n_slots))) % span)
            timeline.append(("video", path, float(dur), float(start)))

    if image_inputs and image_budget > 0.25:
        share = max(1.0, image_budget / float(len(image_inputs)))
        for img in image_inputs:
            timeline.append(("image", img, float(min(5.0, max(1.0, share))), 0.0))

    if not timeline:
        raise RuntimeError("Keine Timeline-Slots erzeugt.")

    total_planned = sum(d for _, _, d, _ in timeline)
    if total_planned > target_duration + 0.05:
        scale = float(target_duration) / total_planned
        timeline = [(k, p, d * scale, ss) for k, p, d, ss in timeline]
    elif total_planned < target_duration - 0.15:
        deficit = float(target_duration) - total_planned
        # Stretch last video slot if possible, else last entry.
        for i in range(len(timeline) - 1, -1, -1):
            kind, path, dur, start = timeline[i]
            if kind != "video":
                continue
            media_duration = max(1.0, _probe_video_duration_sec(path))
            room = max(0.0, media_duration - start - dur)
            add = min(deficit, room)
            if add > 0.05:
                nd = dur + add
                timeline[i] = (kind, path, nd, start)
                deficit -= add
                break
        if deficit > 0.15:
            k, p, d, ss = timeline[-1]
            timeline[-1] = (k, p, d + deficit, ss)

    videos = [t for t in timeline if t[0] == "video"]
    images = [t for t in timeline if t[0] == "image"]
    ordered: List[Tuple[str, str, float, float]] = []
    if videos and images:
        vi = ii = 0
        while vi < len(videos) or ii < len(images):
            if vi < len(videos):
                ordered.append(videos[vi])
                vi += 1
            if ii < len(images):
                ordered.append(images[ii])
                ii += 1
    else:
        ordered = list(timeline)

    with tempfile.TemporaryDirectory(prefix="blop_marketing_segments_") as tmp:
        segment_paths: List[str] = []
        t_cursor = 0.0
        for idx, (kind, media_path, seg_dur, clip_start) in enumerate(ordered):
            if not os.path.exists(media_path):
                continue
            segment_path = os.path.join(tmp, f"segment_{idx:03d}.mp4")
            win0 = t_cursor
            win1 = t_cursor + float(seg_dur)
            subtitle_chain = _marketing_subtitle_chain_for_window(script_text, win0, win1)
            sub_suffix = f",{subtitle_chain}" if subtitle_chain else ""
            vertical_vf = (
                f"scale={target_w}:{target_h}:force_original_aspect_ratio=decrease,"
                f"pad={target_w}:{target_h}:(ow-iw)/2:(oh-ih)/2,setsar=1"
                f"{sub_suffix}"
            )
            if kind == "video":
                cmd = [
                    ffmpeg,
                    "-y",
                    "-ss",
                    f"{clip_start:.3f}",
                    "-i",
                    media_path,
                    "-t",
                    f"{seg_dur:.3f}",
                    "-vf",
                    vertical_vf,
                    "-r",
                    str(target_fps),
                    "-c:v",
                    "libx264",
                    "-preset",
                    "ultrafast",
                    "-crf",
                    "30",
                    "-threads",
                    "1",
                    "-pix_fmt",
                    "yuv420p",
                    "-an",
                    segment_path,
                ]
            else:
                image_frames = max(3, int(round(float(seg_dur) * float(target_fps))))
                image_motion_vf = (
                    f"scale={target_w * 2}:-2,"
                    f"zoompan=z='min(zoom+0.0022,1.12)':d={image_frames}:"
                    f"x='iw/2-(iw/zoom/2)':y='ih/2-(ih/zoom/2)':"
                    f"s={target_w}x{target_h}:fps={target_fps},"
                    f"setsar=1,{subtitle_chain}"
                )
                cmd = [
                    ffmpeg,
                    "-y",
                    "-loop",
                    "1",
                    "-i",
                    media_path,
                    "-t",
                    f"{seg_dur:.3f}",
                    "-vf",
                    image_motion_vf,
                    "-r",
                    str(target_fps),
                    "-c:v",
                    "libx264",
                    "-preset",
                    "ultrafast",
                    "-crf",
                    "30",
                    "-threads",
                    "1",
                    "-pix_fmt",
                    "yuv420p",
                    "-an",
                    segment_path,
                ]
            subprocess.run(cmd, check=True, capture_output=True)
            segment_paths.append(segment_path)
            t_cursor += float(seg_dur)

        if not segment_paths:
            raise RuntimeError("Keine renderbaren Media-Segmente erzeugt.")

        concat_list = os.path.join(tmp, "concat.txt")
        with open(concat_list, "w", encoding="utf-8") as f:
            for seg in segment_paths:
                f.write(f"file '{seg.replace(chr(92), '/')}'\n")

        merged_video = os.path.join(tmp, "merged_vertical.mp4")
        subprocess.run(
            [
                ffmpeg,
                "-y",
                "-f",
                "concat",
                "-safe",
                "0",
                "-i",
                concat_list,
                "-r",
                str(target_fps),
                "-c:v",
                "libx264",
                "-preset",
                "ultrafast",
                "-crf",
                "30",
                "-threads",
                "1",
                "-pix_fmt",
                "yuv420p",
                "-an",
                merged_video,
            ],
            check=True,
            capture_output=True,
        )

        afc = _marketing_audio_filter_complex_from("[1:a]", emotion, float(target_duration))
        subprocess.run(
            [
                ffmpeg,
                "-y",
                "-stream_loop",
                "-1",
                "-i",
                merged_video,
                "-i",
                audio_path,
                "-filter_complex",
                afc,
                "-map",
                "0:v",
                "-map",
                "[aout]",
                "-t",
                str(target_duration),
                "-r",
                str(target_fps),
                "-c:v",
                "libx264",
                "-preset",
                "ultrafast",
                "-crf",
                "30",
                "-threads",
                "1",
                "-c:a",
                "aac",
                "-b:a",
                "96k",
                "-movflags",
                "+faststart",
                output_mp4_path,
            ],
            check=True,
            capture_output=True,
        )


def _pack_brainrot_text_filters(
    hook_line: str,
    list_title: str,
    list_items: List[str],
    cta_line: str,
    duration_sec: float,
    phases: Optional[dict] = None,
) -> str:
    """Hook / gestaffelte Feature-Liste / CTA mit sanften Alpha-Fades (duration-aware)."""
    ph = phases if phases is not None else _brainrot_phase_times(duration_sec)
    d = float(ph["d"])
    hook_end = float(ph["hook_end"])
    list_start = float(ph["list_start"])
    list_end = float(ph["list_end"])
    cta_start = float(ph["cta_start"])
    cta_end = float(ph["cta_end"])

    parts: List[str] = []
    esc_hook = _drawtext_ascii_safe((hook_line or "")[:42])
    fp = _drawtext_fontfile_prefix()
    cx = r"max(6\,min((w-text_w)/2\,w-6-text_w))"
    if esc_hook:
        a_hook = _drawtext_alpha_fade_expr(0.0, min(hook_end + 0.12, d), 0.42, 0.28)
        parts.append(
            "drawtext=" + fp + "text='" + esc_hook + "':fontcolor=white:fontsize=w*0.036:borderw=2:bordercolor=black@0.9:"
            "box=1:boxcolor=#2a1f45@0.58:boxborderw=10:"
            f"x={cx}:y=h*0.08:alpha='{a_hook}':"
            f"enable='between(t,0,{min(hook_end + 0.2, d):.2f})'"
        )
    title_esc = _drawtext_ascii_safe((list_title or "Wichtig:")[:36])
    a_title = _drawtext_alpha_fade_expr(list_start, list_end, 0.45, 0.38)
    parts.append(
        "drawtext=" + fp + "text='" + title_esc + "':fontcolor=#C8B8FF:fontsize=w*0.032:borderw=2:bordercolor=black@0.85:"
        "x=max(8\\,w*0.05):y=h*0.42:"
        f"alpha='{a_title}':"
        f"enable='between(t,{max(0.0, list_start - 0.05):.2f},{min(list_end + 0.15, d):.2f})'"
    )
    y0 = 0.50
    stagger = max(0.28, min(0.52, (list_end - list_start) / 8.0))
    for i, raw in enumerate((list_items or [])[:5]):
        line = _drawtext_ascii_safe((raw or "")[:40])
        if not line:
            continue
        y = y0 + i * 0.065
        t_in = list_start + 0.22 + float(i) * stagger
        a_line = _drawtext_alpha_fade_expr(t_in, list_end - 0.05, 0.38, 0.30)
        parts.append(
            f"drawtext={fp}text='{line}':fontcolor=#F0EAFF:fontsize=w*0.028:borderw=2:bordercolor=black@0.88:"
            f"x=max(8\\,w*0.06):y=h*{y:.3f}:"
            f"alpha='{a_line}':"
            f"enable='between(t,{max(0.0, t_in - 0.08):.2f},{min(list_end + 0.12, d):.2f})'"
        )
    esc_cta = _drawtext_ascii_safe((cta_line or "")[:48])
    if esc_cta:
        a_cta = _drawtext_alpha_fade_expr(max(0.0, cta_start - 0.08), cta_end + 0.02, 0.48, 0.42)
        parts.append(
            "drawtext=" + fp + "text='" + esc_cta + "':fontcolor=#E8E0FF:fontsize=w*0.032:borderw=2:bordercolor=black@0.9:"
            "box=1:boxcolor=#1a1428@0.52:boxborderw=8:"
            f"x={cx}:y=h*0.82:alpha='{a_cta}':"
            f"enable='between(t,{max(0.0, cta_start - 0.15):.2f},{d:.2f})'"
        )
    return ",".join(parts) if parts else ""


def render_marketing_brainrot_clip(
    hero_path: str,
    audio_path: str,
    output_mp4_path: str,
    *,
    clip_script: str,
    hook_line: str,
    list_title: str,
    list_items: List[str],
    cta_line: str,
    emotion: str = "energetic",
    duration_sec: float = 15.0,
    accent_path: Optional[str] = None,
) -> None:
    """15s vertical clip: dark gradient, organic hero motion, phased fades, readable text, TTS."""
    ffmpeg = shutil.which("ffmpeg")
    if not ffmpeg:
        raise RuntimeError("ffmpeg nicht gefunden.")
    if not os.path.exists(hero_path) or not os.path.exists(audio_path):
        raise ValueError("Hero- oder Audio-Datei fehlt.")

    target_w = 540
    target_h = 960
    target_fps = 12
    d = max(5.0, min(20.0, float(duration_sec)))
    ph = _brainrot_phase_times(d)

    sub_chain = _marketing_subtitle_chain_for_window(
        clip_script,
        0.0,
        float(ph["subtitle_end"]),
        max_chunks=14,
        max_words_per_chunk=2,
        max_chars_per_chunk=32,
        fontsize_expr="w*0.038",
        text_x_expr=r"max(6\,min((w-text_w)/2\,w-6-text_w))",
        text_y_expr="h*0.62",
    )
    txt_chain = _pack_brainrot_text_filters(hook_line, list_title, list_items, cta_line, d, phases=ph)
    overlay_extras: List[str] = []
    if sub_chain:
        overlay_extras.append(sub_chain)
    if txt_chain:
        overlay_extras.append(txt_chain)
    # Must be "[vm]drawtext=...,format=..." not "[vm],drawtext=..." (comma after label is invalid).
    overlay_joined = ",".join(overlay_extras) if overlay_extras else ""
    vm_text_tail = f"{overlay_joined},format=yuv420p[vout]" if overlay_joined else "format=yuv420p[vout]"

    # Dark-lilac gradient with subtle motion accents (stylish but non-chaotic).
    bg_core = (
        "[0:v]format=gbrp,"
        "geq=r='19+32*(Y/H)':g='10+24*(Y/H)':b='44+40*(Y/H)',"
        "format=yuv420p,"
        "drawbox=x='W*0.12+8*sin(t*0.22)':y='H*0.18':w='W*0.76':h='H*0.0028':color=#B19CFF@0.18:t=fill,"
        "drawbox=x='W*0.16+6*cos(t*0.18)':y='H*0.79':w='W*0.68':h='H*0.0022':color=#8E7CFF@0.14:t=fill,"
        "drawbox=x='W*0.09':y='H*0.13+5*sin(t*0.17)':w='W*0.004':h='H*0.74':color=#7F6DFF@0.10:t=fill,"
        "format=yuv420p[bg]"
    )

    hero_w, hero_h = 460, 500
    hero_vf = (
        # Fit+pad keeps entire screenshot/frame visible (no hard cutoffs).
        f"[1:v]scale={hero_w}:{hero_h}:force_original_aspect_ratio=decrease,"
        f"pad={hero_w}:{hero_h}:(ow-iw)/2:(oh-ih)/2:color=#120d23,"
        f"setsar=1,format=yuv420p[fg]"
    )
    # Soft drift on hero overlay (organic, low amplitude).
    ov_hero = (
        "[bg][fg]overlay=x='max(0\\,min(W-w\\,(W-w)/2+9*sin(t*0.52)))':"
        "y='max(0\\,min(H-h\\,(H-h)*0.34+7*cos(t*0.41)))':format=yuv420[vm]"
    )

    acc_in = accent_path and os.path.isfile(accent_path)
    if acc_in:
        acc_sparkle = (
            "[2:v]scale=120:-1,format=yuv420p,"
            "hue=H='2.8*sin(2.3*t)':s=0.96[acc]"
        )
        ov_acc = (
            "[vm][acc]overlay=x='max(4\\,min(W-w-4\\,W*0.72-w/2+5*sin(t*0.88)))':"
            "y='max(4\\,min(H-h-90\\,H*0.262+4*cos(t*0.63)))':format=yuv420[vm2]"
        )
        vf_graph = (
            f"{bg_core};"
            f"{hero_vf};"
            f"{ov_hero};"
            f"{acc_sparkle};"
            f"{ov_acc};"
            f"[vm2]{vm_text_tail}"
        )
        fc = _marketing_merge_audio_video_fc(vf_graph, "[3:a]", emotion, d)
    else:
        vf_graph = (
            f"{bg_core};"
            f"{hero_vf};"
            f"{ov_hero};"
            f"[vm]{vm_text_tail}"
        )
        fc = _marketing_merge_audio_video_fc(vf_graph, "[2:a]", emotion, d)

    script_path = os.path.join(os.path.dirname(output_mp4_path), f"_fc_{os.getpid()}.txt")
    try:
        with open(script_path, "w", encoding="utf-8") as sf:
            sf.write(fc)
        cmd_base = [
            ffmpeg,
            "-y",
            "-f",
            "lavfi",
            "-i",
            f"color=c=#1a0033:s={target_w}x{target_h}:d={d}",
            "-loop",
            "1",
            "-i",
            hero_path,
        ]
        if acc_in:
            cmd_base.extend(["-loop", "1", "-i", accent_path])
        cmd_base.extend(
            [
                "-i",
                audio_path,
                "-filter_complex_script",
                script_path,
                "-map",
                "[vout]",
                "-map",
                "[aout]",
                "-t",
                f"{d:.3f}",
                "-r",
                str(target_fps),
                "-c:v",
                "libx264",
                "-preset",
                "ultrafast",
                "-crf",
                "30",
                "-threads",
                "1",
                "-c:a",
                "aac",
                "-b:a",
                "96k",
                "-movflags",
                "+faststart",
                output_mp4_path,
            ]
        )
        proc = subprocess.run(cmd_base, check=False, capture_output=True, text=True)
        if proc.returncode != 0:
            raw = (proc.stderr or proc.stdout or "").strip()
            err = _ffmpeg_stderr_snippet(raw, limit=4500)
            raise RuntimeError(f"ffmpeg brainrot clip failed ({proc.returncode}): {err}")
    finally:
        try:
            os.unlink(script_path)
        except OSError:
            pass


def _merge_segments_xfade(
    ffmpeg: str,
    segment_paths: List[str],
    fade_sec: float,
    out_path: str,
) -> None:
    """Chain [0:v][1:v]xfade...[v1];[v1][2:v]xfade... Requires ffprobe durations."""
    n = len(segment_paths)
    if n == 0:
        raise ValueError("Keine Segmente für xfade.")
    if n == 1:
        shutil.copy2(segment_paths[0], out_path)
        return
    durs = [_probe_video_duration_sec(p) for p in segment_paths]
    fade = min(float(fade_sec), max(0.05, min(durs) * 0.25))
    fc_parts: List[str] = []
    cum = durs[0]
    for i in range(1, n):
        offset = max(0.0, cum - fade)
        if i == 1:
            inp_a, inp_b = "[0:v]", "[1:v]"
        else:
            inp_a = f"[v{i-1}]"
            inp_b = f"[{i}:v]"
        out_lbl = "vout" if i == n - 1 else f"v{i}"
        fc_parts.append(f"{inp_a}{inp_b}xfade=transition=fade:duration={fade}:offset={offset}[{out_lbl}]")
        cum = cum + durs[i] - fade
    filter_complex = ";".join(fc_parts)
    args: List[str] = [ffmpeg, "-y", "-threads", "1"]
    for p in segment_paths:
        args.extend(["-i", p])
    args.extend(
        [
            "-filter_complex",
            filter_complex,
            "-map",
            "[vout]",
            "-c:v",
            "libx264",
            "-preset",
            "ultrafast",
            "-crf",
            str(VIDEO_CRF),
            "-maxrate",
            VIDEO_MAXRATE,
            "-bufsize",
            VIDEO_BUFSIZE,
            "-pix_fmt",
            "yuv420p",
            "-an",
            out_path,
        ]
    )
    subprocess.run(args, check=True, capture_output=True)


def ffmpeg_slideshow_with_audio(
    slides: List[Tuple[str, float]],
    audio_path: str,
    output_mp4_path: str,
    *,
    ken_burns: bool = False,
    crossfade: bool = False,
    xfade_seconds: float = 0.28,
) -> None:
    """slides: list of (png_path, duration_share) with shares summing to 1.0.
    ken_burns: subtle zoom during each still segment. crossfade: fade between segments (more CPU/RAM)."""
    ffmpeg = shutil.which("ffmpeg")
    if not ffmpeg:
        raise RuntimeError(
            "ffmpeg ist nicht installiert oder nicht im PATH. Lernvideo-Erstellung auf dem Server nicht möglich."
        )
    if not slides:
        raise ValueError("Keine Bilder für das Video.")
    for p, w in slides:
        if not p or w <= 0:
            raise ValueError("Ungültige Slide-Einträge (Pfad oder Anteil).")

    dur = _audio_duration_sec(audio_path)
    share_sum = sum(w for _, w in slides)
    if abs(share_sum - 1.0) > 0.02:
        raise ValueError("Slide-Daueranteile müssen sich zu 1.0 summieren.")

    n_slides = len(slides)
    fade_use = float(xfade_seconds) if crossfade and n_slides > 1 else 0.0
    # xfade shortens total video by (n-1)*fade; stretch segment lengths so merged video matches audio dur.
    time_stretch = (dur + max(0, n_slides - 1) * fade_use) / dur if dur > 0 else 1.0

    with tempfile.TemporaryDirectory() as tmp:
        parts: List[str] = []
        for i, (img, share) in enumerate(slides):
            seg_secs = dur * share * time_stretch
            if seg_secs < 0.05:
                seg_secs = max(1.0 / VIDEO_FPS, seg_secs)
            seg = os.path.join(tmp, f"seg_{i:03d}.mp4")
            _encode_segment(ffmpeg, img, seg_secs, seg, ken_burns=ken_burns)
            parts.append(seg)

        merged_v = os.path.join(tmp, "merged.mp4")
        if crossfade and len(parts) > 1:
            try:
                _merge_segments_xfade(ffmpeg, parts, fade_use, merged_v)
            except Exception as e:
                print(f"Lernvideo: xfade fehlgeschlagen ({e}), nutze harten Schnitt (concat).")
                lst = os.path.join(tmp, "vlist.txt")
                with open(lst, "w", encoding="utf-8") as f:
                    for p in parts:
                        f.write(f"file '{p.replace(chr(92), '/')}'\n")
                subprocess.run(
                    [
                        ffmpeg,
                        "-y",
                        "-threads",
                        "1",
                        "-f",
                        "concat",
                        "-safe",
                        "0",
                        "-i",
                        lst,
                        "-c",
                        "copy",
                        merged_v,
                    ],
                    check=True,
                    capture_output=True,
                )
        else:
            lst = os.path.join(tmp, "vlist.txt")
            with open(lst, "w", encoding="utf-8") as f:
                for p in parts:
                    f.write(f"file '{p.replace(chr(92), '/')}'\n")
            subprocess.run(
                [
                    ffmpeg,
                    "-y",
                    "-threads",
                    "1",
                    "-f",
                    "concat",
                    "-safe",
                    "0",
                    "-i",
                    lst,
                    "-c",
                    "copy",
                    merged_v,
                ],
                check=True,
                capture_output=True,
            )

        subprocess.run(
            [
                ffmpeg,
                "-y",
                "-threads",
                "1",
                "-i",
                merged_v,
                "-i",
                audio_path,
                "-c:v",
                "copy",
                "-c:a",
                "aac",
                "-b:a",
                "128k",
                "-movflags",
                "+faststart",
                "-shortest",
                output_mp4_path,
            ],
            check=True,
            capture_output=True,
        )
