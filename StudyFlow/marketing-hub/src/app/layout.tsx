import type { Metadata } from "next";
import "./globals.css";

export const metadata: Metadata = {
  title: "Blop Marketing Hub",
  description: "Interner Marketing-Hub fuer Blop Devlogs.",
};

export default function RootLayout({
  children,
}: Readonly<{
  children: React.ReactNode;
}>) {
  return (
    <html lang="de">
      <body>{children}</body>
    </html>
  );
}
