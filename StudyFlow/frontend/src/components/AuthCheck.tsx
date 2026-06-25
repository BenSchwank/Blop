"use client";

import { useEffect } from 'react';
import { useRouter, usePathname } from 'next/navigation';
import { hydrateNativeSession } from '@/lib/nativeSession';

export default function AuthCheck({ children }: { children: React.ReactNode }) {
    const router = useRouter();
    const pathname = usePathname();

    useEffect(() => {
        // Seed localStorage from the native shell's persisted session (if any)
        // before any auth decision so login survives WebView storage loss.
        hydrateNativeSession();

        // Skip auth check for public pages
        if (
            pathname === '/login' ||
            pathname === '/datenschutz' ||
            pathname === '/auth/reset-password' ||
            pathname === '/marketing' ||
            pathname.startsWith('/marketing/')
        )
            return;


        // Only run on client side
        if (typeof window === 'undefined') return;

        // Check if user is authenticated
        const sessionId = localStorage.getItem('session_id');
        const username = localStorage.getItem('username');
        const params = new URLSearchParams(window.location.search);
        const isNativeEntry = params.get('native') === '1';

        if (!sessionId || !username) {
            // Keep native entry deterministic: preserve native hint and use replace
            // to avoid redirect loops in embedded Android WebView.
            const loginTarget = isNativeEntry ? '/login?native=1' : '/login';
            window.location.replace(loginTarget);
        }
    }, [pathname]); // Removing router dependency as we no longer use it inside the effect

    // Always render children (including login page)
    return <>{children}</>;
}
