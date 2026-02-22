import type { NextConfig } from "next";

const nextConfig: NextConfig = {
  async rewrites() {
    // BACKEND_URL can override in Vercel env vars if URL changes
    const backendUrl = process.env.BACKEND_URL || "https://blop-v1s5.onrender.com";
    return [
      {
        source: "/api/:path*",
        destination: `${backendUrl}/api/:path*`,
      },
    ];
  },
};

export default nextConfig;
