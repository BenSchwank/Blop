import { NextRequest, NextResponse } from "next/server";

export const runtime = "nodejs";
/** Pack baut mehrere Clips (OpenAI + ffmpeg); ohne hohes Limit bricht Vercel ab (502/504). */
export const maxDuration = 300;

function rewriteVideoUrl(backendPath: string): string | undefined {
  const exportId = backendPath.split("/").pop();
  if (exportId) {
    return `/api/marketing/video/${exportId}`;
  }
  return undefined;
}

export async function POST(request: NextRequest) {
  try {
    const formData = await request.formData();
    const mediaFiles = formData.getAll("media_files").filter((item) => item instanceof File) as File[];
    const bulletpoints = String(formData.get("bulletpoints") ?? "").trim();
    const languageRaw = String(formData.get("language") ?? "de").trim().toLowerCase();
    const emotionRaw = String(formData.get("emotion") ?? "energetic").trim().toLowerCase();
    const language = ["de", "en", "tr", "es"].includes(languageRaw) ? languageRaw : "de";
    const emotion = ["energetic", "confident", "calm", "dramatic"].includes(emotionRaw)
      ? emotionRaw
      : "energetic";
    if (!mediaFiles.length) {
      return NextResponse.json({ detail: "Keine Media-Dateien erhalten." }, { status: 400 });
    }

    const backendUrl = process.env.MARKETING_HUB_BACKEND_URL?.replace(/\/$/, "");
    if (!backendUrl) {
      return NextResponse.json({ detail: "MARKETING_HUB_BACKEND_URL fehlt." }, { status: 500 });
    }

    const backendForm = new FormData();
    for (const media of mediaFiles) {
      backendForm.append("media_files", media);
    }
    backendForm.append("bulletpoints", bulletpoints);
    backendForm.append("language", language);
    backendForm.append("emotion", emotion);

    const response = await fetch(`${backendUrl}/api/ai/marketing-pack`, {
      method: "POST",
      body: backendForm,
      signal: AbortSignal.timeout(280_000),
    });
    const raw = await response.text();
    let data: Record<string, unknown> = {};
    try {
      data = raw ? (JSON.parse(raw) as Record<string, unknown>) : {};
    } catch {
      data = { detail: raw || "Backend hat keine JSON-Antwort geliefert." };
    }

    if (!response.ok) {
      return NextResponse.json(
        {
          detail:
            String(data.detail ?? "").trim() ||
            `Backend-Fehler (${response.status} ${response.statusText || "unknown"}).`,
        },
        { status: response.status || 502 },
      );
    }

    const packPath = typeof data.pack_download_url === "string" ? data.pack_download_url : "";
    const packId = packPath.split("/").pop();
    if (packId) {
      data.pack_download_url = `/api/marketing/pack/${packId}`;
    }

    const clips = data.clips;
    if (Array.isArray(clips)) {
      for (const c of clips) {
        if (c && typeof c === "object" && "video_url" in c) {
          const vu = String((c as { video_url?: string }).video_url ?? "");
          const rw = rewriteVideoUrl(vu);
          if (rw) {
            (c as { video_url: string }).video_url = rw;
          }
        }
      }
    }

    return NextResponse.json(data, { status: response.status });
  } catch (error) {
    return NextResponse.json(
      { detail: `Marketing-Pack Route Fehler: ${error instanceof Error ? error.message : "unknown"}` },
      { status: 500 },
    );
  }
}
