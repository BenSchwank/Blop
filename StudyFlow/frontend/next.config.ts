import type { NextConfig } from "next";

const backendUrl = (
  process.env.BACKEND_URL || "https://blop-study-backend.onrender.com"
).replace(/\/$/, "");

const nextConfig: NextConfig = {
  env: {
    NEXT_PUBLIC_BACKEND_ORIGIN: backendUrl,
  },
  async headers() {
    const nativeNoStoreHeaders = [
      { key: "Cache-Control", value: "no-store, no-cache, must-revalidate, max-age=0" },
      { key: "Pragma", value: "no-cache" },
      { key: "Expires", value: "0" },
    ];
    return [
      {
        source: "/",
        has: [{ type: "query", key: "native", value: "1" }],
        headers: nativeNoStoreHeaders,
      },
      {
        source: "/login",
        has: [{ type: "query", key: "native", value: "1" }],
        headers: nativeNoStoreHeaders,
      },
    ];
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
