"""Two-person podcast script parsing and voice choices for Gemini TTS."""
import re
from typing import List, Tuple

# UI ids -> Gemini 3.8 Flash TTS voice. Chosen for a conversation, not a news reader.
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
_STAGE_DIRECTION = re.compile(r"\[[^\[\]]{0,80}\]")
_MAX_TURNS = 80

# Delivery goes to the TTS style field. Gemini 3.8 reads the input aloud,
# so these sentences must never be prefixed onto the spoken line.
ALEX_STYLE = (
    "Warm, curious German conversation with a friend. Natural pace, "
    "living pitch, a small smile, genuine questions. Not a news reader and not monotone."
)
SAM_STYLE = (
    "Warm German explanation to a friend at the same table. Clear and engaged, "
    "a little emphasis on the important word, some energy when the idea lands. "
    "Not a lecture and not monotone."
)


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


def spoken_podcast_line(text: str) -> str:
    """Drop stage directions. Angle-bracket pauses stay, the TTS model uses them."""
    cleaned = _STAGE_DIRECTION.sub(" ", text or "")
    return re.sub(r"\s+", " ", cleaned).strip()


def parse_podcast_dialogue(script: str) -> List[Tuple[str, str]]:
    """Return (ALEX|SAM, spoken text) turns. Unlabeled text is split into a conversation."""
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
    return [(speaker, " ".join(parts).strip()) for speaker, parts in turns if " ".join(parts).strip()]


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
