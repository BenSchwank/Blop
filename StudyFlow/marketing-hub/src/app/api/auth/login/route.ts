import { NextRequest, NextResponse } from "next/server";
import { createSessionToken, getAuthCookieName, isValidCredentials } from "@/lib/auth";
import { SESSION_TTL_SECONDS } from "@/lib/constants";

export async function POST(request: NextRequest) {
  const body = (await request.json()) as { username?: string; password?: string };
  const username = body.username ?? "";
  const password = body.password ?? "";

  if (!isValidCredentials(username, password)) {
    return NextResponse.json({ detail: "Ungueltige Zugangsdaten." }, { status: 401 });
  }

  const response = NextResponse.json({ ok: true });
  response.cookies.set({
    name: getAuthCookieName(),
    value: await createSessionToken(),
    httpOnly: true,
    secure: process.env.NODE_ENV === "production",
    sameSite: "lax",
    maxAge: SESSION_TTL_SECONDS,
    path: "/",
  });
  return response;
}
