"use client";

import React, { useEffect, useState } from "react";
import Link from "next/link";
import { useRouter } from "next/navigation";
import { KeyRound } from "lucide-react";

const API_BASE = "/api";

export default function ResetPasswordPage() {
    const router = useRouter();
    const [accessToken, setAccessToken] = useState<string | null>(null);
    const [linkError, setLinkError] = useState("");
    const [password, setPassword] = useState("");
    const [password2, setPassword2] = useState("");
    const [error, setError] = useState("");
    const [success, setSuccess] = useState("");
    const [loading, setLoading] = useState(false);

    useEffect(() => {
        if (typeof window === "undefined") return;
        const hash = window.location.hash.replace(/^#/, "");
        const params = new URLSearchParams(hash);
        const token = params.get("access_token");
        const type = params.get("type");
        if (token && type === "recovery") {
            setAccessToken(token);
            window.history.replaceState(null, "", window.location.pathname + window.location.search);
            return;
        }
        setLinkError(
            "Dieser Link ist ungültig oder abgelaufen. Bitte fordere unter „Passwort vergessen“ eine neue E-Mail an."
        );
    }, []);

    const handleSubmit = async (e: React.FormEvent) => {
        e.preventDefault();
        setError("");
        setSuccess("");
        if (!accessToken) {
            setError("Kein gültiger Wiederherstellungs-Link.");
            return;
        }
        if (password.length < 8) {
            setError("Passwort muss mindestens 8 Zeichen haben.");
            return;
        }
        if (password !== password2) {
            setError("Die Passwörter stimmen nicht überein.");
            return;
        }
        setLoading(true);
        try {
            const res = await fetch(`${API_BASE}/auth/reset-password-confirm`, {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify({ access_token: accessToken, new_password: password }),
            });
            const text = await res.text();
            let data: { detail?: string; message?: string };
            try {
                data = JSON.parse(text);
            } catch {
                data = { detail: text || "Unbekannter Fehler" };
            }
            if (!res.ok) {
                throw new Error(data.detail || "Zurücksetzen fehlgeschlagen");
            }
            setSuccess(data.message || "Passwort geändert.");
            setTimeout(() => router.push("/login"), 2000);
        } catch (err: unknown) {
            setError(err instanceof Error ? err.message : "Ein Fehler ist aufgetreten");
        } finally {
            setLoading(false);
        }
    };

    return (
        <div className="min-h-screen bg-[#1e1e1e] flex flex-col items-center justify-center p-4">
            <div className="w-full max-w-md blop-card p-8">
                <div className="flex items-center gap-2 mb-6 text-white">
                    <KeyRound className="text-[#5E5CE6]" size={28} />
                    <h1 className="text-xl font-bold">Neues Passwort setzen</h1>
                </div>

                {linkError && (
                    <p className="text-red-400 text-sm mb-4">{linkError}</p>
                )}

                {success && (
                    <p className="text-green-400 text-sm mb-4">{success} Weiterleitung zur Anmeldung …</p>
                )}

                {!linkError && accessToken && !success && (
                    <form onSubmit={handleSubmit} className="space-y-4">
                        <div>
                            <label className="block text-sm font-medium text-[#DDD] mb-2">Neues Passwort</label>
                            <input
                                type="password"
                                value={password}
                                onChange={(e) => setPassword(e.target.value)}
                                required
                                minLength={8}
                                autoComplete="new-password"
                                className="w-full px-4 py-3 bg-[#252526] border border-[#444] rounded-lg text-white placeholder-[#888] focus:outline-none focus:border-[#5E5CE6] focus:ring-2 focus:ring-[#5E5CE6]/20"
                                placeholder="Mindestens 8 Zeichen"
                            />
                        </div>
                        <div>
                            <label className="block text-sm font-medium text-[#DDD] mb-2">Passwort wiederholen</label>
                            <input
                                type="password"
                                value={password2}
                                onChange={(e) => setPassword2(e.target.value)}
                                required
                                minLength={8}
                                autoComplete="new-password"
                                className="w-full px-4 py-3 bg-[#252526] border border-[#444] rounded-lg text-white placeholder-[#888] focus:outline-none focus:border-[#5E5CE6] focus:ring-2 focus:ring-[#5E5CE6]/20"
                                placeholder="Noch einmal eingeben"
                            />
                        </div>
                        {error && (
                            <div className="p-3 rounded-lg text-sm bg-red-500/10 text-red-400 border border-red-500/20">
                                {error}
                            </div>
                        )}
                        <button
                            type="submit"
                            disabled={loading}
                            className="w-full py-3.5 bg-[#5E5CE6] rounded-lg text-white font-semibold hover:bg-[#7D7AFF] disabled:opacity-50"
                        >
                            {loading ? "Wird gespeichert …" : "Passwort speichern"}
                        </button>
                    </form>
                )}

                <p className="mt-6 text-center text-sm text-[#888]">
                    <Link href="/login" className="text-[#5E5CE6] hover:underline">
                        Zurück zur Anmeldung
                    </Link>
                </p>
            </div>
        </div>
    );
}
