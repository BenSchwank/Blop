const API_BASE = '/api';

function sessionHeaders(): Record<string, string> {
    const sid = typeof window !== 'undefined' ? (localStorage.getItem('session_id') || '').trim() : '';
    const headers: Record<string, string> = {};
    if (sid) headers['X-Session-Id'] = sid;
    return headers;
}

function getUsername(): string {
    if (typeof window === 'undefined') return '';
    return localStorage.getItem('username') || '';
}

export interface Tier {
    name: string;
    display_name: string;
    price_monthly_eur: number;
    price_yearly_eur: number;
    tokens_monthly: number;
    is_admin_only: boolean;
    is_default: boolean;
    features: Record<string, boolean>;
}

export interface SubscriptionStatus {
    username: string;
    subscription: {
        tier: string;
        status: string;
        provider: string;
        current_period_end?: string;
        cancel_at_period_end?: boolean;
        features: Record<string, boolean>;
    };
    stripe_publishable_key?: string;
    paypal_client_id?: string;
}

export async function fetchTiers(): Promise<Tier[]> {
    const res = await fetch(`${API_BASE}/subscription/tiers`);
    if (!res.ok) throw new Error('Tiers konnten nicht geladen werden.');
    const data = await res.json();
    return data.tiers || [];
}

export async function fetchSubscriptionStatus(): Promise<SubscriptionStatus> {
    const username = getUsername();
    const sid = typeof window !== 'undefined' ? (localStorage.getItem('session_id') || '') : '';
    const res = await fetch(
        `${API_BASE}/subscription/status?username=${encodeURIComponent(username)}&session_id=${encodeURIComponent(sid)}`,
        { headers: sessionHeaders() }
    );
    if (!res.ok) throw new Error('Abo-Status konnte nicht geladen werden.');
    return res.json();
}

export async function createStripeCheckout(tier: string, interval: 'month' | 'year'): Promise<{ url: string; session_id: string }> {
    const username = getUsername();
    const sid = typeof window !== 'undefined' ? (localStorage.getItem('session_id') || '') : '';
    const res = await fetch(`${API_BASE}/subscription/checkout?session_id=${encodeURIComponent(sid)}`, {
        method: 'POST',
        headers: { ...sessionHeaders(), 'Content-Type': 'application/json' },
        body: JSON.stringify({ tier, interval, username }),
    });
    const data = await res.json();
    if (!res.ok) throw new Error(data.detail || 'Stripe Checkout fehlgeschlagen.');
    return data;
}

export async function createStripePortal(): Promise<{ url: string }> {
    const username = getUsername();
    const sid = typeof window !== 'undefined' ? (localStorage.getItem('session_id') || '') : '';
    const res = await fetch(
        `${API_BASE}/subscription/portal?username=${encodeURIComponent(username)}&session_id=${encodeURIComponent(sid)}`,
        {
            method: 'POST',
            headers: sessionHeaders(),
        }
    );
    const data = await res.json();
    if (!res.ok) throw new Error(data.detail || 'Stripe Portal fehlgeschlagen.');
    return data;
}

export async function createPayPalOrder(tier: string, interval: 'month' | 'year'): Promise<{ order_id: string; approval_url: string }> {
    const username = getUsername();
    const sid = typeof window !== 'undefined' ? (localStorage.getItem('session_id') || '') : '';
    const res = await fetch(`${API_BASE}/subscription/paypal/create?session_id=${encodeURIComponent(sid)}`, {
        method: 'POST',
        headers: { ...sessionHeaders(), 'Content-Type': 'application/json' },
        body: JSON.stringify({ tier, interval, username }),
    });
    const data = await res.json();
    if (!res.ok) throw new Error(data.detail || 'PayPal Order fehlgeschlagen.');
    return data;
}

export async function capturePayPalOrder(orderId: string): Promise<{ status: string; tier: string }> {
    const sid = typeof window !== 'undefined' ? (localStorage.getItem('session_id') || '') : '';
    const res = await fetch(`${API_BASE}/subscription/paypal/capture?session_id=${encodeURIComponent(sid)}`, {
        method: 'POST',
        headers: { ...sessionHeaders(), 'Content-Type': 'application/json' },
        body: JSON.stringify({ order_id: orderId }),
    });
    const data = await res.json();
    if (!res.ok) throw new Error(data.detail || 'PayPal Zahlung konnte nicht abgeschlossen werden.');
    return data;
}

// --- Admin helpers ---

export interface AdminSubscription {
    username: string;
    tier: string;
    status: string;
    provider: string;
    provider_customer_id?: string;
    provider_subscription_id?: string;
    current_period_start?: string;
    current_period_end?: string;
    cancel_at_period_end?: boolean;
}

export async function adminListTiers(adminUsername: string): Promise<Tier[]> {
    const sid = typeof window !== 'undefined' ? (localStorage.getItem('session_id') || '') : '';
    const res = await fetch(
        `${API_BASE}/admin/tiers?admin_username=${encodeURIComponent(adminUsername)}&session_id=${encodeURIComponent(sid)}`,
        { headers: sessionHeaders() }
    );
    const data = await res.json();
    if (!res.ok) throw new Error(data.detail || 'Tiers konnten nicht geladen werden.');
    return data.tiers || [];
}

export async function adminListSubscriptions(adminUsername: string): Promise<AdminSubscription[]> {
    const sid = typeof window !== 'undefined' ? (localStorage.getItem('session_id') || '') : '';
    const res = await fetch(
        `${API_BASE}/admin/subscriptions?admin_username=${encodeURIComponent(adminUsername)}&session_id=${encodeURIComponent(sid)}`,
        { headers: sessionHeaders() }
    );
    const data = await res.json();
    if (!res.ok) throw new Error(data.detail || 'Abos konnten nicht geladen werden.');
    return data.subscriptions || [];
}

export async function adminSetSubscription(
    username: string,
    tier: string,
    months: number,
    status = 'active',
    provider = 'admin'
): Promise<{ status: string; username: string; tier: string }> {
    const adminUsername = getUsername();
    const sid = typeof window !== 'undefined' ? (localStorage.getItem('session_id') || '') : '';
    const res = await fetch(
        `${API_BASE}/admin/subscriptions/${encodeURIComponent(username)}?admin_username=${encodeURIComponent(adminUsername)}&session_id=${encodeURIComponent(sid)}`,
        {
            method: 'PUT',
            headers: { ...sessionHeaders(), 'Content-Type': 'application/json' },
            body: JSON.stringify({ tier, months, status, provider }),
        }
    );
    const data = await res.json();
    if (!res.ok) throw new Error(data.detail || 'Abo konnte nicht gesetzt werden.');
    return data;
}

export async function adminGrantCustom(
    username: string,
    tier: string,
    months: number,
    status = 'active'
): Promise<{ status: string; username: string; tier: string }> {
    const adminUsername = getUsername();
    const sid = typeof window !== 'undefined' ? (localStorage.getItem('session_id') || '') : '';
    const res = await fetch(
        `${API_BASE}/admin/subscriptions/${encodeURIComponent(username)}/custom?admin_username=${encodeURIComponent(adminUsername)}&session_id=${encodeURIComponent(sid)}`,
        {
            method: 'POST',
            headers: { ...sessionHeaders(), 'Content-Type': 'application/json' },
            body: JSON.stringify({ tier, months, status }),
        }
    );
    const data = await res.json();
    if (!res.ok) throw new Error(data.detail || 'Custom-Abo konnte nicht vergeben werden.');
    return data;
}

export interface TierInput {
    display_name: string;
    price_monthly_eur: number;
    price_yearly_eur: number;
    tokens_monthly: number;
    is_admin_only: boolean;
    is_default: boolean;
}

export async function adminUpsertTier(
    name: string,
    tier: TierInput
): Promise<{ status: string; tier: string }> {
    const adminUsername = getUsername();
    const sid = typeof window !== 'undefined' ? (localStorage.getItem('session_id') || '') : '';
    const res = await fetch(
        `${API_BASE}/admin/tiers/${encodeURIComponent(name)}?admin_username=${encodeURIComponent(adminUsername)}&session_id=${encodeURIComponent(sid)}`,
        {
            method: 'PUT',
            headers: { ...sessionHeaders(), 'Content-Type': 'application/json' },
            body: JSON.stringify(tier),
        }
    );
    const data = await res.json();
    if (!res.ok) throw new Error(data.detail || 'Tier konnte nicht gespeichert werden.');
    return data;
}

export async function adminSetFeature(
    tierName: string,
    featureKey: string,
    allowed: boolean
): Promise<{ status: string; tier: string; feature: string; allowed: boolean }> {
    const adminUsername = getUsername();
    const sid = typeof window !== 'undefined' ? (localStorage.getItem('session_id') || '') : '';
    const res = await fetch(
        `${API_BASE}/admin/features/${encodeURIComponent(tierName)}/${encodeURIComponent(featureKey)}?admin_username=${encodeURIComponent(adminUsername)}&session_id=${encodeURIComponent(sid)}`,
        {
            method: 'PUT',
            headers: { ...sessionHeaders(), 'Content-Type': 'application/json' },
            body: JSON.stringify({ allowed }),
        }
    );
    const data = await res.json();
    if (!res.ok) throw new Error(data.detail || 'Feature konnte nicht gesetzt werden.');
    return data;
}

export async function adminDeleteTier(name: string): Promise<{ status: string; deleted: string }> {
    const adminUsername = getUsername();
    const sid = typeof window !== 'undefined' ? (localStorage.getItem('session_id') || '') : '';
    const res = await fetch(
        `${API_BASE}/admin/tiers/${encodeURIComponent(name)}?admin_username=${encodeURIComponent(adminUsername)}&session_id=${encodeURIComponent(sid)}`,
        {
            method: 'DELETE',
            headers: sessionHeaders(),
        }
    );
    const data = await res.json();
    if (!res.ok) throw new Error(data.detail || 'Tier konnte nicht gelöscht werden.');
    return data;
}

export async function fetchUserInfo(username: string): Promise<{ is_admin: boolean; username: string; email: string; tokens: number; subscription_tier: string }> {
    const sid = typeof window !== 'undefined' ? (localStorage.getItem('session_id') || '') : '';
    const res = await fetch(`${API_BASE}/user/${encodeURIComponent(username)}?session_id=${encodeURIComponent(sid)}`, {
        headers: sessionHeaders(),
    });
    const data = await res.json();
    if (!res.ok) throw new Error(data.detail || 'User konnte nicht geladen werden.');
    return data;
}

export const FEATURE_LABELS: Record<string, string> = {
    plan: 'Lernplan',
    quiz: 'Quiz',
    flashcards: 'Karteikarten',
    summary: 'Zusammenfassung',
    audio_transcribe: 'Audio-Transkription',
    image_to_text: 'Bild-zu-Text',
    podcast: 'Podcast',
    learning_video: 'Lernvideo',
    elaboration: 'Ausarbeitung',
    elaboration_refine: 'Ausarbeitung verfeinern',
    repetition: 'Wiederholungsbogen',
    task_help: 'Aufgabenhilfe',
    chat: 'KI-Chat',
};
