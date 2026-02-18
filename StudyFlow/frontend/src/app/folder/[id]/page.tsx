"use client";

import React, { useState, useEffect } from "react";
import { useRouter } from "next/navigation";
import { ArrowLeft, FileText, MoreVertical, Plus, Loader2 } from "lucide-react";

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

    return (
        <div className="bg-[#1e1e1e] min-h-screen">
            <div className="max-w-5xl mx-auto px-6 py-10">

                {/* Header with Back Button */}
                <div className="flex items-center gap-4 mb-8">
                    <button
                        onClick={() => router.back()}
                        className="p-2 hover:bg-[#333] rounded-lg text-gray-400 hover:text-white transition-colors"
                    >
                        <ArrowLeft size={20} />
                    </button>
                    <div>
                        <h1 className="text-2xl font-bold text-white">Ordner Details</h1>
                        <p className="text-sm text-gray-400">ID: {folderId}</p>
                    </div>
                </div>

                {/* Content */}
                {loading ? (
                    <div className="flex justify-center py-12">
                        <Loader2 className="animate-spin text-[#5E5CE6]" size={24} />
                    </div>
                ) : files.length === 0 ? (
                    <div className="bg-[#252526] border border-[#333] rounded-xl p-12 text-center">
                        <div className="flex justify-center mb-4">
                            <div className="w-12 h-12 bg-[#1e1e1e] rounded-full flex items-center justify-center border border-[#333]">
                                <FileText size={20} className="text-gray-500" />
                            </div>
                        </div>
                        <h3 className="text-lg font-medium text-white mb-2">Dieser Ordner ist leer</h3>
                        <p className="text-sm text-gray-400 mb-6">Füge Dateien hinzu, um loszulegen.</p>
                        <button className="inline-flex items-center gap-2 bg-[#5E5CE6] text-white px-4 py-2 rounded-lg text-sm font-medium hover:bg-[#4d4ac9] transition-colors">
                            <Plus size={16} />
                            <span>Datei hinzufügen</span>
                        </button>
                    </div>
                ) : (
                    <div className="grid grid-cols-1 gap-2">
                        {files.map((file) => (
                            <div key={file.id} className="bg-[#252526] hover:bg-[#2d2d2d] border border-[#333] p-4 rounded-lg flex items-center justify-between group transition-colors cursor-pointer">
                                <div className="flex items-center gap-4">
                                    <div className="p-2 bg-[#333] rounded-lg">
                                        <FileText size={20} className="text-[#5E5CE6]" />
                                    </div>
                                    <div>
                                        <h4 className="text-sm font-medium text-white">{file.name}</h4>
                                        <p className="text-xs text-gray-500">{file.created_at} • {file.type}</p>
                                    </div>
                                </div>
                                <button className="text-gray-500 hover:text-white p-2 rounded-lg hover:bg-[#333] opacity-0 group-hover:opacity-100 transition-all">
                                    <MoreVertical size={16} />
                                </button>
                            </div>
                        ))}
                    </div>
                )}
            </div>
        </div>
    );
}
