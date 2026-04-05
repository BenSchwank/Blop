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

# OpenAI TTS max input length per request
TTS_CHUNK_CHARS = 3800

OPENAI_TTS_VOICES = frozenset({"alloy", "echo", "fable", "onyx", "nova", "shimmer"})

VIDEO_W = 1280
VIDEO_H = 720
VIDEO_FPS = 18
XFADE_SEC = 0.45


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
        r = requests.post(
            "https://api.openai.com/v1/audio/speech",
            headers={"Authorization": f"Bearer {key}", "Content-Type": "application/json"},
            json={"model": "tts-1", "input": ch, "voice": v, "response_format": "mp3"},
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


def _load_slide_fonts():
    from PIL import ImageFont

    paths = _slide_font_paths()
    title_font = body_font = None
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


def render_scene_slides(
    scenes: List[dict],
    width: int = VIDEO_W,
    height: int = VIDEO_H,
    visual_style: str = "clean",
    use_stock_images: bool = False,
) -> List[str]:
    """Writes PNG files per scene; returns paths."""
    try:
        from PIL import Image, ImageDraw
    except ImportError as e:
        raise RuntimeError("Pillow (PIL) wird für Lernvideos benötigt.") from e

    style = (visual_style or "clean").strip().lower()
    if style not in ("clean", "rich"):
        style = "clean"

    font_title, font_body = _load_slide_fonts()
    paths: List[str] = []
    tmp_root = tempfile.mkdtemp(prefix="blop_video_")

    palettes = [
        ((24, 32, 72), (12, 14, 38), (94, 156, 255)),
        ((40, 22, 62), (14, 18, 42), (236, 112, 180)),
        ((18, 52, 48), (10, 22, 28), (80, 220, 200)),
        ((62, 28, 24), (20, 14, 22), (255, 180, 100)),
    ]

    for i, sc in enumerate(scenes):
        title = str(sc.get("title") or f"Szene {i+1}")[:120]
        body = str(sc.get("body") or sc.get("bullets") or "")[:2000]
        top_c, bot_c, accent = palettes[i % len(palettes)]

        img = Image.new("RGB", (width, height), color=bot_c)
        draw = ImageDraw.Draw(img)

        if style == "rich":
            _draw_gradient_background(draw, width, height, top_c, bot_c)
            draw.rectangle([0, 0, 12, height], fill=accent)
        else:
            draw.rectangle([0, 0, width, height], fill=bot_c)

        margin = 56
        text_left = margin + (20 if style == "rich" else 0)
        y = margin + 8

        photo_bytes = None
        if use_stock_images:
            q = str(sc.get("image_query") or "").strip()
            if q:
                photo_bytes = pexels_fetch_image_bytes(q)

        if photo_bytes:
            try:
                from io import BytesIO

                bg = Image.open(BytesIO(photo_bytes)).convert("RGB")
                bg = bg.resize((width, height), Image.Resampling.LANCZOS)
                img.paste(bg, (0, 0))
                overlay = Image.new("RGBA", (width, height), (12, 14, 35, 200))
                img = img.convert("RGBA")
                img = Image.alpha_composite(img, overlay).convert("RGB")
                draw = ImageDraw.Draw(img)
                if style == "rich":
                    draw.rectangle([0, 0, 10, height], fill=accent)
            except Exception:
                draw = ImageDraw.Draw(img)

        title_fill = (245, 248, 255)
        body_fill = (210, 218, 235)
        if photo_bytes:
            title_fill = (255, 255, 255)
            body_fill = (230, 235, 245)

        def _line_h(line: str, font) -> int:
            if not font:
                return 26
            b = draw.textbbox((0, 0), line or "Ay", font=font)
            return max(24, b[3] - b[1] + 8)

        for line in _wrap_text(draw, title, font_title, width - 2 * margin - 20):
            draw.text((text_left, y), line, fill=title_fill, font=font_title)
            y += _line_h(line, font_title)
        y += 18
        for line in _wrap_text(draw, body, font_body, width - 2 * margin - 20):
            draw.text((text_left, y), line, fill=body_fill, font=font_body)
            y += _line_h(line, font_body)
            if y > height - 48:
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


def _vf_static() -> str:
    return (
        f"scale={VIDEO_W}:{VIDEO_H}:force_original_aspect_ratio=decrease,"
        f"pad={VIDEO_W}:{VIDEO_H}:(ow-iw)/2:(oh-ih)/2"
    )


def _vf_ken_burns(seg_secs: float) -> str:
    d_frames = max(1, int(seg_secs * VIDEO_FPS))
    # Slight zoom-in over the segment; supersampled then zoompan to output size
    return (
        f"scale=1920:-1,"
        f"zoompan=z='min(zoom+0.0014,1.22)':x='iw/2-(iw/zoom/2)':y='ih/2-(ih/zoom/2)':d={d_frames}:"
        f"s={VIDEO_W}x{VIDEO_H}:fps={VIDEO_FPS}"
    )


def _encode_segment(
    ffmpeg: str,
    img: str,
    seg_secs: float,
    out_path: str,
    motion: str,
) -> None:
    motion = (motion or "static").strip().lower()
    vf = _vf_ken_burns(seg_secs) if motion == "ken_burns" else _vf_static()
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
            vf,
            "-r",
            str(VIDEO_FPS),
            "-c:v",
            "libx264",
            "-preset",
            "ultrafast",
            "-crf",
            "26",
            "-pix_fmt",
            "yuv420p",
            "-an",
            out_path,
        ],
        check=True,
        capture_output=True,
    )


def _xfade_chain_filter(n: int, seg_secs: float) -> str:
    """Build filter_complex for n video inputs of equal duration seg_secs."""
    if n <= 1:
        return ""
    d = XFADE_SEC
    parts: List[str] = []
    cur_label = "[0:v]"
    cur_dur = seg_secs
    for i in range(1, n):
        offset = max(0.1, cur_dur - d)
        out = f"v{i}" if i < n - 1 else "vout"
        parts.append(f"{cur_label}[{i}:v]xfade=transition=fade:duration={d}:offset={offset}[{out}]")
        cur_dur = cur_dur + seg_secs - d
        cur_label = f"[{out}]"
    return ";".join(parts)


def ffmpeg_slideshow_with_audio(
    image_paths: List[str],
    audio_path: str,
    output_mp4_path: str,
    motion: str = "static",
) -> None:
    ffmpeg = shutil.which("ffmpeg")
    if not ffmpeg:
        raise RuntimeError(
            "ffmpeg ist nicht installiert oder nicht im PATH. Lernvideo-Erstellung auf dem Server nicht möglich."
        )
    if not image_paths:
        raise ValueError("Keine Bilder für das Video.")
    motion = (motion or "static").strip().lower()
    if motion not in ("static", "ken_burns"):
        motion = "static"

    dur = _audio_duration_sec(audio_path)
    seg_secs = max(2.5, dur / len(image_paths))
    n = len(image_paths)

    with tempfile.TemporaryDirectory() as tmp:
        parts: List[str] = []
        for i, img in enumerate(image_paths):
            seg = os.path.join(tmp, f"seg_{i:03d}.mp4")
            _encode_segment(ffmpeg, img, seg_secs, seg, motion)
            parts.append(seg)

        if motion == "ken_burns" and n > 1:
            fc = _xfade_chain_filter(n, seg_secs)
            merged_v = os.path.join(tmp, "merged.mp4")
            cmd: List[str] = [ffmpeg, "-y"]
            for p in parts:
                cmd.extend(["-i", p])
            cmd.extend(
                [
                    "-filter_complex",
                    fc,
                    "-map",
                    "[vout]",
                    "-c:v",
                    "libx264",
                    "-preset",
                    "ultrafast",
                    "-crf",
                    "26",
                    "-pix_fmt",
                    "yuv420p",
                    "-an",
                    merged_v,
                ]
            )
            subprocess.run(cmd, check=True, capture_output=True)
        else:
            merged_v = os.path.join(tmp, "merged.mp4")
            lst = os.path.join(tmp, "vlist.txt")
            with open(lst, "w", encoding="utf-8") as f:
                for p in parts:
                    f.write(f"file '{p.replace(chr(92), '/')}'\n")
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
                "-movflags",
                "+faststart",
                "-shortest",
                output_mp4_path,
            ],
            check=True,
            capture_output=True,
        )
