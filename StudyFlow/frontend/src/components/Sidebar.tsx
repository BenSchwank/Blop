"use client";

import React from 'react';
import Link from 'next/link';
import { usePathname } from 'next/navigation';
import { Home, BookOpen, MessageSquare, Brain, Settings, Sparkles } from 'lucide-react';

const navItems = [
    { href: '/', label: 'Dashboard', icon: Home },
    { href: '/plans', label: 'Lernpläne', icon: BookOpen },
    { href: '/chat', label: 'AI Chat', icon: MessageSquare },
    { href: '/flashcards', label: 'Karteikarten', icon: Brain },
    { href: '/settings', label: 'Einstellungen', icon: Settings },
];

export default function Sidebar() {
    const pathname = usePathname();

    return (
        <aside className="fixed left-0 top-0 h-screen w-[280px] bg-[#1e1e1e] border-r border-[#333] flex flex-col">
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
            <nav className="flex-1 p-4 space-y-2">
                {navItems.map((item) => {
                    const Icon = item.icon;
                    const isActive = pathname === item.href;

                    return (
                        <Link
                            key={item.href}
                            href={item.href}
                            className={`
                flex items-center gap-3 px-4 py-3.5 rounded-lg text-[15px] font-medium transition-all min-h-[44px]
                ${isActive
                                    ? 'bg-[#5E5CE6] text-white'
                                    : 'text-[#DDD] hover:bg-[#333] active:bg-[#444]'
                                }
              `}
                        >
                            <Icon size={20} strokeWidth={2} />
                            <span>{item.label}</span>
                        </Link>
                    );
                })}
            </nav>

            {/* User Profile (No Upgrade Card) */}
            <div className="p-4 border-t border-[#333]">
                <div className="flex items-center gap-3 p-3 rounded-lg hover:bg-[#252526] active:bg-[#333] cursor-pointer transition-all min-h-[56px]">
                    <div className="w-10 h-10 rounded-full bg-gradient-to-br from-[#5E5CE6] to-[#7D7AFF] flex items-center justify-center text-white font-semibold text-sm">
                        U
                    </div>
                    <div className="flex-1 min-w-0">
                        <p className="text-sm font-medium text-white truncate">User</p>
                        <p className="text-xs text-[#888]">Blop Study</p>
                    </div>
                </div>
            </div>
        </aside>
    );
}
