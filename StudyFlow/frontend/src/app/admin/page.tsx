"use client";

import React, { useState, useEffect } from 'react';
import Link from 'next/link';
import { Users, TrendingUp, Award, Shield, RefreshCw, CreditCard, Wallet, KeyRound, ExternalLink } from 'lucide-react';

interface User {
    username: string;
    xp: number;
    streak: number;
    created_at: string;
    is_admin?: boolean;
}

interface AiKeyFingerprint {
    env: string;
    configured: boolean;
    suffix: string | null;
    length: number;
    prefix: string | null;
    display?: string;
}

interface AiKeysDebug {
    google_api_key: AiKeyFingerprint;
    openai_api_key: AiKeyFingerprint;
    compare_hint: string;
    fix_urls: {
        ai_studio_keys: string;
        ai_studio_billing: string;
        openai_billing: string;
    };
}

export default function AdminPanel() {
    const [users, setUsers] = useState<User[]>([]);
    const [leaderboard, setLeaderboard] = useState<User[]>([]);
    const [aiKeys, setAiKeys] = useState<AiKeysDebug | null>(null);
    const [aiKeysError, setAiKeysError] = useState('');
    const [loading, setLoading] = useState(true);
    const [isAdmin, setIsAdmin] = useState(false);

    const API_BASE = '/api';

    useEffect(() => {
        // Check if user is admin (check both username and stored is_admin flag)
        const username = localStorage.getItem('username');
        const storedIsAdmin = localStorage.getItem('is_admin') === 'true';
        if (username !== 'admin_' && !storedIsAdmin) {
            window.location.href = '/';
            return;
        }
        setIsAdmin(true);
        loadData();
    }, []);

    const loadData = async () => {
        setLoading(true);
        setAiKeysError('');
        try {
            const username = localStorage.getItem('username');
            const sid = localStorage.getItem('session_id') || '';
            const authQs = `admin_username=${encodeURIComponent(username || '')}&session_id=${encodeURIComponent(sid)}`;
            const authHeaders = { 'X-Session-Id': sid };

            // Fetch all users — send session_id for auth
            const usersRes = await fetch(
                `${API_BASE}/admin/users?${authQs}`,
                { headers: authHeaders }
            );
            if (usersRes.ok) {
                const usersData = await usersRes.json();
                setUsers(usersData);
            }

            // Fetch leaderboard
            const leaderboardRes = await fetch(`${API_BASE}/admin/leaderboard?limit=10`);
            if (leaderboardRes.ok) {
                const leaderboardData = await leaderboardRes.json();
                setLeaderboard(leaderboardData);
            }

            const keysRes = await fetch(
                `${API_BASE}/admin/debug/ai-keys?${authQs}`,
                { headers: authHeaders }
            );
            if (keysRes.ok) {
                setAiKeys(await keysRes.json());
            } else {
                const err = await keysRes.json().catch(() => ({}));
                setAiKeys(null);
                setAiKeysError(
                    typeof err.detail === 'string'
                        ? err.detail
                        : `Keys konnten nicht geladen werden (HTTP ${keysRes.status}).`
                );
            }
        } catch (err) {
            console.error('Failed to load admin data:', err);
            setAiKeysError('Netzwerkfehler beim Laden der Key-Debug-Daten.');
        } finally {
            setLoading(false);
        }
    };

    if (!isAdmin) return null;

    const totalXP = users.reduce((sum, u) => sum + u.xp, 0);
    const activeStreaks = users.filter(u => u.streak > 0).length;

    const KeyCard = ({ title, fp, compareUrl }: { title: string; fp?: AiKeyFingerprint; compareUrl: string }) => (
        <div className="rounded-xl border border-[#333] bg-[#252526] p-4 space-y-2">
            <div className="flex items-center justify-between gap-2">
                <p className="text-sm font-medium text-[#aaa]">{title}</p>
                <span
                    className={`text-[10px] uppercase tracking-wide px-2 py-0.5 rounded-full ${
                        fp?.configured
                            ? 'bg-green-500/15 text-green-400'
                            : 'bg-red-500/15 text-red-400'
                    }`}
                >
                    {fp?.configured ? 'gesetzt' : 'fehlt'}
                </span>
            </div>
            <p className="font-mono text-2xl text-white tracking-wider">
                {fp?.display || '—'}
            </p>
            <p className="text-xs text-[#666]">
                {fp?.configured
                    ? `Länge ${fp.length} · Env ${fp.env}`
                    : `Env ${fp?.env || '—'} nicht gesetzt`}
            </p>
            <a
                href={compareUrl}
                target="_blank"
                rel="noopener noreferrer"
                className="inline-flex items-center gap-1.5 text-xs text-[#8B89F0] hover:text-white underline underline-offset-2"
            >
                Vergleichen <ExternalLink size={12} />
            </a>
        </div>
    );

    return (
        <div className="bg-[#1e1e1e] text-white min-h-screen px-8 sm:px-12 lg:px-16 xl:px-20 py-8 sm:py-10 lg:py-12">
            <div>
                {/* Header */}
                <div className="mb-8">
                    <div className="flex items-center justify-between mb-4">
                        <div className="flex items-center gap-3">
                            <div className="w-12 h-12 rounded-xl bg-gradient-to-br from-[#5E5CE6] to-[#7D7AFF] flex items-center justify-center">
                                <Shield className="w-6 h-6 text-white" strokeWidth={2} />
                            </div>
                            <div>
                                <h1 className="text-3xl font-bold text-white">Admin Panel</h1>
                                <p className="text-[#888]">Nutzerverwaltung & Statistiken</p>
                            </div>
                        </div>
                        <button
                            onClick={loadData}
                            className="px-4 py-2 bg-[#5E5CE6] rounded-lg hover:bg-[#7D7AFF] transition-all flex items-center gap-2"
                        >
                            <RefreshCw size={16} />
                            Aktualisieren
                        </button>
                    </div>
                </div>

                {/* Admin KI-Debug: Key-Fingerprints */}
                <div className="blop-card p-6 mb-8">
                    <div className="flex items-start justify-between gap-4 mb-4">
                        <div className="flex items-center gap-3">
                            <div className="w-10 h-10 rounded-lg bg-[#5E5CE6]/15 flex items-center justify-center">
                                <KeyRound className="w-5 h-5 text-[#8B89F0]" />
                            </div>
                            <div>
                                <h2 className="text-xl font-semibold text-white">KI-Debug · Server-Keys</h2>
                                <p className="text-sm text-[#888]">
                                    Nur Suffix (letzte 4 Zeichen) — zum Abgleich mit AI Studio / Render.
                                </p>
                            </div>
                        </div>
                    </div>
                    {aiKeysError ? (
                        <p className="text-sm text-red-400 mb-3">{aiKeysError}</p>
                    ) : null}
                    {loading && !aiKeys ? (
                        <div className="flex items-center justify-center py-8">
                            <div className="animate-spin rounded-full h-8 w-8 border-3 border-[#5E5CE6] border-t-transparent" />
                        </div>
                    ) : (
                        <>
                            <div className="grid grid-cols-1 sm:grid-cols-2 gap-4 mb-4">
                                <KeyCard
                                    title="GOOGLE_API_KEY (Gemini)"
                                    fp={aiKeys?.google_api_key}
                                    compareUrl={aiKeys?.fix_urls.ai_studio_keys || 'https://aistudio.google.com/app/apikey'}
                                />
                                <KeyCard
                                    title="OPENAI_API_KEY (TTS)"
                                    fp={aiKeys?.openai_api_key}
                                    compareUrl={aiKeys?.fix_urls.openai_billing || 'https://platform.openai.com/settings/organization/billing'}
                                />
                            </div>
                            {aiKeys?.compare_hint ? (
                                <p className="text-xs text-[#777] mb-3">{aiKeys.compare_hint}</p>
                            ) : null}
                            <div className="flex flex-wrap gap-3 text-xs">
                                <a
                                    href={aiKeys?.fix_urls.ai_studio_billing || 'https://ai.studio/projects'}
                                    target="_blank"
                                    rel="noopener noreferrer"
                                    className="inline-flex items-center gap-1.5 px-3 py-1.5 rounded-lg bg-[#5E5CE6]/10 text-[#8B89F0] hover:bg-[#5E5CE6]/20"
                                >
                                    AI Studio Billing <ExternalLink size={12} />
                                </a>
                                <a
                                    href={aiKeys?.fix_urls.ai_studio_keys || 'https://aistudio.google.com/app/apikey'}
                                    target="_blank"
                                    rel="noopener noreferrer"
                                    className="inline-flex items-center gap-1.5 px-3 py-1.5 rounded-lg bg-[#5E5CE6]/10 text-[#8B89F0] hover:bg-[#5E5CE6]/20"
                                >
                                    AI Studio API-Keys <ExternalLink size={12} />
                                </a>
                            </div>
                        </>
                    )}
                </div>

                {/* Stats Cards */}
                <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-4 gap-4 mb-8">
                    <div className="blop-card p-6">
                        <div className="flex items-center gap-3 mb-2">
                            <Users className="w-5 h-5 text-[#5E5CE6]" />
                            <h3 className="text-sm font-medium text-[#888]">Gesamt Nutzer</h3>
                        </div>
                        <p className="text-3xl font-bold text-white">{users.length}</p>
                    </div>

                    <div className="blop-card p-6">
                        <div className="flex items-center gap-3 mb-2">
                            <TrendingUp className="w-5 h-5 text-[#5E5CE6]" />
                            <h3 className="text-sm font-medium text-[#888]">Gesamt XP</h3>
                        </div>
                        <p className="text-3xl font-bold text-white">{totalXP.toLocaleString()}</p>
                    </div>

                    <div className="blop-card p-6">
                        <div className="flex items-center gap-3 mb-2">
                            <Award className="w-5 h-5 text-[#5E5CE6]" />
                            <h3 className="text-sm font-medium text-[#888]">Aktive Streaks</h3>
                        </div>
                        <p className="text-3xl font-bold text-white">{activeStreaks}</p>
                    </div>

                    <Link href="/admin/subscriptions" className="blop-card p-6 hover:bg-[#252526] transition-colors group">
                        <div className="flex items-center gap-3 mb-2">
                            <CreditCard className="w-5 h-5 text-[#5E5CE6] group-hover:text-[#7D7AFF]" />
                            <h3 className="text-sm font-medium text-[#888]">Abonnements</h3>
                        </div>
                        <p className="text-3xl font-bold text-white">Tiers & Features</p>
                    </Link>
                </div>

                <div className="grid grid-cols-1 lg:grid-cols-3 gap-6">
                    {/* Users Table */}
                    <div className="lg:col-span-2 blop-card p-6">
                        <h2 className="text-xl font-semibold mb-4 text-white">Alle Nutzer</h2>

                        {loading ? (
                            <div className="flex items-center justify-center py-12">
                                <div className="animate-spin rounded-full h-10 w-10 border-3 border-[#5E5CE6] border-t-transparent" />
                            </div>
                        ) : (
                            <div className="overflow-x-auto">
                                <table className="w-full">
                                    <thead>
                                        <tr className="border-b border-[#333]">
                                            <th className="text-left py-3 px-4 text-sm font-medium text-[#888]">Benutzer</th>
                                            <th className="text-left py-3 px-4 text-sm font-medium text-[#888]">XP</th>
                                            <th className="text-left py-3 px-4 text-sm font-medium text-[#888]">Streak</th>
                                            <th className="text-left py-3 px-4 text-sm font-medium text-[#888]">Erstellt</th>
                                            <th className="text-right py-3 px-4 text-sm font-medium text-[#888]">Aktion</th>
                                        </tr>
                                    </thead>
                                    <tbody>
                                        {users.map((user) => (
                                            <tr key={user.username} className="border-b border-[#252526] hover:bg-[#252526] transition-colors">
                                                <td className="py-3 px-4">
                                                    <div className="flex items-center gap-2">
                                                        <div className="w-8 h-8 rounded-full bg-gradient-to-br from-[#5E5CE6] to-[#7D7AFF] flex items-center justify-center text-white text-xs font-semibold">
                                                            {user.username.charAt(0).toUpperCase()}
                                                        </div>
                                                        <span className="text-white font-medium">{user.username}</span>
                                                        {user.is_admin && (
                                                            <span className="px-2 py-0.5 bg-[#5E5CE6]/20 text-[#5E5CE6] text-xs rounded-full">Admin</span>
                                                        )}
                                                    </div>
                                                </td>
                                                <td className="py-3 px-4 text-white">{user.xp.toLocaleString()}</td>
                                                <td className="py-3 px-4">
                                                    <span className={`px-2 py-1 rounded-lg text-sm ${user.streak > 7
                                                        ? 'bg-green-500/20 text-green-400'
                                                        : user.streak > 0
                                                            ? 'bg-yellow-500/20 text-yellow-400'
                                                            : 'bg-[#333] text-[#888]'
                                                        }`}>
                                                        {user.streak} Tage
                                                    </span>
                                                </td>
                                                <td className="py-3 px-4 text-[#888] text-sm">{user.created_at}</td>
                                                <td className="py-3 px-4 text-right">
                                                    <Link
                                                        href={`/admin/subscriptions?user=${encodeURIComponent(user.username)}`}
                                                        className="inline-flex items-center gap-1.5 px-3 py-1.5 rounded-lg bg-[#5E5CE6]/10 hover:bg-[#5E5CE6]/20 text-[#5E5CE6] text-xs font-medium transition-colors"
                                                    >
                                                        <Wallet size={12} />
                                                        Abo
                                                    </Link>
                                                </td>
                                            </tr>
                                        ))}
                                    </tbody>
                                </table>
                            </div>
                        )}
                    </div>

                    {/* Leaderboard */}
                    <div className="blop-card p-6">
                        <h2 className="text-xl font-semibold mb-4 text-white">🏆 Leaderboard</h2>

                        {loading ? (
                            <div className="flex items-center justify-center py-12">
                                <div className="animate-spin rounded-full h-8 w-8 border-3 border-[#5E5CE6] border-t-transparent" />
                            </div>
                        ) : (
                            <div className="space-y-3">
                                {leaderboard.map((user, index) => (
                                    <div key={user.username} className="flex items-center gap-3 p-3 bg-[#252526] rounded-lg hover:bg-[#333] transition-colors">
                                        <div className={`w-8 h-8 rounded-full flex items-center justify-center font-bold text-sm ${index === 0 ? 'bg-yellow-500 text-black' :
                                            index === 1 ? 'bg-gray-400 text-black' :
                                                index === 2 ? 'bg-orange-600 text-white' :
                                                    'bg-[#444] text-white'
                                            }`}>
                                            {index + 1}
                                        </div>
                                        <div className="flex-1 min-w-0">
                                            <p className="text-sm font-medium text-white truncate">{user.username}</p>
                                            <p className="text-xs text-[#888]">{user.xp.toLocaleString()} XP</p>
                                        </div>
                                        <div className="text-xs text-[#888]">
                                            🔥 {user.streak}
                                        </div>
                                    </div>
                                ))}
                            </div>
                        )}
                    </div>
                </div>
            </div>
        </div>
    );
}
