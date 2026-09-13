/** Shared session helpers for Blop Study frontend. */

export function getSessionId(): string {
    if (typeof window === 'undefined') return '';
    return (localStorage.getItem('session_id') || '').trim();
}

export function getStoredUsername(): string {
    if (typeof window === 'undefined') return '';
    return localStorage.getItem('username') || '';
}

export function sessionHeaders(extra: Record<string, string> = {}): Record<string, string> {
    const headers: Record<string, string> = { ...extra };
    const sid = getSessionId();
    if (sid) headers['X-Session-Id'] = sid;
    return headers;
}

/** Explicit logout used by the user or after confirmed auth failure. */
export function clearSessionAndRedirect(reason: 'session-expired' | 'logout' = 'session-expired'): void {
    if (typeof window === 'undefined') return;
    localStorage.removeItem('session_id');
    localStorage.removeItem('username');
    localStorage.removeItem('is_admin');
    if (reason === 'logout') {
        window.location.replace('/login');
        return;
    }
    window.location.replace('/login?reason=session-expired');
}

/**
 * React to HTTP 401.
 *
 * Hard redirect is opt-in. Payment/settings flows must not kick the user out on a
 * single flaky auth response (Render cold start / brief DB blip).
 * Never treat 5xx/network errors as logout.
 */
export function handleUnauthorized(
    response: Response,
    options: { redirect?: boolean } = {}
): boolean {
    if (response.status !== 401) return false;
    if (options.redirect === false) return true;
    clearSessionAndRedirect('session-expired');
    return true;
}

/** Dashboard-style fetch: one retry, then logout only on a second confirmed 401. */
export async function fetchWithSessionRetry(
    input: RequestInfo | URL,
    init?: RequestInit
): Promise<Response> {
    const first = await fetch(input, init);
    if (first.status !== 401) return first;
    // Brief pause — covers Render wake-ups racing the first authenticated call.
    await new Promise((resolve) => setTimeout(resolve, 400));
    return fetch(input, init);
}
