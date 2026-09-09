"use client";

import React, { useState, useEffect } from 'react';
import { Trash2, AlertTriangle, Loader2, LogOut, CreditCard, Calendar, ArrowUpRight } from 'lucide-react';
import { useRouter } from 'next/navigation';
import { fetchSubscriptionStatus, syncStripeSubscription, cancelStripeSubscription, confirmStripeCheckout, createStripePortal, SubscriptionStatus } from '@/lib/subscription';

export default function Settings() {
    const router = useRouter();
    const [username, setUsername] = useState("");
    const [accountEmail, setAccountEmail] = useState<string>("");
    const [loading, setLoading] = useState(false);

    // Delete State
    const [isDeleteOpen, setIsDeleteOpen] = useState(false);
    const [deleteConfirmation, setDeleteConfirmation] = useState("");
    const [deleteError, setDeleteError] = useState("");

    // Auth Token Stats
    const [tokens, setTokens] = useState<number | null>(null);
    const [tier, setTier] = useState<string>("free");
    const [subscriptionStatus, setSubscriptionStatus] = useState<SubscriptionStatus | null>(null);
    const [portalLoading, setPortalLoading] = useState(false);
    const [cancelLoading, setCancelLoading] = useState(false);
    const [isCancelOpen, setIsCancelOpen] = useState(false);
    const [upgradeStatus, setUpgradeStatus] = useState<{message: string, isError: boolean} | null>(null);
    const [preferredModel, setPreferredModel] = useState<string>("");
    const [savingModel, setSavingModel] = useState(false);

    const API_BASE = '/api';

    useEffect(() => {
        const user = localStorage.getItem("username");
        if (!user) return;
        setUsername(user);

        const params = new URLSearchParams(window.location.search);
        const checkoutSessionId = params.get("checkout_session_id");
        if (params.get("subscription") === "success" && checkoutSessionId) {
            setUpgradeStatus({ message: "Zahlung wird bestätigt…", isError: false });
            confirmStripeCheckout(checkoutSessionId)
                .catch(() => syncStripeSubscription())
                .then((result) => {
                    if ('found' in result && !result.found) {
                        throw new Error('Stripe hat für diesen Blop-Account kein aktives Abo gefunden.');
                    }
                    setUpgradeStatus({ message: "Dein Abo ist jetzt aktiv.", isError: false });
                    window.history.replaceState({}, "", "/settings");
                    return Promise.all([fetchUserInfo(user), loadSubscriptionStatus()]);
                })
                .catch((err: any) => {
                    setUpgradeStatus({ message: err?.message || "Zahlung konnte noch nicht bestätigt werden.", isError: true });
                });
            return;
        }

        setUpgradeStatus({ message: "Abo-Status wird mit Stripe abgeglichen…", isError: false });
        syncStripeSubscription()
            .then(() => Promise.all([fetchUserInfo(user), loadSubscriptionStatus()]))
            .then(() => setUpgradeStatus(null))
            .catch((err: any) => {
                setUpgradeStatus({ message: err?.message || "Stripe-Abgleich fehlgeschlagen.", isError: true });
                fetchUserInfo(user);
                loadSubscriptionStatus();
            });
    }, []);

    const fetchUserInfo = async (user: string) => {
        try {
            const sid = localStorage.getItem("session_id") || "";
            const res = await fetch(`${API_BASE}/user/${user}?session_id=${encodeURIComponent(sid)}`);
            if (res.ok) {
                const data = await res.json();
                setTokens(data.tokens);
                setTier(data.subscription_tier);
                setPreferredModel(data.preferred_model || "");
                if (typeof data.email === "string") setAccountEmail(data.email);
            }
        } catch (error) {
            console.error("Error fetching user info:", error);
        }
    };

    const loadSubscriptionStatus = async () => {
        try {
            const status = await fetchSubscriptionStatus();
            setSubscriptionStatus(status);
            setTier(status.subscription.tier);
        } catch (error) {
            console.error("Error fetching subscription status:", error);
        }
    };

    const openCustomerPortal = async () => {
        setPortalLoading(true);
        setUpgradeStatus(null);
        try {
            const { url } = await createStripePortal();
            window.location.href = url;
        } catch (err: any) {
            setUpgradeStatus({ message: err?.message || "Portal konnte nicht geöffnet werden.", isError: true });
            setPortalLoading(false);
        }
    };

    const cancelSubscription = async () => {
        setCancelLoading(true);
        setUpgradeStatus(null);
        try {
            await cancelStripeSubscription();
            setIsCancelOpen(false);
            setUpgradeStatus({ message: "Dein Abo wurde zum Ende des Abrechnungszeitraums gekündigt.", isError: false });
            await loadSubscriptionStatus();
        } catch (err: any) {
            setUpgradeStatus({ message: err?.message || "Abo konnte nicht gekündigt werden.", isError: true });
        } finally {
            setCancelLoading(false);
        }
    };

    const savePreferredModel = async () => {
        if (!username) return;
        setSavingModel(true);
        try {
            const res = await fetch(`${API_BASE}/user/model-preference`, {
                method: "PUT",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify({ username, preferred_model: preferredModel })
            });
            const data = await res.json();
            if (!res.ok) {
                setUpgradeStatus({ message: data.detail || "Modell konnte nicht gespeichert werden.", isError: true });
                return;
            }
            setUpgradeStatus({ message: "Standardmodell gespeichert.", isError: false });
        } catch {
            setUpgradeStatus({ message: "Verbindungsfehler beim Speichern des Modells.", isError: true });
        } finally {
            setSavingModel(false);
        }
    };


    const handleDeleteAccount = async (e: React.FormEvent) => {
        e.preventDefault();
        if (deleteConfirmation.trim() !== "Konto Löschen") {
            setDeleteError("Bitte gib exakt 'Konto Löschen' ein.");
            return;
        }
        setLoading(true);
        setDeleteError("");

        try {
            const sid = localStorage.getItem("session_id") || "";
            const res = await fetch(`${API_BASE}/auth/user`, {
                method: "DELETE",
                headers: { "Content-Type": "application/json", "X-Session-Id": sid },
                body: JSON.stringify({ username, confirmation: deleteConfirmation, session_id: sid })
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
                                {accountEmail ? (
                                    <p className="text-sm text-gray-400 mt-2">
                                        <span className="text-gray-500">E-Mail:</span> {accountEmail}
                                    </p>
                                ) : null}
                                <p className="text-xs text-gray-600 mt-2 max-w-md leading-relaxed">
                                    Ist auf dem Server E-Mail (SMTP) eingerichtet, schicken wir an diese Adresse eine kurze Benachrichtigung,
                                    sobald ein KI-Dokument fertig ist (z.&nbsp;B. Lernplan, Quiz, Zusammenfassung, Wiederholung).
                                </p>
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

                {/* Subscription Section */}
                <section>
                    <h2 className="text-xl font-semibold text-white mb-4 flex items-center gap-2">
                        💎 Abonnement & Tokens
                    </h2>
                    <div className="bg-[#252526] border border-[#333] rounded-2xl p-6 space-y-4">
                        <div className="flex flex-col md:flex-row gap-6 justify-between items-start md:items-center">
                            <div className="flex-1">
                                <p className="text-gray-400 text-sm mb-4">
                                    Nutze Tokens für KI-Funktionen. Dein Abo verlängert sich automatisch und kann jederzeit gekündigt werden.
                                </p>
                                <div className="flex items-center gap-3">
                                    <span className="text-3xl font-bold text-white flex items-center gap-2">
                                        🪙 {tokens !== null ? (tokens > 900000 ? '∞' : tokens) : '...'}
                                    </span>
                                    <span className="text-sm text-gray-500 font-medium">Tokens verfügbar</span>
                                </div>
                                <div className="mt-3 flex flex-wrap items-center gap-3">
                                    <span className="inline-block px-3 py-1 bg-[#333] rounded-md text-sm text-[#5E5CE6] font-medium border border-[#444]">
                                        Aktuelles Abo: {tier.toUpperCase()}
                                    </span>
                                    {subscriptionStatus?.subscription?.provider && subscriptionStatus.subscription.provider !== 'none' && (
                                        <span className="inline-flex items-center gap-1.5 px-3 py-1 bg-[#1C1C33] rounded-md text-sm text-gray-400 font-medium border border-[#2A2A40]">
                                            <CreditCard size={14} />
                                            Zahlung über {subscriptionStatus.subscription.provider === 'stripe' ? 'Stripe' : 'PayPal'}
                                        </span>
                                    )}
                                    {subscriptionStatus?.subscription?.current_period_end && (
                                        <span className="inline-flex items-center gap-1.5 px-3 py-1 bg-[#1C1C33] rounded-md text-sm text-gray-400 font-medium border border-[#2A2A40]">
                                            <Calendar size={14} />
                                            Aktiv bis {new Date(subscriptionStatus.subscription.current_period_end).toLocaleDateString('de-DE')}
                                        </span>
                                    )}
                                </div>
                            </div>

                            <div className="flex flex-col gap-3 w-full md:w-auto shrink-0 md:min-w-[180px]">
                                {tier !== 'free' && subscriptionStatus?.subscription?.provider === 'stripe' ? (
                                    <>
                                        <button
                                            onClick={openCustomerPortal}
                                            disabled={portalLoading}
                                            className="bg-[#333] hover:bg-[#444] text-white px-6 py-2.5 rounded-xl text-sm font-medium transition-colors disabled:opacity-50 flex items-center justify-center gap-2 min-w-[180px]"
                                        >
                                            {portalLoading ? <Loader2 size={16} className="animate-spin" /> : <CreditCard size={16} />}
                                            Abo verwalten
                                        </button>
                                        <button
                                            onClick={() => setIsCancelOpen(true)}
                                            disabled={subscriptionStatus.subscription.cancel_at_period_end}
                                            className="border border-red-500/40 bg-red-500/10 hover:bg-red-500/20 text-red-400 px-6 py-2.5 rounded-xl text-sm font-medium transition-colors disabled:opacity-50 min-w-[180px]"
                                        >
                                            {subscriptionStatus.subscription.cancel_at_period_end ? 'Kündigung vorgemerkt' : 'Abo kündigen'}
                                        </button>
                                    </>
                                ) : null}
                                <button
                                    onClick={() => router.push('/pricing')}
                                    className="bg-gradient-to-r from-[#5E5CE6] to-[#7D7AFF] hover:opacity-90 text-white px-6 py-2.5 rounded-xl text-sm font-medium transition-colors flex items-center justify-center gap-2 min-w-[180px]"
                                >
                                    {tier === 'free' ? 'Upgrade wählen' : 'Abo ändern'}
                                    <ArrowUpRight size={16} />
                                </button>
                            </div>
                        </div>

                        {upgradeStatus && (
                            <div className={`mt-4 p-3 rounded-xl border text-sm text-center ${upgradeStatus.isError ? 'bg-red-500/10 border-red-500/20 text-red-400' : 'bg-green-500/10 border-green-500/20 text-green-400'}`}>
                                {upgradeStatus.message}
                            </div>
                        )}
                    </div>
                </section>

                <section>
                    <h2 className="text-xl font-semibold text-white mb-4">KI-Modell (Standard)</h2>
                    <div className="bg-[#252526] border border-[#333] rounded-2xl p-6 space-y-4">
                        <p className="text-sm text-gray-400">
                            Dieses Modell wird verwendet, wenn du in einer Funktion kein Modell explizit auswählst.
                        </p>
                        <select
                            value={preferredModel}
                            onChange={(e) => setPreferredModel(e.target.value)}
                            className="w-full bg-[#151525] border border-[#2A2A40] text-gray-200 rounded-xl px-4 py-2.5"
                        >
                            <option value="">Global: Automatisch (Backend-Auswahl)</option>
                            <option value="gemini-2.5-pro">Gemini 2.5 Pro (sehr stark, teurer)</option>
                            <option value="gemini-2.0-pro-exp">Gemini 2.0 Pro (stark)</option>
                            <option value="gemini-1.5-pro">Gemini 1.5 Pro (stark)</option>
                            <option value="gemini-2.5-flash">Gemini 2.5 Flash (schnell)</option>
                            <option value="gemini-1.5-flash">Gemini 1.5 Flash (günstig)</option>
                            <option value="gemini-2.5-flash-lite">Gemini 2.5 Flash Lite (sehr günstig)</option>
                        </select>
                        <p className="text-xs text-gray-500">
                            Tokenabzug erfolgt dynamisch nach Nutzung. Pro-Modelle ziehen in der Regel mehr Tokens ab als Flash-Modelle.
                        </p>
                        <button
                            onClick={savePreferredModel}
                            disabled={savingModel}
                            className="px-4 py-2.5 bg-[#5E5CE6] hover:bg-[#4d4ac9] text-white rounded-xl text-sm font-semibold disabled:opacity-50"
                        >
                            {savingModel ? "Speichert..." : "Standardmodell speichern"}
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

            {isCancelOpen && (
                <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/80 backdrop-blur-sm p-4">
                    <div className="bg-[#1e1e1e] border border-[#333] rounded-2xl w-full max-w-md p-8 shadow-2xl">
                        <div className="flex flex-col items-center text-center mb-6">
                            <div className="w-16 h-16 bg-red-500/10 rounded-full flex items-center justify-center mb-4 text-red-500">
                                <AlertTriangle size={32} />
                            </div>
                            <h3 className="text-2xl font-bold text-white mb-2">Abo wirklich kündigen?</h3>
                            <p className="text-gray-400 text-sm">
                                Dein Abo bleibt bis zum Ende des bereits bezahlten Zeitraums aktiv. Danach erfolgt keine weitere Abbuchung und dein Account wechselt zu Free.
                            </p>
                        </div>
                        <div className="flex gap-3">
                            <button
                                type="button"
                                onClick={() => setIsCancelOpen(false)}
                                disabled={cancelLoading}
                                className="flex-1 py-3 text-sm font-medium text-gray-300 bg-[#333] hover:bg-[#444] rounded-xl disabled:opacity-50"
                            >
                                Behalten
                            </button>
                            <button
                                type="button"
                                onClick={cancelSubscription}
                                disabled={cancelLoading}
                                className="flex-1 py-3 text-sm font-medium bg-red-600 hover:bg-red-700 text-white rounded-xl disabled:opacity-50 flex justify-center items-center gap-2"
                            >
                                {cancelLoading ? <Loader2 size={18} className="animate-spin" /> : 'Kündigung bestätigen'}
                            </button>
                        </div>
                    </div>
                </div>
            )}

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
                                <label className="block text-xs font-medium text-gray-500 mb-1.5 ml-1">
                                    Zur Bestätigung <strong className="text-white">Konto Löschen</strong> eingeben:
                                </label>
                                <input
                                    type="text"
                                    value={deleteConfirmation}
                                    onChange={(e) => setDeleteConfirmation(e.target.value)}
                                    placeholder="Konto Löschen"
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
                                    onClick={() => { setIsDeleteOpen(false); setDeleteConfirmation(""); setDeleteError(""); }}
                                    className="flex-1 py-3 text-sm font-medium text-gray-300 hover:text-white bg-[#333] hover:bg-[#444] rounded-xl transition-all"
                                >
                                    Abbrechen
                                </button>
                                <button
                                    type="submit"
                                    disabled={deleteConfirmation.trim() !== "Konto Löschen" || loading}
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
