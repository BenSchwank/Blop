"use client";

import Link from "next/link";
import { ArrowLeft, MoreVertical, Plus, Edit, Inbox, Link2 } from "lucide-react";

export type BreadcrumbAncestor = { id: string; name: string };

type Props = {
    folderName: string;
    ancestors: BreadcrumbAncestor[];
    subfolderCount: number;
    materialCount: number;
    isHeaderMenuOpen: boolean;
    setIsHeaderMenuOpen: (v: boolean | ((p: boolean) => boolean)) => void;
    onBack: () => void;
    onUpload: () => void;
    onCreateSubfolder: () => void;
    onNewDocument: () => void;
    onLoadShareRequests: () => void;
    onImportByLink: () => void;
};

export default function FolderPageHeader({
    folderName,
    ancestors,
    subfolderCount,
    materialCount,
    isHeaderMenuOpen,
    setIsHeaderMenuOpen,
    onBack,
    onUpload,
    onCreateSubfolder,
    onNewDocument,
    onLoadShareRequests,
    onImportByLink,
}: Props) {
    return (
        <div className="flex flex-col gap-3 mb-4 min-w-0">
            <div className="flex items-center justify-between gap-3 min-w-0">
                <div className="flex items-center gap-2.5 min-w-0 flex-1">
                    <button
                        type="button"
                        onClick={onBack}
                        aria-label="Zurück"
                        className="h-[34px] w-[34px] flex items-center justify-center hover:bg-[#3A3D45] rounded-lg text-gray-400 hover:text-white transition-colors shrink-0"
                    >
                        <ArrowLeft size={18} />
                    </button>
                    <div className="min-w-0 flex-1">
                        <nav className="flex items-center gap-1.5 text-[12px] text-gray-500 mb-0.5 min-w-0 overflow-hidden">
                            <Link href="/" className="hover:text-gray-300 shrink-0">
                                Dashboard
                            </Link>
                            {ancestors.map((a) => (
                                <span key={a.id} className="flex items-center gap-1.5 min-w-0">
                                    <span className="text-gray-600 shrink-0">/</span>
                                    <Link href={`/folder/${a.id}`} className="hover:text-gray-300 truncate max-w-[8rem]">
                                        {a.name}
                                    </Link>
                                </span>
                            ))}
                            <span className="text-gray-600 shrink-0">/</span>
                            <span className="text-gray-400 truncate">{folderName || "Ordner"}</span>
                        </nav>
                        <div className="flex items-center gap-2.5 min-w-0">
                            <h1
                                className="text-[24px] font-bold text-white whitespace-nowrap overflow-hidden text-ellipsis min-w-0"
                                title={folderName || "Ordner"}
                            >
                                {folderName || "Ordner"}
                            </h1>
                            <span className="shrink-0 text-[11px] px-2 py-0.5 rounded-full bg-white/[0.06] border border-white/[0.08] text-gray-400 whitespace-nowrap">
                                {subfolderCount} Unterordner · {materialCount} Materialien
                            </span>
                        </div>
                    </div>
                </div>

                <div className="flex items-center gap-2 shrink-0 h-[34px]">
                    <button
                        type="button"
                        onClick={onUpload}
                        className="inline-flex items-center gap-1.5 h-[34px] px-3 rounded-lg text-[13px] font-medium text-white bg-[#2563EB] hover:bg-[#1D4ED8] transition-colors"
                    >
                        <Plus size={14} />
                        <span className="hidden sm:inline">Material hinzufügen</span>
                        <span className="sm:hidden">Material</span>
                    </button>
                    <button
                        type="button"
                        onClick={onCreateSubfolder}
                        className="inline-flex items-center gap-1.5 h-[34px] px-3 rounded-lg text-[13px] font-medium text-gray-200 bg-[#353840] border border-[#45484F] hover:bg-[#3A3D45] transition-colors"
                    >
                        <Plus size={14} />
                        <span className="hidden md:inline">Unterordner</span>
                    </button>
                    <button
                        type="button"
                        onClick={onNewDocument}
                        className="inline-flex items-center gap-1.5 h-[34px] px-3 rounded-lg text-[13px] font-medium text-gray-200 bg-[#353840] border border-[#45484F] hover:bg-[#3A3D45] transition-colors"
                    >
                        <Edit size={14} />
                        <span className="hidden md:inline">Neues Dokument</span>
                    </button>
                    <div className="relative">
                        <button
                            type="button"
                            onClick={() => setIsHeaderMenuOpen((v) => !v)}
                            className="h-[34px] w-[34px] flex items-center justify-center rounded-lg text-gray-300 bg-[#353840] border border-[#45484F] hover:bg-[#3A3D45]"
                            title="Ordner-Optionen"
                            aria-label="Ordner-Optionen"
                        >
                            <MoreVertical size={16} />
                        </button>
                        {isHeaderMenuOpen ? (
                            <div className="absolute right-0 top-full mt-1 w-44 bg-[#23252A] border border-[#45484F] rounded-xl shadow-2xl z-50 overflow-hidden">
                                <button
                                    type="button"
                                    onClick={() => {
                                        setIsHeaderMenuOpen(false);
                                        onLoadShareRequests();
                                    }}
                                    className="w-full min-h-10 flex items-center gap-2 px-4 py-2.5 text-sm text-gray-300 hover:bg-[#3A3D45] hover:text-white"
                                >
                                    <Inbox size={15} />
                                    Requests
                                </button>
                                <div className="h-px bg-[#3A3D45]" />
                                <button
                                    type="button"
                                    onClick={() => {
                                        setIsHeaderMenuOpen(false);
                                        onImportByLink();
                                    }}
                                    className="w-full min-h-10 flex items-center gap-2 px-4 py-2.5 text-sm text-gray-300 hover:bg-[#3A3D45] hover:text-white"
                                >
                                    <Link2 size={15} />
                                    Link eingeben
                                </button>
                            </div>
                        ) : null}
                    </div>
                </div>
            </div>
        </div>
    );
}
