import { NextRequest, NextResponse } from "next/server";

export const runtime = "nodejs";

export async function GET(
  request: NextRequest,
  { params }: { params: Promise<{ exportId: string }> },
) {
  const { exportId } = await params;
  const backendUrl = process.env.MARKETING_HUB_BACKEND_URL?.replace(/\/$/, "");
  if (!backendUrl) {
    return NextResponse.json({ detail: "MARKETING_HUB_BACKEND_URL fehlt." }, { status: 500 });
  }

  const response = await fetch(`${backendUrl}/api/ai/marketing-short/video/${encodeURIComponent(exportId)}`);
  if (!response.ok || !response.body) {
    return NextResponse.json({ detail: "Video im Backend nicht abrufbar." }, { status: response.status || 502 });
  }

  return new NextResponse(response.body, {
    headers: {
      "Content-Type": "video/mp4",
      "Content-Disposition": `inline; filename="marketing_short_${exportId}.mp4"`,
      "Cache-Control": "no-store",
    },
  });
}
