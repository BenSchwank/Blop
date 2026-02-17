import type { Metadata } from "next";
import { Inter } from "next/font/google";
import "./globals.css";
import Sidebar from "@/components/Sidebar";
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
          {/* Grid Layout - Sidebar + Content */}
          <div className="grid grid-cols-1 md:grid-cols-[280px_1fr] min-h-screen">
            {/* Sidebar - Hidden on mobile */}
            <div className="hidden md:block">
              <Sidebar />
            </div>

            {/* Main Content */}
            <main className="min-h-screen overflow-x-hidden">
              {children}
            </main>
          </div>
        </AuthCheck>
      </body>
    </html>
  );
}
