"use client";

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

export default function Sidebar() {
    const pathname = usePathname();
    const [username, setUsername] = useState('User');
    const [isAdmin, setIsAdmin] = useState(false);
    const [mounted, setMounted] = useState(false);

    useEffect(() => {
        setMounted(true);
        const user = localStorage.getItem('username') || 'User';
        setUsername(user);
        setIsAdmin(user === 'admin_');
    }, []);

    const handleLogout = () => {
        localStorage.clear();
        window.location.href = '/login';
    };

    // Don't render until mounted to avoid hydration mismatch
    if (!mounted) {
        return (
            <aside className="h-screen w-full bg-[#1e1e1e] border-r border-[#333] flex flex-col sticky top-0">
                <div className="p-6 border-b border-[#333]">
                    <div className="flex items-center gap-3">
                        <div className="w-10 h-10 rounded-xl bg-[#5E5CE6] flex items-center justify-center">
                            <Sparkles className="w-5 h-5 text-white" />
                        </div>
                        <div>
                            <h1 className="text-xl font-semibold text-white">Blop Study</h1>
                            <p className="text-xs text-[#888]">AI Lernassistent</p>
                        </div>
                    </div>
                </div>
            </aside>
        );
    }

    return (
        <aside className="h-screen w-full bg-[#1e1e1e] border-r border-[#333] flex flex-col sticky top-0">
            {/* Logo */}
            <div className="p-6 border-b border-[#333]">
                <div className="flex items-center gap-3">
                    <div className="w-10 h-10 rounded-xl bg-[#5E5CE6] flex items-center justify-center">
                        <Sparkles className="w-5 h-5 text-white" />
                    </div>
                    <div>
                        <h1 className="text-xl font-semibold text-white">Blop Study</h1>
                        <p className="text-xs text-[#888]">AI Lernassistent</p>
                    </div>
                </div>
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
                flex items-center gap-4 px-6 py-4 rounded-xl text-[15px] font-medium transition-all min-h-[52px]
                ${isActive
                                    ? 'bg-[#5E5CE6] text-white shadow-md shadow-[#5E5CE6]/20'
                                    : 'text-[#DDD] hover:bg-[#333] active:bg-[#444]'
                                }
              `}
                        >
                            <Icon size={22} strokeWidth={2} />
                            <span>{item.label}</span>
                        </Link>
                    );
                })}

                {/* Admin Panel (only for admin_) */}
                {isAdmin && (
                    <Link
                        href="/admin"
                        className={`
              flex items-center gap-4 px-6 py-4 rounded-xl text-[15px] font-medium transition-all min-h-[52px]
              ${pathname === '/admin'
                                ? 'bg-gradient-to-r from-[#5E5CE6] to-[#7D7AFF] text-white shadow-md'
                                : 'text-[#DDD] hover:bg-[#333] active:bg-[#444] border border-[#5E5CE6]/30'
                            }
            `}
                    >
                        <Shield size={20} strokeWidth={2} />
                        <span>Admin Panel</span>
                    </Link>
                )}
            </nav>

            {/* User Profile with Logout */}
            <div className="p-4 border-t border-[#333]">
                <div className="flex items-center gap-3 p-3 rounded-lg hover:bg-[#252526] transition-all min-h-[56px]">
                    <div className="w-10 h-10 rounded-full bg-gradient-to-br from-[#5E5CE6] to-[#7D7AFF] flex items-center justify-center text-white font-semibold text-sm">
                        {username.charAt(0).toUpperCase()}
                    </div>
                    <div className="flex-1 min-w-0">
                        <p className="text-sm font-medium text-white truncate">{username}</p>
                        <button
                            onClick={handleLogout}
                            className="text-xs text-[#888] hover:text-[#5E5CE6] transition-colors"
                        >
                            Abmelden
                        </button>
                    </div>
                </div>
            </div>
        </aside>
    );
}
