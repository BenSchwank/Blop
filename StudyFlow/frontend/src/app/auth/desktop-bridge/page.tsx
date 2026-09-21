"use client";

import React, { Suspense, useEffect, useMemo } from "react";
import { useSearchParams } from "next/navigation";
import Script from "next/script";

/**
 * Desktop Qt bridge: system browser opens this page (authorized GIS origin).
 * Uses GIS redirect (same tab) — popup mode looked like a blank/CMD window on Windows.
 * Credential is POSTed to /api/auth/google/desktop/gis-login; Qt polls /claim.
 *
 * login_uri must be an exact Authorized redirect URI (no query). Handoff id is
 * stored in cookie blop_desktop_bridge (SameSite=None for Google's cross-site POST).
 */
function DesktopBridgeInner() {
  const params = useSearchParams();
  const state = params.get("state") || "";
  const wantCalendar = params.get("calendar") === "1";

  const clientId =
    process.env.NEXT_PUBLIC_GOOGLE_CLIENT_ID ||
    "571766217-ruevgp3i4pj9t0imddardh6mnc3rqfah.apps.googleusercontent.com";

  // Match Qt generateRandomString (RFC 7636 unreserved: A-Za-z0-9-._~).
  const valid = useMemo(() => /^[A-Za-z0-9\-._~]{8,128}$/.test(state), [state]);

  const loginUri =
    "https://www.blop-study.com/api/auth/google/desktop/gis-login";

  useEffect(() => {
    if (!valid) return;
    try {
      document.cookie =
        "blop_desktop_bridge=" +
        encodeURIComponent(state) +
        "; Max-Age=600; Path=/api/auth/google/desktop; Secure; SameSite=None";
    } catch {
      /* ignore */
    }
  }, [valid, state]);

  return (
    <div
      style={{
        minHeight: "100vh",
        display: "flex",
        alignItems: "center",
        justifyContent: "center",
        background:
          "radial-gradient(1200px 600px at 50% -10%, #1a2240 0%, #0f1115 55%, #0a0b0f 100%)",
        color: "#e8e4ff",
        fontFamily: "Segoe UI, system-ui, sans-serif",
        padding: 24,
      }}
    >
      <div
        style={{
          width: "min(420px, 92vw)",
          padding: "28px 24px 22px",
          borderRadius: 18,
          background: "rgba(28,30,40,0.92)",
          border: "1px solid rgba(255,255,255,0.08)",
          boxShadow: "0 18px 50px rgba(0,0,0,0.45)",
          textAlign: "center",
        }}
      >
        <h1 style={{ margin: "0 0 8px", fontSize: 22 }}>
          {wantCalendar ? "Google Calendar verbinden" : "Mit Google anmelden"}
        </h1>
        <p
          style={{
            margin: "0 0 20px",
            color: "#a8aec2",
            fontSize: 14,
            lineHeight: 1.45,
          }}
        >
          {!valid
            ? "Ungültige Bridge-Parameter (state). Bitte in Blop erneut anmelden."
            : wantCalendar
              ? "Melde dich an und erlaube den Kalender-Zugriff."
              : "Melde dich für Blop an. Der Login läuft im selben Browser-Tab (kein Extra-Fenster)."}
        </p>
        {valid ? (
          <>
            <Script
              src="https://accounts.google.com/gsi/client"
              strategy="afterInteractive"
            />
            <div
              id="g_id_onload"
              data-client_id={clientId}
              data-context="signin"
              data-ux_mode="redirect"
              data-login_uri={loginUri}
              data-auto_prompt="false"
            />
            <div
              className="g_id_signin"
              data-type="standard"
              data-shape="rectangular"
              data-theme="outline"
              data-text="signin_with"
              data-size="large"
              data-logo_alignment="center"
              style={{ display: "flex", justifyContent: "center" }}
            />
          </>
        ) : null}
      </div>
    </div>
  );
}

export default function DesktopBridgePage() {
  return (
    <Suspense
      fallback={
        <div
          style={{
            minHeight: "100vh",
            display: "flex",
            alignItems: "center",
            justifyContent: "center",
            background: "#0f1115",
            color: "#a8aec2",
            fontFamily: "Segoe UI, system-ui, sans-serif",
          }}
        >
          Laden…
        </div>
      }
    >
      <DesktopBridgeInner />
    </Suspense>
  );
}
