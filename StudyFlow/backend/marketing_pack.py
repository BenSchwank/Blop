"""
Social Content Pack: multimodal OpenAI plan, hero crops, 15s brainrot clips, ZIP export.
"""
from __future__ import annotations

import base64
import json
import os
import re
import shutil
import subprocess
import tempfile
import uuid
import zipfile
from typing import Any, Dict, List, Optional, Tuple

import requests
from PIL import Image

from media_pipeline import (
    _has_video_stream,
    openai_tts_speech_mp3_concat_segments,
    render_marketing_brainrot_clip,
)


def _env_int(name: str, default: int, lo: int, hi: int) -> int:
    raw = (os.environ.get(name, "") or "").strip()
    if not raw:
        return default
    try:
        return max(lo, min(hi, int(raw)))
    except ValueError:
        return default


def _env_flag(name: str, default: bool = False) -> bool:
    raw = (os.environ.get(name, "") or "").strip().lower()
    if not raw:
        return default
    return raw in {"1", "true", "yes", "on"}


def _split_script_for_tts(script: str, max_segments: int = 10) -> List[str]:
    text = (script or "").strip()
    if not text:
        return []
    parts = re.split(r"(?<=[.!?])\s+", text)
    cleaned = [p.strip() for p in parts if p.strip()]
    if not cleaned:
        return [text]
    if len(cleaned) <= max_segments:
        return cleaned
    merged: List[str] = []
    bucket = ""
    for p in cleaned:
        if not bucket:
            bucket = p
            continue
        if len(bucket) + 1 + len(p) < 420:
            bucket = f"{bucket} {p}"
        else:
            merged.append(bucket)
            bucket = p
    if bucket:
        merged.append(bucket)
    return merged[:max_segments]


def _tts_evangelist_instruction(emotion: str, idx: int, total: int) -> str:
    base = (
        "Male voice, hyper-enthusiastic tech evangelist: slightly fast, punchy, selling the idea, "
        "never robotic, micro pauses before punchlines, dynamic energy."
    )
    hooks = [
        "Open with a hype hook, then tighten into crisp facts.",
        "Lean into urgency and FOMO without sounding fake.",
        "Use playful emphasis on product nouns.",
    ]
    return f"{base} {hooks[idx % len(hooks)]}"


def _probe_duration_sec(path: str) -> float:
    ffprobe = shutil.which("ffprobe")
    if not ffprobe:
        return 10.0
    p = subprocess.run(
        [
            ffprobe,
            "-v",
            "error",
            "-show_entries",
            "format=duration",
            "-of",
            "default=nw=1:nk=1",
            path,
        ],
        capture_output=True,
        text=True,
    )
    try:
        return max(0.5, float((p.stdout or "").strip()))
    except ValueError:
        return 10.0


def extract_video_frames_jpeg(video_path: str, out_dir: str, n: int = 5) -> List[str]:
    ffmpeg = shutil.which("ffmpeg")
    if not ffmpeg or not os.path.isfile(video_path):
        return []
    dur = _probe_duration_sec(video_path)
    n = max(1, min(n, 8))
    times = [dur * (0.08 + 0.84 * i / max(1, n - 1)) for i in range(n)] if n > 1 else [min(1.0, dur * 0.1)]
    paths: List[str] = []
    for i, t in enumerate(times):
        out = os.path.join(out_dir, f"frame_{i:02d}.jpg")
        subprocess.run(
            [
                ffmpeg,
                "-y",
                "-ss",
                f"{t:.3f}",
                "-i",
                video_path,
                "-frames:v",
                "1",
                "-vf",
                "scale=512:-1",
                "-q:v",
                "5",
                out,
            ],
            check=False,
            capture_output=True,
        )
        if os.path.isfile(out) and os.path.getsize(out) > 50:
            paths.append(out)
    return paths


def extract_video_poster(video_path: str, out_path: str) -> bool:
    ffmpeg = shutil.which("ffmpeg")
    if not ffmpeg:
        return False
    dur = _probe_duration_sec(video_path)
    t = max(0.0, dur * 0.35)
    subprocess.run(
        [
            ffmpeg,
            "-y",
            "-ss",
            f"{t:.3f}",
            "-i",
            video_path,
            "-frames:v",
            "1",
            "-vf",
            "scale=720:-1",
            "-q:v",
            "4",
            out_path,
        ],
        check=False,
        capture_output=True,
    )
    return os.path.isfile(out_path) and os.path.getsize(out_path) > 50


def _image_to_data_url(path: str) -> str:
    with open(path, "rb") as f:
        raw = f.read()
    ext = os.path.splitext(path)[1].lower()
    mime = "image/png" if ext == ".png" else "image/jpeg"
    b64 = base64.b64encode(raw).decode("ascii")
    return f"data:{mime};base64,{b64}"


def _default_plan(bulletpoints: str, language: str, max_clips: int) -> Dict[str, Any]:
    bp = (bulletpoints or "Update").strip()[:200]
    clips = []
    for i in range(max_clips):
        clips.append(
            {
                "title": f"Clip {i + 1}",
                "hook": "Das musst du sehen — jetzt." if language == "de" else "You need to see this — now.",
                "script": (
                    f"Hook: krasser Update-Moment. {bp} "
                    f"Mittelteil: kurz erklaeren warum es zaehlt. "
                    f"Outro: Follow für mehr Builds."
                    if language == "de"
                    else f"Hook: huge update. {bp} Middle: why it matters. Outro: follow for more."
                ),
                "caption": bp[:120],
                "hashtags": ["#buildinpublic", "#dev", "#app"] if language != "de" else ["#coding", "#app", "#update"],
                "platform_hint": "TikTok" if i % 2 == 0 else "Instagram Reels",
                "posting_hint": "18–22 Uhr lokal, Mo–Do",
                "list_title": "Top 3:" if language == "de" else "Top 3:",
                "list_items": ["Schneller", "Cleaner", "Mehr Wow"] if language == "de" else ["Faster", "Cleaner", "More wow"],
                "cta_line": "Kommentar: Was shipst du als Nächstes?" if language == "de" else "Comment what you ship next.",
                "ki_visual_brief": "Neon glitch abstract with app motif.",
                "hero_bbox_01": None,
            }
        )
    return {
        "material_summary": bp or "User material",
        "extracted_elements": [],
        "clips": clips,
    }


def _parse_json_content(raw: str) -> Dict[str, Any]:
    text = (raw or "").strip()
    if text.startswith("```"):
        text = re.sub(r"^```(?:json)?\s*", "", text)
        text = re.sub(r"\s*```$", "", text)
    return json.loads(text)


def call_openai_pack_plan(
    image_paths: List[str],
    bulletpoints: str,
    language: str,
    max_clips: int,
) -> Dict[str, Any]:
    if not image_paths:
        return _default_plan(bulletpoints, language, max_clips)
    key = os.environ.get("OPENAI_API_KEY", "").strip()
    if not key:
        return _default_plan(bulletpoints, language, max_clips)

    langs = {"de": "German", "en": "English", "tr": "Turkish", "es": "Spanish"}
    lang_name = langs.get((language or "de").strip().lower(), "German")
    vision_cap = min(4, len(image_paths))
    content: List[Dict[str, Any]] = [
        {
            "type": "text",
            "text": (
                f"You are a viral TikTok/Instagram strategist. Analyze the attached image(s). "
                f"User notes (optional): {bulletpoints.strip()[:800]}\n"
                f"Language for ALL user-facing strings: {lang_name}.\n"
                "Return ONLY valid JSON (no markdown). Top-level keys: material_summary (string), "
                "extracted_elements (array of {label, role, bbox_01:{x,y,w,h} 0-1 relative to FIRST image}), "
                "clips (array). Each clip object: title, hook, script (~40-70 words for ~15s VO), caption, hashtags, "
                "platform_hint, posting_hint, list_title, list_items, cta_line, ki_visual_brief, hero_bbox_01 (optional).\n"
                f"Rules: exactly {max_clips} clips; each clip DISTINCT angle/hook; never ask questions; "
                "if unsure use hero_bbox_01 null."
            ),
        }
    ]
    for p in image_paths[:vision_cap]:
        if os.path.isfile(p):
            content.append({"type": "image_url", "image_url": {"url": _image_to_data_url(p)}})

    response = requests.post(
        "https://api.openai.com/v1/chat/completions",
        headers={"Authorization": f"Bearer {key}", "Content-Type": "application/json"},
        json={
            "model": "gpt-4o",
            "messages": [{"role": "user", "content": content}],
            "temperature": 0.85,
            "response_format": {"type": "json_object"},
        },
        timeout=180,
    )
    if not response.ok:
        return _default_plan(bulletpoints, language, max_clips)
    raw = (
        response.json()
        .get("choices", [{}])[0]
        .get("message", {})
        .get("content", "")
        .strip()
    )
    try:
        data = _parse_json_content(raw)
    except (json.JSONDecodeError, ValueError):
        return _default_plan(bulletpoints, language, max_clips)
    clips = data.get("clips")
    if not isinstance(clips, list) or len(clips) == 0:
        return _default_plan(bulletpoints, language, max_clips)
    data["clips"] = clips[:max_clips]
    while len(data["clips"]) < max_clips:
        extra = _default_plan(bulletpoints, language, max_clips - len(data["clips"]))
        data["clips"].extend(extra["clips"])
    data["clips"] = data["clips"][:max_clips]
    return data


def _bbox_from_plan(plan: Dict[str, Any], clip: Dict[str, Any]) -> Optional[Dict[str, float]]:
    hb = clip.get("hero_bbox_01")
    if isinstance(hb, dict) and all(k in hb for k in ("x", "y", "w", "h")):
        try:
            return {k: float(hb[k]) for k in ("x", "y", "w", "h")}
        except (TypeError, ValueError):
            pass
    elems = plan.get("extracted_elements")
    if isinstance(elems, list) and elems:
        first = elems[0]
        if isinstance(first, dict):
            bb = first.get("bbox_01")
            if isinstance(bb, dict) and all(k in bb for k in ("x", "y", "w", "h")):
                try:
                    return {k: float(bb[k]) for k in ("x", "y", "w", "h")}
                except (TypeError, ValueError):
                    pass
    return None


def crop_hero_to_png(src_path: str, bbox: Optional[Dict[str, float]], out_png: str) -> None:
    with Image.open(src_path) as im:
        im = im.convert("RGBA")
        w, h = im.size
        if bbox and w > 0 and h > 0:
            x = int(max(0, min(w - 1, bbox["x"] * w)))
            y = int(max(0, min(h - 1, bbox["y"] * h)))
            bw = int(max(16, min(w - x, bbox["w"] * w)))
            bh = int(max(16, min(h - y, bbox["h"] * h)))
            box = (x, y, x + bw, y + bh)
        else:
            margin_x = int(w * 0.15)
            margin_y = int(h * 0.15)
            box = (margin_x, margin_y, w - margin_x, h - margin_y)
        cropped = im.crop(box)
        cropped.save(out_png, "PNG")


def optional_openai_image(brief: str, out_png: str) -> bool:
    if not _env_flag("MARKETING_PACK_GENERATE_IMAGES", False):
        return False
    key = os.environ.get("OPENAI_API_KEY", "").strip()
    if not key or not brief.strip():
        return False
    r = requests.post(
        "https://api.openai.com/v1/images/generations",
        headers={"Authorization": f"Bearer {key}", "Content-Type": "application/json"},
        json={
            "model": "dall-e-3",
            "prompt": (brief[:900] + " Pop-art neon brainrot sticker vibe, flat shapes, high contrast."),
            "n": 1,
            "size": "1024x1024",
            "response_format": "b64_json",
        },
        timeout=120,
    )
    if not r.ok:
        return False
    try:
        b64 = r.json()["data"][0]["b64_json"]
    except (KeyError, IndexError, TypeError):
        return False
    raw = base64.b64decode(b64)
    with open(out_png, "wb") as f:
        f.write(raw)
    return os.path.isfile(out_png) and os.path.getsize(out_png) > 100


def build_marketing_pack(
    source_paths: List[str],
    bulletpoints: str,
    language: str,
    emotion: str,
    work_tmp: str,
) -> Tuple[Dict[str, Any], bytes, List[Tuple[str, bytes]]]:
    """
    Returns (response_json_dict, zip_bytes, clip_exports list of (export_id, mp4_bytes)).
    """
    max_clips = _env_int("MARKETING_PACK_MAX_CLIPS", 3, 1, 6)
    clip_duration = 15.0

    if not source_paths:
        raise ValueError("Keine Quelldateien.")

    primary = source_paths[0]
    frames_dir = os.path.join(work_tmp, "vision_frames")
    os.makedirs(frames_dir, exist_ok=True)

    hero_src = os.path.join(work_tmp, "hero_source.jpg")
    vision_inputs: List[str] = []

    if _has_video_stream(primary):
        extract_video_poster(primary, hero_src)
        if not os.path.isfile(hero_src):
            subprocess.run(
                [shutil.which("ffmpeg") or "ffmpeg", "-y", "-i", primary, "-frames:v", "1", "-q:v", "6", hero_src],
                check=False,
                capture_output=True,
            )
        vision_inputs = extract_video_frames_jpeg(primary, frames_dir, n=5)
        if hero_src not in vision_inputs and os.path.isfile(hero_src):
            vision_inputs.insert(0, hero_src)
        if (not os.path.isfile(hero_src) or os.path.getsize(hero_src) < 80) and vision_inputs:
            shutil.copy2(vision_inputs[0], hero_src)
    else:
        shutil.copy2(primary, hero_src)
        vision_inputs = [hero_src]

    if not vision_inputs and os.path.isfile(hero_src):
        vision_inputs = [hero_src]
    plan = call_openai_pack_plan(vision_inputs, bulletpoints, language, max_clips)
    clips_out: List[Dict[str, Any]] = []
    mp4_paths: List[str] = []
    clip_exports: List[Tuple[str, bytes]] = []

    for idx, clip in enumerate(plan.get("clips", [])[:max_clips]):
        if not isinstance(clip, dict):
            continue
        bbox = _bbox_from_plan(plan, clip)
        hero_png = os.path.join(work_tmp, f"hero_{idx:02d}.png")
        crop_hero_to_png(hero_src, bbox, hero_png)

        accent_path: Optional[str] = None
        accent_file = os.path.join(work_tmp, f"accent_{idx:02d}.png")
        brief = str(clip.get("ki_visual_brief") or "").strip()
        if optional_openai_image(brief, accent_file):
            accent_path = accent_file

        script = str(clip.get("script") or clip.get("hook") or "Update.").strip()
        segs = _split_script_for_tts(script, max_segments=8)
        pairs: List[Tuple[str, str]] = []
        tot = len(segs) or 1
        for i, seg in enumerate(segs or [script]):
            pairs.append((seg, _tts_evangelist_instruction(emotion, i, tot)))
        mp3 = openai_tts_speech_mp3_concat_segments(pairs, voice="onyx")
        audio_path = os.path.join(work_tmp, f"narr_{idx:02d}.mp3")
        with open(audio_path, "wb") as f:
            f.write(mp3)

        out_mp4 = os.path.join(work_tmp, f"clip_{idx:02d}.mp4")
        render_marketing_brainrot_clip(
            hero_png,
            audio_path,
            out_mp4,
            clip_script=script,
            hook_line=str(clip.get("hook") or "")[:120],
            list_title=str(clip.get("list_title") or "Top:")[:80],
            list_items=[str(x) for x in (clip.get("list_items") or []) if str(x).strip()][:5],
            cta_line=str(clip.get("cta_line") or "")[:120],
            emotion=emotion,
            duration_sec=clip_duration,
            accent_path=accent_path,
        )
        mp4_paths.append(out_mp4)
        with open(out_mp4, "rb") as f:
            b = f.read()
        eid = str(uuid.uuid4())
        clip_exports.append((eid, b))

        clip_response = dict(clip)
        clip_response["video_url"] = f"/api/ai/marketing-short/video/{eid}"
        clips_out.append(clip_response)

    plan_out = {
        "material_summary": plan.get("material_summary", ""),
        "extracted_elements": plan.get("extracted_elements", []),
        "clips": clips_out,
    }

    zip_buf = os.path.join(work_tmp, "pack.zip")
    with zipfile.ZipFile(zip_buf, "w", zipfile.ZIP_DEFLATED) as zf:
        zf.writestr("pack.json", json.dumps(plan_out, ensure_ascii=False, indent=2))
        for i, p in enumerate(mp4_paths):
            zf.write(p, arcname=f"clip_{i+1:02d}.mp4")

    with open(zip_buf, "rb") as f:
        zip_bytes = f.read()

    return plan_out, zip_bytes, clip_exports
