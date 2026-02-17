"use client";

import React, { useState, useEffect } from 'react';
import { Search, Plus, Folder, MoreVertical, Sparkles } from 'lucide-react';
import { api, Folder as FolderType } from '@/lib/api';

export default function Dashboard() {
  const [searchQuery, setSearchQuery] = useState('');
  const [folders, setFolders] = useState<FolderType[]>([]);
  const [loading, setLoading] = useState(true);
  const username = "User";

  useEffect(() => {
    const loadData = async () => {
      try {
        const fetchedFolders = await api.getFolders(username);
        setFolders(fetchedFolders);
      } catch (e) {
        console.error(e);
      } finally {
        setLoading(false);
      }
    };
    loadData();
  }, []);

  const handleCreateFolder = async () => {
    const name = prompt("Ordnername:");
    if (name) {
      await api.createFolder(name, username);
      const updated = await api.getFolders(username);
      setFolders(updated);
    }
  };

  // Placeholder for new summary function
  const handleNewSummary = () => {
    alert("Neue AI-Zusammenfassung clicked!");
  };

  return (
    <div className="bg-[#1e1e1e] text-white min-h-screen">
      {/* Centered container with max-width and generous padding */}
      <div className="max-w-6xl mx-auto px-8 py-12">
        {/* Header */}
        <header className="mb-12">
          <h1 className="text-4xl font-bold mb-3">Willkommen zurück! 👋</h1>
          <p className="text-lg text-[#888]">Bereit zum Lernen?</p>
        </header>

        {/* Search Bar */}
        <div className="mb-10">
          <div className="relative">
            <Search className="absolute left-4 top-1/2 -translate-y-1/2 text-[#888]" size={20} />
            <input
              type="text"
              placeholder="Suche nach Ordnern..."
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              className="w-full bg-[#252526] text-white px-6 py-4 rounded-xl border border-[#333] focus:border-[#5E5CE6] focus:outline-none transition-colors text-base pl-12"
            />
          </div>
        </div>

        {/* Action Buttons */}
        <div className="grid grid-cols-1 sm:grid-cols-2 gap-4 mb-12">
          <button
            onClick={handleNewSummary}
            className="flex items-center justify-center gap-3 bg-gradient-to-r from-[#5E5CE6] to-[#7D7AFF] text-white px-8 py-5 rounded-xl font-semibold text-base hover:shadow-lg hover:shadow-[#5E5CE6]/20 transition-all active:scale-95"
          >
            <Sparkles size={22} />
            Neue AI-Zusammenfassung
          </button>
          <button
            onClick={handleCreateFolder}
            className="flex items-center justify-center gap-3 bg-[#252526] text-white px-8 py-5 rounded-xl font-semibold text-base border border-[#333] hover:bg-[#2d2d2d] transition-all active:scale-95"
          >
            <Folder size={22} /> {/* Changed FolderPlus to Folder as it's not imported */}
            Neuer Ordner
          </button>
        </div>

        {/* My Folders Section */}
        <section>
          <h2 className="text-2xl font-semibold mb-6">Meine Ordner</h2>

          {loading ? (
            <div className="flex items-center justify-center py-20">
              <div className="animate-spin rounded-full h-10 w-10 border-3 border-[#5E5CE6] border-t-transparent" />
            </div>
          ) : folders.length === 0 ? (
            <div className="blop-card p-8 sm:p-12 text-center">
              <div className="w-16 h-16 rounded-2xl bg-[#333] flex items-center justify-center mx-auto mb-4">
                <Folder className="w-8 h-8 text-[#888]" strokeWidth={2} />
              </div>
              <h3 className="text-xl font-semibold text-white mb-2">Noch keine Ordner</h3>
              <p className="text-[#888] mb-6 text-[15px]">Erstelle deinen ersten Ordner, um loszulegen!</p>
              <button
                onClick={handleCreateFolder}
                className="px-6 py-3.5 bg-[#5E5CE6] rounded-xl text-white font-semibold hover:bg-[#7D7AFF] active:bg-[#4D4BC4] transition-all inline-flex items-center gap-2 min-h-[48px] text-[15px]"
              >
                <Plus size={20} strokeWidth={2} />
                Ordner erstellen
              </button>
            </div>
          ) : (
            <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-4">
              {folders.map((folder) => (
                <div
                  key={folder.id}
                  className="blop-card p-5 hover:border-[#5E5CE6] hover:shadow-[0_0_0_1px_#5E5CE6] transition-all cursor-pointer group active:scale-[0.98]"
                >
                  <div className="flex items-start justify-between mb-4">
                    <div className="w-12 h-12 rounded-xl bg-[#5E5CE6]/10 flex items-center justify-center">
                      <Folder className="w-6 h-6 text-[#5E5CE6]" strokeWidth={2} />
                    </div>
                    <button className="opacity-0 group-hover:opacity-100 p-2 hover:bg-[#333] rounded-lg transition-all min-h-[36px] min-w-[36px] flex items-center justify-center">
                      <MoreVertical size={18} className="text-[#888]" />
                    </button>
                  </div>

                  <h3 className="text-base font-semibold text-white mb-1 group-hover:text-[#5E5CE6] transition-colors">
                    {folder.name}
                  </h3>
                  <p className="text-sm text-[#888]">
                    {folder.files?.length || 0} Dateien
                  </p>
                </div>
              ))}

              {/* Add New Card */}
              <button
                onClick={handleCreateFolder}
                className="blop-card p-5 border-dashed hover:border-[#5E5CE6] hover:bg-[#252526] transition-all flex flex-col items-center justify-center min-h-[160px] group active:scale-[0.98]"
              >
                <div className="w-12 h-12 rounded-xl bg-[#333] flex items-center justify-center mb-3 group-hover:bg-[#5E5CE6]/10 transition-all">
                  <Plus className="w-6 h-6 text-[#888] group-hover:text-[#5E5CE6] transition-colors" strokeWidth={2} />
                </div>
                <span className="text-sm font-medium text-[#888] group-hover:text-[#5E5CE6] transition-colors">
                  Neuer Ordner
                </span>
              </button>
            </div>
          )}
        </section>
      </div>
    </div>
  );
}
