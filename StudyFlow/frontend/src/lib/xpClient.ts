function sessionHeaders(): HeadersInit {
    const sid = typeof window !== "undefined" ? localStorage.getItem("session_id") || "" : "";
    return {
        "Content-Type": "application/json",
        "X-Session-Id": sid,
    };
}

export async function awardXp(amount: number, reason = ""): Promise<{ xp?: number; streak_days?: number; daily_xp?: number } | null> {
    const n = Math.round(Number(amount) || 0);
    if (n <= 0) return null;
    const username = typeof window !== "undefined" ? localStorage.getItem("username") || "" : "";
    if (!username) return null;
    try {
        const res = await fetch("/api/me/xp", {
            method: "POST",
            headers: sessionHeaders(),
            body: JSON.stringify({ username, amount: n, reason }),
        });
        if (!res.ok) return null;
        const data = await res.json();
        if (typeof window !== "undefined") {
            window.dispatchEvent(new CustomEvent("blop_xp_updated", { detail: data }));
        }
        return data;
    } catch {
        return null;
    }
}
