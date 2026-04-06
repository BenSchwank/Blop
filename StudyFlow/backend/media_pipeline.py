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

# 960x540: ~56% pixels vs 720p — fits Render Starter (512MB) while staying sharp for slides + text.
VIDEO_W = 960
VIDEO_H = 540
VIDEO_FPS = 15
# CRF + maxrate keep file size under typical Supabase per-object limits on long TTS videos.
VIDEO_CRF = 25
# Cap peak bitrate (960p learning video); limits MP4 size vs. raw CRF-only VBR.
VIDEO_MAXRATE = "2M"
VIDEO_BUFSIZE = "4M"
# Ken-Burns: pad to exact 16:9 before zoompan (avoids odd-dimension crop glitches / clipped edges).
_KEN_PAD_W = 1152
_KEN_PAD_H = 648


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
    """Simple marker-like lines and shapes (PIL only — evokes whiteboard / explainer style)."""
    import random

    rng = random.Random(scene_idx * 9973 + 31)
    # Top-left squiggle
    pts = []
    x0, y0 = 36 + rng.randint(0, 24), 44 + rng.randint(0, 20)
    for k in range(10):
        pts.append((x0 + k * 16 + rng.randint(-5, 5), y0 + rng.randint(-8, 8)))
    if len(pts) >= 2:
        draw.line(pts, fill=accent, width=3)
    # Lightbulb / idea circle
    cx = width - 140 - rng.randint(0, 40)
    cy = 70 + rng.randint(0, 30)
    r = 22 + rng.randint(0, 8)
    draw.ellipse([cx - r, cy - r, cx + r, cy + r], outline=accent, width=2)
    draw.line([(cx, cy + r), (cx, cy + r + 18)], fill=accent, width=2)
    # Arrow
    ax1, ay1 = width // 2 + 120, canvas_h // 3
    ax2, ay2 = ax1 + 60, ay1 + 40
    draw.line([(ax1, ay1), (ax2, ay2)], fill=accent, width=2)
    draw.polygon([(ax2, ay2), (ax2 - 12, ay2 - 6), (ax2 - 8, ay2 + 4)], fill=accent)
    # Underline wave under imaginary title area
    wx = 48
    wy = min(canvas_h - 120, 160 + rng.randint(0, 40))
    wpts = [(wx + j * 28, wy + (j % 3 - 1) * 4) for j in range(18)]
    draw.line(wpts, fill=(accent[0] // 2 + 80, accent[1] // 2 + 80, accent[2] // 2 + 80), width=2)


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


def render_scene_slides(
    scenes: List[dict],
    width: int = VIDEO_W,
    height: int = VIDEO_H,
    visual_style: str = "clean",
    use_stock_images: bool = False,
) -> List[str]:
    """Writes PNG files per scene; returns paths."""
    try:
        from PIL import Image, ImageDraw, ImageFont
    except ImportError as e:
        raise RuntimeError("Pillow (PIL) wird für Lernvideos benötigt.") from e

    style = (visual_style or "clean").strip().lower()
    if style not in ("clean", "rich", "whiteboard"):
        style = "clean"

    font_title, font_body = _load_slide_fonts()
    bold_path, regular_path = _resolve_ttf_paths()
    font_footer = font_badge = None
    try:
        if regular_path:
            font_footer = ImageFont.truetype(regular_path, 17)
            font_badge = ImageFont.truetype(regular_path, 18)
    except OSError:
        font_footer = font_badge = font_body

    paths: List[str] = []
    tmp_root = tempfile.mkdtemp(prefix="blop_video_")

    palettes = [
        ((24, 32, 72), (12, 14, 38), (94, 156, 255)),
        ((40, 22, 62), (14, 18, 42), (236, 112, 180)),
        ((18, 52, 48), (10, 22, 28), (80, 220, 200)),
        ((62, 28, 24), (20, 14, 22), (255, 180, 100)),
    ]

    footer_h = 46
    margin = 56

    def line_h(line: str, font) -> int:
        if not font:
            return 26
        b = draw.textbbox((0, 0), line or "Ay", font=font)
        return max(24, b[3] - b[1] + 8)

    for i, sc in enumerate(scenes):
        is_title = str(sc.get("slide_kind") or "") == "title"
        title = str(sc.get("title") or (f"Szene {i+1}" if not is_title else "Lernvideo"))[:120]
        body = str(sc.get("body") or sc.get("bullets") or "")[:2000]
        chapter = str(sc.get("chapter") or "").strip()
        top_c, bot_c, accent = palettes[i % len(palettes)]
        scene_accent = (42, 118, 92) if style == "whiteboard" else accent

        if style == "whiteboard":
            wb_bg = (242, 239, 230)
            img = Image.new("RGB", (width, height), color=wb_bg)
            draw = ImageDraw.Draw(img)
            wb_accent = (42, 118, 92)
            _draw_whiteboard_doodles(draw, width, height - footer_h, i, wb_accent)
            draw.rounded_rectangle(
                [28, 28, width - 28, height - footer_h - 28],
                radius=8,
                outline=(198, 192, 176),
                width=2,
            )
        else:
            img = Image.new("RGB", (width, height), color=bot_c)
            draw = ImageDraw.Draw(img)

            if style == "rich":
                _draw_gradient_background(draw, width, height - footer_h, top_c, bot_c)
                draw.rectangle([0, 0, 12, height - footer_h], fill=accent)
            else:
                draw.rectangle([0, 0, width, height - footer_h], fill=bot_c)

        text_top_limit = height - footer_h - 24
        text_left = margin + (22 if style == "rich" else (28 if style == "whiteboard" else 0))

        photo_bytes = None
        if use_stock_images and not is_title and style != "whiteboard":
            q = str(sc.get("image_query") or "").strip()
            if q:
                photo_bytes = pexels_fetch_image_bytes(q)

        if photo_bytes:
            try:
                from io import BytesIO

                bg = Image.open(BytesIO(photo_bytes)).convert("RGB")
                bg = bg.resize((width, height - footer_h), Image.Resampling.LANCZOS)
                img.paste(bg, (0, 0))
                overlay = Image.new("RGBA", (width, height - footer_h), (12, 14, 35, 200))
                img = img.convert("RGBA")
                sub = img.crop((0, 0, width, height - footer_h))
                sub = Image.alpha_composite(sub.convert("RGBA"), overlay).convert("RGB")
                img.paste(sub, (0, 0))
                img = img.convert("RGB")
                draw = ImageDraw.Draw(img)
                if style == "rich":
                    draw.rectangle([0, 0, 10, height - footer_h], fill=accent)
            except Exception:
                draw = ImageDraw.Draw(img)

        title_fill = (245, 248, 255)
        body_fill = (210, 218, 235)
        if photo_bytes:
            title_fill = (255, 255, 255)
            body_fill = (230, 235, 245)
        if style == "whiteboard":
            title_fill = (28, 32, 36)
            body_fill = (52, 60, 68)

        if is_title:
            ft_big = fb_sub = font_title
            try:
                if bold_path:
                    ft_big = ImageFont.truetype(bold_path, 54)
                if regular_path:
                    fb_sub = ImageFont.truetype(regular_path, 28)
            except OSError:
                pass
            # Decorative frame
            if style == "rich":
                inset = 48
                draw.rounded_rectangle(
                    [inset, inset, width - inset, height - footer_h - inset],
                    radius=20,
                    outline=accent,
                    width=4,
                )
                inner = inset + 28
                draw.rounded_rectangle(
                    [inner, inner, width - inner, height - footer_h - inner],
                    radius=14,
                    fill=(14, 16, 32),
                )
            elif style == "whiteboard":
                inset = 40
                draw.rounded_rectangle(
                    [inset, inset, width - inset, height - footer_h - inset],
                    radius=18,
                    fill=(250, 247, 240),
                    outline=scene_accent,
                    width=3,
                )
            max_w = width - 2 * margin - 80
            y = (height - footer_h) // 3
            for line in _wrap_text(draw, title, ft_big, max_w):
                bbox = draw.textbbox((0, 0), line, font=ft_big)
                lw = bbox[2] - bbox[0]
                draw.text(((width - lw) // 2, y), line, fill=title_fill, font=ft_big)
                y += line_h(line, ft_big)
            y += 28
            for line in _wrap_text(draw, body, fb_sub, max_w - 40):
                bbox = draw.textbbox((0, 0), line, font=fb_sub)
                lw = bbox[2] - bbox[0]
                draw.text(((width - lw) // 2, y), line, fill=body_fill, font=fb_sub)
                y += line_h(line, fb_sub)
                if y > text_top_limit - 40:
                    break
        else:
            if chapter and style in ("rich", "whiteboard"):
                _draw_chapter_badge(
                    draw,
                    width,
                    chapter,
                    font_badge,
                    scene_accent if style == "whiteboard" else accent,
                    margin,
                )
            card_margin = margin
            card_top = margin + (52 if chapter and style in ("rich", "whiteboard") else 8)
            card_w = width - 2 * card_margin - (22 if style == "rich" else 0)
            # Title card background (rich)
            if style == "rich" and not photo_bytes:
                title_lines = _wrap_text(draw, title, font_title, card_w - 40)
                body_lines = _wrap_text(draw, body, font_body, card_w - 40)
                est_h = 20 + len(title_lines) * 52 + 20 + min(len(body_lines), 12) * 36
                card_h = min(max(120, est_h), text_top_limit - card_top + 20)
                draw.rounded_rectangle(
                    [text_left - 16, card_top - 12, text_left + card_w, card_top + card_h],
                    radius=16,
                    fill=(28, 30, 52),
                    outline=accent,
                    width=2,
                )
            elif style == "whiteboard" and not photo_bytes:
                title_lines = _wrap_text(draw, title, font_title, card_w - 40)
                body_lines = _wrap_text(draw, body, font_body, card_w - 40)
                est_h = 20 + len(title_lines) * 52 + 20 + min(len(body_lines), 12) * 36
                card_h = min(max(120, est_h), text_top_limit - card_top + 20)
                draw.rounded_rectangle(
                    [text_left - 16, card_top - 12, text_left + card_w, card_top + card_h],
                    radius=14,
                    fill=(252, 249, 242),
                    outline=scene_accent,
                    width=2,
                )
            y = card_top + 8
            for line in _wrap_text(draw, title, font_title, card_w - 24):
                draw.text((text_left, y), line, fill=title_fill, font=font_title)
                y += line_h(line, font_title)
            y += 16
            for line in _wrap_text(draw, body, font_body, card_w - 24):
                draw.text((text_left, y), line, fill=body_fill, font=font_body)
                y += line_h(line, font_body)
                if y > text_top_limit:
                    break

        _draw_footer_bar(draw, width, height, font_footer, scene_accent)

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
        f"pad={VIDEO_W}:{VIDEO_H}:(ow-iw)/2:(oh-ih)/2,setsar=1"
    )


def _vf_kb_pre_pad() -> str:
    """Pad to fixed 16:9 before zoompan so crop math stays centered (fixes left/right clipping)."""
    return (
        f"scale={_KEN_PAD_W}:{_KEN_PAD_H}:force_original_aspect_ratio=decrease,"
        f"pad={_KEN_PAD_W}:{_KEN_PAD_H}:(ow-iw)/2:(oh-ih)/2,setsar=1"
    )


def _vf_ken_burns(seg_secs: float, max_zoom: float = 1.12) -> str:
    d_frames = max(1, int(seg_secs * VIDEO_FPS))
    zmax = max(1.02, min(max_zoom, 1.2))
    denom = max(1, d_frames - 1)
    step = (zmax - 1.0) / denom
    return (
        f"{_vf_kb_pre_pad()},"
        f"zoompan=z='min(zoom+{step:.8f},{zmax})':x='(iw-iw/zoom)/2':y='(ih-ih/zoom)/2':d={d_frames}:"
        f"s={VIDEO_W}x{VIDEO_H}:fps={VIDEO_FPS},setsar=1"
    )


def _vf_pan_horizontal(seg_secs: float) -> str:
    """Slow horizontal pan across a slightly oversized frame (no zoompan)."""
    d_frames = max(1, int(seg_secs * VIDEO_FPS))
    denom = max(1, d_frames - 1)
    return (
        f"scale=1440:810:force_original_aspect_ratio=decrease,pad=1440:810:(ow-iw)/2:(oh-ih)/2,"
        f"crop=960:540:x='floor((iw-960)*n/{denom})':y='(ih-540)/2',setsar=1"
    )


def _vf_for_segment(seg_idx: int, seg_secs: float, motion: str) -> str:
    """motion: static | ken_burns | mixed (cycles static / soft zoom / pan)."""
    m = (motion or "static").strip().lower()
    if m == "mixed":
        # 0 static, 1 soft zoom, 2 pan, 3 soft zoom (repeat)
        mode = ("static", "kb_soft", "pan", "kb_soft")[seg_idx % 4]
    elif m == "ken_burns":
        mode = "ken_burns"
    else:
        mode = "static"

    if mode == "static":
        return _vf_static()
    if mode == "pan":
        return _vf_pan_horizontal(seg_secs)
    if mode == "kb_soft":
        return _vf_ken_burns(seg_secs, max_zoom=1.08)
    return _vf_ken_burns(seg_secs, max_zoom=1.12)


def _encode_segment(
    ffmpeg: str,
    img: str,
    seg_secs: float,
    out_path: str,
    motion: str,
    seg_index: int,
) -> None:
    vf = _vf_for_segment(seg_index, seg_secs, motion)
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
        ],
        check=True,
        capture_output=True,
    )


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
    if motion not in ("static", "ken_burns", "mixed"):
        motion = "static"

    dur = _audio_duration_sec(audio_path)
    seg_secs = max(2.5, dur / len(image_paths))

    with tempfile.TemporaryDirectory() as tmp:
        parts: List[str] = []
        for i, img in enumerate(image_paths):
            seg = os.path.join(tmp, f"seg_{i:03d}.mp4")
            _encode_segment(ffmpeg, img, seg_secs, seg, motion, i)
            parts.append(seg)

        # Always concat with stream copy (no xfade filter graph). xfade loaded N decoders at once and
        # OOM'd Starter (512MB). Ken-Burns motion stays per segment; transitions are clean cuts.
        merged_v = os.path.join(tmp, "merged.mp4")
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
