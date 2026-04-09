"use client";

import { useState, useEffect } from "react";
import { usePathname } from "next/navigation";
import Sidebar from "@/components/Sidebar";
import MobileNav from "@/components/MobileNav";
import CommandMenu from "@/components/CommandMenu";
import GlobalQueueStatus from "@/components/GlobalQueueStatus";
import { PanelLeftOpen, PanelLeftClose } from "lucide-react";

export default function ClientLayout({
    children,
}: {
    children: React.ReactNode;
}) {
    const pathname = usePathname();
    const isAuthPage = pathname === "/login" || pathname === "/datenschutz";

    // Sidebar starts expanded by default on desktop
    const [sidebarCollapsed, setSidebarCollapsed] = useState(false);

    // Persist preference across navigation
    useEffect(() => {
        const saved = sessionStorage.getItem("sidebarCollapsed");
        if (saved !== null) {
            setSidebarCollapsed(saved === "true");
        }
    }, []);

    const toggleSidebar = () => {
        const next = !sidebarCollapsed;
        setSidebarCollapsed(next);
        sessionStorage.setItem("sidebarCollapsed", String(next));
    };

    // For Auth pages (login/datenschutz), render WITHOUT sidebar
    if (isAuthPage) {
        return (
            <div className="flex min-h-screen">
                <main className="flex-1 min-h-screen relative">{children}</main>
            </div>
        );
    }

    return (
        <div className="flex min-h-screen">
            {/* Command Menu Modal (Ctrl/Cmd+K) */}
            <CommandMenu />

            {/* Sidebar panel (visible on desktop) */}
            <div className="hidden md:flex">
                <Sidebar isCollapsed={sidebarCollapsed} onToggle={toggleSidebar} />
            </div>

            {/* Main content */}
            <main
                className="flex-1 min-h-screen overflow-x-hidden pb-[var(--mobile-nav-reserve)] md:pb-0 print:pb-0 transition-all duration-300 relative"
            >
                {children}
            </main>

            <GlobalQueueStatus />

            <MobileNav />
        </div>
    );
}
