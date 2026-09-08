"use client";

import React, { useEffect, useMemo, useState } from 'react';
import { useRouter } from 'next/navigation';
import { Loader2, Shield, Users, Layers, ToggleRight, Search, Save, Pencil, Trash2, Plus, Check, X } from 'lucide-react';
import { motion } from 'framer-motion';
import {
    adminListTiers,
    adminListSubscriptions,
    adminSetSubscription,
    adminUpsertTier,
    adminSetFeature,
    adminDeleteTier,
    fetchUserInfo,
    FEATURE_LABELS,
    Tier,
    AdminSubscription,
    TierInput,
} from '@/lib/subscription';

const ALL_FEATURES = Object.keys(FEATURE_LABELS);

export default function AdminSubscriptionsPage() {
    const router = useRouter();
    const [isAdmin, setIsAdmin] = useState<boolean | null>(null);
    const [loading, setLoading] = useState(true);
    const [error, setError] = useState('');
    const [activeTab, setActiveTab] = useState<'subscriptions' | 'tiers' | 'features'>('subscriptions');

    const [tiers, setTiers] = useState<Tier[]>([]);
    const [subscriptions, setSubscriptions] = useState<AdminSubscription[]>([]);
    const [refresh, setRefresh] = useState(0);
    const [initialSearch, setInitialSearch] = useState('');

    useEffect(() => {
        // Read ?user=... from URL to prefill search.
        if (typeof window !== 'undefined') {
            const params = new URLSearchParams(window.location.search);
            const userParam = params.get('user') || '';
            if (userParam) {
                setInitialSearch(userParam);
                setActiveTab('subscriptions');
            }
        }

        const username = localStorage.getItem('username') || '';
        if (!username) {
            router.replace('/login');
            return;
        }
        fetchUserInfo(username)
            .then((info) => {
                if (!info.is_admin) {
                    router.replace('/');
                    return;
                }
                setIsAdmin(true);
                loadData();
            })
            .catch(() => router.replace('/login'));
    }, [refresh, router]);

    async function loadData() {
        setLoading(true);
        setError('');
        try {
            const username = localStorage.getItem('username') || '';
            const [tiersData, subsData] = await Promise.all([
                adminListTiers(username),
                adminListSubscriptions(username),
            ]);
            setTiers(tiersData);
            setSubscriptions(subsData);
        } catch (err: any) {
            setError(err?.message || 'Daten konnten nicht geladen werden.');
        } finally {
            setLoading(false);
        }
    }

    if (isAdmin === null) {
        return (
            <div className="min-h-screen bg-[#0B0B1A] flex items-center justify-center">
                <Loader2 className="animate-spin text-[#5E5CE6]" size={32} />
            </div>
        );
    }

    return (
        <div className="min-h-screen bg-[#0B0B1A] text-white p-6 md:p-10">
            <div className="max-w-6xl mx-auto">
                <div className="flex items-center gap-3 mb-8">
                    <div className="p-2.5 bg-[#5E5CE6]/20 rounded-xl text-[#5E5CE6]">
                        <Shield size={24} />
                    </div>
                    <div>
                        <h1 className="text-2xl font-bold">Admin: Abonnements</h1>
                        <p className="text-sm text-gray-400">Tiers, Features und User-Abos verwalten</p>
                    </div>
                </div>

                {error && (
                    <div className="mb-6 p-4 rounded-xl bg-red-500/10 border border-red-500/20 text-red-400 text-sm">
                        {error}
                    </div>
                )}

                <div className="flex gap-2 mb-8 border-b border-[#2A2A40]">
                    {[
                        { id: 'subscriptions', label: 'User-Abos', icon: Users },
                        { id: 'tiers', label: 'Tiers', icon: Layers },
                        { id: 'features', label: 'Features', icon: ToggleRight },
                    ].map((tab) => (
                        <button
                            key={tab.id}
                            onClick={() => setActiveTab(tab.id as any)}
                            className={`flex items-center gap-2 px-5 py-3 text-sm font-medium border-b-2 transition-colors ${
                                activeTab === tab.id
                                    ? 'border-[#5E5CE6] text-[#5E5CE6]'
                                    : 'border-transparent text-gray-400 hover:text-white'
                            }`}
                        >
                            <tab.icon size={16} />
                            {tab.label}
                        </button>
                    ))}
                </div>

                {loading ? (
                    <div className="flex items-center justify-center py-20">
                        <Loader2 className="animate-spin text-[#5E5CE6]" size={32} />
                    </div>
                ) : (
                    <>
                        {activeTab === 'subscriptions' && (
                            <SubscriptionsPanel subscriptions={subscriptions} tiers={tiers} initialSearch={initialSearch} onRefresh={() => setRefresh((r) => r + 1)} onError={setError} />
                        )}
                        {activeTab === 'tiers' && (
                            <TiersPanel tiers={tiers} onRefresh={() => setRefresh((r) => r + 1)} onError={setError} />
                        )}
                        {activeTab === 'features' && (
                            <FeaturesPanel tiers={tiers} onRefresh={() => setRefresh((r) => r + 1)} onError={setError} />
                        )}
                    </>
                )}
            </div>
        </div>
    );
}

// --- Subscriptions panel ---

function SubscriptionsPanel({
    subscriptions,
    tiers,
    initialSearch,
    onRefresh,
    onError,
}: {
    subscriptions: AdminSubscription[];
    tiers: Tier[];
    initialSearch?: string;
    onRefresh: () => void;
    onError: (msg: string) => void;
}) {
    const [search, setSearch] = useState(initialSearch || '');
    const [editing, setEditing] = useState<AdminSubscription | null>(null);
    const [saving, setSaving] = useState(false);

    const filtered = useMemo(() => {
        return subscriptions.filter((s) => s.username.toLowerCase().includes(search.toLowerCase()));
    }, [subscriptions, search]);

    async function saveSubscription(months: number, tier: string) {
        if (!editing) return;
        setSaving(true);
        try {
            await adminSetSubscription(editing.username, tier, months);
            setEditing(null);
            onRefresh();
        } catch (err: any) {
            onError(err?.message || 'Speichern fehlgeschlagen.');
        } finally {
            setSaving(false);
        }
    }

    return (
        <div className="space-y-4">
            <div className="relative">
                <Search className="absolute left-3 top-1/2 -translate-y-1/2 text-gray-500" size={18} />
                <input
                    type="text"
                    value={search}
                    onChange={(e) => setSearch(e.target.value)}
                    placeholder="User suchen..."
                    className="w-full md:w-80 bg-[#151525] border border-[#2A2A40] rounded-xl pl-10 pr-4 py-2.5 text-sm text-white placeholder-gray-500 focus:outline-none focus:border-[#5E5CE6]"
                />
            </div>

            <div className="bg-[#151525] border border-[#2A2A40] rounded-2xl overflow-hidden">
                <table className="w-full text-sm">
                    <thead className="bg-[#1C1C33] text-gray-400">
                        <tr>
                            <th className="text-left px-4 py-3 font-medium">User</th>
                            <th className="text-left px-4 py-3 font-medium">Tier</th>
                            <th className="text-left px-4 py-3 font-medium">Status</th>
                            <th className="text-left px-4 py-3 font-medium">Provider</th>
                            <th className="text-left px-4 py-3 font-medium">Aktiv bis</th>
                            <th className="text-right px-4 py-3 font-medium">Aktion</th>
                        </tr>
                    </thead>
                    <tbody className="divide-y divide-[#2A2A40]">
                        {filtered.map((sub) => (
                            <tr key={sub.username} className="hover:bg-[#1C1C33]/50">
                                <td className="px-4 py-3 font-medium">{sub.username}</td>
                                <td className="px-4 py-3">
                                    <span className="px-2 py-1 rounded-md bg-[#2A2A40] text-xs">{sub.tier}</span>
                                </td>
                                <td className="px-4 py-3">{sub.status}</td>
                                <td className="px-4 py-3">{sub.provider}</td>
                                <td className="px-4 py-3 text-gray-400">
                                    {sub.current_period_end
                                        ? new Date(sub.current_period_end).toLocaleDateString('de-DE')
                                        : '-'}
                                </td>
                                <td className="px-4 py-3 text-right">
                                    <button
                                        onClick={() => setEditing(sub)}
                                        className="text-[#5E5CE6] hover:text-white text-sm font-medium"
                                    >
                                        Bearbeiten
                                    </button>
                                </td>
                            </tr>
                        ))}
                    </tbody>
                </table>
                {filtered.length === 0 && (
                    <div className="p-8 text-center text-gray-500 text-sm">Keine Abos gefunden.</div>
                )}
            </div>

            {editing && (
                <EditSubscriptionDialog
                    subscription={editing}
                    tiers={tiers}
                    onClose={() => setEditing(null)}
                    onSave={saveSubscription}
                    saving={saving}
                />
            )}
        </div>
    );
}

function EditSubscriptionDialog({
    subscription,
    tiers,
    onClose,
    onSave,
    saving,
}: {
    subscription: AdminSubscription;
    tiers: Tier[];
    onClose: () => void;
    onSave: (months: number, tier: string) => void;
    saving: boolean;
}) {
    const [months, setMonths] = useState(1);
    const [tier, setTier] = useState(subscription.tier);

    return (
        <div className="fixed inset-0 bg-black/60 backdrop-blur-sm z-50 flex items-center justify-center p-4">
            <motion.div
                initial={{ opacity: 0, scale: 0.95 }}
                animate={{ opacity: 1, scale: 1 }}
                className="bg-[#151525] border border-[#2A2A40] rounded-2xl p-6 w-full max-w-md"
            >
                <h3 className="text-lg font-bold mb-4">Abo bearbeiten: {subscription.username}</h3>
                <div className="space-y-4">
                    <div>
                        <label className="block text-xs text-gray-400 mb-1.5">Tier</label>
                        <select
                            value={tier}
                            onChange={(e) => setTier(e.target.value)}
                            className="w-full bg-[#0B0B1A] border border-[#2A2A40] rounded-xl px-3 py-2 text-sm text-white focus:outline-none focus:border-[#5E5CE6]"
                        >
                            {tiers.map((t) => (
                                <option key={t.name} value={t.name}>
                                    {t.display_name} ({t.tokens_monthly} Tokens/Monat)
                                </option>
                            ))}
                        </select>
                    </div>
                    <div>
                        <label className="block text-xs text-gray-400 mb-1.5">Laufzeit (Monate)</label>
                        <input
                            type="number"
                            min={1}
                            value={months}
                            onChange={(e) => setMonths(parseInt(e.target.value || '1', 10))}
                            className="w-full bg-[#0B0B1A] border border-[#2A2A40] rounded-xl px-3 py-2 text-sm text-white focus:outline-none focus:border-[#5E5CE6]"
                        />
                    </div>
                </div>
                <div className="flex gap-3 mt-6">
                    <button
                        onClick={onClose}
                        className="flex-1 py-2.5 rounded-xl border border-[#2A2A40] text-sm font-medium text-gray-300 hover:bg-[#2A2A40]"
                    >
                        Abbrechen
                    </button>
                    <button
                        onClick={() => onSave(months, tier)}
                        disabled={saving}
                        className="flex-1 py-2.5 rounded-xl bg-[#5E5CE6] text-sm font-medium text-white hover:bg-[#4d4ac9] disabled:opacity-50 flex items-center justify-center gap-2"
                    >
                        {saving ? <Loader2 size={16} className="animate-spin" /> : <Save size={16} />}
                        Speichern
                    </button>
                </div>
            </motion.div>
        </div>
    );
}

// --- Tiers panel ---

function TiersPanel({ tiers, onRefresh, onError }: { tiers: Tier[]; onRefresh: () => void; onError: (msg: string) => void }) {
    const [editing, setEditing] = useState<Tier | null>(null);
    const [isCreating, setIsCreating] = useState(false);

    return (
        <div className="space-y-4">
            <div className="flex justify-end">
                <button
                    onClick={() => setIsCreating(true)}
                    className="flex items-center gap-2 px-4 py-2 rounded-xl bg-[#5E5CE6] text-sm font-medium text-white hover:bg-[#4d4ac9]"
                >
                    <Plus size={16} />
                    Neues Tier
                </button>
            </div>

            <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
                {tiers.map((tier) => (
                    <div key={tier.name} className="bg-[#151525] border border-[#2A2A40] rounded-2xl p-5">
                        <div className="flex justify-between items-start mb-4">
                            <div>
                                <h3 className="font-bold text-lg">{tier.display_name}</h3>
                                <p className="text-xs text-gray-400">Name: {tier.name}</p>
                            </div>
                            <div className="flex gap-2">
                                <button
                                    onClick={() => setEditing(tier)}
                                    className="p-2 rounded-lg hover:bg-[#2A2A40] text-gray-400 hover:text-white"
                                    title="Bearbeiten"
                                >
                                    <Pencil size={16} />
                                </button>
                                {!['free', 'pro', 'premium', 'custom'].includes(tier.name) && (
                                    <DeleteTierButton tierName={tier.name} onRefresh={onRefresh} onError={onError} />
                                )}
                            </div>
                        </div>
                        <div className="space-y-2 text-sm text-gray-300">
                            <p>Monat: {tier.price_monthly_eur.toFixed(2)} €</p>
                            <p>Jahr: {tier.price_yearly_eur.toFixed(2)} €</p>
                            <p>Tokens/Monat: {tier.tokens_monthly}</p>
                            <p className="text-xs text-gray-500">
                                {tier.is_admin_only ? 'Nur Admin' : 'Öffentlich'} · {tier.is_default ? 'Default' : 'Nicht Default'}
                            </p>
                        </div>
                    </div>
                ))}
            </div>

            {(editing || isCreating) && (
                <EditTierDialog
                    tier={editing}
                    isCreating={isCreating}
                    onClose={() => {
                        setEditing(null);
                        setIsCreating(false);
                    }}
                    onRefresh={onRefresh}
                    onError={onError}
                />
            )}
        </div>
    );
}

function DeleteTierButton({ tierName, onRefresh, onError }: { tierName: string; onRefresh: () => void; onError: (msg: string) => void }) {
    const [confirming, setConfirming] = useState(false);
    const [deleting, setDeleting] = useState(false);

    async function doDelete() {
        setDeleting(true);
        try {
            await adminDeleteTier(tierName);
            setConfirming(false);
            onRefresh();
        } catch (err: any) {
            onError(err?.message || 'Löschen fehlgeschlagen.');
        } finally {
            setDeleting(false);
        }
    }

    if (confirming) {
        return (
            <div className="fixed inset-0 bg-black/60 z-50 flex items-center justify-center p-4">
                <div className="bg-[#151525] border border-[#2A2A40] rounded-2xl p-6 max-w-sm w-full">
                    <p className="text-sm text-gray-300 mb-4">
                        Tier <strong>{tierName}</strong> wirklich löschen? Alle betroffenen User werden auf das Default-Tier zurückgesetzt.
                    </p>
                    <div className="flex gap-3">
                        <button onClick={() => setConfirming(false)} className="flex-1 py-2 rounded-xl border border-[#2A2A40] text-sm">Abbrechen</button>
                        <button onClick={doDelete} disabled={deleting} className="flex-1 py-2 rounded-xl bg-red-500 text-sm text-white flex items-center justify-center gap-2">
                            {deleting ? <Loader2 size={16} className="animate-spin" /> : <Trash2 size={16} />}
                            Löschen
                        </button>
                    </div>
                </div>
            </div>
        );
    }

    return (
        <button onClick={() => setConfirming(true)} className="p-2 rounded-lg hover:bg-red-500/10 text-gray-400 hover:text-red-400">
            <Trash2 size={16} />
        </button>
    );
}

function EditTierDialog({
    tier,
    isCreating,
    onClose,
    onRefresh,
    onError,
}: {
    tier: Tier | null;
    isCreating: boolean;
    onClose: () => void;
    onRefresh: () => void;
    onError: (msg: string) => void;
}) {
    const [saving, setSaving] = useState(false);
    const [form, setForm] = useState<TierInput & { name: string }>({
        name: tier?.name || '',
        display_name: tier?.display_name || '',
        price_monthly_eur: tier?.price_monthly_eur || 0,
        price_yearly_eur: tier?.price_yearly_eur || 0,
        tokens_monthly: tier?.tokens_monthly || 0,
        is_admin_only: tier?.is_admin_only || false,
        is_default: tier?.is_default || false,
    });

    async function save() {
        if (!form.name.trim() || !form.display_name.trim()) return;
        setSaving(true);
        try {
            await adminUpsertTier(form.name, {
                display_name: form.display_name,
                price_monthly_eur: form.price_monthly_eur,
                price_yearly_eur: form.price_yearly_eur,
                tokens_monthly: form.tokens_monthly,
                is_admin_only: form.is_admin_only,
                is_default: form.is_default,
            });
            onClose();
            onRefresh();
        } catch (err: any) {
            onError(err?.message || 'Tier konnte nicht gespeichert werden.');
        } finally {
            setSaving(false);
        }
    }

    return (
        <div className="fixed inset-0 bg-black/60 backdrop-blur-sm z-50 flex items-center justify-center p-4">
            <motion.div
                initial={{ opacity: 0, scale: 0.95 }}
                animate={{ opacity: 1, scale: 1 }}
                className="bg-[#151525] border border-[#2A2A40] rounded-2xl p-6 w-full max-w-lg"
            >
                <h3 className="text-lg font-bold mb-4">{isCreating ? 'Neues Tier' : `Tier bearbeiten: ${tier?.display_name}`}</h3>
                <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
                    <Field label="Name (intern)" value={form.name} onChange={(v) => setForm({ ...form, name: v })} disabled={!isCreating} />
                    <Field label="Anzeigename" value={form.display_name} onChange={(v) => setForm({ ...form, display_name: v })} />
                    <NumberField label="Preis/Monat (€)" value={form.price_monthly_eur} onChange={(v) => setForm({ ...form, price_monthly_eur: v })} />
                    <NumberField label="Preis/Jahr (€)" value={form.price_yearly_eur} onChange={(v) => setForm({ ...form, price_yearly_eur: v })} />
                    <NumberField label="Tokens/Monat" value={form.tokens_monthly} onChange={(v) => setForm({ ...form, tokens_monthly: v })} integer />
                    <div className="flex items-center gap-6 md:col-span-2">
                        <label className="flex items-center gap-2 text-sm text-gray-300 cursor-pointer">
                            <input
                                type="checkbox"
                                checked={form.is_admin_only}
                                onChange={(e) => setForm({ ...form, is_admin_only: e.target.checked })}
                                className="rounded border-[#2A2A40] bg-[#0B0B1A] text-[#5E5CE6] focus:ring-0"
                            />
                            Nur Admin kann vergeben
                        </label>
                        <label className="flex items-center gap-2 text-sm text-gray-300 cursor-pointer">
                            <input
                                type="checkbox"
                                checked={form.is_default}
                                onChange={(e) => setForm({ ...form, is_default: e.target.checked })}
                                className="rounded border-[#2A2A40] bg-[#0B0B1A] text-[#5E5CE6] focus:ring-0"
                            />
                            Default-Tier für neue User
                        </label>
                    </div>
                </div>
                <div className="flex gap-3 mt-6">
                    <button onClick={onClose} className="flex-1 py-2.5 rounded-xl border border-[#2A2A40] text-sm font-medium text-gray-300 hover:bg-[#2A2A40]">
                        Abbrechen
                    </button>
                    <button
                        onClick={save}
                        disabled={saving}
                        className="flex-1 py-2.5 rounded-xl bg-[#5E5CE6] text-sm font-medium text-white hover:bg-[#4d4ac9] disabled:opacity-50 flex items-center justify-center gap-2"
                    >
                        {saving ? <Loader2 size={16} className="animate-spin" /> : <Save size={16} />}
                        Speichern
                    </button>
                </div>
            </motion.div>
        </div>
    );
}

function Field({ label, value, onChange, disabled = false }: { label: string; value: string; onChange: (v: string) => void; disabled?: boolean }) {
    return (
        <div>
            <label className="block text-xs text-gray-400 mb-1.5">{label}</label>
            <input
                type="text"
                value={value}
                disabled={disabled}
                onChange={(e) => onChange(e.target.value)}
                className="w-full bg-[#0B0B1A] border border-[#2A2A40] rounded-xl px-3 py-2 text-sm text-white placeholder-gray-500 focus:outline-none focus:border-[#5E5CE6] disabled:opacity-50"
            />
        </div>
    );
}

function NumberField({ label, value, onChange, integer = false }: { label: string; value: number; onChange: (v: number) => void; integer?: boolean }) {
    return (
        <div>
            <label className="block text-xs text-gray-400 mb-1.5">{label}</label>
            <input
                type="number"
                value={value}
                onChange={(e) => onChange(integer ? parseInt(e.target.value || '0', 10) : parseFloat(e.target.value || '0'))}
                className="w-full bg-[#0B0B1A] border border-[#2A2A40] rounded-xl px-3 py-2 text-sm text-white focus:outline-none focus:border-[#5E5CE6]"
            />
        </div>
    );
}

// --- Features panel ---

function FeaturesPanel({ tiers, onRefresh, onError }: { tiers: Tier[]; onRefresh: () => void; onError: (msg: string) => void }) {
    const [selectedTier, setSelectedTier] = useState(tiers[0]?.name || '');
    const [savingFeature, setSavingFeature] = useState<string | null>(null);

    const currentTier = tiers.find((t) => t.name === selectedTier);

    async function toggle(featureKey: string, allowed: boolean) {
        if (!selectedTier) return;
        setSavingFeature(featureKey);
        try {
            await adminSetFeature(selectedTier, featureKey, allowed);
            onRefresh();
        } catch (err: any) {
            onError(err?.message || 'Feature konnte nicht gesetzt werden.');
        } finally {
            setSavingFeature(null);
        }
    }

    return (
        <div className="space-y-4">
            <div>
                <label className="block text-xs text-gray-400 mb-1.5">Tier auswählen</label>
                <select
                    value={selectedTier}
                    onChange={(e) => setSelectedTier(e.target.value)}
                    className="bg-[#151525] border border-[#2A2A40] rounded-xl px-4 py-2.5 text-sm text-white focus:outline-none focus:border-[#5E5CE6]"
                >
                    {tiers.map((t) => (
                        <option key={t.name} value={t.name}>
                            {t.display_name}
                        </option>
                    ))}
                </select>
            </div>

            {currentTier && (
                <div className="bg-[#151525] border border-[#2A2A40] rounded-2xl overflow-hidden">
                    <table className="w-full text-sm">
                        <thead className="bg-[#1C1C33] text-gray-400">
                            <tr>
                                <th className="text-left px-4 py-3 font-medium">Feature</th>
                                <th className="text-right px-4 py-3 font-medium">Erlaubt</th>
                            </tr>
                        </thead>
                        <tbody className="divide-y divide-[#2A2A40]">
                            {ALL_FEATURES.map((key) => {
                                const allowed = currentTier.features[key] ?? false;
                                return (
                                    <tr key={key} className="hover:bg-[#1C1C33]/50">
                                        <td className="px-4 py-3">{FEATURE_LABELS[key]}</td>
                                        <td className="px-4 py-3 text-right">
                                            <button
                                                onClick={() => toggle(key, !allowed)}
                                                disabled={savingFeature === key}
                                                className={`inline-flex items-center gap-2 px-3 py-1.5 rounded-lg text-xs font-medium transition-colors ${
                                                    allowed
                                                        ? 'bg-green-500/10 text-green-400 border border-green-500/20'
                                                        : 'bg-red-500/10 text-red-400 border border-red-500/20'
                                                }`}
                                            >
                                                {savingFeature === key ? (
                                                    <Loader2 size={14} className="animate-spin" />
                                                ) : allowed ? (
                                                    <Check size={14} />
                                                ) : (
                                                    <X size={14} />
                                                )}
                                                {allowed ? 'Erlaubt' : 'Gesperrt'}
                                            </button>
                                        </td>
                                    </tr>
                                );
                            })}
                        </tbody>
                    </table>
                </div>
            )}
        </div>
    );
}
