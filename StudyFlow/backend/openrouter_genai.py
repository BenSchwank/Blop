"""
OpenRouter client with the small slice of the Google Generative AI API that StudyFlow calls.

Model ids in the app stay `gemini-2.5-flash` / `gemini-2.5-pro`. This module maps them to
OpenRouter ids (`google/gemini-2.5-flash`, …) and sends PDFs, audio, and images inline.
New text features should call `complete_chat` instead of adding another provider SDK.
"""
from __future__ import annotations

import base64
import json
import os
import re
import uuid
from io import BytesIO
from pathlib import Path
from typing import Any, Dict, Iterable, List, Optional

import requests

API_URL = "https://openrouter.ai/api/v1/chat/completions"

# Internal Blop ids → OpenRouter. Any provider slug with a slash is sent as-is.
_MODEL_MAP = {
    "claude-sonnet-5.5": "anthropic/claude-sonnet-5.5",
    "gpt-6.1-sol": "openai/gpt-6.1-sol",
    "gemini-3.7-flash": "google/gemini-3.7-flash",
    "gemini-3-flash": "google/gemini-3-flash-preview",
    "gemini-3.1-pro": "google/gemini-3.1-pro-preview",
    "gemini-2.5-flash": "google/gemini-2.5-flash",
    "gemini-2.5-flash-lite": "google/gemini-2.5-flash-lite",
    "gemini-2.5-pro": "google/gemini-2.5-pro",
    "gemini-2.0-flash": "google/gemini-2.5-flash",
    "gemini-2.0-pro-exp": "google/gemini-2.5-pro",
    "gemini-1.5-flash": "google/gemini-2.5-flash",
    "gemini-1.5-flash-8b": "google/gemini-2.5-flash",
    "gemini-1.5-pro": "google/gemini-2.5-flash",
    "gemini-pro": "google/gemini-2.5-flash",
}

# One hop if a slug disappears. Targets are stable enough to finish the request.
_MODEL_FALLBACK = {
    "anthropic/claude-sonnet-5.5": "openai/gpt-6.1-sol",
    "openai/gpt-6.1-sol": "anthropic/claude-sonnet-5.5",
    "google/gemini-3.7-flash": "google/gemini-2.5-flash",
    "google/gemini-3-flash-preview": "google/gemini-2.5-flash",
    "google/gemini-3.1-pro-preview": "google/gemini-2.5-pro",
}


class OpenRouterError(RuntimeError):
    pass


class FinishReason:
    MAX_TOKENS = "MAX_TOKENS"
    SAFETY = "SAFETY"
    RECITATION = "RECITATION"
    STOP = "STOP"


class _CandidateNS:
    FinishReason = FinishReason


class _Protos:
    Candidate = _CandidateNS()


protos = _Protos()


class RequestOptions:
    def __init__(self, timeout: Optional[float] = None, **_kwargs: Any):
        self.timeout = timeout


class _Part:
    def __init__(self, text: str):
        self.text = text


class _Content:
    def __init__(self, text: str):
        self.parts = [_Part(text)] if text else []
        self.role = "model"


class _Candidate:
    def __init__(self, text: str, finish_reason: str):
        self.finish_reason = finish_reason
        self.content = _Content(text)


class _Usage:
    def __init__(self, prompt: int, output: int, total: int = 0):
        self.prompt_token_count = int(prompt or 0)
        self.candidates_token_count = int(output or 0)
        self.total_token_count = int(total or (self.prompt_token_count + self.candidates_token_count))


class GenerateContentResponse:
    def __init__(self, text: str, finish_reason: str, usage: _Usage):
        self.text = text or ""
        self.candidates = [_Candidate(self.text, finish_reason)]
        self.usage_metadata = usage


class _Chunk:
    def __init__(self, text: str):
        self.text = text or ""


class _FileState:
    def __init__(self, name: str):
        self.name = name


class UploadedFile:
    """Local stand-in for a Gemini Files API object. Bytes are sent with the next chat call."""

    def __init__(self, path: str, mime_type: Optional[str], name: str):
        self.path = path
        self.mime_type = mime_type or "application/octet-stream"
        self.name = name
        self.state = _FileState("ACTIVE")


class _ModelInfo:
    def __init__(self, name: str):
        self.name = f"models/{name}"
        self.supported_generation_methods = ["generateContent"]


_configured_key = ""
_files: Dict[str, UploadedFile] = {}


def configure(api_key: Optional[str] = None, **_kwargs: Any) -> None:
    global _configured_key
    _configured_key = (api_key or "").strip()


def api_key_configured() -> bool:
    return bool(_api_key())


def map_model(name: Optional[str]) -> str:
    raw = (name or "").strip().replace("models/", "")
    if not raw:
        return _MODEL_MAP["gemini-2.5-flash"]
    if "/" in raw:
        return raw
    return _MODEL_MAP.get(raw, f"google/{raw}")


def list_models() -> Iterable[_ModelInfo]:
    for name in _MODEL_MAP:
        yield _ModelInfo(name)


def upload_file(path: str, mime_type: Optional[str] = None, **_kwargs: Any) -> UploadedFile:
    if not path or not os.path.isfile(path):
        raise OpenRouterError(f"OpenRouter: Datei nicht gefunden: {path}")
    name = f"files/{uuid.uuid4().hex}"
    uploaded = UploadedFile(path, mime_type, name)
    _files[name] = uploaded
    return uploaded


def get_file(name: str) -> UploadedFile:
    found = _files.get(name)
    if not found:
        raise OpenRouterError(f"OpenRouter: unbekannte Datei {name}")
    return found


def delete_file(name: str) -> None:
    _files.pop(name, None)


def complete_chat(
    messages: List[Dict[str, Any]],
    model: str = "google/gemini-2.5-flash",
    temperature: Optional[float] = None,
    max_tokens: Optional[int] = None,
    response_format: Optional[Dict[str, Any]] = None,
    timeout: Optional[float] = None,
) -> str:
    """One-shot chat completion. Used by StudyFlow and later text features."""
    response = _complete(
        model=map_model(model),
        messages=messages,
        temperature=temperature,
        max_tokens=max_tokens,
        json_mode=bool(response_format),
        timeout=timeout or _default_timeout(),
        stream=False,
    )
    assert isinstance(response, GenerateContentResponse)
    return response.text


class GenerativeModel:
    def __init__(
        self,
        model_name: Optional[str] = None,
        generation_config: Optional[Dict[str, Any]] = None,
        system_instruction: Optional[str] = None,
        safety_settings: Any = None,
        **_kwargs: Any,
    ):
        self.model_name = str(model_name or "gemini-2.5-flash").replace("models/", "")
        self._openrouter_model = map_model(self.model_name)
        self.generation_config = dict(generation_config or {})
        self.system_instruction = system_instruction or ""

    def generate_content(self, contents: Any, **kwargs: Any) -> Any:
        cfg = dict(self.generation_config)
        if kwargs.get("generation_config"):
            cfg.update(kwargs["generation_config"])
        messages: List[Dict[str, Any]] = []
        if self.system_instruction:
            messages.append({"role": "system", "content": self.system_instruction})
        messages.append({"role": "user", "content": _content_parts(_as_parts(contents))})
        return _complete(
            model=self._openrouter_model,
            messages=messages,
            temperature=_opt_float(cfg.get("temperature")),
            max_tokens=_opt_int(cfg.get("max_output_tokens")),
            json_mode=str(cfg.get("response_mime_type") or "") == "application/json",
            timeout=_timeout_of(kwargs.get("request_options")),
            stream=bool(kwargs.get("stream")),
            reasoning_effort=_opt_str(cfg.get("reasoning_effort")),
        )

    def start_chat(self, history: Optional[List[Dict[str, Any]]] = None) -> "ChatSession":
        return ChatSession(self, history or [])


class ChatSession:
    def __init__(self, model: GenerativeModel, history: List[Dict[str, Any]]):
        self._model = model
        self._messages = _history_to_messages(history)

    def send_message(self, content: Any, **kwargs: Any) -> Any:
        user_msg = {"role": "user", "content": _content_parts(_as_parts(content))}
        self._messages.append(user_msg)
        cfg = self._model.generation_config
        stream = bool(kwargs.get("stream"))

        def _remember(text: str) -> None:
            self._messages.append({"role": "assistant", "content": text or ""})

        try:
            result = _complete(
                model=self._model._openrouter_model,
                messages=_with_system(self._model.system_instruction, self._messages),
                temperature=_opt_float(cfg.get("temperature")),
                max_tokens=_opt_int(cfg.get("max_output_tokens")),
                json_mode=str(cfg.get("response_mime_type") or "") == "application/json",
                timeout=_timeout_of(kwargs.get("request_options")),
                stream=stream,
                on_stream_done=_remember if stream else None,
                reasoning_effort=_opt_str(cfg.get("reasoning_effort")),
            )
        except Exception:
            if self._messages and self._messages[-1] is user_msg:
                self._messages.pop()
            raise
        if not stream and isinstance(result, GenerateContentResponse):
            _remember(result.text)
        return result


class StreamResponse:
    def __init__(self, http_response: requests.Response, on_done: Any = None):
        self._http = http_response
        self._on_done = on_done
        self.text = ""
        self.usage_metadata = _Usage(0, 0, 0)
        self.candidates = [_Candidate("", FinishReason.STOP)]

    def __iter__(self):
        pieces: List[str] = []
        finish = FinishReason.STOP
        try:
            for event in _iter_sse(self._http):
                choices = event.get("choices") or []
                choice = choices[0] if choices else {}
                delta = choice.get("delta") or {}
                piece = _message_text(delta.get("content"))
                if piece:
                    pieces.append(piece)
                    yield _Chunk(piece)
                mapped = _map_finish(choice.get("finish_reason"))
                if choice.get("finish_reason"):
                    finish = mapped
                usage = event.get("usage")
                if isinstance(usage, dict):
                    self.usage_metadata = _usage_from(usage)
        finally:
            try:
                self._http.close()
            except Exception:
                pass
            self.text = "".join(pieces)
            self.candidates = [_Candidate(self.text, finish)]
            if self._on_done:
                self._on_done(self.text)


def _api_key() -> str:
    return (_configured_key or os.environ.get("OPENROUTER_API_KEY") or "").strip()


def _default_timeout() -> float:
    raw = (os.environ.get("OPENROUTER_TIMEOUT") or "").strip()
    if raw:
        try:
            return float(raw)
        except ValueError:
            pass
    return 300.0


def _timeout_of(request_options: Any) -> float:
    timeout = getattr(request_options, "timeout", None) if request_options is not None else None
    try:
        if timeout:
            return float(timeout)
    except (TypeError, ValueError):
        pass
    return _default_timeout()


def _opt_float(value: Any) -> Optional[float]:
    if value is None or value == "":
        return None
    try:
        return float(value)
    except (TypeError, ValueError):
        return None


def _opt_str(value: Any) -> Optional[str]:
    text = str(value or "").strip()
    return text or None


def _opt_int(value: Any) -> Optional[int]:
    if value is None or value == "":
        return None
    try:
        return int(value)
    except (TypeError, ValueError):
        return None


def _as_parts(content: Any) -> List[Any]:
    if content is None:
        return []
    if isinstance(content, str):
        return [content]
    if isinstance(content, list):
        return content
    return [content]


def _content_parts(parts: List[Any]) -> Any:
    blocks: List[Dict[str, Any]] = []
    for part in parts:
        if part is None:
            continue
        if isinstance(part, str):
            if part:
                blocks.append({"type": "text", "text": part})
            continue
        if isinstance(part, UploadedFile):
            blocks.append(_file_block(part))
            continue
        if hasattr(part, "save") and hasattr(part, "mode"):
            blocks.append({"type": "image_url", "image_url": {"url": _pil_to_data_url(part)}})
            continue
        if isinstance(part, dict) and part.get("type"):
            blocks.append(part)
            continue
        text = str(part)
        if text:
            blocks.append({"type": "text", "text": text})
    if len(blocks) == 1 and blocks[0].get("type") == "text":
        return blocks[0]["text"]
    return blocks


_AUDIO_FORMATS = {
    "audio/mpeg": "mp3",
    "audio/mp3": "mp3",
    "audio/wav": "wav",
    "audio/x-wav": "wav",
    "audio/mp4": "m4a",
    "audio/m4a": "m4a",
    "audio/aac": "aac",
    "audio/ogg": "ogg",
    "audio/flac": "flac",
}


def extract_pdf_text(path: str, max_pages: int = 40, max_chars: int = 100_000) -> str:
    """Read a PDF text layer locally. Avoids OpenRouter's paid file parser."""
    try:
        from PyPDF2 import PdfReader
    except ImportError:
        return ""
    try:
        reader = PdfReader(path)
        pages = getattr(reader, "pages", None)
        if pages is None:
            count = int(reader.getNumPages())
            pages = [reader.getPage(index) for index in range(count)]
        chunks = []
        total = 0
        for page in list(pages)[:max_pages]:
            if hasattr(page, "extract_text"):
                piece = page.extract_text() or ""
            else:
                piece = page.extractText() or ""
            piece = piece.strip()
            if not piece:
                continue
            chunks.append(piece)
            total += len(piece)
            if total >= max_chars:
                break
        text = "\n\n".join(chunks).strip()
        if len(text) > max_chars:
            text = text[:max_chars] + "\n\n[… PDF-Text gekürzt …]"
        return text
    except Exception as exc:
        print(f"PDF text extract failed: {exc}")
        return ""


def _file_block(uploaded: UploadedFile) -> Dict[str, Any]:
    mime = uploaded.mime_type or "application/octet-stream"
    filename = Path(uploaded.path).name or "material"
    if mime == "application/pdf" or filename.lower().endswith(".pdf"):
        text = extract_pdf_text(uploaded.path)
        body = text or f"[PDF {filename} enthält keinen lesbaren Text.]"
        return {"type": "text", "text": f"--- {filename} (PDF) ---\n{body}"}
    raw = Path(uploaded.path).read_bytes()
    b64 = base64.b64encode(raw).decode("ascii")
    audio_format = _AUDIO_FORMATS.get(mime)
    if audio_format:
        return {
            "type": "input_audio",
            "input_audio": {"data": b64, "format": audio_format},
        }
    return {
        "type": "file",
        "file": {
            "filename": filename,
            "file_data": f"data:{mime};base64,{b64}",
        },
    }


def _pil_to_data_url(image: Any) -> str:
    buf = BytesIO()
    img = image
    if getattr(img, "mode", "") in ("RGBA", "P", "LA"):
        img = img.convert("RGB")
    img.save(buf, format="JPEG", quality=85)
    b64 = base64.b64encode(buf.getvalue()).decode("ascii")
    return f"data:image/jpeg;base64,{b64}"


def _history_to_messages(history: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    messages: List[Dict[str, Any]] = []
    for item in history or []:
        role = item.get("role") or "user"
        if role == "model":
            role = "assistant"
        if role not in ("user", "assistant", "system"):
            role = "user"
        messages.append({"role": role, "content": _content_parts(_as_parts(item.get("parts") or item.get("content")))})
    return messages


def _with_system(system_instruction: str, messages: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    out: List[Dict[str, Any]] = []
    if system_instruction:
        out.append({"role": "system", "content": system_instruction})
    out.extend(messages)
    return out


def _affordable_max_tokens(detail: str, current: Optional[int]) -> Optional[int]:
    """Parse OpenRouter 402 'can only afford N' into a smaller max_tokens."""
    match = re.search(r"can only afford\s+(\d+)", detail or "", re.IGNORECASE)
    if not match:
        return None
    affordable = int(match.group(1)) - 100
    if affordable < 256:
        return None
    if current is not None and affordable >= int(current):
        return None
    return affordable


def _complete(
    model: str,
    messages: List[Dict[str, Any]],
    temperature: Optional[float],
    max_tokens: Optional[int],
    json_mode: bool,
    timeout: float,
    stream: bool,
    on_stream_done: Any = None,
    reasoning_effort: Optional[str] = None,
) -> Any:
    body: Dict[str, Any] = {
        "model": model,
        "messages": messages,
    }
    if temperature is not None:
        body["temperature"] = temperature
    if max_tokens is not None:
        body["max_tokens"] = max_tokens
    if json_mode:
        body["response_format"] = {"type": "json_object"}
    if stream:
        body["stream"] = True
        body["stream_options"] = {"include_usage": True}
    if reasoning_effort:
        body["reasoning"] = {"effort": reasoning_effort}

    active = dict(body)
    response = _post(active, timeout, stream=stream)
    if response.status_code == 400:
        err = _error_text(response)
        low = err.lower()
        changed = False
        if json_mode and "response_format" in low:
            active.pop("response_format", None)
            changed = True
        if stream and "stream_options" in low:
            active.pop("stream_options", None)
            changed = True
        if "temperature" in low and "temperature" in active:
            active.pop("temperature", None)
            changed = True
        if "reasoning" in low and "reasoning" in active:
            active.pop("reasoning", None)
            changed = True
        if changed:
            response.close()
            response = _post(active, timeout, stream=stream)
    if not response.ok:
        detail = _error_text(response)
        fallback = _MODEL_FALLBACK.get(str(active.get("model") or ""))
        low = detail.lower()
        if fallback and (
            response.status_code == 404
            or "not found" in low
            or "no longer available" in low
            or "not available" in low
        ):
            print(f"OpenRouter model {active.get('model')} unavailable, fallback {fallback}")
            response.close()
            active["model"] = fallback
            response = _post(active, timeout, stream=stream)
            detail = _error_text(response) if not response.ok else detail
        if (
            not response.ok
            and response.status_code == 402
            and "can only afford" in (detail or "").lower()
        ):
            smaller = _affordable_max_tokens(detail, active.get("max_tokens"))
            if smaller:
                print(f"OpenRouter 402 retry with max_tokens={smaller}")
                response.close()
                active["max_tokens"] = smaller
                response = _post(active, timeout, stream=stream)
                detail = _error_text(response) if not response.ok else detail
    if not response.ok:
        response.close()
        raise OpenRouterError(f"OpenRouter {response.status_code}: {detail[:800]}")
    if stream:
        return StreamResponse(response, on_done=on_stream_done)
    try:
        data = response.json()
    finally:
        response.close()
    return _parse_completion(data, json_mode=json_mode)


def _post(body: Dict[str, Any], timeout: float, stream: bool) -> requests.Response:
    key = _api_key()
    if not key:
        raise OpenRouterError("OpenRouter: OPENROUTER_API_KEY fehlt")
    headers = {
        "Authorization": f"Bearer {key}",
        "Content-Type": "application/json",
        "HTTP-Referer": os.environ.get("OPENROUTER_HTTP_REFERER", "https://www.blop-study.com"),
        "X-Title": os.environ.get("OPENROUTER_APP_TITLE", "Blop Study"),
    }
    return requests.post(API_URL, headers=headers, json=body, timeout=timeout, stream=stream)


def _error_text(response: requests.Response) -> str:
    try:
        payload = response.json()
    except Exception:
        return (response.text or "").strip()
    err = payload.get("error") if isinstance(payload, dict) else None
    if isinstance(err, dict):
        return str(err.get("message") or err)
    if isinstance(err, str):
        return err
    return json.dumps(payload, ensure_ascii=False)[:800]


def _parse_completion(data: Dict[str, Any], json_mode: bool) -> GenerateContentResponse:
    choices = data.get("choices") or []
    choice = choices[0] if choices else {}
    message = choice.get("message") or {}
    text = _message_text(message.get("content"))
    if json_mode:
        text = _strip_json_fence(text)
    finish = _map_finish(choice.get("finish_reason"))
    return GenerateContentResponse(text, finish, _usage_from(data.get("usage") or {}))


def _message_text(content: Any) -> str:
    if content is None:
        return ""
    if isinstance(content, str):
        return content
    if isinstance(content, list):
        bits: List[str] = []
        for part in content:
            if isinstance(part, str):
                bits.append(part)
            elif isinstance(part, dict):
                kind = str(part.get("type") or "").lower()
                if kind in ("thinking", "reasoning"):
                    continue
                bits.append(str(part.get("text") or part.get("content") or ""))
        return "".join(bits)
    return str(content)


def _strip_json_fence(text: str) -> str:
    stripped = (text or "").strip()
    if not stripped.startswith("```"):
        return stripped
    lines = stripped.splitlines()
    if lines and lines[0].startswith("```"):
        lines = lines[1:]
    if lines and lines[-1].strip() == "```":
        lines = lines[:-1]
    return "\n".join(lines).strip()


def _map_finish(raw: Any) -> str:
    token = str(raw or "").strip().upper().replace("-", "_")
    if token in ("LENGTH", "MAX_TOKENS", "MAX_OUTPUT_TOKENS"):
        return FinishReason.MAX_TOKENS
    if token in ("CONTENT_FILTER", "SAFETY", "SAFETY_CHECK"):
        return FinishReason.SAFETY
    if "RECIT" in token:
        return FinishReason.RECITATION
    return FinishReason.STOP


def _usage_from(usage: Dict[str, Any]) -> _Usage:
    prompt = int(usage.get("prompt_tokens") or usage.get("input_tokens") or 0)
    output = int(usage.get("completion_tokens") or usage.get("output_tokens") or 0)
    total = int(usage.get("total_tokens") or (prompt + output))
    return _Usage(prompt, output, total)


def _iter_sse(response: requests.Response) -> Iterable[Dict[str, Any]]:
    for raw in response.iter_lines(decode_unicode=True):
        if not raw:
            continue
        line = raw.strip()
        if not line.startswith("data:"):
            continue
        data = line[5:].strip()
        if not data or data == "[DONE]":
            if data == "[DONE]":
                break
            continue
        try:
            event = json.loads(data)
        except json.JSONDecodeError:
            continue
        if isinstance(event, dict):
            yield event
