"use client";

import React, { useState, useEffect } from 'react';
import { Search, Sparkles, Folder, FolderOpen, Plus, Trash2, X, Loader2 } from 'lucide-react';
import { useRouter } from 'next/navigation';

interface FolderData {
  id: string;
  name: string;
}

export default function Dashboard() {
  const router = useRouter();
  const [searchQuery, setSearchQuery] = useState('');
  const [folders, setFolders] = useState<FolderData[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState("");

  // Create Modal State
  const [isCreateOpen, setIsCreateOpen] = useState(false);
  const [newFolderName, setNewFolderName] = useState("");
  const [isCreating, setIsCreating] = useState(false);

  const API_BASE = process.env.NEXT_PUBLIC_API_URL || "http://localhost:8000/api";

  const fetchFolders = async () => {
    try {
      const username = localStorage.getItem("username");
      if (!username) return;

      const res = await fetch(`${API_BASE}/folders?username=${username}`);
      if (res.ok) {
        const data = await res.json();
        setFolders(data);
        setError("");
      } else {
        setError("Server antwortet nicht (Fehler " + res.status + ")");
      }
    } catch (err) {
      console.error("Failed to fetch folders:", err);
      setError("Keine Verbindung zum Backend. Laufen die Server?");
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    fetchFolders();
  }, []);

  const handleCreateFolder = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!newFolderName.trim()) return;

    setIsCreating(true);
    try {
      const username = localStorage.getItem("username");
      const res = await fetch(`${API_BASE}/folders`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ name: newFolderName, username }),
      });

      if (res.ok) {
        setNewFolderName("");
        setIsCreateOpen(false);
        fetchFolders(); // Refresh list
      }
    } catch (error) {
      console.error("Error creating folder:", error);
    } finally {
      setIsCreating(false);
    }
  };

  const handleDeleteFolder = async (folderId: string, e: React.MouseEvent) => {
    e.stopPropagation(); // Prevent folder click
    if (!confirm("Ordner wirklich löschen? Alle Inhalte gehen verloren.")) return;

    try {
      const username = localStorage.getItem("username");
      await fetch(`${API_BASE}/folders/${folderId}?username=${username}`, {
        method: "DELETE",
      });
      fetchFolders(); // Refresh
    } catch (error) {
      console.error("Error deleting folder:", error);
    }
  };

  const filteredFolders = folders.filter(f =>
    f.name.toLowerCase().includes(searchQuery.toLowerCase())
  );

  return (
    <div className="bg-[#1e1e1e] min-h-screen relative">
      {/* Main Container - Professional Width */}
      <div className="max-w-7xl mx-auto px-8 py-12">

        {/* Header */}
        <div className="mb-10 flex items-center justify-between">
          <div>
            <h1 className="text-3xl font-bold text-white mb-2">
              Willkommen zurück! 👋
            </h1>
            <p className="text-base text-gray-400">
              Bereit zum Lernen? Hier sind deine Ordner.
            </p>
          </div>
        </div>

        {/* Search Bar & Actions Row */}
        <div className="flex flex-col md:flex-row items-start md:items-center gap-4 mb-10">
          <div className="relative w-full md:max-w-2xl">
            <div className="absolute inset-y-0 left-0 pl-5 flex items-center pointer-events-none text-gray-400 pb-0.5">
              <Search size={22} />
            </div>
            <input
              type="text"
              placeholder="Suche nach Ordnern..."
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              className="w-full pl-14 pr-6 py-4 bg-[#252526] text-white text-base rounded-2xl border border-[#333] focus:border-[#5E5CE6] focus:outline-none transition-all placeholder:text-gray-500 shadow-sm"
            />
          </div>

          <div className="flex gap-4">
            <button
              onClick={() => alert("Coming soon!")}
              className="flex items-center gap-3 bg-gradient-to-r from-[#5E5CE6] to-[#7D7AFF] text-white px-6 py-4 rounded-xl text-sm font-semibold hover:shadow-lg hover:shadow-[#5E5CE6]/25 transition-all whitespace-nowrap"
            >
              <Sparkles size={20} />
              <span>AI-Summary</span>
            </button>

            <button
              onClick={() => setIsCreateOpen(true)}
              className="flex items-center gap-3 bg-[#252526] text-white px-6 py-4 rounded-xl text-sm font-semibold border border-[#333] hover:bg-[#2d2d2d] transition-all whitespace-nowrap shadow-sm"
            >
              <Folder size={20} />
              <span>Neuer Ordner</span>
            </button>
          </div>
        </div>

        {/* Folders Section */}
        <div>
          <h2 className="text-lg font-semibold text-gray-300 mb-6 uppercase tracking-wider pl-1">
            Meine Ordner
          </h2>

          {loading ? (
            <div className="flex justify-center py-20">
              <Loader2 className="animate-spin text-[#5E5CE6]" size={32} />
            </div>
          ) : error ? (
            <div className="bg-[#252526] border border-red-500/30 rounded-2xl p-10 text-center">
              <div className="text-red-400 font-semibold mb-2">{error}</div>
              <p className="text-gray-500 text-sm mb-4">Prüfe die Render-URL in den Vercel Settings.</p>
              <button onClick={fetchFolders} className="text-sm bg-red-500/10 text-red-400 px-4 py-2 rounded-lg hover:bg-red-500/20 transition-colors">
                Erneut versuchen
              </button>
            </div>
          ) : folders.length === 0 ? (
            /* Empty State */
            <div className="bg-[#252526] border border-[#333] rounded-2xl p-16 text-center border-dashed">
              <div className="flex justify-center mb-6">
                <div className="w-16 h-16 bg-[#1e1e1e] rounded-2xl flex items-center justify-center border border-[#333] shadow-inner">
                  <FolderOpen size={32} className="text-gray-500" />
                </div>
              </div>

              <h3 className="text-xl font-semibold text-white mb-2">
                Noch keine Ordner
              </h3>

              <p className="text-base text-gray-400 mb-8 max-w-md mx-auto">
                Erstelle deinen ersten Ordner, um deine Lernmaterialien zu organisieren.
              </p>

              <button
                onClick={() => setIsCreateOpen(true)}
                className="inline-flex items-center gap-2 bg-[#333] hover:bg-[#444] text-white px-5 py-2.5 rounded-lg text-sm font-medium transition-all border border-[#444]"
              >
                <Plus size={18} />
                <span>Ordner erstellen</span>
              </button>
            </div>
          ) : (
            /* Folder Grid */
            <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 xl:grid-cols-4 gap-6">
              {filteredFolders.map((folder) => (
                <div
                  key={folder.id}
                  onClick={() => router.push(`/folder/${folder.id}`)}
                  className="group relative bg-[#252526] border border-[#333] p-6 rounded-2xl hover:border-[#5E5CE6]/60 transition-all cursor-pointer flex flex-col gap-4 shadow-sm hover:shadow-md hover:shadow-black/20"
                >
                  <div className="flex items-start justify-between">
                    <div className="p-3 bg-[#1e1e1e] rounded-xl border border-[#333] group-hover:border-[#5E5CE6]/30 transition-colors">
                      <Folder size={24} className="text-[#5E5CE6]" fill="currentColor" fillOpacity={0.15} />
                    </div>
                    <button
                      onClick={(e) => handleDeleteFolder(folder.id, e)}
                      className="opacity-0 group-hover:opacity-100 p-2 text-gray-500 hover:text-red-400 hover:bg-[#333] rounded-lg transition-all"
                    >
                      <Trash2 size={16} />
                    </button>
                  </div>

                  <div>
                    <h3 className="text-base font-semibold text-white group-hover:text-[#5E5CE6] transition-colors truncate">
                      {folder.name}
                    </h3>
                    <p className="text-xs text-gray-500 mt-1">
                      0 Dateien
                    </p>
                  </div>
                </div>
              ))}
            </div>
          )}
        </div>

      </div>

      {/* Create Folder Modal */}
      {isCreateOpen && (
        <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/60 backdrop-blur-sm p-4">
          <div className="bg-[#1e1e1e] border border-[#333] rounded-xl w-full max-w-sm p-6 shadow-2xl animate-in fade-in zoom-in duration-200">
            <div className="flex justify-between items-center mb-4">
              <h3 className="text-lg font-semibold text-white">Neuer Ordner</h3>
              <button onClick={() => setIsCreateOpen(false)} className="text-gray-400 hover:text-white">
                <X size={20} />
              </button>
            </div>

            <form onSubmit={handleCreateFolder}>
              <input
                autoFocus
                type="text"
                placeholder="Name des Ordners..."
                value={newFolderName}
                onChange={(e) => setNewFolderName(e.target.value)}
                className="w-full bg-[#252526] border border-[#333] text-white rounded-lg px-4 py-2.5 mb-4 focus:ring-2 focus:ring-[#5E5CE6] focus:border-transparent outline-none placeholder:text-gray-500"
              />

              <div className="flex justify-end gap-2">
                <button
                  type="button"
                  onClick={() => setIsCreateOpen(false)}
                  className="px-4 py-2 text-sm font-medium text-gray-300 hover:text-white transition-colors"
                >
                  Abbrechen
                </button>
                <button
                  type="submit"
                  disabled={!newFolderName.trim() || isCreating}
                  className="px-4 py-2 text-sm font-medium bg-[#5E5CE6] hover:bg-[#4d4ac9] text-white rounded-lg transition-colors disabled:opacity-50 disabled:cursor-not-allowed flex items-center gap-2"
                >
                  {isCreating && <Loader2 size={14} className="animate-spin" />}
                  Erstellen
                </button>
              </div>
            </form>
          </div>
        </div>
      )}

    </div>
  );
}
