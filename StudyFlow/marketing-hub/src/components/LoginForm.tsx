"use client";

import { FormEvent, useState } from "react";
import { useRouter } from "next/navigation";

export default function LoginForm() {
  const router = useRouter();
  const [username, setUsername] = useState("");
  const [password, setPassword] = useState("");
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState("");

  async function onSubmit(event: FormEvent<HTMLFormElement>) {
    event.preventDefault();
    setError("");
    setLoading(true);
    try {
      const response = await fetch("/api/auth/login", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ username, password }),
      });
      if (!response.ok) {
        const body = (await response.json()) as { detail?: string };
        setError(body.detail ?? "Login fehlgeschlagen.");
        return;
      }
      router.push("/dashboard");
      router.refresh();
    } catch {
      setError("Netzwerkfehler beim Login.");
    } finally {
      setLoading(false);
    }
  }

  return (
    <form onSubmit={onSubmit} className="glass mx-auto w-full max-w-md space-y-5 p-8">
      <h1 className="text-2xl font-semibold text-white">Blop Marketing Hub</h1>
      <p className="text-sm text-[#A9ADC8]">Interner Zugriff fuer Marketing-Automation</p>

      <div className="space-y-2">
        <label className="text-sm text-[#C9CCE1]">Benutzername</label>
        <input
          className="w-full rounded-xl border border-white/20 bg-white/5 px-4 py-3 text-white outline-none focus:border-[#5E5CE6]"
          value={username}
          onChange={(e) => setUsername(e.target.value)}
          autoComplete="username"
          required
        />
      </div>

      <div className="space-y-2">
        <label className="text-sm text-[#C9CCE1]">Passwort</label>
        <input
          type="password"
          className="w-full rounded-xl border border-white/20 bg-white/5 px-4 py-3 text-white outline-none focus:border-[#5E5CE6]"
          value={password}
          onChange={(e) => setPassword(e.target.value)}
          autoComplete="current-password"
          required
        />
      </div>

      {error ? <p className="text-sm text-red-300">{error}</p> : null}

      <button
        type="submit"
        disabled={loading}
        className="w-full rounded-xl bg-[#5E5CE6] px-4 py-3 font-semibold text-white hover:bg-[#6d6bee] disabled:opacity-60"
      >
        {loading ? "Melde an..." : "Einloggen"}
      </button>
    </form>
  );
}
