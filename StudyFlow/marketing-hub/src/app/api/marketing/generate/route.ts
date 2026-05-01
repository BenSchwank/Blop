import { NextRequest, NextResponse } from "next/server";

export const runtime = "nodejs";

export async function POST(request: NextRequest) {
  try {
    const formData = await request.formData();
    const mediaFiles = formData.getAll("media_files").filter((item) => item instanceof File) as File[];
    const bulletpoints = String(formData.get("bulletpoints") ?? "").trim();
    const videoLengthSec = Number(formData.get("video_length_sec") ?? 30);
    if (!mediaFiles.length) {
      return NextResponse.json({ detail: "Keine Media-Dateien erhalten." }, { status: 400 });
    }
    if (!bulletpoints) {
      return NextResponse.json({ detail: "Keine Stichpunkte erhalten." }, { status: 400 });
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
    backendForm.append("video_length_sec", String(Number.isFinite(videoLengthSec) ? videoLengthSec : 30));
    const response = await fetch(`${backendUrl}/api/ai/marketing-short`, {
      method: "POST",
      body: backendForm,
    });
    const raw = await response.text();
    let data: { video_url?: string; [key: string]: unknown } = {};
    try {
      data = raw ? (JSON.parse(raw) as { video_url?: string; [key: string]: unknown }) : {};
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

    const backendVideoPath = typeof data.video_url === "string" ? data.video_url : "";
    const exportId = backendVideoPath.split("/").pop();
    if (exportId) {
      data.video_url = `/api/marketing/video/${exportId}`;
    }
    return NextResponse.json(data, { status: response.status });
  } catch (error) {
    return NextResponse.json(
      { detail: `Marketing-Generate Route Fehler: ${error instanceof Error ? error.message : "unknown"}` },
      { status: 500 },
    );
  }
}
