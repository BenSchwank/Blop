"use client";

import { useState, useEffect } from "react";
import { usePathname } from "next/navigation";
import Sidebar from "@/components/Sidebar";
import MobileNav from "@/components/MobileNav";
import { PanelLeftOpen, PanelLeftClose } from "lucide-react";

export default function ClientLayout({
    children,
}: {
    children: React.ReactNode;
}) {
    const pathname = usePathname();
    const isAuthPage = pathname === "/login" || pathname === "/datenschutz";

    // Sidebar starts collapsed – expand on user click
    const [sidebarOpen, setSidebarOpen] = useState(false);

    // Persist preference across navigation, but collapse again after fresh login
    useEffect(() => {
        const saved = sessionStorage.getItem("sidebarOpen");
        setSidebarOpen(saved === "true");
    }, []);

    const toggleSidebar = () => {
        const next = !sidebarOpen;
        setSidebarOpen(next);
        sessionStorage.setItem("sidebarOpen", String(next));
    };

    // Login / Datenschutz – no sidebar at all
    if (isAuthPage) {
        return <main className="min-h-screen w-full relative">{children}</main>;
    }

    return (
        <div className="flex min-h-screen">
            {/* Sidebar panel – slides in/out */}
            <aside
                className={`
                    hidden md:flex flex-col
                    transition-all duration-300 ease-in-out overflow-hidden
                    bg-[#1e1e1e] border-r border-[#333] sticky top-0 h-screen
                    ${sidebarOpen ? "w-[220px]" : "w-0"}
                `}
            >
                {/*
                  Only render Sidebar content when open, so it doesn't
                  overflow or ghost-render when collapsed.
                */}
                {sidebarOpen && <Sidebar />}
            </aside>

            {/* Toggle button (always visible) */}
            <button
                onClick={toggleSidebar}
                className="
                    hidden md:flex
                    fixed top-4 left-4 z-50
                    items-center justify-center
                    w-9 h-9 rounded-lg
                    bg-[#252526] border border-[#3a3a3a]
                    text-[#aaa] hover:text-white hover:bg-[#333]
                    transition-all
                "
                title={sidebarOpen ? "Sidebar einklappen" : "Sidebar ausklappen"}
            >
                {sidebarOpen
                    ? <PanelLeftClose size={18} />
                    : <PanelLeftOpen size={18} />
                }
            </button>

            {/* Main content – shifts left when sidebar is open */}
            <main
                className={`
                    flex-1 min-h-screen overflow-x-hidden pb-20 md:pb-0
                    transition-all duration-300
                    ${sidebarOpen ? "md:pl-4" : "md:pl-14"}
                `}
            >
                {children}
            </main>

            <MobileNav />
        </div>
    );
}
