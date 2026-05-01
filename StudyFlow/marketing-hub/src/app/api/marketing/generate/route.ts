import { NextRequest, NextResponse } from "next/server";

export const runtime = "nodejs";

export async function POST(request: NextRequest) {
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
  const data = (await response.json()) as { video_url?: string; [key: string]: unknown };
  const backendVideoPath = typeof data.video_url === "string" ? data.video_url : "";
  const exportId = backendVideoPath.split("/").pop();
  if (exportId) {
    data.video_url = `/api/marketing/video/${exportId}`;
  }
  return NextResponse.json(data, { status: response.status });
}
