import type { Metadata } from "next";
import { Inter } from "next/font/google";
import "./globals.css";
import Sidebar from "@/components/Sidebar";

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
        <div className="flex min-h-screen bg-[#1e1e1e]">
          {/* Sidebar - Hidden on mobile, visible on desktop */}
          <div className="hidden md:block">
            <Sidebar />
          </div>

          {/* Main Content - Full width on mobile, with margin on desktop */}
          <main className="flex-1 md:ml-[280px]">
            {children}
          </main>
        </div>
      </body>
    </html>
  );
}
