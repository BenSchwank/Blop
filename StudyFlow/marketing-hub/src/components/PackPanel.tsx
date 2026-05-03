"use client";

import { useState } from "react";

type PackClip = {
  title?: string;
  hook?: string;
  script?: string;
  caption?: string;
  hashtags?: string[];
  platform_hint?: string;
  posting_hint?: string;
  list_title?: string;
  list_items?: string[];
  cta_line?: string;
  ki_visual_brief?: string;
  video_url?: string;
};

type PackResponse = {
  material_summary?: string;
  extracted_elements?: unknown[];
  clips?: PackClip[];
  pack_download_url?: string;
  detail?: string;
};

export default function PackPanel() {
  const [mediaFiles, setMediaFiles] = useState<File[]>([]);
  const [bulletpoints, setBulletpoints] = useState("");
  const [language, setLanguage] = useState("de");
  const [emotion, setEmotion] = useState("energetic");
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState("");
  const [result, setResult] = useState<PackResponse | null>(null);

  async function onGenerate() {
    if (!mediaFiles.length) {
      setError("Bitte mindestens eine Bild-/Videodatei hochladen.");
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
      form.append("language", language);
      form.append("emotion", emotion);
      const response = await fetch("/api/marketing/pack", { method: "POST", body: form });
      const body = (await response.json()) as PackResponse & { detail?: string };
      if (!response.ok) {
        setError(body.detail ?? "Pack-Generierung fehlgeschlagen.");
        return;
      }
      setResult(body);
    } catch {
      setError("Fehler bei der Pack-Generierung.");
    } finally {
      setLoading(false);
    }
  }

  return (
    <section className="glass mt-8 space-y-6 p-6">
      <h2 className="text-xl font-semibold">Social Content Pack (15s Multi-Clip)</h2>
      <p className="text-xs text-[#A9ADC8]">
        Strategischer Multi-Clip-Export mit Brainrot-Style, optional KI-Zusatzbildern (Backend:
        MARKETING_PACK_GENERATE_IMAGES=1).
      </p>

      <div className="space-y-2">
        <label className="text-sm text-[#C9CCE1]">Rohmaterial (Bild oder Video)</label>
        <input
          type="file"
          accept="image/*,video/*"
          multiple
          onChange={(e) => setMediaFiles(Array.from(e.target.files ?? []))}
          className="w-full rounded-xl border border-white/20 bg-white/5 p-3"
        />
        <p className="text-xs text-[#A9ADC8]">
          {mediaFiles.length ? `${mediaFiles.length} Datei(en)` : "Keine Datei"}
        </p>
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
          <label className="text-sm text-[#C9CCE1]">Stimm-Emotion</label>
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
        <label className="text-sm text-[#C9CCE1]">Kontext / Stichpunkte (optional)</label>
        <textarea
          className="min-h-24 w-full rounded-xl border border-white/20 bg-white/5 p-3 outline-none focus:border-[#5E5CE6]"
          value={bulletpoints}
          onChange={(e) => setBulletpoints(e.target.value)}
          placeholder="Was soll viral gehen? Leer lassen = KI entscheidet aus dem Material."
        />
      </div>

      <button
        type="button"
        onClick={onGenerate}
        disabled={loading}
        className="rounded-xl bg-[#7B68EE] px-5 py-3 font-semibold hover:bg-[#8b7af0] disabled:opacity-60"
      >
        {loading ? "Erzeuge Pack..." : "Content Pack generieren"}
      </button>

      {error ? <p className="text-sm text-red-300">{error}</p> : null}

      {result ? (
        <div className="space-y-4 rounded-xl border border-white/20 bg-black/25 p-4">
          {result.material_summary ? (
            <div>
              <p className="text-sm font-semibold text-[#C9CCE1]">Material-Analyse</p>
              <p className="text-sm text-[#E0E3F0]">{result.material_summary}</p>
            </div>
          ) : null}

          {result.pack_download_url ? (
            <a
              className="inline-block text-[#9ea2ff] underline"
              href={result.pack_download_url}
              rel="noreferrer"
            >
              ZIP herunterladen (MP4s + pack.json)
            </a>
          ) : null}

          {Array.isArray(result.clips) && result.clips.length ? (
            <div className="space-y-6">
              {result.clips.map((clip, idx) => (
                <div key={idx} className="rounded-lg border border-white/10 p-3">
                  <p className="text-sm font-semibold text-[#C9CCE1]">{clip.title ?? `Clip ${idx + 1}`}</p>
                  {clip.video_url ? (
                    <video
                      src={clip.video_url}
                      controls
                      className="mt-2 max-h-80 w-full rounded-lg border border-white/10"
                    />
                  ) : null}
                  {clip.hook ? <p className="mt-2 text-xs text-[#A9ADC8]">Hook: {clip.hook}</p> : null}
                  {clip.script ? (
                    <pre className="mt-2 max-h-40 overflow-auto whitespace-pre-wrap text-xs text-[#E0E3F0]">
                      {clip.script}
                    </pre>
                  ) : null}
                  {clip.caption ? (
                    <p className="mt-2 text-xs text-[#C9CCE1]">
                      Caption: <span className="text-[#E0E3F0]">{clip.caption}</span>
                    </p>
                  ) : null}
                  {Array.isArray(clip.hashtags) && clip.hashtags.length ? (
                    <p className="text-xs text-[#9ea2ff]">{clip.hashtags.join(" ")}</p>
                  ) : null}
                  {clip.platform_hint ? (
                    <p className="text-xs text-[#A9ADC8]">Plattform: {clip.platform_hint}</p>
                  ) : null}
                  {clip.posting_hint ? (
                    <p className="text-xs text-[#A9ADC8]">Timing: {clip.posting_hint}</p>
                  ) : null}
                </div>
              ))}
            </div>
          ) : null}
        </div>
      ) : null}
    </section>
  );
}
