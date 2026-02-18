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

  // Create Modal State
  const [isCreateOpen, setIsCreateOpen] = useState(false);
  const [newFolderName, setNewFolderName] = useState("");
  const [isCreating, setIsCreating] = useState(false);

  const API_BASE = "http://localhost:8000/api";

  const fetchFolders = async () => {
    try {
      const username = localStorage.getItem("username");
      if (!username) return;

      const res = await fetch(`${API_BASE}/folders?username=${username}`);
      if (res.ok) {
        const data = await res.json();
        setFolders(data);
      }
    } catch (error) {
      console.error("Failed to fetch folders:", error);
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
      {/* Main Container - Reduced max-width for cleaner look */}
      <div className="max-w-4xl mx-auto px-6 py-8">

        {/* Header */}
        <div className="mb-6 flex items-center justify-between">
          <div>
            <h1 className="text-xl font-bold text-white mb-0.5">
              Willkommen zurück! 👋
            </h1>
            <p className="text-xs text-gray-400">
              Bereit zum Lernen?
            </p>
          </div>
        </div>

        {/* Search Bar & Actions Row - Combine to save space */}
        <div className="flex flex-col md:flex-row gap-4 mb-6">
          <div className="relative flex-1">
            <div className="absolute left-3 top-1/2 -translate-y-1/2 text-gray-400 pointer-events-none flex items-center justify-center">
              <Search size={16} />
            </div>
            <input
              type="text"
              placeholder="Suche nach Ordnern..."
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              className="w-full pl-10 pr-4 py-2 bg-[#252526] text-white text-sm rounded-lg border border-[#333] focus:border-[#5E5CE6] focus:outline-none transition-all placeholder:text-gray-500"
            />
          </div>

          <div className="flex gap-2">
            <button
              onClick={() => alert("Coming soon!")}
              className="flex items-center gap-2 bg-gradient-to-r from-[#5E5CE6] to-[#7D7AFF] text-white px-3 py-2 rounded-lg text-xs font-medium hover:shadow-lg hover:shadow-[#5E5CE6]/20 transition-all whitespace-nowrap"
            >
              <Sparkles size={14} />
              <span>AI-Summary</span>
            </button>

            <button
              onClick={() => setIsCreateOpen(true)}
              className="flex items-center gap-2 bg-[#252526] text-white px-3 py-2 rounded-lg text-xs font-medium border border-[#333] hover:bg-[#2d2d2d] transition-all whitespace-nowrap"
            >
              <Folder size={14} />
              <span>Ordner</span>
            </button>
          </div>
        </div>

        {/* Folders Section */}
        <div>
          <h2 className="text-sm font-semibold text-gray-300 mb-3 uppercase tracking-wider">
            Meine Ordner
          </h2>

          {loading ? (
            <div className="flex justify-center py-12">
              <Loader2 className="animate-spin text-[#5E5CE6]" size={24} />
            </div>
          ) : folders.length === 0 ? (
            /* Empty State - Compact */
            <div className="bg-[#252526] border border-[#333] rounded-lg p-8 text-center border-dashed">
              <div className="flex justify-center mb-3">
                <div className="w-10 h-10 bg-[#1e1e1e] rounded-lg flex items-center justify-center border border-[#333]">
                  <FolderOpen size={20} className="text-gray-500" />
                </div>
              </div>

              <h3 className="text-sm font-medium text-white mb-1">
                Noch keine Ordner
              </h3>

              <p className="text-xs text-gray-500 mb-4">
                Erstelle deinen ersten Ordner.
              </p>

              <button
                onClick={() => setIsCreateOpen(true)}
                className="inline-flex items-center gap-1.5 bg-[#333] hover:bg-[#444] text-white px-3 py-1.5 rounded-md text-xs transition-all border border-[#444]"
              >
                <Plus size={12} />
                <span>Erstellen</span>
              </button>
            </div>
          ) : (
            /* Folder Grid */
            <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-3">
              {filteredFolders.map((folder) => (
                <div
                  key={folder.id}
                  onClick={() => router.push(`/folder/${folder.id}`)}
                  className="group relative bg-[#252526] border border-[#333] p-4 rounded-lg hover:border-[#5E5CE6]/50 transition-all cursor-pointer flex items-center justify-between"
                >
                  <div className="flex items-center gap-3">
                    <div className="text-[#5E5CE6]">
                      <Folder size={18} fill="#5E5CE6" fillOpacity={0.2} />
                    </div>
                    <span className="text-sm font-medium text-gray-200 group-hover:text-white transition-colors truncate max-w-[120px]">
                      {folder.name}
                    </span>
                  </div>

                  <button
                    onClick={(e) => handleDeleteFolder(folder.id, e)}
                    className="opacity-0 group-hover:opacity-100 p-1.5 text-gray-500 hover:text-red-400 hover:bg-[#333] rounded transition-all"
                  >
                    <Trash2 size={12} />
                  </button>
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
