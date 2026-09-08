"use client";

import React, { useEffect, useState } from 'react';
import { useRouter } from 'next/navigation';
import { Check, Loader2, LogIn, Sparkles, Zap, Crown } from 'lucide-react';
import { motion } from 'framer-motion';
import {
    fetchTiers,
    fetchSubscriptionStatus,
    createStripeCheckout,
    createPayPalOrder,
    capturePayPalOrder,
    FEATURE_LABELS,
    Tier,
    SubscriptionStatus,
} from '@/lib/subscription';

const BILLING_INTERVALS = [
    { value: 'month', label: 'Monatlich' },
    { value: 'year', label: 'Jährlich', badge: 'Sparen' },
];

const TIER_ICONS: Record<string, React.ReactNode> = {
    free: <Sparkles size={24} />,
    pro: <Zap size={24} />,
    premium: <Crown size={24} />,
};

const TIER_COLORS: Record<string, string> = {
    free: 'from-gray-500 to-gray-400',
    pro: 'from-blue-500 to-indigo-500',
    premium: 'from-amber-400 to-orange-500',
};

export default function PricingPage() {
    const router = useRouter();
    const [tiers, setTiers] = useState<Tier[]>([]);
    const [status, setStatus] = useState<SubscriptionStatus | null>(null);
    const [interval, setInterval] = useState<'month' | 'year'>('month');
    const [loading, setLoading] = useState(true);
    const [error, setError] = useState('');
    const [sessionExpired, setSessionExpired] = useState(false);
    const [isAuthenticated, setIsAuthenticated] = useState(false);
    const [checkoutTier, setCheckoutTier] = useState<string | null>(null);
    const [paypalProvider, setPaypalProvider] = useState<'stripe' | 'paypal' | null>(null);

    useEffect(() => {
        loadData();

        // Handle PayPal return after approval (client-side only).
        if (typeof window === 'undefined') return;
        const params = new URLSearchParams(window.location.search);
        const paypal = params.get('paypal');
        const orderId = params.get('token');
        if (paypal === 'success' && orderId) {
            handlePayPalCapture(orderId);
        } else if (paypal === 'canceled') {
            setError('PayPal-Zahlung abgebrochen.');
        }
    }, []);

    async function loadData() {
        try {
            const tiersData = await fetchTiers();
            setTiers(tiersData);
        } catch (err: any) {
            setError(err?.message || 'Preise konnten nicht geladen werden.');
            setLoading(false);
            return;
        }

        // Subscription status is optional; only load if user is logged in.
        const username = localStorage.getItem('username');
        const sid = localStorage.getItem('session_id');
        const hasSession = Boolean(username && sid);
        setIsAuthenticated(hasSession);
        if (hasSession) {
            try {
                const statusData = await fetchSubscriptionStatus();
                setStatus(statusData);
            } catch {
                // If status fails, treat it as a stale session but still show public pricing.
                setStatus(null);
                setSessionExpired(true);
            }
        }
        setLoading(false);
    }

    async function handlePayPalCapture(orderId: string) {
        setLoading(true);
        try {
            await capturePayPalOrder(orderId);
            router.replace('/settings?subscription=success');
        } catch (err: any) {
            setError(err?.message || 'PayPal-Zahlung konnte nicht abgeschlossen werden.');
        } finally {
            setLoading(false);
        }
    }

    async function handleStripeCheckout(tier: string) {
        setCheckoutTier(tier);
        setPaypalProvider('stripe');
        try {
            const { url } = await createStripeCheckout(tier, interval);
            window.location.href = url;
        } catch (err: any) {
            setError(err?.message || 'Stripe Checkout fehlgeschlagen.');
            setCheckoutTier(null);
            setPaypalProvider(null);
        }
    }

    async function handlePayPalCheckout(tier: string) {
        setCheckoutTier(tier);
        setPaypalProvider('paypal');
        try {
            const { approval_url } = await createPayPalOrder(tier, interval);
            window.location.href = approval_url;
        } catch (err: any) {
            setError(err?.message || 'PayPal Checkout fehlgeschlagen.');
            setCheckoutTier(null);
            setPaypalProvider(null);
        }
    }

    function isCurrentTier(tierName: string) {
        return status?.subscription?.tier === tierName;
    }

    if (loading) {
        return (
            <div className="min-h-screen bg-[#0B0B1A] flex items-center justify-center">
                <Loader2 className="animate-spin text-[#5E5CE6]" size={32} />
            </div>
        );
    }

    return (
        <div className="min-h-screen bg-[#0B0B1A] text-white py-16 px-6">
            <div className="max-w-6xl mx-auto">
                <div className="text-center mb-12">
                    <h1 className="text-4xl font-bold mb-4">Wähle dein Abo</h1>
                    <p className="text-gray-400 text-lg max-w-2xl mx-auto">
                        Mehr Tokens, mehr KI-Funktionen. Monatlich kündbar.
                    </p>
                </div>

                {error && (
                    <div className="max-w-xl mx-auto mb-8 p-4 rounded-xl bg-red-500/10 border border-red-500/20 text-red-400 text-center">
                        {error}
                    </div>
                )}

                {sessionExpired && (
                    <div className="max-w-xl mx-auto mb-8 p-4 rounded-xl bg-amber-500/10 border border-amber-500/20 text-amber-400 text-center text-sm">
                        Deine Sitzung ist abgelaufen. Melde dich neu an, um ein Abo zu buchen.
                        <div className="mt-3">
                            <button
                                onClick={() => router.push('/login')}
                                className="inline-flex items-center gap-2 px-4 py-2 rounded-lg bg-[#5E5CE6] text-white text-sm font-medium hover:bg-[#4d4ac9]"
                            >
                                <LogIn size={16} />
                                Anmelden
                            </button>
                        </div>
                    </div>
                )}

                {/* Billing toggle */}
                <div className="flex justify-center mb-12">
                    <div className="bg-[#151525] p-1 rounded-xl inline-flex border border-[#2A2A40]">
                        {BILLING_INTERVALS.map((opt) => (
                            <button
                                key={opt.value}
                                onClick={() => setInterval(opt.value as 'month' | 'year')}
                                className={`relative px-6 py-2.5 rounded-lg text-sm font-semibold transition-all ${
                                    interval === opt.value
                                        ? 'bg-[#5E5CE6] text-white shadow-lg'
                                        : 'text-gray-400 hover:text-white'
                                }`}
                            >
                                {opt.label}
                                {opt.badge && interval === opt.value && (
                                    <span className="ml-2 text-[10px] bg-white/20 px-1.5 py-0.5 rounded">{opt.badge}</span>
                                )}
                            </button>
                        ))}
                    </div>
                </div>

                {/* Cards */}
                <div className="grid grid-cols-1 md:grid-cols-3 gap-6">
                    {tiers.map((tier, idx) => {
                        const price = interval === 'year' ? tier.price_yearly_eur : tier.price_monthly_eur;
                        const isCurrent = isCurrentTier(tier.name);
                        const features = Object.entries(tier.features).sort(
                            ([, a], [, b]) => (b ? 1 : 0) - (a ? 1 : 0)
                        );
                        return (
                            <motion.div
                                key={tier.name}
                                initial={{ opacity: 0, y: 20 }}
                                animate={{ opacity: 1, y: 0 }}
                                transition={{ delay: idx * 0.1 }}
                                className={`relative rounded-2xl border p-6 flex flex-col ${
                                    tier.name === 'premium'
                                        ? 'border-amber-500/40 bg-gradient-to-b from-[#1C1C33] to-[#151525] shadow-xl shadow-amber-500/10'
                                        : 'border-[#2A2A40] bg-[#151525]'
                                }`}
                            >
                                {tier.name === 'premium' && (
                                    <div className="absolute -top-3 left-1/2 -translate-x-1/2 px-3 py-1 bg-gradient-to-r from-amber-500 to-orange-500 rounded-full text-xs font-bold text-white shadow-lg">
                                        Beliebt
                                    </div>
                                )}
                                <div className="mb-4">
                                    <div
                                        className={`inline-flex items-center justify-center w-12 h-12 rounded-xl bg-gradient-to-br ${
                                            TIER_COLORS[tier.name] || TIER_COLORS.free
                                        } text-white mb-4`}
                                    >
                                        {TIER_ICONS[tier.name] || <Sparkles size={24} />}
                                    </div>
                                    <h3 className="text-2xl font-bold">{tier.display_name}</h3>
                                </div>

                                <div className="mb-6">
                                    <span className="text-4xl font-bold">{price.toFixed(2)} €</span>
                                    <span className="text-gray-400 text-sm">/{interval === 'year' ? 'Jahr' : 'Monat'}</span>
                                </div>

                                <div className="mb-6">
                                    <p className="text-sm text-gray-400 mb-2">
                                        {tier.tokens_monthly} Tokens / Monat
                                    </p>
                                </div>

                                <ul className="space-y-3 mb-8 flex-1">
                                    {features.map(([key, allowed]) => (
                                        <li key={key} className="flex items-start gap-3 text-sm">
                                            <span
                                                className={`mt-0.5 ${
                                                    allowed ? 'text-green-400' : 'text-gray-600'
                                                }`}
                                            >
                                                <Check size={16} />
                                            </span>
                                            <span className={allowed ? 'text-gray-200' : 'text-gray-500 line-through'}>
                                                {FEATURE_LABELS[key] || key}
                                            </span>
                                        </li>
                                    ))}
                                </ul>

                                {isCurrent ? (
                                    <button
                                        disabled
                                        className="w-full py-3 rounded-xl bg-[#2A2A40] text-gray-400 font-semibold cursor-default"
                                    >
                                        Aktuelles Abo
                                    </button>
                                ) : !isAuthenticated || sessionExpired ? (
                                    <button
                                        onClick={() => router.push('/login')}
                                        className="w-full py-3 rounded-xl bg-[#2A2A40] hover:bg-[#333] text-white font-semibold transition-colors flex items-center justify-center gap-2"
                                    >
                                        <LogIn size={18} />
                                        Anmelden zum Buchen
                                    </button>
                                ) : tier.name === 'free' ? (
                                    <button
                                        onClick={() => router.push('/login')}
                                        className="w-full py-3 rounded-xl bg-[#2A2A40] hover:bg-[#333] text-white font-semibold transition-colors"
                                    >
                                        Kostenlos starten
                                    </button>
                                ) : (
                                    <div className="space-y-3">
                                        <button
                                            onClick={() => handleStripeCheckout(tier.name)}
                                            disabled={checkoutTier !== null}
                                            className={`w-full py-3 rounded-xl font-semibold transition-all flex items-center justify-center gap-2 ${
                                                tier.name === 'premium'
                                                    ? 'bg-gradient-to-r from-amber-500 to-orange-500 hover:opacity-90 text-white shadow-lg shadow-orange-500/20'
                                                    : 'bg-[#5E5CE6] hover:bg-[#4d4ac9] text-white'
                                            }`}
                                        >
                                            {checkoutTier === tier.name && paypalProvider === 'stripe' ? (
                                                <Loader2 size={18} className="animate-spin" />
                                            ) : null}
                                            Mit Karte / Google Pay / Klarna
                                        </button>
                                        <button
                                            onClick={() => handlePayPalCheckout(tier.name)}
                                            disabled={checkoutTier !== null}
                                            className="w-full py-3 rounded-xl bg-[#003087] hover:bg-[#00246b] text-white font-semibold transition-all flex items-center justify-center gap-2"
                                        >
                                            {checkoutTier === tier.name && paypalProvider === 'paypal' ? (
                                                <Loader2 size={18} className="animate-spin" />
                                            ) : null}
                                            Mit PayPal
                                        </button>
                                    </div>
                                )}
                            </motion.div>
                        );
                    })}
                </div>
            </div>
        </div>
    );
}
