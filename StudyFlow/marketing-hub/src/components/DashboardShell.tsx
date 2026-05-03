"use client";

import { useRouter } from "next/navigation";
import UploadPanel from "@/components/UploadPanel";
import PackPanel from "@/components/PackPanel";

export default function DashboardShell() {
  const router = useRouter();

  async function logout() {
    await fetch("/api/auth/logout", { method: "POST" });
    router.push("/login");
    router.refresh();
  }

  return (
    <main className="mx-auto min-h-screen w-full max-w-5xl p-6">
      <div className="mb-6 flex items-center justify-between">
        <div>
          <h1 className="text-3xl font-bold">Marketing-Hub</h1>
          <p className="text-sm text-[#A9ADC8]">Interne Blop Devlog Automatisierung</p>
        </div>
        <button onClick={logout} className="rounded-lg border border-white/25 px-4 py-2 text-sm hover:border-[#5E5CE6]">
          Logout
        </button>
      </div>
      <UploadPanel />
      <PackPanel />
    </main>
  );
}
