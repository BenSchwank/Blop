"use client";

import React, { useEffect, useRef, useState } from "react";
import Link from "next/link";
import { usePathname, useRouter } from "next/navigation";
import { FolderOpen, Home, Plus, Settings, Shield, User } from "lucide-react";

export const STUDY_NEW_EVENT = "blop-study-new";

export function requestStudyNew() {
    if (typeof window === "undefined") return;
    window.dispatchEvent(new CustomEvent(STUDY_NEW_EVENT));
}

function RailButton({
    label,
    active,
    onClick,
    href,
    children,
}: {
    label: string;
    active?: boolean;
    onClick?: () => void;
    href?: string;
    children: React.ReactNode;
}) {
    const className = `
        relative w-10 h-10 rounded-[10px] flex items-center justify-center transition-colors
        ${active
            ? "bg-[#5B9DFF]/18 text-[#5B9DFF]"
            : "text-[#9AA0AB] hover:bg-white/[0.06] hover:text-[#F4F5F7]"}
    `;
    const inner = (
        <span className="flex items-center justify-center" title={label} aria-label={label}>
            {children}
        </span>
    );
    if (href) {
        return (
            <Link href={href} className={className} title={label} aria-label={label} aria-current={active ? "page" : undefined}>
                {inner}
            </Link>
        );
    }
    return (
        <button type="button" onClick={onClick} className={className} title={label} aria-label={label}>
            {inner}
        </button>
    );
}

export default function StudyIconRail() {
    const pathname = usePathname();
    const router = useRouter();
    const [username, setUsername] = useState("");
    const [isAdmin, setIsAdmin] = useState(false);
    const [accountOpen, setAccountOpen] = useState(false);
    const accountRef = useRef<HTMLDivElement | null>(null);

    useEffect(() => {
        const user = localStorage.getItem("username") || "";
        setUsername(user);
        setIsAdmin(localStorage.getItem("is_admin") === "true" || user === "admin_");
    }, [pathname]);

    useEffect(() => {
        if (!accountOpen) return;
        const onPointer = (event: PointerEvent) => {
            if (!accountRef.current?.contains(event.target as Node)) {
                setAccountOpen(false);
            }
        };
        document.addEventListener("pointerdown", onPointer);
        return () => document.removeEventListener("pointerdown", onPointer);
    }, [accountOpen]);

    const handleNew = () => {
        if (pathname === "/" || pathname.startsWith("/folder/")) {
            requestStudyNew();
            return;
        }
        router.push("/?new=1");
    };

    const handleLogout = () => {
        localStorage.clear();
        window.location.href = "/login";
    };

    const initial = username ? username.charAt(0).toUpperCase() : "?";
    const onFolder = pathname.startsWith("/folder/");

    return (
        <aside className="h-screen w-12 bg-[#1A1916] border-r border-white/10 flex flex-col items-center py-2.5 sticky top-0 shrink-0 z-30">
            <Link href="/" title="Blop Study" className="mb-3 shrink-0">
                <img src="/logo.jpg" alt="Blop" className="w-8 h-8 rounded-[8px] object-cover" />
            </Link>

            <nav className="flex-1 flex flex-col items-center gap-1.5">
                <RailButton label="Home" href="/" active={pathname === "/"}>
                    <Home size={18} strokeWidth={pathname === "/" ? 2.4 : 2} />
                </RailButton>
                <RailButton label="Bibliothek" href="/" active={onFolder}>
                    <FolderOpen size={18} strokeWidth={onFolder ? 2.4 : 2} />
                </RailButton>
                <RailButton label="Neu" onClick={handleNew}>
                    <Plus size={18} />
                </RailButton>
                <RailButton label="Einstellungen" href="/settings" active={pathname === "/settings"}>
                    <Settings size={18} strokeWidth={pathname === "/settings" ? 2.4 : 2} />
                </RailButton>
                {isAdmin && (
                    <RailButton label="Admin" href="/admin" active={pathname.startsWith("/admin")}>
                        <Shield size={18} strokeWidth={pathname.startsWith("/admin") ? 2.4 : 2} />
                    </RailButton>
                )}
            </nav>

            <div ref={accountRef} className="relative mt-auto">
                <button
                    type="button"
                    onClick={() => setAccountOpen((open) => !open)}
                    title="Konto"
                    aria-label="Konto"
                    aria-expanded={accountOpen}
                    className={`w-10 h-10 rounded-[10px] flex items-center justify-center transition-colors ${
                        accountOpen ? "bg-[#5B9DFF]/18 text-[#5B9DFF]" : "text-[#9AA0AB] hover:bg-white/[0.06] hover:text-[#F4F5F7]"
                    }`}
                >
                    {username ? (
                        <span className="w-7 h-7 rounded-full bg-[#5B9DFF] text-white text-[11px] font-semibold flex items-center justify-center">
                            {initial}
                        </span>
                    ) : (
                        <User size={18} />
                    )}
                </button>
                {accountOpen && (
                    <div className="absolute left-full bottom-0 ml-2 w-48 bg-[#23252A] border border-[#45484F] rounded-[10px] shadow-2xl overflow-hidden z-50">
                        <div className="px-3 py-2.5 border-b border-[#45484F]">
                            <p className="text-[13px] font-semibold text-white truncate">{username || "Konto"}</p>
                            <p className="text-[11px] text-[#8B919C]">Tokens & Abo in den Einstellungen</p>
                        </div>
                        <Link
                            href="/settings"
                            onClick={() => setAccountOpen(false)}
                            className="flex min-h-10 items-center px-3 text-[13px] text-[#D5D8DE] hover:bg-[#3A3D45] hover:text-white"
                        >
                            Einstellungen
                        </Link>
                        <Link
                            href="/datenschutz"
                            onClick={() => setAccountOpen(false)}
                            className="flex min-h-10 items-center px-3 text-[13px] text-[#D5D8DE] hover:bg-[#3A3D45] hover:text-white"
                        >
                            Datenschutz
                        </Link>
                        <button
                            type="button"
                            onClick={handleLogout}
                            className="w-full min-h-10 text-left px-3 text-[13px] text-red-400 hover:bg-red-500/10"
                        >
                            Abmelden
                        </button>
                    </div>
                )}
            </div>
        </aside>
    );
}
