"use client";

import React, { useState } from 'react';
import { Search, Sparkles, Folder, FolderOpen, Plus } from 'lucide-react';

export default function Dashboard() {
  const [searchQuery, setSearchQuery] = useState('');

  const handleCreateFolder = () => {
    alert("Neuer Ordner clicked!");
  };

  const handleNewSummary = () => {
    alert("Neue AI-Zusammenfassung clicked!");
  };

  return (
    <div className="bg-[#1e1e1e] min-h-screen">
      {/* Main Container */}
      <div className="max-w-6xl mx-auto px-8 py-10">

        {/* Header */}
        <div className="mb-10">
          <h1 className="text-3xl font-bold text-white mb-2">
            Willkommen zurück! 👋
          </h1>
          <p className="text-base text-gray-400">
            Bereit zum Lernen?
          </p>
        </div>

        {/* Search Bar */}
        <div className="mb-8">
          <div className="relative">
            <Search className="absolute left-4 top-1/2 -translate-y-1/2 text-gray-400" size={20} />
            <input
              type="text"
              placeholder="Suche nach Ordnern..."
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              className="w-full pl-12 pr-4 py-3.5 bg-[#252526] text-white rounded-xl border border-[#333] focus:border-[#5E5CE6] focus:outline-none transition-all"
            />
          </div>
        </div>

        {/* Action Buttons */}
        <div className="grid grid-cols-1 md:grid-cols-2 gap-4 mb-10">
          <button
            onClick={handleNewSummary}
            className="flex items-center justify-center gap-3 bg-gradient-to-r from-[#5E5CE6] to-[#7D7AFF] text-white px-6 py-4 rounded-xl font-medium hover:shadow-lg hover:shadow-[#5E5CE6]/20 transition-all"
          >
            <Sparkles size={20} />
            <span>Neue AI-Zusammenfassung</span>
          </button>

          <button
            onClick={handleCreateFolder}
            className="flex items-center justify-center gap-3 bg-[#252526] text-white px-6 py-4 rounded-xl font-medium border border-[#333] hover:bg-[#2d2d2d] transition-all"
          >
            <Folder size={20} />
            <span>Neuer Ordner</span>
          </button>
        </div>

        {/* Folders Section */}
        <div>
          <h2 className="text-xl font-semibold text-white mb-6">
            Meine Ordner
          </h2>

          {/* Empty State */}
          <div className="bg-[#252526] border border-[#333] rounded-2xl p-12 text-center">
            <div className="flex justify-center mb-6">
              <div className="w-16 h-16 bg-[#1e1e1e] rounded-xl flex items-center justify-center border border-[#333]">
                <FolderOpen size={32} className="text-gray-600" />
              </div>
            </div>

            <h3 className="text-lg font-semibold text-white mb-2">
              Noch keine Ordner
            </h3>

            <p className="text-sm text-gray-400 mb-6">
              Erstelle deinen ersten Ordner, um loszulegen!
            </p>

            <button
              onClick={handleCreateFolder}
              className="inline-flex items-center gap-2 bg-[#5E5CE6] text-white px-6 py-3 rounded-xl font-medium hover:bg-[#7D7AFF] transition-all"
            >
              <Plus size={20} />
              <span>Ordner erstellen</span>
            </button>
          </div>
        </div>

      </div>
    </div>
  );
}
