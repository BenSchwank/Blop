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
          {/* Sidebar - Fixed position, hidden on mobile */}
          <Sidebar />

          {/* Main Content - Margin to account for sidebar */}
          <main className="min-h-screen ml-0 md:ml-[280px]">
            {children}
          </main>
        </AuthCheck>
      </body>
    </html>
  );
}
