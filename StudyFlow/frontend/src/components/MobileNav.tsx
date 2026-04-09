"use client";

import React from 'react';
import Link from 'next/link';
import { usePathname } from 'next/navigation';
import { Home, Settings, Shield } from 'lucide-react';

export default function MobileNav() {
    const pathname = usePathname();
    const [isAdmin, setIsAdmin] = React.useState(false);
    const [mounted, setMounted] = React.useState(false);
    const [isNativeApp, setIsNativeApp] = React.useState(false);

    React.useEffect(() => {
        setMounted(true);
        const username = localStorage.getItem('username');
        setIsAdmin(username === 'admin_');

        const win = window as Window & { isBlopNativeApp?: boolean };
        if (win.isBlopNativeApp) {
            setIsNativeApp(true);
            return;
        }
        const checkNative = setInterval(() => {
            if (win.isBlopNativeApp) {
                setIsNativeApp(true);
                clearInterval(checkNative);
            }
        }, 100);
        return () => clearInterval(checkNative);
    }, [pathname]);

    const navItems = [
        { href: '/', label: 'Home', icon: Home },
        { href: '/settings', label: 'Settings', icon: Settings },
    ];

    if (mounted && isAdmin) {
        navItems.splice(1, 0, { href: '/admin', label: 'Admin', icon: Shield });
    }


    return (
        <div
            role="navigation"
            aria-label="Mobile"
            className="no-print md:hidden fixed bottom-0 left-0 right-0 bg-[#1e1e1e]/90 backdrop-blur-lg border-t border-[#333] z-50 pt-1"
            style={{
                paddingBottom: isNativeApp ? "0px" : "calc(env(safe-area-inset-bottom, 0px) + 8px)",
                borderTop: isNativeApp ? "0px solid transparent" : undefined,
            }}
        >
            <div className="flex justify-around items-center h-16 px-2">
                {navItems.map((item) => {
                    const Icon = item.icon;
                    const isActive = pathname === item.href;

                    return (
                        <Link
                            key={item.href}
                            href={item.href}
                            className={`
                                flex flex-col items-center justify-center w-full h-full space-y-1
                                ${isActive ? 'text-[#5E5CE6]' : 'text-[#888] hover:text-[#DDD]'}
                            `}
                        >
                            <Icon size={24} strokeWidth={isActive ? 2.5 : 2} />
                            <span className="text-[10px] font-medium">{item.label}</span>
                        </Link>
                    );
                })}
            </div>
        </div>
    );
}
