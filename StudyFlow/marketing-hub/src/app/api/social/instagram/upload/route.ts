import { NextRequest, NextResponse } from "next/server";

export async function POST(request: NextRequest) {
  const token = process.env.INSTAGRAM_ACCESS_TOKEN;
  const body = (await request.json()) as { videoUrl?: string };
  return NextResponse.json({
    ok: true,
    platform: "instagram_reels",
    message:
      "Placeholder: Instagram Graph API Upload noch nicht aktiv. App-ID, User-Token, Container/Create-Publish Schritte ergaenzen.",
    tokenConfigured: Boolean(token),
    videoUrl: body.videoUrl ?? null,
  });
}
