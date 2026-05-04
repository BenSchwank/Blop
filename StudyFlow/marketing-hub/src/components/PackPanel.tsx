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

/** Browser → Render (umgeht Vercel-Body-Limit ~4.5 MB beim Proxy). Gleiche URL wie MARKETING_HUB_BACKEND_URL. */
function publicBackendUrl(): string | undefined {
  const u = process.env.NEXT_PUBLIC_MARKETING_HUB_BACKEND_URL?.trim();
  return u ? u.replace(/\/$/, "") : undefined;
}

function normalizePackUrlsForHub(data: PackResponse): void {
  const dl = data.pack_download_url;
  if (typeof dl === "string") {
    const m = dl.match(/\/api\/ai\/marketing-pack\/download\/([^/?#]+)/);
    if (m?.[1]) {
      data.pack_download_url = `/api/marketing/pack/${m[1]}`;
    }
  }
  const clips = data.clips;
  if (!Array.isArray(clips)) return;
  for (const c of clips) {
    if (!c || typeof c !== "object") continue;
    const vu = c.video_url;
    if (typeof vu !== "string") continue;
    const vm = vu.match(/\/api\/ai\/marketing-short\/video\/([^/?#]+)/);
    if (vm?.[1]) {
      c.video_url = `/api/marketing/video/${vm[1]}`;
    }
  }
}

export default function PackPanel() {
  const [mediaFiles, setMediaFiles] = useState<File[]>([]);
  const [bulletpoints, setBulletpoints] = useState("");
  const [language, setLanguage] = useState("de");
  const [emotion, setEmotion] = useState("energetic");
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState("");
  const [result, setResult] = useState<PackResponse | null>(null);
  /** true = object-cover (näher an Instagram-Reel-Fill), false = letterbox im 9/16-Frame */
  const [previewReelCover, setPreviewReelCover] = useState(false);

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

      const direct = publicBackendUrl();
      const url = direct ? `${direct}/api/ai/marketing-pack` : "/api/marketing/pack";
      const response = await fetch(url, { method: "POST", body: form });

      const raw = await response.text();
      let body: PackResponse & { detail?: string };
      try {
        body = raw ? (JSON.parse(raw) as PackResponse & { detail?: string }) : {};
      } catch {
        setError(
          `Antwort ist kein JSON (HTTP ${response.status}). ` +
            `Oft: Vercel-Timeout, Body zu gross, oder Backend-Fehler. ` +
            `Tipp: NEXT_PUBLIC_MARKETING_HUB_BACKEND_URL setzen (direkter Upload zum Render). ` +
            `Snippet: ${raw.slice(0, 280)}`,
        );
        return;
      }
      if (!response.ok) {
        setError(body.detail ?? `Pack-Generierung fehlgeschlagen (HTTP ${response.status}).`);
        return;
      }
      if (direct) {
        normalizePackUrlsForHub(body);
      }
      setResult(body);
    } catch (e) {
      setError(
        e instanceof Error
          ? `Fehler: ${e.message}`
          : "Fehler bei der Pack-Generierung (Netzwerk oder abgebrochen).",
      );
    } finally {
      setLoading(false);
    }
  }

  return (
    <section className="glass mt-8 space-y-6 p-6">
      <h2 className="text-xl font-semibold">Social Content Pack (15s Multi-Clip)</h2>
      <p className="text-xs text-[#A9ADC8]">
        Strategischer Multi-Clip-Export mit Brainrot-Style, optional KI-Zusatzbildern (Backend:
        MARKETING_PACK_GENERATE_IMAGES=1). Viele grosse Bilder: in Vercel{" "}
        <code className="rounded bg-black/40 px-1">NEXT_PUBLIC_MARKETING_HUB_BACKEND_URL</code> setzen (gleiche URL
        wie <code className="rounded bg-black/40 px-1">MARKETING_HUB_BACKEND_URL</code>), sonst greift der Proxy und
        kann bei ~4.5 MB abbrechen.
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
              <label className="flex cursor-pointer items-center gap-2 text-xs text-[#A9ADC8]">
                <input
                  type="checkbox"
                  className="rounded border-white/30"
                  checked={previewReelCover}
                  onChange={(e) => setPreviewReelCover(e.target.checked)}
                />
                Vorschau wie Reel (Cover statt Letterbox)
              </label>
              {result.clips.map((clip, idx) => (
                <div key={idx} className="rounded-lg border border-white/10 p-3">
                  <p className="text-sm font-semibold text-[#C9CCE1]">{clip.title ?? `Clip ${idx + 1}`}</p>
                  {clip.video_url ? (
                    <div className="relative mx-auto mt-2 aspect-[9/16] w-full max-w-[min(100%,360px)] overflow-hidden rounded-lg border border-white/10 bg-black">
                      <video
                        src={clip.video_url}
                        controls
                        playsInline
                        className={
                          previewReelCover ? "h-full w-full object-cover" : "h-full w-full object-contain"
                        }
                      />
                    </div>
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
