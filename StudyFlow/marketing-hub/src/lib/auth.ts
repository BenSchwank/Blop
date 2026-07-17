import { AUTH_COOKIE_NAME, SESSION_TTL_SECONDS } from "@/lib/constants";

function getAdminUsername(): string {
  return (process.env.MARKETING_HUB_ADMIN_USERNAME || "").trim();
}

function getAdminPassword(): string {
  return (process.env.MARKETING_HUB_ADMIN_PASSWORD || "").trim();
}

function getSecret(): string {
  const secret = process.env.MARKETING_HUB_SESSION_SECRET;
  if (!secret || secret.trim().length < 16) {
    throw new Error("MARKETING_HUB_SESSION_SECRET fehlt oder ist zu kurz.");
  }
  return secret;
}

export function isValidCredentials(username: string, password: string): boolean {
  const expectedUser = getAdminUsername();
  const expectedPass = getAdminPassword();
  if (!expectedUser || !expectedPass) {
    // Fail closed: if credentials are not configured, deny all logins
    return false;
  }
  return username === expectedUser && password === expectedPass;
}

function toBase64Url(input: string): string {
  const bytes = new TextEncoder().encode(input);
  let binary = "";
  bytes.forEach((byte) => {
    binary += String.fromCharCode(byte);
  });
  return btoa(binary).replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/g, "");
}

function fromBase64Url(input: string): string {
  const base64 = input.replace(/-/g, "+").replace(/_/g, "/");
  const padded = base64 + "=".repeat((4 - (base64.length % 4)) % 4);
  const binary = atob(padded);
  const bytes = Uint8Array.from(binary, (char) => char.charCodeAt(0));
  return new TextDecoder().decode(bytes);
}

async function signPayload(payloadB64: string): Promise<string> {
  const keyData = new TextEncoder().encode(getSecret());
  const key = await crypto.subtle.importKey("raw", keyData, { name: "HMAC", hash: "SHA-256" }, false, ["sign"]);
  const signature = await crypto.subtle.sign("HMAC", key, new TextEncoder().encode(payloadB64));
  const bytes = new Uint8Array(signature);
  let binary = "";
  bytes.forEach((byte) => {
    binary += String.fromCharCode(byte);
  });
  return btoa(binary).replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/g, "");
}

export async function createSessionToken(): Promise<string> {
  const payload = JSON.stringify({
    sub: getAdminUsername(),
    exp: Math.floor(Date.now() / 1000) + SESSION_TTL_SECONDS,
  });
  const payloadB64 = toBase64Url(payload);
  const sig = await signPayload(payloadB64);
  return `${payloadB64}.${sig}`;
}

export async function verifySessionToken(token: string | undefined): Promise<boolean> {
  if (!token || !token.includes(".")) {
    return false;
  }
  const [payloadB64, signature] = token.split(".");
  if (!payloadB64 || !signature) {
    return false;
  }
  const expected = await signPayload(payloadB64);
  if (signature.length !== expected.length) {
    return false;
  }
  let mismatch = 0;
  for (let i = 0; i < signature.length; i += 1) {
    mismatch |= signature.charCodeAt(i) ^ expected.charCodeAt(i);
  }
  if (mismatch !== 0) {
    return false;
  }
  try {
    const payload = JSON.parse(fromBase64Url(payloadB64)) as {
      sub: string;
      exp: number;
    };
    const adminUser = getAdminUsername();
    return Boolean(adminUser) && payload.sub === adminUser && payload.exp > Math.floor(Date.now() / 1000);
  } catch {
    return false;
  }
}

export function getAuthCookieName(): string {
  return AUTH_COOKIE_NAME;
}
