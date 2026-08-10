"use client";

import React, { Suspense, useEffect, useMemo, useState } from "react";
import { useSearchParams } from "next/navigation";
import Script from "next/script";

/**
 * Desktop Qt bridge: system browser opens this page (authorized GIS origin).
 * Credential is POSTed to the backend; the Qt app polls /claim — no localhost
 * redirect (Chrome Private Network Access blocks https→127.0.0.1).
 */
function DesktopBridgeInner() {
  const params = useSearchParams();
  const state = params.get("state") || "";
  const [error, setError] = useState("");
  const [done, setDone] = useState(false);

  const clientId =
    process.env.NEXT_PUBLIC_GOOGLE_CLIENT_ID ||
    "571766217-ruevgp3i4pj9t0imddardh6mnc3rqfah.apps.googleusercontent.com";

  const valid = useMemo(() => /^[A-Za-z0-9_-]{8,128}$/.test(state), [state]);

  useEffect(() => {
    if (!valid) {
      setError("Ungültige Bridge-Parameter (state). Bitte in Blop erneut anmelden.");
      return;
    }
    (window as any).blopDesktopGoogleCb = async (response: any) => {
      try {
        const cred = response?.credential ? String(response.credential) : "";
        if (!cred) throw new Error("Kein Google-Token erhalten");
        const res = await fetch("/api/auth/google/desktop/complete", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ state, credential: cred }),
        });
        const data = await res.json().catch(() => ({} as any));
        if (!res.ok) {
          throw new Error(data?.detail ? String(data.detail) : `HTTP ${res.status}`);
        }
        setDone(true);
        setError("");
      } catch (e: any) {
        setError(e?.message || "Google-Anmeldung fehlgeschlagen");
      }
    };
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
        <h1 style={{ margin: "0 0 8px", fontSize: 22 }}>Mit Google anmelden</h1>
        <p style={{ margin: "0 0 20px", color: "#a8aec2", fontSize: 14, lineHeight: 1.45 }}>
          {done
            ? "Fertig — zurück zu Blop. Dieses Fenster kannst du schließen."
            : "Melde dich für Blop an. Die App holt die Anmeldung automatisch ab."}
        </p>
        {valid && !done ? (
          <>
            <Script src="https://accounts.google.com/gsi/client" strategy="afterInteractive" />
            <div
              id="g_id_onload"
              data-client_id={clientId}
              data-context="signin"
              data-ux_mode="popup"
              data-callback="blopDesktopGoogleCb"
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
        {error ? (
          <p style={{ marginTop: 14, color: "#ff8f8f", fontSize: 13 }}>{error}</p>
        ) : null}
        {done ? (
          <p style={{ marginTop: 14, color: "#9dffc9", fontSize: 14 }}>
            Anmeldung an Blop übermittelt.
          </p>
        ) : null}
      </div>
    </div>
  );
}

export default function DesktopBridgePage() {
  return (
    <Suspense
      fallback={
        <div style={{ minHeight: "100vh", background: "#0f1115", color: "#e8e4ff", padding: 40 }}>
          Lade…
        </div>
      }
    >
      <DesktopBridgeInner />
    </Suspense>
  );
}
