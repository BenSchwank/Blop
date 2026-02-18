import type { Metadata } from "next";
import { Inter } from "next/font/google";
import "./globals.css";
import Sidebar from "@/components/Sidebar";
import MobileNav from "@/components/MobileNav";
import AuthCheck from "@/components/AuthCheck";

const inter = Inter({ subsets: ["latin"] });

export const metadata: Metadata = {
  title: "Blop Study - AI Lernassistent",
  description: "Dein intelligenter Begleiter für effizientes Lernen",
};

export default function RootLayout({
  children,
}: Readonly<{
  children: React.ReactNode;
}>) {
  return (
    <html lang="de">
      <body className={`${inter.className} antialiased bg-[#1e1e1e]`}>
        <AuthCheck>
          {/* Grid Layout - Sidebar + Content with larger gap for safety */}
          <div className="grid grid-cols-1 md:grid-cols-[220px_1fr] gap-0 min-h-screen">
            {/* Sidebar - Hidden on mobile */}
            <div className="hidden md:block">
              <Sidebar />
            </div>

            {/* Main Content with extra padding */}
            <main className="min-h-screen overflow-x-hidden md:pl-4 pb-20 md:pb-0">
              {children}
            </main>

            {/* Mobile Navigation - Visible only on mobile */}
            <MobileNav />
          </div>
        </AuthCheck>
      </body>
    </html>
  );
}
