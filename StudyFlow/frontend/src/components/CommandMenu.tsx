"use client";

import React, { useState, useEffect, useRef } from "react";
import { useRouter } from "next/navigation";
import { Search, Folder, FileText, Loader2, BrainCircuit, Youtube, HelpCircle, Layers, FileOutput } from "lucide-react";
import { motion, AnimatePresence } from "framer-motion";

interface SearchResult {
    type: "folder" | "file";
    id: string;
    name: string;
    folder_id: string;
    file_type?: string;
    folder_name?: string;
}

export default function CommandMenu() {
    const router = useRouter();
    const [isOpen, setIsOpen] = useState(false);
    const [query, setQuery] = useState("");
    const [results, setResults] = useState<SearchResult[]>([]);
    const [isLoading, setIsLoading] = useState(false);
    const [selectedIndex, setSelectedIndex] = useState(0);
    const inputRef = useRef<HTMLInputElement>(null);

    // Toggle menu on Ctrl+K or Cmd+K
    useEffect(() => {
        const handleKeyDown = (e: KeyboardEvent) => {
            if ((e.ctrlKey || e.metaKey) && e.key === "k") {
                e.preventDefault();
                setIsOpen((prev) => !prev);
            }
            if (e.key === "Escape") {
                setIsOpen(false);
            }
        };

        window.addEventListener("keydown", handleKeyDown);
        return () => window.removeEventListener("keydown", handleKeyDown);
    }, []);

    // Focus input when opened
    useEffect(() => {
        if (isOpen) {
            setQuery("");
            setResults([]);
            setSelectedIndex(0);
            setTimeout(() => inputRef.current?.focus(), 100);
        }
    }, [isOpen]);

    // Handle Search with Debounce
    useEffect(() => {
        if (!isOpen) return;

        const fetchResults = async () => {
            if (!query.trim()) {
                setResults([]);
                return;
            }

            setIsLoading(true);
            try {
                const username = localStorage.getItem("username");
                const res = await fetch(`/api/search?username=${username}&q=${encodeURIComponent(query)}`);
                if (res.ok) {
                    const data = await res.json();
                    setResults(data);
                }
            } catch (error) {
                console.error("Search failed:", error);
            } finally {
                setIsLoading(false);
                setSelectedIndex(0); // Reset selection on new results
            }
        };

        const timeoutId = setTimeout(fetchResults, 250);
        return () => clearTimeout(timeoutId);
    }, [query, isOpen]);

    // Keyboard navigation
    useEffect(() => {
        const handleNavigation = (e: KeyboardEvent) => {
            if (!isOpen || results.length === 0) return;

            if (e.key === "ArrowDown") {
                e.preventDefault();
                setSelectedIndex((prev) => (prev + 1) % results.length);
            } else if (e.key === "ArrowUp") {
                e.preventDefault();
                setSelectedIndex((prev) => (prev - 1 + results.length) % results.length);
            } else if (e.key === "Enter") {
                e.preventDefault();
                handleSelect(results[selectedIndex]);
            }
        };

        window.addEventListener("keydown", handleNavigation);
        return () => window.removeEventListener("keydown", handleNavigation);
    }, [isOpen, results, selectedIndex]);

    const handleSelect = (item: SearchResult) => {
        setIsOpen(false);
        // Both folder and file navigations currently go to the folder view where the file lives.
        // We could theoretically add an ?openFile=id query param, but for now just taking them to the folder is great.
        router.push(`/folder/${item.folder_id}`);
    };

    const getFileIcon = (type?: string) => {
        switch (type) {
            case "plan": return <BrainCircuit size={16} className="text-purple-400" />;
            case "transcript": return <Youtube size={16} className="text-red-500" />;
            case "quiz": return <HelpCircle size={16} className="text-orange-400" />;
            case "flashcards": return <Layers size={16} className="text-green-400" />;
            case "summary": return <FileOutput size={16} className="text-blue-400" />;
            default: return <FileText size={16} className="text-[#5E5CE6]" />;
        }
    };

    return (
        <AnimatePresence>
            {isOpen && (
                <>
                    {/* Backdrop */}
                    <motion.div
                        initial={{ opacity: 0 }}
                        animate={{ opacity: 1 }}
                        exit={{ opacity: 0 }}
                        onClick={() => setIsOpen(false)}
                        className="fixed inset-0 bg-black/60 backdrop-blur-sm z-[9999]"
                    />

                    {/* Modal */}
                    <motion.div
                        initial={{ opacity: 0, scale: 0.95, y: -20 }}
                        animate={{ opacity: 1, scale: 1, y: 0 }}
                        exit={{ opacity: 0, scale: 0.95, y: -20 }}
                        transition={{ duration: 0.15, ease: "easeOut" }}
                        className="fixed top-[15%] left-1/2 -translate-x-1/2 w-full max-w-xl bg-[#151525] border border-[#333] rounded-2xl shadow-2xl z-[10000] overflow-hidden flex flex-col"
                    >
                        {/* Search Input Area */}
                        <div className="flex items-center gap-3 px-4 py-4 border-b border-[#2A2A40]">
                            <Search size={22} className="text-gray-400" />
                            <input
                                ref={inputRef}
                                type="text"
                                placeholder="Suche nach Ordnern, Notizen, Quizzes..."
                                value={query}
                                onChange={(e) => setQuery(e.target.value)}
                                className="flex-1 bg-transparent border-none outline-none text-lg text-white placeholder:text-gray-500"
                            />
                            {isLoading && <Loader2 size={20} className="animate-spin text-gray-400" />}
                            <div className="flex items-center gap-1 text-[10px] text-gray-500 font-medium bg-[#1C1C33] px-2 py-1 rounded border border-[#2A2A40]">
                                <span>ESC</span>
                            </div>
                        </div>

                        {/* Search Results Area */}
                        <div className="max-h-[350px] overflow-y-auto p-2 custom-scrollbar">
                            {!query.trim() ? (
                                <div className="p-6 text-center text-gray-500 text-sm">
                                    Tippe, um in all deinen Materialien zu suchen.
                                </div>
                            ) : results.length === 0 && !isLoading ? (
                                <div className="p-6 text-center text-gray-500 text-sm">
                                    Keine Ergebnisse für "{query}" gefunden.
                                </div>
                            ) : (
                                <div className="flex flex-col gap-1">
                                    {results.map((item, index) => {
                                        const isSelected = index === selectedIndex;
                                        return (
                                            <button
                                                key={`${item.type}-${item.id}`}
                                                className={`w-full flex items-center gap-4 p-3 rounded-xl text-left transition-colors ${isSelected ? "bg-[#5E5CE6]/10 border border-[#5E5CE6]/30" : "hover:bg-[#1C1C33] border border-transparent"
                                                    }`}
                                                onMouseEnter={() => setSelectedIndex(index)}
                                                onClick={() => handleSelect(item)}
                                            >
                                                <div className={`p-2 rounded-lg ${item.type === "folder" ? "bg-[#1C1C33] text-gray-400" : "bg-[#1C1C33]/50"
                                                    }`}>
                                                    {item.type === "folder" ? <Folder size={18} /> : getFileIcon(item.file_type)}
                                                </div>
                                                <div className="flex-1 min-w-0">
                                                    <h4 className={`text-sm font-medium truncate ${isSelected ? "text-white" : "text-gray-300"}`}>
                                                        {item.name}
                                                    </h4>
                                                    <p className="text-xs text-gray-500 truncate mt-0.5">
                                                        {item.type === "folder" ? "Ordner" : `In Ordner: ${item.folder_name || 'Unbekannt'}`}
                                                    </p>
                                                </div>
                                                {isSelected && (
                                                    <div className="text-gray-500">
                                                        <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><polyline points="9 18 15 12 9 6"></polyline></svg>
                                                    </div>
                                                )}
                                            </button>
                                        );
                                    })}
                                </div>
                            )}
                        </div>

                        {/* Footer Hints */}
                        <div className="px-4 py-3 border-t border-[#2A2A40] bg-[#0B0B1A] flex items-center justify-between text-[11px] text-gray-500">
                            <div className="flex items-center gap-4">
                                <span className="flex items-center gap-1.5"><span className="bg-[#1C1C33] p-1 rounded">↑</span><span className="bg-[#1C1C33] p-1 rounded">↓</span> Navigieren</span>
                                <span className="flex items-center gap-1.5"><span className="bg-[#1C1C33] p-1 rounded">↵</span> Öffnen</span>
                            </div>
                            <div className="flex items-center gap-1.5 font-semibold text-[#5E5CE6]">Blop Search</div>
                        </div>
                    </motion.div>
                </>
            )}
        </AnimatePresence>
    );
}
