"use client";

import { useState } from "react";

type GenerationResponse = {
  script: string;
  video_url: string;
  subtitles_url?: string;
};

export default function UploadPanel() {
  const [mediaFiles, setMediaFiles] = useState<File[]>([]);
  const [bulletpoints, setBulletpoints] = useState("");
  const [videoLengthSec, setVideoLengthSec] = useState(30);
  const [language, setLanguage] = useState("de");
  const [emotion, setEmotion] = useState("energetic");
  const [autoComposition, setAutoComposition] = useState(true);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState("");
  const [result, setResult] = useState<GenerationResponse | null>(null);

  async function onGenerate() {
    if (!mediaFiles.length) {
      setError("Bitte mindestens eine Bild-/Videodatei hochladen.");
      return;
    }
    if (!bulletpoints.trim()) {
      setError("Bitte Fortschritts-Stichpunkte eingeben.");
      return;
    }
    setLoading(true);
    setError("");
    setResult(null);
    try {
      const form = new FormData();
      for (const file of mediaFiles) {
        form.append("media_files", file);
      }
      form.append("bulletpoints", bulletpoints);
      form.append("video_length_sec", String(videoLengthSec));
      form.append("language", language);
      form.append("emotion", emotion);
      form.append("marketing_auto_mode", autoComposition ? "1" : "0");
      const response = await fetch("/api/marketing/generate", { method: "POST", body: form });
      const body = (await response.json()) as GenerationResponse & { detail?: string };
      if (!response.ok) {
        setError(body.detail ?? "Generierung fehlgeschlagen.");
        return;
      }
      setResult(body);
    } catch {
      setError("Fehler bei der Video-Generierung.");
    } finally {
      setLoading(false);
    }
  }

  async function runSocialUpload(path: string) {
    if (!result?.video_url) {
      setError("Erst ein Video generieren.");
      return;
    }
    setError("");
    await fetch(path, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ videoUrl: result.video_url }),
    });
  }

  return (
    <section className="glass space-y-6 p-6">
      <h2 className="text-xl font-semibold">Video Engine</h2>
      <div className="space-y-2">
        <label className="text-sm text-[#C9CCE1]">Bild oder Video</label>
        <input
          type="file"
          accept="image/*,video/*"
          multiple
          onChange={(e) => setMediaFiles(Array.from(e.target.files ?? []))}
          className="w-full rounded-xl border border-white/20 bg-white/5 p-3"
        />
        <p className="text-xs text-[#A9ADC8]">
          {mediaFiles.length ? `${mediaFiles.length} Datei(en) ausgewaehlt` : "Keine Datei ausgewaehlt"}
        </p>
        <label className="flex items-start gap-3 text-xs text-[#A9ADC8]">
          <input
            type="checkbox"
            checked={autoComposition}
            onChange={(e) => setAutoComposition(e.target.checked)}
            className="mt-1"
          />
          <span>
            <span className="font-semibold text-[#C9CCE1]">Auto-Modus</span> (Standard: an): 1–2 Bilder werden als
            Whiteboard/Kinetic-Text gerendert; Video+Bilder werden video-lastig gemischt (B-Roll). Aus = klassischer
            Slideshow-Pfad fuer Bilder.
          </span>
        </label>
      </div>

      <div className="space-y-2">
        <label className="text-sm text-[#C9CCE1]">Videolaenge</label>
        <select
          value={videoLengthSec}
          onChange={(e) => setVideoLengthSec(Number(e.target.value))}
          className="w-full rounded-xl border border-white/20 bg-white/5 px-3 py-3 outline-none focus:border-[#5E5CE6]"
        >
          <option value={15}>15 Sekunden</option>
          <option value={25}>25 Sekunden</option>
          <option value={35}>35 Sekunden</option>
          <option value={45}>45 Sekunden</option>
        </select>
      </div>

      <div className="grid gap-4 md:grid-cols-2">
        <div className="space-y-2">
          <label className="text-sm text-[#C9CCE1]">Sprache</label>
          <select
            value={language}
            onChange={(e) => setLanguage(e.target.value)}
            className="w-full rounded-xl border border-white/20 bg-white/5 px-3 py-3 outline-none focus:border-[#5E5CE6]"
          >
            <option value="de">Deutsch</option>
            <option value="en">English</option>
            <option value="tr">Tuerkisch</option>
            <option value="es">Spanisch</option>
          </select>
        </div>
        <div className="space-y-2">
          <label className="text-sm text-[#C9CCE1]">Stimm-Emotion (maennliche Stimme)</label>
          <select
            value={emotion}
            onChange={(e) => setEmotion(e.target.value)}
            className="w-full rounded-xl border border-white/20 bg-white/5 px-3 py-3 outline-none focus:border-[#5E5CE6]"
          >
            <option value="energetic">Energiegeladen</option>
            <option value="confident">Selbstbewusst</option>
            <option value="calm">Ruhig</option>
            <option value="dramatic">Dramatisch</option>
          </select>
        </div>
      </div>

      <div className="space-y-2">
        <label className="text-sm text-[#C9CCE1]">Stichpunkte zum Fortschritt</label>
        <textarea
          className="min-h-36 w-full rounded-xl border border-white/20 bg-white/5 p-3 outline-none focus:border-[#5E5CE6]"
          value={bulletpoints}
          onChange={(e) => setBulletpoints(e.target.value)}
          placeholder={"- Neues Feature in Blop\n- Bugfix im Canvas\n- Performance verbessert"}
        />
      </div>

      <button
        onClick={onGenerate}
        disabled={loading}
        className="rounded-xl bg-[#5E5CE6] px-5 py-3 font-semibold hover:bg-[#6d6bee] disabled:opacity-60"
      >
        {loading ? "Generiere..." : "KI-Video generieren"}
      </button>

      {error ? <p className="text-sm text-red-300">{error}</p> : null}

      {result ? (
        <div className="space-y-3 rounded-xl border border-white/20 bg-black/25 p-4">
          <p className="text-sm text-[#C9CCE1]">Generiertes Skript:</p>
          <pre className="whitespace-pre-wrap text-sm">{result.script}</pre>
          <a className="text-[#9ea2ff] underline" href={result.video_url} target="_blank" rel="noreferrer">
            Ergebnisvideo (MP4, 9:16) oeffnen
          </a>
          <div className="flex flex-wrap gap-3 pt-2">
            <button
              onClick={() => runSocialUpload("/api/social/tiktok/upload")}
              className="rounded-lg border border-white/20 px-3 py-2 text-sm hover:border-[#5E5CE6]"
            >
              Direkt-Upload zu TikTok
            </button>
            <button
              onClick={() => runSocialUpload("/api/social/instagram/upload")}
              className="rounded-lg border border-white/20 px-3 py-2 text-sm hover:border-[#5E5CE6]"
            >
              Direkt-Upload zu Instagram Reels
            </button>
          </div>
        </div>
      ) : null}
    </section>
  );
}
