import { NextRequest, NextResponse } from "next/server";

export async function POST(request: NextRequest) {
  const token = process.env.TIKTOK_ACCESS_TOKEN;
  const body = (await request.json()) as { videoUrl?: string };
  return NextResponse.json({
    ok: true,
    platform: "tiktok",
    message:
      "Placeholder: TikTok Content Posting API noch nicht aktiv. Token/Scopes und finalen Publish-Call hinterlegen.",
    tokenConfigured: Boolean(token),
    videoUrl: body.videoUrl ?? null,
  });
}
