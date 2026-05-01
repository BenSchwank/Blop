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


def _subtitle_chunks(script_text: str, max_words_per_chunk: int = 4, max_chunks: int = 26) -> List[str]:
    tokens = [t.strip() for t in re.split(r"\s+", (script_text or "").strip()) if t.strip()]
    if not tokens:
        return ["BLOP DEVLOG"]
    chunks: List[str] = []
    for i in range(0, len(tokens), max_words_per_chunk):
        chunks.append(" ".join(tokens[i : i + max_words_per_chunk]).upper())
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
    segment_duration = max(1.5, float(target_duration) / float(len(media_paths)))

    subtitle_chunks = _subtitle_chunks(script_text)
    chunk_duration = max(0.45, float(target_duration) / float(len(subtitle_chunks)))
    subtitle_filters: List[str] = []
    for idx, chunk in enumerate(subtitle_chunks):
        start = idx * chunk_duration
        end = min(float(target_duration), start + chunk_duration + 0.18)
        subtitle_filters.append(
            "drawtext="
            f"text='{_escape_drawtext(chunk)}':"
            "fontcolor=white:"
            "fontsize=h*0.08:"
            "borderw=3:"
            "bordercolor=black@0.88:"
            "box=1:"
            "boxcolor=black@0.42:"
            "boxborderw=16:"
            "x=(w-text_w)/2:"
            "y=h*0.70:"
            f"enable='between(t,{start:.2f},{end:.2f})'"
        )
    subtitle_chain = ",".join(subtitle_filters)
    vertical_vf = (
        f"scale={target_w}:{target_h}:force_original_aspect_ratio=decrease,"
        f"pad={target_w}:{target_h}:(ow-iw)/2:(oh-ih)/2,setsar=1,{subtitle_chain}"
    )

    with tempfile.TemporaryDirectory(prefix="blop_marketing_segments_") as tmp:
        segment_paths: List[str] = []
        for idx, media_path in enumerate(media_paths):
            if not os.path.exists(media_path):
                continue
            segment_path = os.path.join(tmp, f"segment_{idx:03d}.mp4")
            if _has_video_stream(media_path):
                media_duration = max(1.0, _probe_video_duration_sec(media_path))
                clip_duration = min(segment_duration, media_duration)
                clip_start = 0.0
                if media_duration > clip_duration + 0.2:
                    slot = (idx * 1.37) % max(0.5, media_duration - clip_duration)
                    clip_start = max(0.0, slot)
                cmd = [
                    ffmpeg,
                    "-y",
                    "-ss",
                    f"{clip_start:.3f}",
                    "-i",
                    media_path,
                    "-t",
                    f"{clip_duration:.3f}",
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
                image_frames = max(3, int(round(segment_duration * float(target_fps))))
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
                    f"{segment_duration:.3f}",
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

        if not segment_paths:
            raise RuntimeError("Keine renderbaren Media-Segmente erzeugt.")

        concat_list = os.path.join(tmp, "concat.txt")
        with open(concat_list, "w", encoding="utf-8") as f:
            for seg in segment_paths:
                f.write(f"file '{seg.replace(chr(92), '/')}'\n")

        merged_video = os.path.join(tmp, "merged_vertical.mp4")
        emotion_tempo = {
            "energetic": "1.16",
            "confident": "1.1",
            "calm": "1.02",
            "dramatic": "1.06",
        }.get((emotion or "energetic").strip().lower(), "1.16")
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
                "-af",
                f"atempo={emotion_tempo},loudnorm=I=-16:TP=-1.5:LRA=11,apad",
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
