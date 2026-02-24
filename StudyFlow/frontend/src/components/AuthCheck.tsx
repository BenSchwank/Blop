"use client";

import { useEffect } from 'react';
import { useRouter, usePathname } from 'next/navigation';

export default function AuthCheck({ children }: { children: React.ReactNode }) {
    const router = useRouter();
    const pathname = usePathname();

    useEffect(() => {
        // Skip auth check for public pages
        if (pathname === '/login' || pathname === '/datenschutz') return;


        // Only run on client side
        if (typeof window === 'undefined') return;

        // Check if user is authenticated
        const sessionId = localStorage.getItem('session_id');
        const username = localStorage.getItem('username');

        if (!sessionId || !username) {
            // Not authenticated, redirect to login
            router.push('/login');
        }
    }, [pathname, router]);

    // Always render children (including login page)
    return <>{children}</>;
}
