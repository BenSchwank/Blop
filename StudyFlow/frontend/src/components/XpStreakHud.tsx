"use client";

import React, { useEffect, useState } from "react";
import { Flame, Sparkles } from "lucide-react";

type MeStats = {
    xp: number;
    streak_days: number;
    daily_xp: number;
};

export default function XpStreakHud() {
    const [stats, setStats] = useState<MeStats | null>(null);

    const load = async () => {
        const username = localStorage.getItem("username") || "";
        const sid = localStorage.getItem("session_id") || "";
        if (!username) return;
        try {
            const res = await fetch(`/api/me?session_id=${encodeURIComponent(sid)}&username=${encodeURIComponent(username)}`, {
                headers: { "X-Session-Id": sid },
            });
            if (!res.ok) return;
            const data = await res.json();
            setStats({
                xp: Number(data.xp) || 0,
                streak_days: Number(data.streak_days) || 0,
                daily_xp: Number(data.daily_xp) || 0,
            });
        } catch {
            /* ignore */
        }
    };

    useEffect(() => {
        void load();
        const onXp = () => void load();
        window.addEventListener("blop_xp_updated", onXp);
        return () => window.removeEventListener("blop_xp_updated", onXp);
    }, []);

    if (!stats) return null;
    const goal = 50;
    const pct = Math.min(100, Math.round((100 * stats.daily_xp) / goal));

    return (
        <div className="flex items-center gap-2 text-xs text-gray-300">
            <span className="inline-flex items-center gap-1 px-2 py-1 rounded-lg bg-[#5B9DFF]/10 text-[#9EC5FF] border border-[#5B9DFF]/20">
                <Sparkles size={12} /> {stats.xp.toLocaleString()} XP
            </span>
            <span className="inline-flex items-center gap-1 px-2 py-1 rounded-lg bg-amber-500/10 text-amber-200 border border-amber-500/20">
                <Flame size={12} /> {stats.streak_days}d
            </span>
            <span className="hidden lg:inline-flex items-center gap-1.5 px-2 py-1 rounded-lg border border-[#2A3140] bg-[#12141A]">
                <span className="w-12 h-1.5 rounded-full bg-[#2A3140] overflow-hidden">
                    <span className="block h-full bg-[#5B9DFF]" style={{ width: `${pct}%` }} />
                </span>
                <span className="text-[10px] text-gray-500">{stats.daily_xp}/{goal}</span>
            </span>
        </div>
    );
}
