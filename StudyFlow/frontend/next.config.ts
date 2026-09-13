import type { NextConfig } from "next";

const backendUrl = (
  process.env.BACKEND_URL || "https://blop-study-backend.onrender.com"
).replace(/\/$/, "");

/** Separate Vercel project (Marketing Hub). Rewrites must live here so they run before /api/:path* → Render. */
const marketingHubUrl = (
  process.env.MARKETING_HUB_PUBLIC_URL || "https://blop-marketinghub.vercel.app"
).replace(/\/$/, "");

const nextConfig: NextConfig = {
  env: {
    NEXT_PUBLIC_BACKEND_ORIGIN: backendUrl,
  },
  async redirects() {
    // Keep auth localStorage on one origin. Stripe return URLs use BLOP_APP_PUBLIC_URL
    // (typically www); apex ↔ www mismatches look like random logouts.
    return [
      {
        source: "/:path*",
        has: [{ type: "host", value: "blop-study.com" }],
        destination: "https://www.blop-study.com/:path*",
        permanent: false,
      },
    ];
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
      {
        source: "/native-smoke",
        headers: nativeNoStoreHeaders,
      },
    ];
  },
  async rewrites() {
    return [
      {
        source: "/marketing",
        destination: `${marketingHubUrl}/`,
      },
      {
        source: "/marketing/",
        destination: `${marketingHubUrl}/`,
      },
      {
        source: "/marketing/:path*",
        destination: `${marketingHubUrl}/:path*`,
      },
      {
        source: "/api/marketing/:path*",
        destination: `${marketingHubUrl}/api/marketing/:path*`,
      },
      {
        source: "/api/:path*",
        destination: `${backendUrl}/api/:path*`,
      },
    ];
  },
};

export default nextConfig;
