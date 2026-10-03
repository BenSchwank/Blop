"""Two-person podcast script parsing and human-like voice delivery for TTS."""
import re
from typing import List, Optional, Tuple

# UI ids -> Gemini / OpenRouter TTS voice names.
# Pairing is meant to sound like two real people at a table, not two announcers.
PODCAST_VOICES = {
    "aoede": "Aoede",
    "leda": "Leda",
    "zephyr": "Zephyr",
    "puck": "Puck",
    "charon": "Charon",
    "kore": "Kore",
}

LEGACY_PODCAST_VOICES = {
    "alloy": "aoede",
    "echo": "charon",
    "fable": "leda",
    "onyx": "kore",
    "nova": "zephyr",
    "shimmer": "puck",
}

_CONTRAST = {
    "aoede": "charon",
    "leda": "puck",
    "zephyr": "charon",
    "puck": "aoede",
    "charon": "aoede",
    "kore": "leda",
}

_SPEAKER_LINE = re.compile(r"^(ALEX|SAM)\s*[:：\-–]\s*(.*)$", re.IGNORECASE)
_STAGE_DIRECTION = re.compile(r"\[([^\[\]]{0,80})\]")
_MAX_TURNS = 96

# Base delivery for gpt-4o-mini-tts / Gemini speech_metadata.style.
# Keep this concrete: emotion, pace, breath, German conversational cadence.
ALEX_STYLE = (
    "You are Alex, a curious German student talking to a close friend at the same table. "
    "Sound alive: warm smile in the voice, rising intonation on questions, soft laughter "
    "when something clicks, brief thoughtful hesitations like 'ähm' or 'warte mal'. "
    "Natural conversational pace — not rushed, not a newsreader, not monotone. "
    "Vary pitch and energy. React emotionally before you ask the next thing."
)
SAM_STYLE = (
    "You are Sam, a warm German explainer talking to a friend, not giving a lecture. "
    "Sound engaged and human: emphasize the key word, lean in when the insight lands, "
    "soften when reassuring, light chuckle when a comparison is funny. "
    "Natural pauses to breathe. Conversational German rhythm. "
    "Never flat, never robotic, never like an audiobook narrator reading slides."
)

_VOICE_STYLE = {
    "aoede": ALEX_STYLE + " Bright, friendly, slightly higher energy.",
    "leda": ALEX_STYLE + " Clear younger tone, quick curiosity, soft edges.",
    "zephyr": ALEX_STYLE + " Light, direct, playful emphasis.",
    "puck": SAM_STYLE + " Lively storyteller energy, vivid examples.",
    "charon": SAM_STYLE + " Calm confidence, grounded, reassuring.",
    "kore": SAM_STYLE + " Thoughtful, precise, still warm and human.",
}

_EMOTION_HINTS = {
    "lacht": "with a soft natural laugh in the voice",
    "lacht leise": "with a quiet amused chuckle",
    "freut sich": "bright, happy, energized",
    "überrascht": "surprised, raised eyebrows in the voice, slightly higher pitch",
    "erstaunt": "genuinely astonished, short intake of breath",
    "nachdenklich": "thoughtful, slower, searching for words",
    "verwirrt": "mildly confused, hesitant, questioning tone",
    "begeistert": "excited, faster, warm enthusiasm",
    "beruhigend": "calm, reassuring, soft and steady",
    "ernst": "serious but still human, focused",
    "flüstert": "softer, intimate, closer-mic energy — not a stage whisper",
    "seufzt": "with a short human sigh before the words",
    "stöhnt": "light exasperated groan, then continue warmly",
    "nickt": "affirming, agreeing tone",
    "aha": "a small aha-moment, discovery in the voice",
    "genau": "affirming, nodding energy",
}


def normalize_podcast_voice(voice: str, fallback: str) -> str:
    key = (voice or "").strip().lower()
    key = LEGACY_PODCAST_VOICES.get(key, key)
    if key in PODCAST_VOICES:
        return key
    return fallback


def podcast_voice_pair(voice_a: str, voice_b: str) -> Tuple[str, str]:
    first = normalize_podcast_voice(voice_a, "aoede")
    second = normalize_podcast_voice(voice_b, _CONTRAST.get(first, "charon"))
    if second == first:
        second = _CONTRAST.get(first, "charon")
    return first, second


def gemini_voice_name(voice_id: str) -> str:
    return PODCAST_VOICES.get(normalize_podcast_voice(voice_id, "aoede"), "Aoede")


def style_for_voice(voice_id: str, speaker: str = "ALEX") -> str:
    key = normalize_podcast_voice(voice_id, "aoede" if speaker == "ALEX" else "charon")
    return _VOICE_STYLE.get(key, ALEX_STYLE if speaker == "ALEX" else SAM_STYLE)


def _emotion_from_tags(tags: List[str]) -> str:
    hints: List[str] = []
    for raw in tags:
        key = re.sub(r"\s+", " ", (raw or "").strip().lower())
        if not key:
            continue
        if key in _EMOTION_HINTS:
            hints.append(_EMOTION_HINTS[key])
            continue
        for needle, hint in _EMOTION_HINTS.items():
            if needle in key:
                hints.append(hint)
                break
        else:
            # Unknown stage note → soft delivery cue, never spoken aloud.
            hints.append(f"with a natural human color of '{key}'")
    # de-dupe while keeping order
    seen = set()
    out = []
    for hint in hints:
        if hint not in seen:
            seen.add(hint)
            out.append(hint)
    return "; ".join(out[:3])


def split_spoken_and_cues(text: str) -> Tuple[str, str]:
    """Return (spoken_text, emotion_cues). Stage tags become delivery, not words."""
    tags = [m.group(1) for m in _STAGE_DIRECTION.finditer(text or "")]
    cleaned = _STAGE_DIRECTION.sub(" ", text or "")
    spoken = re.sub(r"\s+", " ", cleaned).strip()
    return spoken, _emotion_from_tags(tags)


def spoken_podcast_line(text: str) -> str:
    """Drop stage directions. Angle-bracket pauses stay for the TTS model."""
    spoken, _ = split_spoken_and_cues(text)
    return spoken


def tts_instructions_for_turn(speaker: str, voice_id: str, raw_line: str) -> str:
    """Combine persona style with per-line emotion tags for human delivery."""
    base = style_for_voice(voice_id, speaker)
    _, cues = split_spoken_and_cues(raw_line)
    if not cues:
        return base
    return f"{base} This line specifically: {cues}."


def breath_segments(spoken: str, max_chars: int = 220) -> List[str]:
    """Split a long turn into breath-sized chunks so TTS does not drone."""
    text = (spoken or "").strip()
    if not text:
        return []
    if len(text) <= max_chars:
        return [text]
    parts = [p.strip() for p in re.split(r"(?<=[.!?…])\s+", text) if p.strip()]
    if len(parts) <= 1:
        # soft split on commas / dashes for very long clauses
        parts = [p.strip() for p in re.split(r"(?<=[,;:—–])\s+", text) if p.strip()]
    segments: List[str] = []
    bucket = ""
    for part in parts:
        candidate = f"{bucket} {part}".strip() if bucket else part
        if len(candidate) <= max_chars:
            bucket = candidate
            continue
        if bucket:
            segments.append(bucket)
        if len(part) <= max_chars:
            bucket = part
        else:
            # hard wrap as last resort
            while len(part) > max_chars:
                cut = part.rfind(" ", 0, max_chars)
                if cut < max_chars // 3:
                    cut = max_chars
                segments.append(part[:cut].strip())
                part = part[cut:].strip()
            bucket = part
    if bucket:
        segments.append(bucket)
    return segments or [text]


def gap_seconds_between(prev_speaker: Optional[str], speaker: str, spoken: str) -> float:
    """Human podcast spacing: snappy replies vs. space after a meaty explanation."""
    if not prev_speaker:
        return 0.0
    words = len((spoken or "").split())
    if prev_speaker == speaker:
        return 0.18 if words < 18 else 0.28
    if words >= 40:
        return 0.72
    if words >= 22:
        return 0.58
    return 0.45


def parse_podcast_dialogue(script: str) -> List[Tuple[str, str]]:
    """Return (ALEX|SAM, raw turn text) turns. Unlabeled text becomes a conversation."""
    labeled = _parse_labeled(script or "")
    if len(labeled) >= 4:
        return labeled[:_MAX_TURNS]
    plain = _strip_speaker_labels(script or "")
    alternated = _alternate_plain(plain)
    if alternated:
        return alternated[:_MAX_TURNS]
    text = plain.strip()
    if not text:
        return []
    return [("ALEX", text[:4000])]


def _parse_labeled(script: str) -> List[Tuple[str, str]]:
    turns: List[Tuple[str, List[str]]] = []
    for raw_line in script.splitlines():
        line = re.sub(r"[*_`#>]", "", raw_line).strip()
        if not line:
            continue
        match = _SPEAKER_LINE.match(line)
        if match:
            speaker = match.group(1).upper()
            spoken = match.group(2).strip()
            if turns and turns[-1][0] == speaker:
                if spoken:
                    turns[-1][1].append(spoken)
            else:
                turns.append((speaker, [spoken] if spoken else []))
        elif turns:
            turns[-1][1].append(line)
    return [
        (speaker, " ".join(parts).strip())
        for speaker, parts in turns
        if " ".join(parts).strip()
    ]


def _strip_speaker_labels(script: str) -> str:
    lines = []
    for raw_line in (script or "").splitlines():
        line = re.sub(r"[*_`#>]", "", raw_line).strip()
        match = _SPEAKER_LINE.match(line)
        lines.append(match.group(2).strip() if match else line)
    return "\n".join(lines)


def _alternate_plain(text: str) -> List[Tuple[str, str]]:
    chunks = [part.strip() for part in re.split(r"\n\s*\n", text) if part.strip()]
    if len(chunks) < 2:
        sentences = [part.strip() for part in re.split(r"(?<=[.!?])\s+", text) if part.strip()]
        chunks = []
        bucket: List[str] = []
        for sentence in sentences:
            bucket.append(sentence)
            if len(bucket) >= 2:
                chunks.append(" ".join(bucket))
                bucket = []
        if bucket:
            chunks.append(" ".join(bucket))
    turns = []
    for index, chunk in enumerate(chunks):
        if chunk:
            turns.append(("ALEX" if index % 2 == 0 else "SAM", chunk))
    return turns
