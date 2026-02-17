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
      <body className={`${inter.className} antialiased`}>
        <AuthCheck>
          <div className="flex min-h-screen bg-[#1e1e1e]">
            {/* Sidebar - Always visible on desktop, hidden on mobile */}
            <Sidebar />

            {/* Main Content - Pushed right by sidebar on desktop */}
            <main className="flex-1 w-full ml-0 md:ml-[280px]">
              {children}
            </main>
          </div>
        </AuthCheck>
      </body>
    </html>
  );
}
