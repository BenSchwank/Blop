import { NextRequest, NextResponse } from "next/server";

export const runtime = "nodejs";

export async function GET(
  request: NextRequest,
  { params }: { params: Promise<{ packId: string }> },
) {
  const { packId } = await params;
  const backendUrl = process.env.MARKETING_HUB_BACKEND_URL?.replace(/\/$/, "");
  if (!backendUrl) {
    return NextResponse.json({ detail: "MARKETING_HUB_BACKEND_URL fehlt." }, { status: 500 });
  }

  const response = await fetch(
    `${backendUrl}/api/ai/marketing-pack/download/${encodeURIComponent(packId)}`,
  );
  if (!response.ok || !response.body) {
    return NextResponse.json({ detail: "Pack im Backend nicht abrufbar." }, { status: response.status || 502 });
  }

  return new NextResponse(response.body, {
    headers: {
      "Content-Type": "application/zip",
      "Content-Disposition": `attachment; filename="marketing_pack_${packId}.zip"`,
      "Cache-Control": "no-store",
    },
  });
}
