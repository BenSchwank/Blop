"use client";

import React, { useState, useEffect } from 'react';
import { Search, Sparkles, Folder, FolderOpen, Plus } from 'lucide-react';

export default function Dashboard() {
  const [searchQuery, setSearchQuery] = useState('');
  const [loading, setLoading] = useState(false);

  const handleCreateFolder = () => {
    alert("Neuer Ordner clicked!");
  };

  const handleNewSummary = () => {
    alert("Neue AI-Zusammenfassung clicked!");
  };

  return (
    <div className="bg-[#1e1e1e] min-h-screen">
      {/* Main Container - Centered with max-width */}
      <div className="max-w-5xl mx-auto px-6 py-16">

        {/* Header Section */}
        <div className="mb-16">
          <h1 className="text-5xl font-bold text-white mb-4">
            Willkommen zurück! 👋
          </h1>
          <p className="text-xl text-gray-400">
            Bereit zum Lernen?
          </p>
        </div>

        {/* Search Bar */}
        <div className="mb-12">
          <div className="relative">
            <Search className="absolute left-5 top-1/2 -translate-y-1/2 text-gray-400" size={22} />
            <input
              type="text"
              placeholder="Suche nach Ordnern..."
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              className="w-full pl-14 pr-6 py-5 bg-[#252526] text-white text-lg rounded-2xl border-2 border-[#333] focus:border-[#5E5CE6] focus:outline-none transition-all"
            />
          </div>
        </div>

        {/* Action Buttons */}
        <div className="grid grid-cols-1 md:grid-cols-2 gap-6 mb-16">
          <button
            onClick={handleNewSummary}
            className="group flex items-center justify-center gap-4 bg-gradient-to-r from-[#5E5CE6] to-[#7D7AFF] text-white px-10 py-6 rounded-2xl font-semibold text-lg hover:shadow-2xl hover:shadow-[#5E5CE6]/30 transition-all duration-300 hover:scale-105"
          >
            <Sparkles size={24} className="group-hover:rotate-12 transition-transform" />
            <span>Neue AI-Zusammenfassung</span>
          </button>

          <button
            onClick={handleCreateFolder}
            className="group flex items-center justify-center gap-4 bg-[#252526] text-white px-10 py-6 rounded-2xl font-semibold text-lg border-2 border-[#333] hover:border-[#444] hover:bg-[#2d2d2d] transition-all duration-300 hover:scale-105"
          >
            <Folder size={24} className="group-hover:scale-110 transition-transform" />
            <span>Neuer Ordner</span>
          </button>
        </div>

        {/* Folders Section */}
        <div>
          <h2 className="text-3xl font-bold text-white mb-8">
            Meine Ordner
          </h2>

          {/* Empty State */}
          <div className="bg-[#252526] border-2 border-[#333] rounded-3xl p-20 text-center">
            <div className="flex justify-center mb-8">
              <div className="w-24 h-24 bg-[#1e1e1e] rounded-3xl flex items-center justify-center border-2 border-[#333]">
                <FolderOpen size={48} className="text-gray-600" />
              </div>
            </div>

            <h3 className="text-2xl font-bold text-white mb-4">
              Noch keine Ordner
            </h3>

            <p className="text-lg text-gray-400 mb-10 max-w-md mx-auto">
              Erstelle deinen ersten Ordner, um loszulegen!
            </p>

            <button
              onClick={handleCreateFolder}
              className="inline-flex items-center gap-3 bg-[#5E5CE6] text-white px-10 py-5 rounded-2xl font-semibold text-lg hover:bg-[#7D7AFF] transition-all duration-300 hover:scale-105"
            >
              <Plus size={24} />
              <span>Ordner erstellen</span>
            </button>
          </div>
        </div>

      </div>
    </div>
  );
}
