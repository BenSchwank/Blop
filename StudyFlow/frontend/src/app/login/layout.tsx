import type { Metadata } from "next";

export const metadata: Metadata = {
    title: "Login - Blop Study",
    description: "Melde dich bei Blop Study an",
};

export default function LoginLayout({
    children,
}: Readonly<{
    children: React.ReactNode;
}>) {
    return children;
}
