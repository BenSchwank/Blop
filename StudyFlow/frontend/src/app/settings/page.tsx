"use client";

import React, { useState, useEffect } from 'react';
import { Trash2, AlertTriangle, Loader2, LogOut } from 'lucide-react';
import { useRouter } from 'next/navigation';

export default function Settings() {
    const router = useRouter();
    const [username, setUsername] = useState("");
    const [loading, setLoading] = useState(false);

    // Delete State
    const [isDeleteOpen, setIsDeleteOpen] = useState(false);
    const [deletePassword, setDeletePassword] = useState("");
    const [deleteError, setDeleteError] = useState("");

    const API_BASE = process.env.NEXT_PUBLIC_API_URL || 'http://localhost:8000/api';

    useEffect(() => {
        const user = localStorage.getItem("username");
        if (user) setUsername(user);
    }, []);

    const handleDeleteAccount = async (e: React.FormEvent) => {
        e.preventDefault();
        setLoading(true);
        setDeleteError("");

        try {
            const res = await fetch(`${API_BASE}/auth/user`, {
                method: "DELETE",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify({ username, password: deletePassword })
            });

            if (res.ok) {
                // Success
                localStorage.clear();
                window.location.href = "/login";
            } else {
                const err = await res.json();
                setDeleteError(err.detail || "Fehler beim Löschen.");
            }
        } catch (error) {
            setDeleteError("Verbindungsfehler.");
        } finally {
            setLoading(false);
        }
    };

    return (
        <div className="bg-[#1e1e1e] min-h-screen p-8 md:p-12">
            <div className="max-w-2xl mx-auto space-y-12">

                {/* Header */}
                <div className="flex items-center gap-4 border-b border-[#333] pb-8">
                    <div className="w-16 h-16 bg-[#252526] rounded-2xl flex items-center justify-center text-3xl shadow-inner border border-[#333]">
                        ⚙️
                    </div>
                    <div>
                        <h1 className="text-3xl font-bold text-white">Einstellungen</h1>
                        <p className="text-gray-400">Verwalte deinen Account und deine Daten.</p>
                    </div>
                </div>

                {/* Profile Section */}
                <section>
                    <h2 className="text-xl font-semibold text-white mb-4">Profil</h2>
                    <div className="bg-[#252526] border border-[#333] rounded-2xl p-6 flex items-center justify-between">
                        <div className="flex items-center gap-4">
                            <div className="w-12 h-12 rounded-full bg-gradient-to-br from-[#5E5CE6] to-[#7D7AFF] flex items-center justify-center text-white font-bold text-lg">
                                {username.charAt(0).toUpperCase()}
                            </div>
                            <div>
                                <p className="text-white font-medium">{username}</p>
                                <p className="text-sm text-gray-500">Angemeldet</p>
                            </div>
                        </div>
                        <button
                            onClick={() => { localStorage.clear(); window.location.href = '/login'; }}
                            className="text-sm text-gray-400 hover:text-white flex items-center gap-2 px-4 py-2 hover:bg-[#333] rounded-lg transition-colors"
                        >
                            <LogOut size={16} /> Abmelden
                        </button>
                    </div>
                </section>

                {/* Danger Zone */}
                <section>
                    <h2 className="text-xl font-semibold text-red-400 mb-4 flex items-center gap-2">
                        <AlertTriangle size={20} /> Danger Zone
                    </h2>
                    <div className="bg-red-500/5 border border-red-500/20 rounded-2xl p-6">
                        <div className="flex flex-col md:flex-row md:items-center justify-between gap-6">
                            <div>
                                <h3 className="text-lg font-medium text-white mb-1">Account löschen</h3>
                                <p className="text-sm text-red-300/70">
                                    Löscht unwiderruflich deinen Account und alle Daten (Ordner, Pläne, Uploads).
                                </p>
                            </div>
                            <button
                                onClick={() => setIsDeleteOpen(true)}
                                className="px-5 py-2.5 bg-red-500/10 hover:bg-red-500/20 text-red-400 border border-red-500/30 rounded-xl text-sm font-medium transition-all whitespace-nowrap flex items-center gap-2"
                            >
                                <Trash2 size={16} />
                                Account löschen
                            </button>
                        </div>
                    </div>
                </section>

            </div>

            {/* Delete Confirmation Modal */}
            {isDeleteOpen && (
                <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/80 backdrop-blur-sm p-4">
                    <div className="bg-[#1e1e1e] border border-[#333] rounded-2xl w-full max-w-md p-8 shadow-2xl animate-in fade-in zoom-in duration-200">
                        <div className="flex flex-col items-center text-center mb-6">
                            <div className="w-16 h-16 bg-red-500/10 rounded-full flex items-center justify-center mb-4 text-red-500">
                                <AlertTriangle size={32} />
                            </div>
                            <h3 className="text-2xl font-bold text-white mb-2">Bist du sicher?</h3>
                            <p className="text-gray-400 text-sm">
                                Diese Aktion kann <span className="text-red-400 font-semibold">nicht rückgängig</span> gemacht werden.
                                Alle deine Daten werden sofort gelöscht.
                            </p>
                        </div>

                        <form onSubmit={handleDeleteAccount} className="space-y-4">
                            <div>
                                <label className="block text-xs font-medium text-gray-500 mb-1.5 ml-1">Zur Bestätigung Passwort eingeben:</label>
                                <input
                                    type="password"
                                    value={deletePassword}
                                    onChange={(e) => setDeletePassword(e.target.value)}
                                    placeholder="Dein Passwort"
                                    className="w-full bg-[#252526] border border-[#333] text-white rounded-xl px-4 py-3 focus:ring-2 focus:ring-red-500/50 focus:border-red-500 outline-none transition-all"
                                    autoFocus
                                />
                            </div>

                            {deleteError && (
                                <div className="p-3 bg-red-500/10 border border-red-500/20 rounded-lg text-red-400 text-sm text-center">
                                    {deleteError}
                                </div>
                            )}

                            <div className="flex gap-3 pt-2">
                                <button
                                    type="button"
                                    onClick={() => { setIsDeleteOpen(false); setDeletePassword(""); setDeleteError(""); }}
                                    className="flex-1 py-3 text-sm font-medium text-gray-300 hover:text-white bg-[#333] hover:bg-[#444] rounded-xl transition-all"
                                >
                                    Abbrechen
                                </button>
                                <button
                                    type="submit"
                                    disabled={!deletePassword || loading}
                                    className="flex-1 py-3 text-sm font-medium bg-red-600 hover:bg-red-700 text-white rounded-xl transition-all disabled:opacity-50 flex justify-center items-center gap-2 shadow-lg shadow-red-900/20"
                                >
                                    {loading ? <Loader2 size={18} className="animate-spin" /> : "Endgültig löschen"}
                                </button>
                            </div>
                        </form>
                    </div>
                </div>
            )}
        </div>
    );
}
