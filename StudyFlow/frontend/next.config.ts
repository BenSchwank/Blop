import type { NextConfig } from "next";

const backendUrl = (
  process.env.BACKEND_URL || "https://blop-study-backend.onrender.com"
).replace(/\/$/, "");

const nextConfig: NextConfig = {
  // Für <video>/<audio>: Browser lädt direkt vom API-Host (Vercel-Rewrites können große Binärantworten leeren/failen)
  env: {
    NEXT_PUBLIC_BACKEND_ORIGIN: backendUrl,
  },
  async rewrites() {
    return [
      {
        source: "/api/:path*",
        destination: `${backendUrl}/api/:path*`,
      },
    ];
  },
};

export default nextConfig;
