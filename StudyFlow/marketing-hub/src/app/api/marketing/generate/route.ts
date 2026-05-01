import { NextRequest, NextResponse } from "next/server";

export const runtime = "nodejs";

export async function POST(request: NextRequest) {
  const formData = await request.formData();
  const media = formData.get("media");
  const bulletpoints = String(formData.get("bulletpoints") ?? "").trim();
  if (!(media instanceof File)) {
    return NextResponse.json({ detail: "Kein Media-File erhalten." }, { status: 400 });
  }
  if (!bulletpoints) {
    return NextResponse.json({ detail: "Keine Stichpunkte erhalten." }, { status: 400 });
  }

  const backendUrl = process.env.MARKETING_HUB_BACKEND_URL?.replace(/\/$/, "");
  if (!backendUrl) {
    return NextResponse.json({ detail: "MARKETING_HUB_BACKEND_URL fehlt." }, { status: 500 });
  }

  const backendForm = new FormData();
  backendForm.append("media", media);
  backendForm.append("bulletpoints", bulletpoints);
  const response = await fetch(`${backendUrl}/api/ai/marketing-short`, {
    method: "POST",
    body: backendForm,
  });
  const data = await response.json();
  return NextResponse.json(data, { status: response.status });
}
