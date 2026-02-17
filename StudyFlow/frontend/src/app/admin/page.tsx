"use client";

import React, { useState, useEffect } from 'react';
import { Users, TrendingUp, Award, Shield } from 'lucide-react';

interface User {
    username: string;
    xp: number;
    streak: number;
    created_at: string;
}

export default function AdminPanel() {
    const [users, setUsers] = useState<User[]>([]);
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

        // Fetch users (you'll need to create this endpoint)
        // For now, mock data
        setUsers([
            { username: 'admin_', xp: 9999, streak: 100, created_at: '2024-01-01' },
            { username: 'test_user', xp: 150, streak: 5, created_at: '2024-02-15' },
        ]);
        setLoading(false);
    }, []);

    if (!isAdmin) return null;

    return (
        <div className="min-h-screen bg-[#1e1e1e] text-white">
            <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-6">
                {/* Header */}
                <div className="mb-8">
                    <div className="flex items-center gap-3 mb-4">
                        <div className="w-12 h-12 rounded-xl bg-gradient-to-br from-[#5E5CE6] to-[#7D7AFF] flex items-center justify-center">
                            <Shield className="w-6 h-6 text-white" strokeWidth={2} />
                        </div>
                        <div>
                            <h1 className="text-3xl font-bold text-white">Admin Panel</h1>
                            <p className="text-[#888]">Nutzerverwaltung & Statistiken</p>
                        </div>
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
                        <p className="text-3xl font-bold text-white">
                            {users.reduce((sum, u) => sum + u.xp, 0).toLocaleString()}
                        </p>
                    </div>

                    <div className="blop-card p-6">
                        <div className="flex items-center gap-3 mb-2">
                            <Award className="w-5 h-5 text-[#5E5CE6]" />
                            <h3 className="text-sm font-medium text-[#888]">Aktive Streaks</h3>
                        </div>
                        <p className="text-3xl font-bold text-white">
                            {users.filter(u => u.streak > 0).length}
                        </p>
                    </div>
                </div>

                {/* Users Table */}
                <div className="blop-card p-6">
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
                                        <th className="text-left py-3 px-4 text-sm font-medium text-[#888]">Benutzername</th>
                                        <th className="text-left py-3 px-4 text-sm font-medium text-[#888]">XP</th>
                                        <th className="text-left py-3 px-4 text-sm font-medium text-[#888]">Streak</th>
                                        <th className="text-left py-3 px-4 text-sm font-medium text-[#888]">Erstellt am</th>
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
                                                    {user.username === 'admin_' && (
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
                                            <td className="py-3 px-4 text-[#888]">{user.created_at}</td>
                                        </tr>
                                    ))}
                                </tbody>
                            </table>
                        </div>
                    )}
                </div>
            </div>
        </div>
    );
}
