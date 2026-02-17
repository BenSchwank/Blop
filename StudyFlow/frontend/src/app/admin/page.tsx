"use client";

import React, { useState, useEffect } from 'react';
import { Users, TrendingUp, Award, Shield, RefreshCw } from 'lucide-react';

interface User {
    username: string;
    xp: number;
    streak: number;
    created_at: string;
    is_admin?: boolean;
}

export default function AdminPanel() {
    const [users, setUsers] = useState<User[]>([]);
    const [leaderboard, setLeaderboard] = useState<User[]>([]);
    const [loading, setLoading] = useState(true);
    const [isAdmin, setIsAdmin] = useState(false);

    const API_BASE = process.env.NEXT_PUBLIC_API_URL || 'http://localhost:8000/api';

    useEffect(() => {
        // Check if user is admin
        const username = localStorage.getItem('username');
        if (username !== 'admin_') {
            window.location.href = '/';
            return;
        }
        setIsAdmin(true);
        loadData();
    }, []);

    const loadData = async () => {
        setLoading(true);
        try {
            const username = localStorage.getItem('username');

            // Fetch all users
            const usersRes = await fetch(`${API_BASE}/admin/users?admin_username=${username}`);
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
        } catch (err) {
            console.error('Failed to load admin data:', err);
        } finally {
            setLoading(false);
        }
    };

    if (!isAdmin) return null;

    const totalXP = users.reduce((sum, u) => sum + u.xp, 0);
    const activeStreaks = users.filter(u => u.streak > 0).length;

    return (
        <div className="bg-[#1e1e1e] text-white p-4 md:p-6 pl-0 md:pl-[280px]">
            <div className="max-w-7xl mx-auto min-h-screen">
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

                {/* Stats Cards */}
                <div className="grid grid-cols-1 sm:grid-cols-3 gap-4 mb-8">
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
