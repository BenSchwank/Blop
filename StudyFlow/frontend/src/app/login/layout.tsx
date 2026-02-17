import type { Metadata } from "next";
import { Inter } from "next/font/google";
import "../globals.css";

const inter = Inter({ subsets: ["latin"] });

export const metadata: Metadata = {
    title: "Login - Blop Study",
    description: "Melde dich bei Blop Study an",
};

export default function LoginLayout({
    children,
}: Readonly<{
    children: React.ReactNode;
}>) {
    return (
        <html lang="de">
            <body className={`${inter.className} antialiased`}>
                {children}
            </body>
        </html>
    );
}
