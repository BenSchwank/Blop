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
            <Search className="absolute left-3 top-1/2 -translate-y-1/2 text-gray-400" size={16} />
            <input
              type="text"
              placeholder="Suche nach Ordnern..."
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              className="w-full pl-9 pr-4 py-2 bg-[#252526] text-white text-sm rounded-lg border border-[#333] focus:border-[#5E5CE6] focus:outline-none transition-all placeholder:text-xs"
            />
          </div>

          <div className="flex gap-2">
            <button
              onClick={handleNewSummary}
              className="flex items-center gap-2 bg-gradient-to-r from-[#5E5CE6] to-[#7D7AFF] text-white px-3 py-2 rounded-lg text-xs font-medium hover:shadow-lg hover:shadow-[#5E5CE6]/20 transition-all whitespace-nowrap"
            >
              <Sparkles size={14} />
              <span>AI-Summary</span>
            </button>

            <button
              onClick={handleCreateFolder}
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

          {/* Empty State - Compact */}
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
              onClick={handleCreateFolder}
              className="inline-flex items-center gap-1.5 bg-[#333] hover:bg-[#444] text-white px-3 py-1.5 rounded-md text-xs transition-all border border-[#444]"
            >
              <Plus size={12} />
              <span>Erstellen</span>
            </button>
          </div>
        </div>

      </div>
    </div>
  );
}
