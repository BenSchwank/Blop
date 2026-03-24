"use client";

import React, { useState, useEffect } from 'react';
import Link from 'next/link';
import { usePathname } from 'next/navigation';
import { Home, Settings, Shield } from 'lucide-react';

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

    useEffect(() => {
        setMounted(true);
        const user = localStorage.getItem('username') || '';
        setUsername(user);
        setIsAdmin(user.startsWith('admin_'));
        
        if (user) {
            fetch(`/api/user/${user}`)
                .then(res => res.json())
                .then(data => {
                    setTokens(data.tokens);
                    setTier(data.subscription_tier);
                })
                .catch(err => console.error("Error fetching user info:", err));
        }
    }, [pathname]);

    const handleLogout = () => {
        localStorage.clear();
        window.location.href = '/login';
    };

    const initial = username ? username.charAt(0).toUpperCase() : '?';

    return (
        <aside
            className="h-screen bg-[#1e1e1e] border-r border-[#333] flex flex-col sticky top-0 shrink-0 transition-all duration-300 overflow-hidden"
            style={{ width: isCollapsed ? '72px' : '260px' }}
        >
            {/* Header */}
            <div className="h-[74px] border-b border-[#333] flex items-center px-3 justify-between shrink-0">
                {/* Logo */}
                <div className="flex items-center gap-2.5 overflow-hidden">
                    <img src="/logo.jpg" alt="Blop Logo" className="w-[38px] h-[38px] rounded-[10px] object-cover shrink-0" />
                    {!isCollapsed && (
                        <div className="flex flex-col gap-0.5 overflow-hidden">
                            <h1 className="text-[16px] font-bold text-white leading-tight whitespace-nowrap">Blop</h1>
                            <p className="text-[10px] text-[#888] leading-tight whitespace-nowrap">Lernassistent</p>
                        </div>
                    )}
                </div>
                {/* Collapse Button */}
                <button
                    onClick={onToggle}
                    className="text-[#888] hover:text-white hover:bg-[#333] w-7 h-7 rounded flex items-center justify-center transition-colors text-base shrink-0 ml-1"
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
                                flex items-center gap-3 py-2.5 rounded-lg text-[14px] transition-all relative overflow-hidden group
                                ${isCollapsed ? 'justify-center px-0' : 'px-4'}
                                ${isActive
                                    ? 'bg-[#252526] text-white font-medium'
                                    : 'text-[#DDD] hover:bg-[#252526] active:bg-[#333]'
                                }
                            `}
                        >
                            <Icon
                                size={18}
                                strokeWidth={isActive ? 2.5 : 2}
                                className={`${isActive ? 'text-[#5E5CE6]' : 'text-[#888]'} relative z-10 shrink-0`}
                            />
                            {!isCollapsed && <span className="relative z-10 whitespace-nowrap">{item.label}</span>}
                            {isActive && !isCollapsed && <div className="absolute left-0 top-1 bottom-1 w-1 bg-[#5E5CE6] rounded-r-full" />}
                        </Link>
                    );
                })}

                {/* Admin Panel */}
                {mounted && isAdmin && (
                    <Link
                        href="/admin"
                        title={isCollapsed ? 'Admin Panel' : undefined}
                        className={`
                            flex items-center gap-3 py-2.5 rounded-lg text-[14px] transition-all relative overflow-hidden group
                            ${isCollapsed ? 'justify-center px-0' : 'px-4'}
                            ${pathname === '/admin'
                                ? 'bg-[#252526] text-white font-medium'
                                : 'text-[#DDD] hover:bg-[#252526] active:bg-[#333]'
                            }
                        `}
                    >
                        <Shield
                            size={18}
                            strokeWidth={pathname === '/admin' ? 2.5 : 2}
                            className={`${pathname === '/admin' ? 'text-red-500' : 'text-[#888]'} relative z-10 shrink-0`}
                        />
                        {!isCollapsed && <span className="relative z-10 whitespace-nowrap">Admin Panel</span>}
                        {pathname === '/admin' && !isCollapsed && <div className="absolute left-0 top-1 bottom-1 w-1 bg-red-500 rounded-r-full" />}
                    </Link>
                )}
            </nav>

            {/* User Profile */}
            <div
                className={`border-t border-[#333] flex items-center py-3 gap-3 transition-all overflow-hidden shrink-0 ${isCollapsed ? 'justify-center px-0' : 'px-4'}`}
                style={{ minHeight: '74px' }}
            >
                {/* Avatar */}
                <div className="w-10 h-10 rounded-full bg-gradient-to-br from-[#5E5CE6] to-[#7D7AFF] flex items-center justify-center text-white font-bold text-[15px] shrink-0">
                    {mounted ? initial : '?'}
                </div>
                {!isCollapsed && (
                    <div className="flex-1 min-w-0 flex flex-col justify-center gap-[0px]">
                        <p suppressHydrationWarning className="text-[13px] font-semibold text-white truncate leading-tight">
                            {mounted ? (username || 'Gast') : '...'}
                        </p>
                        <Link
                            href="/settings"
                            className="text-[11px] text-[#888] hover:text-[#5E5CE6] transition-colors leading-tight block mt-0.5"
                        >
                            Einstellungen
                        </Link>
                        {tokens !== null && (
                            <div className="flex items-center gap-1.5 mt-1.5">
                                <span className={`text-[9px] font-medium px-1.5 py-0.5 rounded ${tier === 'premium' ? 'bg-amber-500/20 text-amber-400' : tier === 'pro' ? 'bg-blue-500/20 text-blue-400' : 'bg-[#333] text-gray-300'}`}>
                                    {tier.toUpperCase()}
                                </span>
                                <span className="text-[10px] text-[#7D7AFF] font-medium flex items-center gap-0.5">
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
