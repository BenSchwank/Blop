import type { Metadata } from "next";
import { Inter } from "next/font/google";
import "./globals.css";
import ClientLayout from "@/components/ClientLayout";
import AuthCheck from "@/components/AuthCheck";

const inter = Inter({ subsets: ["latin"] });

export const metadata: Metadata = {
  title: "Blop Study - AI Lernassistent",
  description: "Dein intelligenter Begleiter für effizientes Lernen",
};

export const viewport = {
  width: "device-width",
  initialScale: 1,
  viewportFit: "cover",
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
          <ClientLayout>
            {children}
          </ClientLayout>
        </AuthCheck>
      </body>
    </html>
  );
}
