"use client";

export default function NativeSmoke() {
  return (
    <div className="min-h-screen bg-[#0B0B1A] flex items-center justify-center">
      <div className="px-6 py-5 rounded-2xl border border-[#2A2A40] bg-[#151525] max-w-md text-center">
        <h1 className="text-xl font-bold text-white mb-2">
          Blop Study Native Smoke Test
        </h1>
        <p className="text-sm text-gray-400">
          Diese Seite wird nur vom eingebetteten Blop‑WebView aufgerufen.
          Wenn du sie siehst, funktioniert die grundlegende Verbindung
          von App&nbsp;&rarr;&nbsp;Web ohne ERR_CACHE_MISS.
        </p>
      </div>
    </div>
  );
}

