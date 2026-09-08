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
