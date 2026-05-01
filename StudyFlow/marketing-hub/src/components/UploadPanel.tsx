"use client";

import { useState } from "react";

type GenerationResponse = {
  script: string;
  video_url: string;
  subtitles_url?: string;
};

export default function UploadPanel() {
  const [mediaFile, setMediaFile] = useState<File | null>(null);
  const [bulletpoints, setBulletpoints] = useState("");
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState("");
  const [result, setResult] = useState<GenerationResponse | null>(null);

  async function onGenerate() {
    if (!mediaFile) {
      setError("Bitte Bild oder Video hochladen.");
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
      form.append("media", mediaFile);
      form.append("bulletpoints", bulletpoints);
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
          onChange={(e) => setMediaFile(e.target.files?.[0] ?? null)}
          className="w-full rounded-xl border border-white/20 bg-white/5 p-3"
        />
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
            Ergebnisvideo oeffnen
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
