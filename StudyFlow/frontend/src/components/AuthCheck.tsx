"use client";

import { useEffect } from 'react';
import { useRouter, usePathname } from 'next/navigation';

export default function AuthCheck({ children }: { children: React.ReactNode }) {
    const router = useRouter();
    const pathname = usePathname();

    useEffect(() => {
        // Skip auth check for login page
        if (pathname === '/login') return;

        // Check if user is authenticated
        const sessionId = localStorage.getItem('session_id');
        const username = localStorage.getItem('username');

        if (!sessionId || !username) {
            // Not authenticated, redirect to login
            router.push('/login');
        }
    }, [pathname, router]);

    return <>{children}</>;
}
