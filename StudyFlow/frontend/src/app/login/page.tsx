"use client";

import React, { useState } from 'react';
import Link from 'next/link';
import { useRouter } from 'next/navigation';
import { LogIn, UserPlus, Shield } from 'lucide-react';
import Script from 'next/script';
import Image from 'next/image';

export default function LoginPage() {
    const router = useRouter();
    const [isLogin, setIsLogin] = useState(true);
    const [username, setUsername] = useState('');
    const [email, setEmail] = useState('');
    const [password, setPassword] = useState('');
    const [error, setError] = useState('');
    const [loading, setLoading] = useState(false);
    const [isNativeApp, setIsNativeApp] = useState(false);

    const API_BASE = '/api';

    // Setup Auth Context
    React.useEffect(() => {
        // Clear stale session on login page load
        localStorage.removeItem('session_id');
        localStorage.removeItem('username');

        // Check if inside Qt Native App (injected by QWebEngine)
        const checkNative = setInterval(() => {
            if ((window as any).isBlopNativeApp) {
                setIsNativeApp(true);
                clearInterval(checkNative);
            }
        }, 100);
        setTimeout(() => clearInterval(checkNative), 3000);

        // Global callback for Google Web Login GIS
        (window as any).handleGoogleLoginSuccess = async (response: any) => {
            try {
                const res = await fetch(`${API_BASE}/auth/google/verify`, {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ token: response.credential })
                });
                const data = await res.json();
                if (res.ok) {
                    localStorage.setItem('session_id', data.session_id);
                    localStorage.setItem('username', data.username);
                    router.push('/');
                } else {
                    setError('Google Login fehlgeschlagen: ' + (data.detail || 'Unbekannter Fehler'));
                }
            } catch (err: any) {
                setError('Verbindungsfehler bei Google Login');
            }
        };

        return () => clearInterval(checkNative);
    }, [router]);

    const handleSubmit = async (e: React.FormEvent) => {
        e.preventDefault();
        setError('');
        setLoading(true);

        try {
            const endpoint = isLogin ? '/auth/login' : '/auth/register';
            const payload = isLogin ? { username, password } : { username, email, password };
            
            const res = await fetch(`${API_BASE}${endpoint}`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload),
            });

            // Try to parse JSON safely, since 500 errors might be returned as plain text by the proxy
            const textData = await res.text();
            let data;
            try {
                data = JSON.parse(textData);
            } catch (parseError) {
                // If parsing fails but the response is not OK, the text is the error message
                data = { detail: res.ok ? 'Unbekannter Fehler' : (textData || 'Server Fehler (500)') };
            }

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
                setError('Erfolgreich! Bitte bestätige den Link in deiner E-Mail, bevor du dich anmeldest.');
            }
        } catch (err: any) {
            setError(err.message || 'Ein Fehler ist aufgetreten');
        } finally {
            setLoading(false);
        }
    };

    return (
        <div className="min-h-screen bg-[#1e1e1e] flex flex-col items-center justify-center p-4">
            <div className="w-full max-w-md">
                {/* Logo */}
                <div className="text-center mb-8">
                    <div className="w-20 h-20 rounded-2xl bg-[#5E5CE6] flex items-center justify-center mx-auto mb-4 overflow-hidden shadow-2xl shadow-[#5E5CE6]/20">
                        <Image src="/logo.jpg" alt="Blop Logo" width={80} height={80} className="object-cover w-full h-full" />
                    </div>
                    <h1 className="text-3xl font-bold text-white mb-2">Blop</h1>
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
                                {isLogin ? 'E-Mail oder Benutzername' : 'Benutzername'}
                            </label>
                            <input
                                type="text"
                                value={username}
                                onChange={(e) => setUsername(e.target.value)}
                                required
                                className="w-full px-4 py-3 bg-[#252526] border border-[#444] rounded-lg text-white placeholder-[#888] focus:outline-none focus:border-[#5E5CE6] focus:ring-2 focus:ring-[#5E5CE6]/20 transition-all"
                                placeholder={isLogin ? 'Deine E-Mail oder dein Benutzername' : 'Dein sichtbarer Benutzername'}
                            />
                        </div>

                        {!isLogin && (
                            <div>
                                <label className="block text-sm font-medium text-[#DDD] mb-2">
                                    E-Mail Adresse
                                </label>
                                <input
                                    type="email"
                                    value={email}
                                    onChange={(e) => setEmail(e.target.value)}
                                    required
                                    className="w-full px-4 py-3 bg-[#252526] border border-[#444] rounded-lg text-white placeholder-[#888] focus:outline-none focus:border-[#5E5CE6] focus:ring-2 focus:ring-[#5E5CE6]/20 transition-all"
                                    placeholder="Deine E-Mail Adresse"
                                />
                            </div>
                        )}

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
                        
                        {isLogin && (
                            <>
                                <div className="flex items-center gap-4 my-2 mt-4">
                                    <div className="h-px bg-[#444] flex-1"></div>
                                    <span className="text-[#888] text-sm">oder</span>
                                    <div className="h-px bg-[#444] flex-1"></div>
                                </div>
                                
                                {isNativeApp ? (
                                    <button
                                        type="button"
                                        onClick={() => localStorage.setItem('trigger_google_login', '1')}
                                        className="w-full py-3.5 bg-white text-gray-800 rounded-lg font-semibold hover:bg-gray-100 active:bg-gray-200 transition-all flex items-center justify-center gap-2 border border-gray-300"
                                    >
                                        <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 48 48" width="20px" height="20px">
                                            <path fill="#EA4335" d="M24 9.5c3.54 0 6.71 1.22 9.21 3.6l6.85-6.85C35.9 2.38 30.47 0 24 0 14.62 0 6.51 5.38 2.56 13.22l7.98 6.19C12.43 13.72 17.74 9.5 24 9.5z"/>
                                            <path fill="#4285F4" d="M46.98 24.55c0-1.57-.15-3.09-.38-4.55H24v9.02h12.94c-.58 2.96-2.26 5.48-4.78 7.18l7.73 6c4.51-4.14 7.09-10.36 7.09-17.65z"/>
                                            <path fill="#FBBC05" d="M10.53 28.59c-.48-1.45-.76-2.99-.76-4.59s.27-3.14.76-4.59l-7.98-6.19C.92 16.46 0 20.12 0 24c0 3.88.92 7.54 2.56 10.78l7.97-6.19z"/>
                                            <path fill="#34A853" d="M24 48c6.48 0 11.93-2.13 15.89-5.81l-7.73-6c-2.15 1.45-4.92 2.3-8.16 2.3-6.26 0-11.57-4.22-13.47-9.91l-7.98 6.19C6.51 42.62 14.62 48 24 48z"/>
                                        </svg>
                                        Über Google anmelden
                                    </button>
                                ) : (
                                    <div className="w-full flex flex-col items-center">
                                        <Script src="https://accounts.google.com/gsi/client" async defer strategy="lazyOnload" />
                                        <div id="g_id_onload"
                                             data-client_id={process.env.NEXT_PUBLIC_GOOGLE_CLIENT_ID || "BITTE_WEB_CLIENT_ID_IN_VERCEL_HINTERLEGEN.apps.googleusercontent.com"}
                                             data-context="signin"
                                             data-ux_mode="popup"
                                             data-callback="handleGoogleLoginSuccess"
                                             data-auto_prompt="false">
                                        </div>

                                        <div className="g_id_signin w-full"
                                             data-type="standard"
                                             data-shape="rectangular"
                                             data-theme="outline"
                                             data-text="signin_with"
                                             data-size="large"
                                             data-logo_alignment="center">
                                        </div>
                                    </div>
                                )}
                            </>
                        )}
                    </form>
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
        </div>
    );
}
