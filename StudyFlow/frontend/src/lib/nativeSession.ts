/**
 * Native (Android WebView) session hydration.
 *
 * When the Blop Android shell recreates the Study WebView (e.g. switching back
 * to the Study tab), the WebView's DOM — and therefore its localStorage — is
 * destroyed. The native shell still holds the logged-in session in QSettings
 * and appends it to the entry URL as `blop_usr` / `blop_sid`.
 *
 * This helper copies those params into localStorage synchronously, then strips
 * them from the URL. Calling it before any auth check guarantees the session is
 * present before AuthCheck (or the dashboard) decides whether to redirect to
 * /login, closing the race against the shell's asynchronous JS injection.
 *
 * Safe to call repeatedly and during SSR (it no-ops without `window`).
 */
export function hydrateNativeSession(): void {
    if (typeof window === 'undefined') return;

    let params: URLSearchParams;
    try {
        params = new URLSearchParams(window.location.search);
    } catch {
        return;
    }

    const usr = params.get('blop_usr');
    const sid = params.get('blop_sid');
    if (!usr || !sid) return;

    try {
        localStorage.setItem('username', usr);
        localStorage.setItem('session_id', sid);
    } catch {
        // localStorage unavailable (private mode / disabled) — nothing to do.
    }

    // Remove the credentials from the visible URL without reloading.
    try {
        params.delete('blop_usr');
        params.delete('blop_sid');
        const query = params.toString();
        const newUrl =
            window.location.pathname +
            (query ? `?${query}` : '') +
            window.location.hash;
        window.history.replaceState(null, '', newUrl);
    } catch {
        // replaceState can throw in exotic embeddings — the session is already
        // in localStorage, so the auth check still succeeds.
    }
}
