// Bridge between the native Android/desktop shell and the embedded web app.
//
// The native shell persists the logged-in session (username + session_id) and
// appends it to the Study entry URL as `blop_usr` / `blop_sid` query params.
// On Android the embedded WebView's localStorage can be wiped or recreated
// (Loader recreation, storage resets, DOM-storage races), which logged the
// user out repeatedly. Seeding localStorage from these params before the auth
// guard runs keeps the session alive across those events.
//
// The params are stripped from the URL immediately after consumption so the
// session_id does not linger in browser history.
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
        // Only seed when the WebView lost its session; never clobber a fresher
        // one the user may have just established in this view.
        if (!localStorage.getItem('session_id') || !localStorage.getItem('username')) {
            localStorage.setItem('username', usr);
            localStorage.setItem('session_id', sid);
        }
    } catch {
        // localStorage unavailable (DOM storage disabled) — nothing we can do.
    }

    // Drop the credentials from the visible URL / history entry.
    try {
        params.delete('blop_usr');
        params.delete('blop_sid');
        const query = params.toString();
        const newUrl =
            window.location.pathname + (query ? `?${query}` : '') + window.location.hash;
        window.history.replaceState(window.history.state, '', newUrl);
    } catch {
        // replaceState can fail in some embedded contexts; safe to ignore.
    }
}
