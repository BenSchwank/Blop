"use client";

import { usePathname } from "next/navigation";
import Sidebar from "@/components/Sidebar";
import MobileNav from "@/components/MobileNav";

export default function ClientLayout({
    children,
}: {
    children: React.ReactNode;
}) {
    const pathname = usePathname();
    const isAuthPage = pathname === "/login" || pathname === "/datenschutz";

    // Für Login/Datenschutz-Seiten keine Sidebar anzeigen
    if (isAuthPage) {
        return <main className="min-h-screen w-full relative">{children}</main>;
    }

    // Für alle restlichen Seiten: Das Grid mit Sidebar
    return (
        <div className="grid grid-cols-1 md:grid-cols-[220px_1fr] gap-0 min-h-screen">
            <div className="hidden md:block">
                <Sidebar />
            </div>
            <main className="min-h-screen overflow-x-hidden md:pl-4 pb-20 md:pb-0">
                {children}
            </main>
            <MobileNav />
        </div>
    );
}
