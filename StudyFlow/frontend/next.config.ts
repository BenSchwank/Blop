import type { NextConfig } from "next";

const backendUrl = (
  process.env.BACKEND_URL || "https://blop-study-backend.onrender.com"
).replace(/\/$/, "");

const nextConfig: NextConfig = {
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
