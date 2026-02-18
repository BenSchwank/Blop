"use client";

import React, { useState, useEffect } from "react";
import { useRouter } from "next/navigation";
import { ArrowLeft, FileText, MoreVertical, Plus, Loader2, Youtube, Upload, BrainCircuit, X, FileType } from "lucide-react";

interface FileData {
    id: string;
    name: string;
    type: string;
    created_at: string;
}

export default function FolderPage({ params }: { params: { id: string } }) {
    const router = useRouter();
    const [files, setFiles] = useState<FileData[]>([]);
    const [loading, setLoading] = useState(true);
    const folderId = params.id;

    // Upload / Action States
    const [isUploadOpen, setIsUploadOpen] = useState(false);
    const [uploadType, setUploadType] = useState<'pdf' | 'youtube'>('pdf');
    const [fileToUpload, setFileToUpload] = useState<File | null>(null);
    const [youtubeUrl, setYoutubeUrl] = useState("");
    const [isProcessing, setIsProcessing] = useState(false);

    // AI Plan State
    const [isGeneratingPlan, setIsGeneratingPlan] = useState(false);

    const API_BASE = "http://localhost:8000/api";

    const fetchFiles = async () => {
        try {
            const username = localStorage.getItem("username");
            if (!username) return;

            const res = await fetch(`${API_BASE}/files/${folderId}?username=${username}`);
            if (res.ok) {
                const data = await res.json();
                setFiles(data);
            }
        } catch (error) {
            console.error("Failed to fetch files:", error);
        } finally {
            setLoading(false);
        }
    };

    useEffect(() => {
        fetchFiles();
    }, [folderId]);

    const handleFileUpload = async (e: React.FormEvent) => {
        e.preventDefault();
        if (!fileToUpload) return;

        setIsProcessing(true);
        try {
            const username = localStorage.getItem("username");
            const formData = new FormData();
            formData.append("file", fileToUpload);

            const res = await fetch(`${API_BASE}/files/upload?username=${username}&folder_id=${folderId}`, {
                method: "POST",
                body: formData
            });

            if (res.ok) {
                setFileToUpload(null);
                setIsUploadOpen(false);
                fetchFiles();
            }
        } catch (error) {
            console.error(error);
            alert("Fehler beim Upload.");
        } finally {
            setIsProcessing(false);
        }
    };

    const handleYoutubeImport = async (e: React.FormEvent) => {
        e.preventDefault();
        if (!youtubeUrl) return;

        setIsProcessing(true);
        try {
            const username = localStorage.getItem("username");
            const res = await fetch(`${API_BASE}/files/youtube?username=${username}&folder_id=${folderId}`, {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify({ url: youtubeUrl })
            });

            if (res.ok) {
                setYoutubeUrl("");
                setIsUploadOpen(false);
                fetchFiles();
            } else {
                alert("Fehler: Ungültige URL oder kein Transkript verfügbar.");
            }
        } catch (error) {
            console.error(error);
        } finally {
            setIsProcessing(false);
        }
    };

    const handleGeneratePlan = async () => {
        if (!confirm("Soll ein Lernplan für 7 Tage basierend auf den Inhalten erstellt werden?")) return;

        setIsGeneratingPlan(true);
        try {
            const username = localStorage.getItem("username");
            const res = await fetch(`${API_BASE}/ai/plan`, {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify({ username, folder_id: folderId, duration_days: 7 })
            });

            if (res.ok) {
                fetchFiles();
                alert("Lernplan erfolgreich erstellt!");
            } else {
                const err = await res.json();
                alert(`Fehler: ${err.detail}`);
            }
        } catch (error) {
            console.error(error);
            alert("Fehler bei der AI-Generierung.");
        } finally {
            setIsGeneratingPlan(false);
        }
    };

    return (
        <div className="bg-[#1e1e1e] min-h-screen">
            <div className="max-w-6xl mx-auto px-6 py-10">

                {/* Header */}
                <div className="flex items-center justify-between mb-8">
                    <div className="flex items-center gap-4">
                        <button
                            onClick={() => router.back()}
                            className="p-2 hover:bg-[#333] rounded-xl text-gray-400 hover:text-white transition-colors"
                        >
                            <ArrowLeft size={22} />
                        </button>
                        <div>
                            <h1 className="text-2xl font-bold text-white">Ordner Details</h1>
                            <p className="text-sm text-gray-400">ID: {folderId}</p>
                        </div>
                    </div>

                    <div className="flex gap-3">
                        <button
                            onClick={handleGeneratePlan}
                            disabled={isGeneratingPlan}
                            className="flex items-center gap-2 bg-gradient-to-r from-purple-500 to-indigo-600 text-white px-4 py-2.5 rounded-xl text-sm font-semibold hover:opacity-90 transition-all disabled:opacity-50"
                        >
                            {isGeneratingPlan ? <Loader2 size={18} className="animate-spin" /> : <BrainCircuit size={18} />}
                            <span>AI Lernplan</span>
                        </button>

                        <button
                            onClick={() => setIsUploadOpen(true)}
                            className="flex items-center gap-2 bg-[#252526] text-white px-4 py-2.5 rounded-xl text-sm font-semibold border border-[#333] hover:bg-[#333] transition-all"
                        >
                            <Plus size={18} />
                            <span>Material hinzufügen</span>
                        </button>
                    </div>
                </div>

                {/* Content */}
                {loading ? (
                    <div className="flex justify-center py-20">
                        <Loader2 className="animate-spin text-[#5E5CE6]" size={32} />
                    </div>
                ) : files.length === 0 ? (
                    <div className="bg-[#252526] border border-[#333] rounded-2xl p-16 text-center border-dashed">
                        <div className="flex justify-center mb-6">
                            <div className="w-16 h-16 bg-[#1e1e1e] rounded-2xl flex items-center justify-center border border-[#333]">
                                <FileText size={24} className="text-gray-500" />
                            </div>
                        </div>
                        <h3 className="text-xl font-medium text-white mb-2">Dieser Ordner ist noch leer</h3>
                        <p className="text-base text-gray-400 mb-8">
                            Lade Skripte (PDF) hoch oder füge YouTube-Videos hinzu,<br />damit die AI einen Lernplan erstellen kann.
                        </p>
                        <button
                            onClick={() => setIsUploadOpen(true)}
                            className="inline-flex items-center gap-2 bg-[#5E5CE6] hover:bg-[#4d4ac9] text-white px-5 py-2.5 rounded-xl text-sm font-medium transition-all"
                        >
                            <Plus size={18} />
                            <span>Material hinzufügen</span>
                        </button>
                    </div>
                ) : (
                    <div className="grid grid-cols-1 gap-3">
                        {files.map((file) => (
                            <div key={file.id} className="bg-[#252526] hover:bg-[#2d2d2d] border border-[#333] p-4 rounded-xl flex items-center justify-between group transition-colors cursor-pointer">
                                <div className="flex items-center gap-4">
                                    <div className={`p-3 rounded-lg ${file.type === 'plan' ? 'bg-purple-500/10 text-purple-400' : 'bg-[#333] text-[#5E5CE6]'}`}>
                                        {file.type === 'plan' ? <BrainCircuit size={20} /> :
                                            file.type === 'transcript' ? <Youtube size={20} /> :
                                                <FileText size={20} />}
                                    </div>
                                    <div>
                                        <h4 className="text-sm font-medium text-white">{file.name}</h4>
                                        <p className="text-xs text-gray-500 capitalize">{file.created_at} • {file.type}</p>
                                    </div>
                                </div>
                                <button className="text-gray-500 hover:text-white p-2 rounded-lg hover:bg-[#333] opacity-0 group-hover:opacity-100 transition-all">
                                    <MoreVertical size={18} />
                                </button>
                            </div>
                        ))}
                    </div>
                )}
            </div>

            {/* Upload Modal */}
            {isUploadOpen && (
                <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/60 backdrop-blur-sm p-4">
                    <div className="bg-[#1e1e1e] border border-[#333] rounded-2xl w-full max-w-md p-6 shadow-2xl animate-in fade-in zoom-in duration-200">
                        <div className="flex justify-between items-center mb-6">
                            <h3 className="text-lg font-semibold text-white">Material hinzufügen</h3>
                            <button onClick={() => setIsUploadOpen(false)} className="text-gray-400 hover:text-white">
                                <X size={20} />
                            </button>
                        </div>

                        {/* Tabs */}
                        <div className="flex gap-2 p-1 bg-[#252526] rounded-xl mb-6">
                            <button
                                onClick={() => setUploadType('pdf')}
                                className={`flex-1 flex items-center justify-center gap-2 py-2 text-sm font-medium rounded-lg transition-all ${uploadType === 'pdf' ? 'bg-[#333] text-white shadow-sm' : 'text-gray-400 hover:text-gray-200'}`}
                            >
                                <Upload size={16} />
                                PDF Upload
                            </button>
                            <button
                                onClick={() => setUploadType('youtube')}
                                className={`flex-1 flex items-center justify-center gap-2 py-2 text-sm font-medium rounded-lg transition-all ${uploadType === 'youtube' ? 'bg-[#333] text-red-400 shadow-sm' : 'text-gray-400 hover:text-gray-200'}`}
                            >
                                <Youtube size={16} />
                                YouTube
                            </button>
                        </div>

                        {uploadType === 'pdf' ? (
                            <form onSubmit={handleFileUpload} className="space-y-4">
                                <div className="border-2 border-dashed border-[#333] rounded-xl p-8 text-center hover:border-[#5E5CE6] transition-colors cursor-pointer relative">
                                    <input
                                        type="file"
                                        accept=".pdf"
                                        onChange={(e) => setFileToUpload(e.target.files?.[0] || null)}
                                        className="absolute inset-0 w-full h-full opacity-0 cursor-pointer"
                                    />
                                    <div className="flex flex-col items-center gap-2 text-gray-400">
                                        <Upload size={24} />
                                        <span className="text-sm">
                                            {fileToUpload ? fileToUpload.name : "Klicken zum Auswählen"}
                                        </span>
                                    </div>
                                </div>
                                <button
                                    type="submit"
                                    disabled={!fileToUpload || isProcessing}
                                    className="w-full bg-[#5E5CE6] hover:bg-[#4d4ac9] text-white py-2.5 rounded-xl text-sm font-medium transition-all disabled:opacity-50 flex justify-center items-center gap-2"
                                >
                                    {isProcessing && <Loader2 size={16} className="animate-spin" />}
                                    Hochladen
                                </button>
                            </form>
                        ) : (
                            <form onSubmit={handleYoutubeImport} className="space-y-4">
                                <div>
                                    <label className="block text-xs font-medium text-gray-400 mb-1.5 ml-1">YouTube URL</label>
                                    <input
                                        type="url"
                                        placeholder="https://youtube.com/watch?v=..."
                                        value={youtubeUrl}
                                        onChange={(e) => setYoutubeUrl(e.target.value)}
                                        className="w-full bg-[#252526] border border-[#333] text-white rounded-xl px-4 py-2.5 focus:ring-2 focus:ring-red-500/50 focus:border-red-500 outline-none placeholder:text-gray-600 transition-all font-sans"
                                    />
                                </div>
                                <button
                                    type="submit"
                                    disabled={!youtubeUrl || isProcessing}
                                    className="w-full bg-red-600 hover:bg-red-700 text-white py-2.5 rounded-xl text-sm font-medium transition-all disabled:opacity-50 flex justify-center items-center gap-2"
                                >
                                    {isProcessing && <Loader2 size={16} className="animate-spin" />}
                                    Importieren
                                </button>
                            </form>
                        )}
                    </div>
                </div>
            )}
        </div>
    );
}
