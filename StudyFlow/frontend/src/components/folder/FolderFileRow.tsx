"use client";

import { useEffect, useRef, useState } from "react";
import { useDraggable } from "@dnd-kit/core";
import { MoreVertical, Edit, Archive, ArchiveRestore, Trash2, FolderInput, ExternalLink } from "lucide-react";
import {
    formatCompactDate,
    isAiContextEligible,
    isArchived,
    typeBadgeLabel,
    typeIconTone,
    type FolderFile,
} from "@/lib/folderFileUtils";

type Props = {
    file: FolderFile;
    icon: React.ReactNode;
    selected: boolean;
    aiActive: boolean;
    openMenuFileId: string | null;
    setOpenMenuFileId: (id: string | null) => void;
    onToggleSelect: (id: string, checked: boolean) => void;
    onToggleAi: (id: string, checked: boolean) => void;
    onOpen: (file: FolderFile) => void;
    onRename: (file: FolderFile, newName: string) => Promise<void> | void;
    onArchive: (file: FolderFile, archived: boolean) => void;
    onDelete: (file: FolderFile) => void;
    onMoveMenu: (file: FolderFile) => void;
    canRename: boolean;
    youtubeId?: string | null;
    onOpenYoutube?: (id: string) => void;
};

export default function FolderFileRow({
    file,
    icon,
    selected,
    aiActive,
    openMenuFileId,
    setOpenMenuFileId,
    onToggleSelect,
    onToggleAi,
    onOpen,
    onRename,
    onArchive,
    onDelete,
    onMoveMenu,
    canRename,
    youtubeId,
    onOpenYoutube,
}: Props) {
    const { attributes, listeners, setNodeRef, isDragging } = useDraggable({
        id: `drag-${file.id}`,
        data: { type: "file", file },
    });
    const [editing, setEditing] = useState(false);
    const [draft, setDraft] = useState(file.name);
    const inputRef = useRef<HTMLInputElement>(null);
    const archived = isArchived(file);
    const aiEligible = isAiContextEligible(file);
    const menuOpen = openMenuFileId === file.id;

    useEffect(() => {
        if (editing) inputRef.current?.focus();
    }, [editing]);

    const commitRename = async () => {
        const next = draft.trim();
        setEditing(false);
        if (!next || next === file.name) {
            setDraft(file.name);
            return;
        }
        await onRename(file, next);
    };

    return (
        <div
            ref={setNodeRef}
            {...attributes}
            {...listeners}
            className={`group flex h-11 items-center gap-2.5 border-b border-white/[0.06] px-3 transition-colors ${
                isDragging ? "opacity-40" : ""
            } ${
                aiActive && !archived
                    ? "bg-[rgba(59,130,246,0.06)]"
                    : selected
                      ? "bg-white/[0.03]"
                      : "hover:bg-white/[0.03]"
            }`}
        >
            <input
                type="checkbox"
                className="h-3.5 w-3.5 rounded border-[#45484F] bg-[#23252A] shrink-0"
                checked={selected}
                onChange={(e) => {
                    e.stopPropagation();
                    onToggleSelect(file.id, e.target.checked);
                }}
                onPointerDown={(e) => e.stopPropagation()}
                onClick={(e) => e.stopPropagation()}
                title="Mehrfachauswahl"
                aria-label="Datei auswählen"
            />
            <input
                type="checkbox"
                className="h-3.5 w-3.5 rounded border-[#45484F] bg-[#23252A] shrink-0 disabled:opacity-40"
                checked={aiActive}
                disabled={archived || !aiEligible}
                onChange={(e) => {
                    e.stopPropagation();
                    onToggleAi(file.id, e.target.checked);
                }}
                onPointerDown={(e) => e.stopPropagation()}
                onClick={(e) => e.stopPropagation()}
                title="Als Quelle für KI-Tools verwenden"
                aria-label="Als KI-Quelle verwenden"
            />

            <div className={`h-7 w-7 rounded-md flex items-center justify-center shrink-0 ${typeIconTone(file.type)}`}>
                {icon}
            </div>

            <div className="min-w-0 flex-1 flex items-center gap-2">
                {editing ? (
                    <input
                        ref={inputRef}
                        value={draft}
                        onChange={(e) => setDraft(e.target.value)}
                        onBlur={() => void commitRename()}
                        onKeyDown={(e) => {
                            if (e.key === "Enter") void commitRename();
                            if (e.key === "Escape") {
                                setDraft(file.name);
                                setEditing(false);
                            }
                        }}
                        onPointerDown={(e) => e.stopPropagation()}
                        onClick={(e) => e.stopPropagation()}
                        className="w-full max-w-xs h-7 rounded-md bg-[#12141A] border border-[#5B9DFF]/40 px-2 text-[13px] text-white outline-none"
                    />
                ) : (
                    <button
                        type="button"
                        className="min-w-0 text-left truncate text-[13px] font-medium text-white hover:underline"
                        onClick={(e) => {
                            e.stopPropagation();
                            onOpen(file);
                        }}
                        onPointerDown={(e) => e.stopPropagation()}
                        title={file.name}
                    >
                        {file.name}
                    </button>
                )}
                {aiActive && !archived ? (
                    <span className="hidden sm:inline shrink-0 rounded-full bg-[#2563EB]/15 px-1.5 py-0.5 text-[10px] font-medium text-[#93C5FD]">
                        KI-Quelle aktiv
                    </span>
                ) : null}
                {canRename && !editing ? (
                    <button
                        type="button"
                        className="hidden group-hover:inline-flex items-center gap-1 shrink-0 text-[11px] text-gray-400 hover:text-white"
                        onClick={(e) => {
                            e.stopPropagation();
                            setDraft(file.name);
                            setEditing(true);
                        }}
                        onPointerDown={(e) => e.stopPropagation()}
                        title="Umbenennen"
                    >
                        <Edit size={12} />
                        Umbenennen
                    </button>
                ) : null}
            </div>

            <span className="hidden md:inline shrink-0 rounded-md border border-white/[0.08] bg-white/[0.04] px-1.5 py-0.5 text-[10px] font-semibold tracking-wide text-gray-400">
                {typeBadgeLabel(file.type)}
            </span>
            <span className="hidden sm:inline shrink-0 text-[12px] text-[#9CA3AF] w-[9.5rem] text-right">
                {formatCompactDate(file.created_at)}
            </span>

            <div className="relative shrink-0" onPointerDown={(e) => e.stopPropagation()} onClick={(e) => e.stopPropagation()}>
                <button
                    type="button"
                    onClick={() => setOpenMenuFileId(menuOpen ? null : file.id)}
                    className="h-8 w-8 flex items-center justify-center rounded-lg text-gray-400 hover:text-white hover:bg-[#3A3D45]"
                    aria-label="Dateiaktionen"
                >
                    <MoreVertical size={15} />
                </button>
                {menuOpen ? (
                    <div className="absolute right-0 top-full mt-1 w-48 bg-[#23252A] border border-[#45484F] rounded-xl shadow-2xl z-50 overflow-hidden">
                        <button
                            type="button"
                            onClick={() => {
                                setOpenMenuFileId(null);
                                onOpen(file);
                            }}
                            className="w-full min-h-9 flex items-center gap-2 px-3 py-2 text-sm text-gray-300 hover:bg-[#3A3D45]"
                        >
                            Öffnen
                        </button>
                        {youtubeId && onOpenYoutube ? (
                            <button
                                type="button"
                                onClick={() => {
                                    setOpenMenuFileId(null);
                                    onOpenYoutube(youtubeId);
                                }}
                                className="w-full min-h-9 flex items-center gap-2 px-3 py-2 text-sm text-gray-300 hover:bg-[#3A3D45]"
                            >
                                <ExternalLink size={14} /> YouTube
                            </button>
                        ) : null}
                        {canRename ? (
                            <button
                                type="button"
                                onClick={() => {
                                    setOpenMenuFileId(null);
                                    setDraft(file.name);
                                    setEditing(true);
                                }}
                                className="w-full min-h-9 flex items-center gap-2 px-3 py-2 text-sm text-gray-300 hover:bg-[#3A3D45]"
                            >
                                <Edit size={14} /> Umbenennen
                            </button>
                        ) : null}
                        <button
                            type="button"
                            onClick={() => {
                                setOpenMenuFileId(null);
                                onMoveMenu(file);
                            }}
                            className="w-full min-h-9 flex items-center gap-2 px-3 py-2 text-sm text-gray-300 hover:bg-[#3A3D45]"
                        >
                            <FolderInput size={14} /> In Unterordner verschieben
                        </button>
                        <button
                            type="button"
                            onClick={() => {
                                setOpenMenuFileId(null);
                                onArchive(file, !archived);
                            }}
                            className="w-full min-h-9 flex items-center gap-2 px-3 py-2 text-sm text-gray-300 hover:bg-[#3A3D45]"
                        >
                            {archived ? <ArchiveRestore size={14} /> : <Archive size={14} />}
                            {archived ? "Wiederherstellen" : "Archivieren"}
                        </button>
                        <button
                            type="button"
                            onClick={() => {
                                setOpenMenuFileId(null);
                                onDelete(file);
                            }}
                            className="w-full min-h-9 flex items-center gap-2 px-3 py-2 text-sm text-red-400 hover:bg-red-500/10"
                        >
                            <Trash2 size={14} /> Löschen
                        </button>
                    </div>
                ) : null}
            </div>
        </div>
    );
}
