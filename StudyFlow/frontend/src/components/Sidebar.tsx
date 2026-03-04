"use client";

import Image from 'next/image';
import React, { useState, useEffect } from 'react';
import Link from 'next/link';
import { usePathname } from 'next/navigation';
import { Home, BookOpen, MessageSquare, Brain, Settings, Sparkles, Shield } from 'lucide-react';

const navItems = [
    { href: '/', label: 'Dashboard', icon: Home },
    // { href: '/plans', label: 'Lernpläne', icon: BookOpen },
    // { href: '/chat', label: 'AI Chat', icon: MessageSquare },
    // { href: '/flashcards', label: 'Karteikarten', icon: Brain },
    { href: '/settings', label: 'Einstellungen', icon: Settings },
];
interface SidebarProps {
    isCollapsed: boolean;
    onToggle: () => void;
}

export default function Sidebar({ isCollapsed, onToggle }: SidebarProps) {
    const pathname = usePathname();
    const [username, setUsername] = useState('User');
    const [isAdmin, setIsAdmin] = useState(false);
    const [mounted, setMounted] = useState(false);

    useEffect(() => {
        setMounted(true);
        const user = localStorage.getItem('username') || 'User';
        setUsername(user);
        setIsAdmin(user.startsWith('admin_'));
    }, [pathname]);

    const handleLogout = () => {
        localStorage.clear();
        window.location.href = '/login';
    };

    // We use suppressHydrationWarning on dynamic client elements instead of an early return 
    // so the entire sidebar layout renders immediately without layout jumps.

    return (
        <aside className="h-screen w-[260px] bg-[#1e1e1e] border-r border-[#333] flex flex-col sticky top-0 shrink-0">
            {/* Header matches Blop Study style */}
            <div className="h-[74px] border-b border-[#333] flex items-center px-4 justify-between">
                <div className="flex items-center gap-2.5">
                    {/* Logo box (oval, accent color) */}
                    <div className="w-[38px] h-[38px] rounded-[10px] bg-[#5E5CE6] flex items-center justify-center text-white font-bold text-sm">
                        B
                    </div>
                    <div className="flex flex-col gap-0.5">
                        <h1 className="text-[16px] font-bold text-white leading-tight">Blop</h1>
                        <p className="text-[10px] text-[#888] leading-tight">Lernassistent</p>
                    </div>
                </div>
                {/* Collapse button style like in Qt */}
                <button
                    onClick={onToggle}
                    className="text-[#888] hover:text-white hover:bg-[#333] w-6 h-6 rounded flex items-center justify-center transition-colors text-base"
                >
                    {isCollapsed ? '»' : '«'}
                </button>
            </div>

            {/* Navigation */}
            <nav className="flex-1 p-4 space-y-2 overflow-y-auto">
                {navItems.map((item) => {
                    const Icon = item.icon;
                    const isActive = pathname === item.href;

                    return (
                        <Link
                            key={item.href}
                            href={item.href}
                            className={`
                                flex items-center gap-3 px-4 py-2.5 rounded-none text-[14px] transition-all relative overflow-hidden group
                                ${isActive
                                    ? 'bg-[#252526] text-white font-medium'
                                    : 'text-[#DDD] hover:bg-[#252526] active:bg-[#333]'
                                }
                            `}
                        >
                            <Icon size={18} strokeWidth={isActive ? 2.5 : 2} className={isActive ? 'text-[#5E5CE6] relative z-10' : 'text-[#888] relative z-10'} />
                            <span className="relative z-10">{item.label}</span>
                            {isActive && <div className="absolute left-0 top-1 bottom-1 w-1 bg-[#5E5CE6] rounded-r-full" />}
                        </Link>
                    );
                })}

                {/* Admin Panel (only for admin_) */}
                {isAdmin && (
                    <Link
                        href="/admin"
                        title={isCollapsed ? "Admin Panel" : undefined}
                        className={`
                            flex items-center gap-3 py-2.5 rounded-none text-[14px] transition-all relative overflow-hidden group
                            ${isCollapsed ? 'justify-center px-0' : 'px-4'}
                            ${pathname === '/admin'
                                ? 'bg-[#252526] text-white font-medium'
                                : 'text-[#DDD] hover:bg-[#252526] active:bg-[#333]'
                            }
                        `}
                    >
                        <Shield size={18} strokeWidth={pathname === '/admin' ? 2.5 : 2} className={pathname === '/admin' ? 'text-red-500 relative z-10 shrink-0' : 'text-[#888] relative z-10 shrink-0'} />
                        {!isCollapsed && <span className="relative z-10 whitespace-nowrap">Admin Panel</span>}
                        {pathname === '/admin' && <div className="absolute left-0 top-1 bottom-1 w-1 bg-red-500 rounded-r-full" />}
                    </Link>
                )}
            </nav>

            {/* User Profile with Logout */}
            <div className={`h-[72px] border-t border-[#333] flex items-center py-[10px] gap-2.5 transition-all overflow-hidden ${isCollapsed ? 'justify-center px-0' : 'px-[14px]'}`}>
                {/* Avatar circle */}
                <div className="w-[36px] h-[36px] rounded-[18px] bg-gradient-to-br from-[#5E5CE6] to-[#7D7AFF] flex items-center justify-center text-white font-bold text-[14px] shrink-0">
                    {username.charAt(0).toUpperCase()}
                </div>
                {!isCollapsed && (
                    <div className="flex-1 min-w-0 flex flex-col justify-center gap-[1px]">
                        <p suppressHydrationWarning className="text-[12px] font-semibold text-white truncate max-w-[130px]">
                            {mounted ? username : 'Lade...'}
                        </p>
                        <button
                            onClick={handleLogout}
                            className="text-[10px] text-[#888] hover:text-[#5E5CE6] transition-colors text-left p-0 border-none bg-transparent"
                        >
                            Abmelden
                        </button>
                    </div>
                )}
            </div>

            {/* Datenschutz Link */}
            {!isCollapsed && (
                <div className="px-4 pb-4">
                    <Link
                        href="/datenschutz"
                        className="flex items-center gap-2 text-[11px] text-[#888] hover:text-[#DDD] transition-colors py-1.5"
                    >
                        <Shield size={12} />
                        Datenschutzerklärung
                    </Link>
                </div>
            )}
        </aside>
    );
}
