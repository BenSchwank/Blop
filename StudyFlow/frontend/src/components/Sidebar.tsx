"use client";

import React, { useState, useEffect } from 'react';
import Link from 'next/link';
import { usePathname } from 'next/navigation';
import { Home, Settings, Shield, CreditCard } from 'lucide-react';

const navItems = [
    { href: '/', label: 'Dashboard', icon: Home },
    { href: '/settings', label: 'Einstellungen', icon: Settings },
];

interface SidebarProps {
    isCollapsed: boolean;
    onToggle: () => void;
}

export default function Sidebar({ isCollapsed, onToggle }: SidebarProps) {
    const pathname = usePathname();
    const [username, setUsername] = useState('');
    const [isAdmin, setIsAdmin] = useState(false);
    const [tokens, setTokens] = useState<number | null>(null);
    const [tier, setTier] = useState<string>('free');
    const [mounted, setMounted] = useState(false);

    const refreshUserInfo = async (user: string) => {
        if (!user) return;
        try {
            const sid = localStorage.getItem('session_id') || '';
            const res = await fetch(`/api/user/${user}?session_id=${encodeURIComponent(sid)}`);
            if (!res.ok) {
                // Keep last known sidebar values on 5xx/401 — never crash-parse HTML/text 500 bodies.
                console.warn('Sidebar user info unavailable:', res.status);
                return;
            }
            const data = await res.json();
            if (data && data.tokens !== undefined) setTokens(data.tokens);
            if (data && data.subscription_tier) setTier(data.subscription_tier);
            if (data && typeof data.is_admin === 'boolean') {
                setIsAdmin(data.is_admin);
                localStorage.setItem('is_admin', String(data.is_admin));
            }
        } catch (err) {
            console.error("Error fetching user info:", err);
        }
    };

    useEffect(() => {
        setMounted(true);
        const user = localStorage.getItem('username') || '';
        setUsername(user);
        // Read is_admin from localStorage (set by refreshUserInfo); fall back to exact username match
        setIsAdmin(localStorage.getItem('is_admin') === 'true' || user === 'admin_');

        if (user) void refreshUserInfo(user);

        const onTokensUpdated = () => {
            const current = localStorage.getItem('username') || '';
            if (current) void refreshUserInfo(current);
        };
        window.addEventListener('blop_tokens_updated', onTokensUpdated);
        return () => window.removeEventListener('blop_tokens_updated', onTokensUpdated);
    }, []);

    const handleLogout = () => {
        localStorage.clear();
        window.location.href = '/login';
    };

    const initial = username ? username.charAt(0).toUpperCase() : '?';

    return (
        <aside
            className="h-screen bg-[#1A1916] border-r border-white/10 flex flex-col sticky top-0 shrink-0 transition-all duration-300 overflow-hidden"
            style={{ width: isCollapsed ? '64px' : '220px' }}
        >
            {/* Header */}
            <div className="h-[64px] border-b border-white/10 flex items-center px-2.5 justify-between shrink-0">
                {/* Logo */}
                <div className="flex items-center gap-2.5 overflow-hidden">
                    <img src="/logo.jpg" alt="Blop Logo" className="w-9 h-9 rounded-[10px] object-cover shrink-0" />
                    {!isCollapsed && (
                        <div className="flex flex-col gap-0.5 overflow-hidden">
                            <h1 className="text-[15px] font-bold text-[#F4F5F7] leading-tight whitespace-nowrap">Blop</h1>
                            <p className="text-[10px] text-[#B8BEC9] leading-tight whitespace-nowrap">Study</p>
                        </div>
                    )}
                </div>
                {/* Collapse Button */}
                <button
                    onClick={onToggle}
                    className="text-[#B8BEC9] hover:text-white hover:bg-white/10 w-7 h-7 rounded-lg flex items-center justify-center transition-colors text-base shrink-0 ml-1"
                    title={isCollapsed ? 'Sidebar ausklappen' : 'Sidebar einklappen'}
                >
                    {isCollapsed ? '»' : '«'}
                </button>
            </div>

            {/* Navigation */}
            <nav className="flex-1 p-2 space-y-1 overflow-y-auto overflow-x-hidden">
                {navItems.map((item) => {
                    const Icon = item.icon;
                    const isActive = pathname === item.href;
                    return (
                        <Link
                            key={item.href}
                            href={item.href}
                            title={isCollapsed ? item.label : undefined}
                            className={`
                                flex items-center gap-3 py-2.5 rounded-[10px] text-[14px] transition-all relative overflow-hidden group
                                ${isCollapsed ? 'justify-center px-0' : 'px-3'}
                                ${isActive
                                    ? 'bg-[#5B9DFF]/18 text-white font-medium'
                                    : 'text-[#B8BEC9] hover:bg-white/[0.06] hover:text-[#F4F5F7]'
                                }
                            `}
                        >
                            <Icon
                                size={18}
                                strokeWidth={isActive ? 2.5 : 2}
                                className={`${isActive ? 'text-[#5B9DFF]' : 'text-[#B8BEC9]'} relative z-10 shrink-0`}
                            />
                            {!isCollapsed && <span className="relative z-10 whitespace-nowrap">{item.label}</span>}
                        </Link>
                    );
                })}

                <Link
                    href="/pricing"
                    title={isCollapsed ? 'Abos' : undefined}
                    className={`
                        flex items-center gap-3 py-2.5 rounded-[10px] text-[14px] transition-all relative overflow-hidden group
                        ${isCollapsed ? 'justify-center px-0' : 'px-3'}
                        ${pathname === '/pricing'
                            ? 'bg-[#5B9DFF]/18 text-white font-medium'
                            : 'text-[#B8BEC9] hover:bg-white/[0.06] hover:text-[#F4F5F7]'
                        }
                    `}
                >
                    <CreditCard
                        size={18}
                        strokeWidth={pathname === '/pricing' ? 2.5 : 2}
                        className={`${pathname === '/pricing' ? 'text-[#5B9DFF]' : 'text-[#B8BEC9]'} relative z-10 shrink-0`}
                    />
                    {!isCollapsed && <span className="relative z-10 whitespace-nowrap">Abos</span>}
                </Link>

                {/* Admin Panel */}
                {mounted && isAdmin && (
                    <>
                        <Link
                            href="/admin"
                            title={isCollapsed ? 'Admin Panel' : undefined}
                            className={`
                                flex items-center gap-3 py-2.5 rounded-[10px] text-[14px] transition-all relative overflow-hidden group
                                ${isCollapsed ? 'justify-center px-0' : 'px-3'}
                                ${pathname === '/admin'
                                    ? 'bg-red-500/15 text-white font-medium'
                                    : 'text-[#B8BEC9] hover:bg-white/[0.06] hover:text-[#F4F5F7]'
                                }
                            `}
                        >
                            <Shield
                                size={18}
                                strokeWidth={pathname === '/admin' ? 2.5 : 2}
                                className={`${pathname === '/admin' ? 'text-red-500' : 'text-[#888]'} relative z-10 shrink-0`}
                            />
                            {!isCollapsed && <span className="relative z-10 whitespace-nowrap">Admin Panel</span>}
                        </Link>
                        <Link
                            href="/admin/subscriptions"
                            title={isCollapsed ? 'Admin-Abos' : undefined}
                            className={`
                                flex items-center gap-3 py-2 rounded-[10px] text-[14px] transition-all relative overflow-hidden group
                                ${isCollapsed ? 'justify-center px-0' : 'pl-9 pr-3'}
                                ${pathname === '/admin/subscriptions'
                                    ? 'bg-[#5B9DFF]/18 text-white font-medium'
                                    : 'text-[#B8BEC9] hover:bg-white/[0.06] hover:text-[#F4F5F7]'
                                }
                            `}
                        >
                            <CreditCard
                                size={16}
                                strokeWidth={pathname === '/admin/subscriptions' ? 2.5 : 2}
                                className={`${pathname === '/admin/subscriptions' ? 'text-[#5B9DFF]' : 'text-[#B8BEC9]'} relative z-10 shrink-0`}
                            />
                            {!isCollapsed && <span className="relative z-10 whitespace-nowrap">Admin-Abos</span>}
                        </Link>
                    </>
                )}
            </nav>

            {/* User Profile */}
            <div
                className={`border-t border-white/10 flex items-center py-3 gap-3 transition-all overflow-hidden shrink-0 ${isCollapsed ? 'justify-center px-0' : 'px-3'}`}
                style={{ minHeight: '64px' }}
            >
                {/* Avatar */}
                <div className="w-9 h-9 rounded-full bg-[#5B9DFF] flex items-center justify-center text-white font-bold text-[14px] shrink-0">
                    {mounted ? initial : '?'}
                </div>
                {!isCollapsed && (
                    <div className="flex-1 min-w-0 flex flex-col justify-center gap-[0px]">
                        <p suppressHydrationWarning className="text-[13px] font-semibold text-white truncate leading-tight">
                            {mounted ? (username || 'Gast') : '...'}
                        </p>
                        <Link
                            href="/settings"
                            className="text-[11px] text-[#888] hover:text-[#5B9DFF] transition-colors leading-tight block mt-0.5"
                        >
                            Einstellungen
                        </Link>
                        {tokens !== null && (
                            <div className="flex items-center gap-1.5 mt-1.5">
                                <span className={`text-[9px] font-medium px-1.5 py-0.5 rounded ${tier === 'premium' ? 'bg-amber-500/20 text-amber-400' : tier === 'pro' ? 'bg-blue-500/20 text-blue-400' : 'bg-[#333] text-gray-300'}`}>
                                    {(tier || 'free').toUpperCase()}
                                </span>
                                <span className="text-[10px] text-[#7EB2FF] font-medium flex items-center gap-0.5">
                                    🪙 {tokens > 900000 ? '∞' : tokens}
                                </span>
                            </div>
                        )}
                        <p className="text-[10px] text-[#555] opacity-80 leading-tight mt-[2px]">
                            v3.13.5.11
                        </p>
                    </div>
                )}
            </div>

            {/* Datenschutz */}
            {!isCollapsed && (
                <div className="px-4 pb-3 shrink-0">
                    <Link
                        href="/datenschutz"
                        className="flex items-center gap-2 text-[11px] text-[#888] hover:text-[#DDD] transition-colors py-1.5"
                    >
                        <Shield size={12} />
                        <span className="whitespace-nowrap">Datenschutzerklärung</span>
                    </Link>
                </div>
            )}
        </aside>
    );
}
