"use client";

import React, { useState } from 'react';
import Link from 'next/link';
import { useRouter } from 'next/navigation';
import { LogIn, UserPlus, Shield } from 'lucide-react';

import Image from 'next/image';

export default function LoginPage() {
    const router = useRouter();
    const [isLogin, setIsLogin] = useState(true);
    const [username, setUsername] = useState('');
    const [password, setPassword] = useState('');
    const [error, setError] = useState('');
    const [loading, setLoading] = useState(false);

    const API_BASE = '/api';


    const handleSubmit = async (e: React.FormEvent) => {
        e.preventDefault();
        setError('');
        setLoading(true);

        try {
            const endpoint = isLogin ? '/auth/login' : '/auth/register';
            const res = await fetch(`${API_BASE}${endpoint}`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ username, password }),
            });

            const data = await res.json();

            if (!res.ok) {
                throw new Error(data.detail || 'Fehler bei der Anmeldung');
            }

            if (isLogin) {
                // Save session
                localStorage.setItem('session_id', data.session_id);
                localStorage.setItem('username', data.username);

                // Redirect to dashboard
                router.push('/');
            } else {
                // After registration, switch to login
                setIsLogin(true);
                setPassword('');
                setError('Registrierung erfolgreich! Bitte melde dich an.');
            }
        } catch (err: any) {
            setError(err.message);
        } finally {
            setLoading(false);
        }
    };

    return (
        <div className="min-h-screen bg-[#1e1e1e] flex items-center justify-center p-4">
            <div className="w-full max-w-md">
                {/* Logo */}
                <div className="text-center mb-8">
                    <div className="w-20 h-20 rounded-2xl bg-[#5E5CE6] flex items-center justify-center mx-auto mb-4 overflow-hidden shadow-2xl shadow-[#5E5CE6]/20">
                        <Image src="/logo.jpg" alt="Blop Logo" width={80} height={80} className="object-cover w-full h-full" />
                    </div>
                    <h1 className="text-3xl font-bold text-white mb-2">Blop Study</h1>
                    <p className="text-[#888]">Dein AI Lernassistent</p>
                </div>

                {/* Form Card */}
                <div className="blop-card p-8">
                    <div className="flex gap-2 mb-6">
                        <button
                            onClick={() => setIsLogin(true)}
                            className={`flex-1 py-3 rounded-lg font-semibold transition-all ${isLogin
                                ? 'bg-[#5E5CE6] text-white'
                                : 'bg-[#252526] text-[#888] hover:bg-[#333]'
                                }`}
                        >
                            Anmelden
                        </button>
                        <button
                            onClick={() => setIsLogin(false)}
                            className={`flex-1 py-3 rounded-lg font-semibold transition-all ${!isLogin
                                ? 'bg-[#5E5CE6] text-white'
                                : 'bg-[#252526] text-[#888] hover:bg-[#333]'
                                }`}
                        >
                            Registrieren
                        </button>
                    </div>

                    <form onSubmit={handleSubmit} className="space-y-4">
                        <div>
                            <label className="block text-sm font-medium text-[#DDD] mb-2">
                                Benutzername
                            </label>
                            <input
                                type="text"
                                value={username}
                                onChange={(e) => setUsername(e.target.value)}
                                required
                                className="w-full px-4 py-3 bg-[#252526] border border-[#444] rounded-lg text-white placeholder-[#888] focus:outline-none focus:border-[#5E5CE6] focus:ring-2 focus:ring-[#5E5CE6]/20 transition-all"
                                placeholder="Dein Benutzername"
                            />
                        </div>

                        <div>
                            <label className="block text-sm font-medium text-[#DDD] mb-2">
                                Passwort
                            </label>
                            <input
                                type="password"
                                value={password}
                                onChange={(e) => setPassword(e.target.value)}
                                required
                                className="w-full px-4 py-3 bg-[#252526] border border-[#444] rounded-lg text-white placeholder-[#888] focus:outline-none focus:border-[#5E5CE6] focus:ring-2 focus:ring-[#5E5CE6]/20 transition-all"
                                placeholder="Dein Passwort"
                            />
                        </div>

                        {error && (
                            <div className={`p-3 rounded-lg text-sm ${error.includes('erfolgreich')
                                ? 'bg-green-500/10 text-green-400 border border-green-500/20'
                                : 'bg-red-500/10 text-red-400 border border-red-500/20'
                                }`}>
                                {error}
                            </div>
                        )}

                        <button
                            type="submit"
                            disabled={loading}
                            className="w-full py-3.5 bg-[#5E5CE6] rounded-lg text-white font-semibold hover:bg-[#7D7AFF] active:bg-[#4D4BC4] transition-all disabled:opacity-50 disabled:cursor-not-allowed flex items-center justify-center gap-2"
                        >
                            {loading ? (
                                <div className="w-5 h-5 border-2 border-white border-t-transparent rounded-full animate-spin" />
                            ) : (
                                <>
                                    {isLogin ? <LogIn size={20} /> : <UserPlus size={20} />}
                                    {isLogin ? 'Anmelden' : 'Registrieren'}
                                </>
                            )}
                        </button>
                    </form>
                </div>
            </div>
            {/* Privacy footer */}
            <p className="text-center mt-5 text-xs text-gray-600">
                Mit der Nutzung stimmst du unserer{' '}
                <Link href="/datenschutz" className="text-gray-500 hover:text-[#5E5CE6] underline underline-offset-2 transition-colors inline-flex items-center gap-1">
                    <Shield size={10} /> Datenschutzerklärung
                </Link>
                {' '}zu.
            </p>
        </div>
    );
}
