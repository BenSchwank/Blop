"use client";

import React, { useState, useEffect } from 'react';
import { Search, Sparkles, Folder, FolderOpen, Plus, Trash2, X, Loader2, Edit } from 'lucide-react';
import { useRouter } from 'next/navigation';
import { motion, AnimatePresence } from 'framer-motion';
import { DndContext, DragOverlay, closestCenter, useDraggable, useDroppable, DragStartEvent, DragEndEvent, useSensor, useSensors, MouseSensor, TouchSensor } from '@dnd-kit/core';

interface FolderData {
  id: string;
  name: string;
  files?: any[];
}

function DraggableFolder({ folder, onClick, onRename, onDelete }: { folder: FolderData, onClick: () => void, onRename: (e: React.MouseEvent) => void, onDelete: (e: React.MouseEvent) => void }) {
  const { attributes, listeners, setNodeRef: setDraggableRef, isDragging } = useDraggable({
    id: `drag-${folder.id}`,
    data: { type: 'folder', folder }
  });

  const { isOver, setNodeRef: setDroppableRef } = useDroppable({
    id: `drop-${folder.id}`,
    data: { type: 'folder', folder }
  });

  const setNodeRef = (node: HTMLElement | null) => {
    setDraggableRef(node);
    setDroppableRef(node);
  };

  return (
    <motion.div
      layout
      ref={setNodeRef}
      initial={{ opacity: 0, scale: 0.9, y: 10 }}
      animate={{ opacity: isDragging ? 0.3 : 1, scale: 1, y: 0 }}
      exit={{ opacity: 0, scale: 0.9, y: 10 }}
      transition={{ duration: 0.2, type: "spring", bounce: 0.3 }}
      {...attributes}
      {...listeners}
      onClick={onClick}
      className={`group relative bg-[#151525] border rounded-[18px] p-5 transition-all cursor-pointer shadow-sm flex flex-col items-center justify-center gap-3 min-h-[150px] text-center
        ${isOver ? 'border-[#5E5CE6] bg-[#1C1C33] shadow-lg shadow-[#5E5CE6]/20 scale-105' : 'border-[#2A2A40] hover:bg-[#1C1C33] hover:border-[#5E5CE6]/50 hover:shadow-lg hover:shadow-[#5E5CE6]/10'}
      `}
    >
      <div className="absolute top-2 right-2 flex gap-1 opacity-0 group-hover:opacity-100 transition-all z-10">
        <div className="relative group/tooltip">
          <button
            onPointerDown={(e) => e.stopPropagation()}
            onClick={onRename}
            className="text-gray-500 hover:text-white p-1.5 hover:bg-[#2A2A40] rounded-lg transition-colors"
          >
            <Edit size={16} />
          </button>
          <div className="absolute bottom-full left-1/2 -translate-x-1/2 mb-1 px-2 py-1 bg-[#0B0B1A] text-[10px] text-white rounded opacity-0 group-hover/tooltip:opacity-100 transition-all pointer-events-none whitespace-nowrap border border-[#2A2A40] shadow-xl scale-95 group-hover/tooltip:scale-100">
            Umbenennen
          </div>
        </div>
        <div className="relative group/tooltip">
          <button
            onPointerDown={(e) => e.stopPropagation()}
            onClick={onDelete}
            className="text-gray-500 hover:text-red-400 p-1.5 hover:bg-[#2A2A40] rounded-lg transition-colors"
          >
            <Trash2 size={16} />
          </button>
          <div className="absolute bottom-full left-1/2 -translate-x-1/2 mb-1 px-2 py-1 bg-[#0B0B1A] text-[10px] text-white rounded opacity-0 group-hover/tooltip:opacity-100 transition-all pointer-events-none whitespace-nowrap border border-[#2A2A40] shadow-xl scale-95 group-hover/tooltip:scale-100">
            Löschen
          </div>
        </div>
      </div>

      <div className="p-3.5 bg-[#0B0B1A] group-hover:bg-[#5E5CE6]/10 rounded-full transition-colors duration-300">
        <Folder size={40} className="text-gray-400 group-hover:text-[#5E5CE6] transition-colors" fill="currentColor" fillOpacity={isOver ? 0.3 : 0.1} />
      </div>

      <div className="w-full px-2">
        <h3 className="text-sm font-bold text-white mb-0.5 line-clamp-1">{folder.name}</h3>
        <p className="text-[11px] text-gray-500">{folder.files?.length || 0} Dateien</p>
      </div>
    </motion.div>
  );
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

  // Rename Modal State
  const [isRenameOpen, setIsRenameOpen] = useState(false);
  const [folderToRename, setFolderToRename] = useState<FolderData | null>(null);
  const [renameValue, setRenameValue] = useState("");
  const [isRenaming, setIsRenaming] = useState(false);

  // AI Summary Modal State
  const [isSummaryOpen, setIsSummaryOpen] = useState(false);

  // Drag & Drop State
  const [activeDragFolder, setActiveDragFolder] = useState<FolderData | null>(null);

  const sensors = useSensors(
    useSensor(MouseSensor, {
      activationConstraint: {
        distance: 8, // Require 8px of movement to start dragging. This allows clicks to fire.
      },
    }),
    useSensor(TouchSensor, {
      activationConstraint: {
        delay: 200, // Press and hold for 200ms to drag on mobile
        tolerance: 5,
      },
    })
  );

  const API_BASE = '/api';

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

  const handleRenameFolder = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!renameValue.trim() || !folderToRename) return;

    setIsRenaming(true);
    try {
      const username = localStorage.getItem("username");
      const res = await fetch(`${API_BASE}/folders/${folderToRename.id}`, {
        method: "PUT",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ name: renameValue, username }),
      });

      if (res.ok) {
        setIsRenameOpen(false);
        setFolderToRename(null);
        setRenameValue("");
        fetchFolders(); // Refresh list
      }
    } catch (error) {
      console.error("Error renaming folder:", error);
    } finally {
      setIsRenaming(false);
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

  const handleFolderSelect = (folderId: string) => {
    router.push(`/folder/${folderId}`);
  };

  const filteredFolders = folders.filter(f =>
    f.name.toLowerCase().includes(searchQuery.toLowerCase())
  );

  const handleDragStart = (event: DragStartEvent) => {
    const { active } = event;
    const folder = active.data.current?.folder as FolderData;
    if (folder) {
      setActiveDragFolder(folder);
    }
  };

  const handleDragEnd = async (event: DragEndEvent) => {
    setActiveDragFolder(null);
    const { active, over } = event;

    if (!over) return;

    const sourceFolder = active.data.current?.folder as FolderData;
    const targetFolder = over.data.current?.folder as FolderData;

    if (!sourceFolder || !targetFolder || sourceFolder.id === targetFolder.id) return;

    // Call backend to move folder
    try {
      const username = localStorage.getItem("username");
      // The API endpoint PUT /api/folders/{id}/move needs a body containing the new parent_id
      const res = await fetch(`${API_BASE}/folders/${sourceFolder.id}/move`, {
        method: "PUT",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ parent_id: targetFolder.id, username }),
      });

      if (res.ok) {
        // Refresh folders list
        fetchFolders();
      } else {
        const err = await res.json();
        alert(`Fehler beim Verschieben: ${err.detail || 'Unbekannt'}`);
      }
    } catch (error) {
      console.error("Error moving folder:", error);
      alert("Netzwerkfehler beim Verschieben des Ordners.");
    }
  };

  return (
    <div className="bg-[#0B0B1A] min-h-screen relative">
      {/* Main Container - Professional Width */}
      <div className="max-w-7xl mr-auto px-8 py-12">

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

        {/* Search Bar & Actions Row - GRID LAYOUT */}
        <div className="grid grid-cols-1 xl:grid-cols-[1fr_auto] items-center gap-4 mb-8 h-12">

          {/* Search */}
          <div className="relative w-full h-full">
            <div className="absolute inset-y-0 left-0 pl-6 flex items-center pointer-events-none text-gray-400">
              <Search size={22} className="text-gray-400" />
            </div>
            <input
              type="text"
              placeholder="Suche nach Ordnern..."
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              style={{ paddingLeft: '68px' }} // Force padding to ensure icon doesn't overlap
              className="w-full h-full pr-4 bg-[#151525] text-white text-sm rounded-xl border border-[#2A2A40] focus:border-[#5E5CE6] focus:outline-none transition-all placeholder:text-gray-500 shadow-sm"
            />
          </div>

          {/* Buttons */}
          <div className="flex flex-col sm:flex-row gap-3 h-full">
            <button
              onClick={() => setIsSummaryOpen(true)}
              className="h-full flex items-center justify-center gap-2 bg-gradient-to-r from-[#5E5CE6] to-[#7D7AFF] text-white px-6 rounded-xl text-sm font-semibold hover:shadow-lg hover:shadow-[#5E5CE6]/25 transition-all shadow-md min-w-[140px]"
            >
              <Sparkles size={18} />
              <span>AI-Summary</span>
            </button>

            <button
              onClick={() => setIsCreateOpen(true)}
              className="h-full flex items-center justify-center gap-2 bg-[#151525] hover:bg-[#1C1C33] text-white px-6 rounded-xl text-sm font-semibold border border-[#2A2A40] transition-all shadow-md min-w-[140px]"
            >
              <Folder size={18} />
              <span>Neuer Ordner</span>
            </button>
          </div>
        </div>

        {/* Folders Section */}
        <div>
          <h2 className="text-xs font-bold text-gray-500 mb-4 uppercase tracking-wider pl-1">
            Meine Ordner
          </h2>

          {loading ? (
            <div className="flex justify-center py-20">
              <Loader2 className="animate-spin text-[#5E5CE6]" size={32} />
            </div>
          ) : error ? (
            <div className="bg-[#151525] border border-red-500/30 rounded-2xl p-10 text-center">
              <div className="text-red-400 font-semibold mb-2">{error}</div>
              <p className="text-gray-500 text-sm mb-4">Prüfe die Render-URL in den Vercel Settings.</p>
              <button onClick={fetchFolders} className="text-sm bg-red-500/10 text-red-400 px-4 py-2 rounded-lg hover:bg-red-500/20 transition-colors">
                Erneut versuchen
              </button>
            </div>
          ) : folders.length === 0 ? (
            /* Empty State */
            <div className="bg-[#151525] border border-[#2A2A40] rounded-2xl p-12 text-center border-dashed">
              <div className="flex justify-center mb-4">
                <div className="w-12 h-12 bg-[#0B0B1A] rounded-xl flex items-center justify-center border border-[#2A2A40] shadow-inner">
                  <FolderOpen size={24} className="text-gray-500" />
                </div>
              </div>

              <h3 className="text-lg font-semibold text-white mb-1">
                Noch keine Ordner
              </h3>

              <p className="text-sm text-gray-400 mb-6 max-w-sm mx-auto">
                Erstelle deinen ersten Ordner.
              </p>

              <button
                onClick={() => setIsCreateOpen(true)}
                className="inline-flex items-center gap-2 bg-[#1C1C33] hover:bg-[#2A2A40] text-white px-4 py-2 rounded-lg text-sm font-medium transition-all border border-[#2A2A40]"
              >
                <Plus size={16} />
                <span>Ordner erstellen</span>
              </button>
            </div>
          ) : (
            /* Folder Grid */
            <DndContext
              sensors={sensors}
              onDragStart={handleDragStart}
              onDragEnd={handleDragEnd}
              collisionDetection={closestCenter}
            >
              <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 xl:grid-cols-4 gap-4">
                <AnimatePresence mode='popLayout'>
                  {filteredFolders.map((folder) => (
                    <DraggableFolder
                      key={folder.id}
                      folder={folder}
                      onClick={() => router.push(`/folder/${folder.id}`)}
                      onRename={(e) => {
                        e.stopPropagation();
                        setFolderToRename(folder);
                        setRenameValue(folder.name);
                        setIsRenameOpen(true);
                      }}
                      onDelete={(e) => handleDeleteFolder(folder.id, e)}
                    />
                  ))}
                </AnimatePresence>
              </div>

              <DragOverlay dropAnimation={{ duration: 250, easing: 'cubic-bezier(0.18, 0.67, 0.6, 1.22)' }}>
                {activeDragFolder ? (
                  <div className="bg-[#1C1C33] border border-[#5E5CE6] rounded-[18px] p-5 shadow-2xl flex flex-col items-center justify-center gap-3 min-h-[150px] text-center opacity-90 scale-105">
                    <div className="p-3.5 bg-[#0B0B1A] rounded-full">
                      <Folder size={40} className="text-[#5E5CE6]" fill="currentColor" fillOpacity={0.2} />
                    </div>
                    <div className="w-full px-2">
                      <h3 className="text-sm font-bold text-white mb-0.5 line-clamp-1">{activeDragFolder.name}</h3>
                      <p className="text-[11px] text-gray-500">{activeDragFolder.files?.length || 0} Dateien</p>
                    </div>
                  </div>
                ) : null}
              </DragOverlay>
            </DndContext>
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

      {/* Rename Folder Modal */}
      {isRenameOpen && folderToRename && (
        <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/60 backdrop-blur-sm p-4">
          <div className="bg-[#1e1e1e] border border-[#333] rounded-xl w-full max-w-sm p-6 shadow-2xl animate-in fade-in zoom-in duration-200">
            <div className="flex justify-between items-center mb-4">
              <h3 className="text-lg font-semibold text-white">Ordner umbenennen</h3>
              <button
                onClick={() => {
                  setIsRenameOpen(false);
                  setFolderToRename(null);
                }}
                className="text-gray-400 hover:text-white"
              >
                <X size={20} />
              </button>
            </div>

            <form onSubmit={handleRenameFolder}>
              <input
                autoFocus
                type="text"
                placeholder="Neuer Name..."
                value={renameValue}
                onChange={(e) => setRenameValue(e.target.value)}
                className="w-full bg-[#252526] border border-[#333] text-white rounded-lg px-4 py-2.5 mb-4 focus:ring-2 focus:ring-[#5E5CE6] focus:border-transparent outline-none placeholder:text-gray-500"
              />

              <div className="flex justify-end gap-2">
                <button
                  type="button"
                  onClick={() => {
                    setIsRenameOpen(false);
                    setFolderToRename(null);
                  }}
                  className="px-4 py-2 text-sm font-medium text-gray-300 hover:text-white transition-colors"
                >
                  Abbrechen
                </button>
                <button
                  type="submit"
                  disabled={!renameValue.trim() || isRenaming || renameValue === folderToRename.name}
                  className="px-4 py-2 text-sm font-medium bg-[#5E5CE6] hover:bg-[#4d4ac9] text-white rounded-lg transition-colors disabled:opacity-50 disabled:cursor-not-allowed flex items-center gap-2"
                >
                  {isRenaming && <Loader2 size={14} className="animate-spin" />}
                  Speichern
                </button>
              </div>
            </form>
          </div>
        </div>
      )}

      {/* Selection Modal for AI-Summary */}
      {isSummaryOpen && (
        <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/60 backdrop-blur-sm p-4 animate-in fade-in duration-200">
          <div className="bg-[#1e1e1e] border border-[#333] rounded-2xl w-full max-w-md shadow-2xl p-6 transform scale-100 transition-all">
            <div className="flex justify-between items-center mb-6">
              <h2 className="text-xl font-bold text-white">Wähle einen Ordner</h2>
              <button onClick={() => setIsSummaryOpen(false)} className="text-gray-400 hover:text-white transition-colors">
                <X size={20} />
              </button>
            </div>

            <div className="flex flex-col gap-2 max-h-[300px] overflow-y-auto pr-1 custom-scrollbar">
              {folders.length === 0 ? (
                <p className="text-gray-500 text-center py-4">Keine Ordner verfügbar.</p>
              ) : (
                folders.map(folder => (
                  <button
                    key={folder.id}
                    onClick={() => handleFolderSelect(folder.id)}
                    className="flex items-center gap-3 p-3 rounded-xl hover:bg-[#252526] border border-transparent hover:border-[#333] transition-all text-left group"
                  >
                    <div className="p-2 bg-[#252526] group-hover:bg-[#333] rounded-lg transition-colors">
                      <FolderOpen size={20} className="text-[#5E5CE6]" />
                    </div>
                    <span className="text-gray-200 font-medium">{folder.name}</span>
                  </button>
                ))
              )}
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
