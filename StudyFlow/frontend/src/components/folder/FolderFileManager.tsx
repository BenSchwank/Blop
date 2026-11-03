"use client";

import { useMemo, useState } from "react";
import { Archive, ChevronDown, ChevronRight, Search, Trash2, FolderInput, Target } from "lucide-react";
import FolderFileRow from "@/components/folder/FolderFileRow";
import type { SubfolderRow } from "@/components/folder/SubfolderChipGrid";
import {
    activeAiContextIds,
    countForTab,
    filterSearch,
    groupFiles,
    isAiContextEligible,
    matchesTypeTab,
    sortFiles,
    type FileGroupMode,
    type FileSortMode,
    type FileTypeTab,
    type FolderFile,
} from "@/lib/folderFileUtils";

type Props = {
    files: FolderFile[];
    subfolders: SubfolderRow[];
    aiContextIncludedIds: string[] | null;
    selectedIds: Set<string>;
    openMenuFileId: string | null;
    setOpenMenuFileId: (id: string | null) => void;
    getFileIcon: (type: string) => React.ReactNode;
    youtubeIdFor: (file: FolderFile) => string | null;
    onOpenYoutube: (id: string) => void;
    onToggleSelect: (id: string, checked: boolean) => void;
    onClearSelection: () => void;
    onToggleAi: (id: string, checked: boolean) => void;
    onSetGroupAsAiSources: (ids: string[]) => void;
    onOpen: (file: FolderFile) => void;
    onRename: (file: FolderFile, newName: string) => Promise<void> | void;
    onArchive: (file: FolderFile, archived: boolean) => void;
    onDelete: (file: FolderFile) => void;
    onBulkArchive: () => void;
    onBulkDelete: () => void;
    onBulkUseAsAi: () => void;
    onBulkMove: (targetFolderId: string) => void;
    onMoveFile: (fileId: string, targetFolderId: string) => void;
    onCreateSubfolder: () => void;
    emptyHint?: string;
};

const TABS: { id: FileTypeTab; label: string }[] = [
    { id: "all", label: "Alle" },
    { id: "sources", label: "Quellen & Skripte" },
    { id: "media", label: "Audio & Video" },
    { id: "creations", label: "KI-Erstellungen" },
    { id: "archive", label: "Archiv" },
];

export default function FolderFileManager({
    files,
    subfolders,
    aiContextIncludedIds,
    selectedIds,
    openMenuFileId,
    setOpenMenuFileId,
    getFileIcon,
    youtubeIdFor,
    onOpenYoutube,
    onToggleSelect,
    onClearSelection,
    onToggleAi,
    onSetGroupAsAiSources,
    onOpen,
    onRename,
    onArchive,
    onDelete,
    onBulkArchive,
    onBulkDelete,
    onBulkUseAsAi,
    onBulkMove,
    onMoveFile,
    onCreateSubfolder,
    emptyHint,
}: Props) {
    const [query, setQuery] = useState("");
    const [tab, setTab] = useState<FileTypeTab>("all");
    const [sortMode, setSortMode] = useState<FileSortMode>("newest");
    const [groupMode, setGroupMode] = useState<FileGroupMode>("none");
    const [collapsed, setCollapsed] = useState<Record<string, boolean>>({});
    const [moveOpen, setMoveOpen] = useState(false);
    const [rowMoveFile, setRowMoveFile] = useState<FolderFile | null>(null);

    const activeIds = useMemo(
        () => new Set(activeAiContextIds(files, aiContextIncludedIds)),
        [files, aiContextIncludedIds]
    );

    const filtered = useMemo(() => {
        const searched = filterSearch(files, query);
        const tabbed = searched.filter((f) => matchesTypeTab(f, tab));
        return sortFiles(tabbed, sortMode);
    }, [files, query, tab, sortMode]);

    const groups = useMemo(() => groupFiles(filtered, groupMode), [filtered, groupMode]);

    const selectionCount = selectedIds.size;

    return (
        <div className="relative space-y-3">
            <div className="flex flex-wrap items-center gap-2 justify-between">
                <div className="relative">
                    <Search size={14} className="absolute left-2.5 top-1/2 -translate-y-1/2 text-gray-500" />
                    <input
                        value={query}
                        onChange={(e) => setQuery(e.target.value)}
                        placeholder="Dateien in diesem Ordner suchen..."
                        className="h-8 w-[240px] max-w-[70vw] rounded-lg border border-[#45484F] bg-[#1A1916] pl-8 pr-3 text-[13px] text-white placeholder:text-gray-600 outline-none focus:border-[#5B9DFF]/50"
                    />
                </div>
                <div className="flex flex-wrap items-center gap-2">
                    <select
                        value={sortMode}
                        onChange={(e) => setSortMode(e.target.value as FileSortMode)}
                        className="h-8 rounded-lg border border-[#45484F] bg-[#23252A] px-2 text-[12px] text-gray-300"
                    >
                        <option value="newest">Sortieren: Neueste zuerst</option>
                        <option value="oldest">Älteste zuerst</option>
                        <option value="name">Name A-Z</option>
                        <option value="type">Dateityp</option>
                    </select>
                    <select
                        value={groupMode}
                        onChange={(e) => setGroupMode(e.target.value as FileGroupMode)}
                        className="h-8 rounded-lg border border-[#45484F] bg-[#23252A] px-2 text-[12px] text-gray-300"
                    >
                        <option value="none">Gruppieren: Keine</option>
                        <option value="time">Nach Zeitraum</option>
                        <option value="type">Nach Dateityp</option>
                    </select>
                </div>
            </div>

            <div className="flex flex-wrap gap-1.5">
                {TABS.map((t) => {
                    const n = countForTab(files, t.id);
                    const active = tab === t.id;
                    return (
                        <button
                            key={t.id}
                            type="button"
                            onClick={() => setTab(t.id)}
                            className={`h-8 px-2.5 rounded-lg text-[12px] font-medium border transition-colors ${
                                active
                                    ? "bg-[#2563EB]/15 border-[#2563EB]/40 text-[#93C5FD]"
                                    : "bg-transparent border-white/[0.08] text-gray-400 hover:text-white"
                            }`}
                        >
                            {t.label} ({n})
                        </button>
                    );
                })}
            </div>

            <div className="rounded-xl border border-[#45484F] bg-[#1A1916] overflow-hidden">
                {filtered.length === 0 ? (
                    <p className="px-4 py-10 text-center text-sm text-gray-500">
                        {emptyHint || "Keine Dateien in dieser Ansicht."}
                    </p>
                ) : (
                    groups.map((g) => {
                        const isCollapsed = Boolean(collapsed[g.key]);
                        const groupEligible = g.files.filter(isAiContextEligible);
                        const allAi =
                            groupEligible.length > 0 && groupEligible.every((f) => activeIds.has(f.id));
                        const someAi = groupEligible.some((f) => activeIds.has(f.id));
                        return (
                            <div key={g.key}>
                                {groupMode !== "none" ? (
                                    <div className="flex items-center gap-2 px-3 h-9 bg-white/[0.02] border-b border-white/[0.06]">
                                        <button
                                            type="button"
                                            className="text-gray-400 hover:text-white"
                                            onClick={() =>
                                                setCollapsed((prev) => ({ ...prev, [g.key]: !prev[g.key] }))
                                            }
                                        >
                                            {isCollapsed ? <ChevronRight size={14} /> : <ChevronDown size={14} />}
                                        </button>
                                        {groupEligible.length > 0 ? (
                                            <input
                                                type="checkbox"
                                                className="h-3.5 w-3.5 rounded border-[#45484F] bg-[#23252A]"
                                                checked={allAi}
                                                ref={(el) => {
                                                    if (el) el.indeterminate = !allAi && someAi;
                                                }}
                                                onChange={(e) => {
                                                    if (e.target.checked) {
                                                        onSetGroupAsAiSources(groupEligible.map((f) => f.id));
                                                    } else {
                                                        const remove = new Set(groupEligible.map((f) => f.id));
                                                        const next = [...activeIds].filter((id) => !remove.has(id));
                                                        onSetGroupAsAiSources(next);
                                                    }
                                                }}
                                                title="KI-Quellen dieser Gruppe"
                                            />
                                        ) : null}
                                        <span className="text-[12px] font-medium text-gray-300">
                                            {g.label}
                                            <span className="text-gray-500 ml-1.5">({g.files.length})</span>
                                        </span>
                                    </div>
                                ) : null}
                                {!isCollapsed
                                    ? g.files.map((file) => (
                                          <FolderFileRow
                                              key={file.id}
                                              file={file}
                                              icon={getFileIcon(file.type)}
                                              selected={selectedIds.has(file.id)}
                                              aiActive={activeIds.has(file.id)}
                                              openMenuFileId={openMenuFileId}
                                              setOpenMenuFileId={setOpenMenuFileId}
                                              onToggleSelect={onToggleSelect}
                                              onToggleAi={onToggleAi}
                                              onOpen={onOpen}
                                              onRename={onRename}
                                              onArchive={onArchive}
                                              onDelete={onDelete}
                                              onMoveMenu={(f) => {
                                                  setRowMoveFile(f);
                                                  setMoveOpen(true);
                                              }}
                                              canRename={file.type !== "pdf"}
                                              youtubeId={youtubeIdFor(file)}
                                              onOpenYoutube={onOpenYoutube}
                                          />
                                      ))
                                    : null}
                            </div>
                        );
                    })
                )}
            </div>

            {selectionCount > 0 ? (
                <div className="fixed bottom-24 left-1/2 z-[90] -translate-x-1/2 flex flex-wrap items-center gap-2 rounded-2xl border border-[#45484F] bg-[#1A1916]/95 px-3 py-2.5 shadow-2xl backdrop-blur-md max-w-[min(96vw,42rem)]">
                    <span className="text-[12px] text-gray-300 px-1">{selectionCount} ausgewählt</span>
                    <button
                        type="button"
                        onClick={onBulkUseAsAi}
                        className="inline-flex items-center gap-1.5 h-8 px-2.5 rounded-lg text-[12px] font-medium bg-[#2563EB] text-white hover:bg-[#1D4ED8]"
                    >
                        <Target size={13} /> Nur diese {selectionCount} als KI-Quelle nutzen
                    </button>
                    <div className="relative">
                        <button
                            type="button"
                            onClick={() => setMoveOpen((v) => !v)}
                            className="inline-flex items-center gap-1.5 h-8 px-2.5 rounded-lg text-[12px] font-medium bg-[#353840] border border-[#45484F] text-gray-200"
                        >
                            <FolderInput size={13} /> In Unterordner verschieben
                            <ChevronDown size={12} />
                        </button>
                        {moveOpen && !rowMoveFile ? (
                            <div className="absolute bottom-full mb-1 left-0 w-56 rounded-xl border border-[#45484F] bg-[#23252A] shadow-2xl overflow-hidden z-50">
                                {subfolders.length === 0 ? (
                                    <p className="px-3 py-2 text-xs text-gray-500">Keine Unterordner</p>
                                ) : (
                                    subfolders.map((sf) => (
                                        <button
                                            key={sf.id}
                                            type="button"
                                            onClick={() => {
                                                setMoveOpen(false);
                                                onBulkMove(sf.id);
                                            }}
                                            className="w-full text-left px-3 py-2 text-sm text-gray-300 hover:bg-[#3A3D45] truncate"
                                        >
                                            {sf.name}
                                        </button>
                                    ))
                                )}
                                <div className="h-px bg-[#3A3D45]" />
                                <button
                                    type="button"
                                    onClick={() => {
                                        setMoveOpen(false);
                                        onCreateSubfolder();
                                    }}
                                    className="w-full text-left px-3 py-2 text-sm text-[#93C5FD] hover:bg-[#3A3D45]"
                                >
                                    + Neuer Unterordner
                                </button>
                            </div>
                        ) : null}
                    </div>
                    <button
                        type="button"
                        onClick={onBulkArchive}
                        className="inline-flex items-center gap-1.5 h-8 px-2.5 rounded-lg text-[12px] font-medium bg-[#353840] border border-[#45484F] text-gray-200"
                    >
                        <Archive size={13} /> Archivieren
                    </button>
                    <button
                        type="button"
                        onClick={onBulkDelete}
                        className="inline-flex items-center gap-1.5 h-8 px-2.5 rounded-lg text-[12px] font-medium bg-red-500/10 border border-red-500/25 text-red-300"
                    >
                        <Trash2 size={13} /> Löschen
                    </button>
                    <button
                        type="button"
                        onClick={onClearSelection}
                        className="text-[11px] text-gray-500 hover:text-white px-1"
                    >
                        Abwählen
                    </button>
                </div>
            ) : null}

            {rowMoveFile ? (
                <div className="fixed inset-0 z-[120] flex items-end sm:items-center justify-center bg-black/50 p-4" onClick={() => setRowMoveFile(null)}>
                    <div
                        className="w-full max-w-sm rounded-2xl border border-[#45484F] bg-[#23252A] shadow-2xl overflow-hidden"
                        onClick={(e) => e.stopPropagation()}
                    >
                        <div className="px-4 py-3 border-b border-[#45484F] text-sm text-white font-medium">
                            Verschieben: {rowMoveFile.name}
                        </div>
                        <div className="max-h-64 overflow-y-auto">
                            {subfolders.length === 0 ? (
                                <p className="px-4 py-3 text-xs text-gray-500">Keine Unterordner</p>
                            ) : (
                                subfolders.map((sf) => (
                                    <button
                                        key={sf.id}
                                        type="button"
                                        onClick={() => {
                                            const id = rowMoveFile.id;
                                            setRowMoveFile(null);
                                            onMoveFile(id, sf.id);
                                        }}
                                        className="w-full text-left px-4 py-2.5 text-sm text-gray-300 hover:bg-[#3A3D45]"
                                    >
                                        {sf.name}
                                    </button>
                                ))
                            )}
                        </div>
                        <button
                            type="button"
                            className="w-full px-4 py-2.5 text-sm text-[#93C5FD] border-t border-[#45484F] hover:bg-[#3A3D45]"
                            onClick={() => {
                                setRowMoveFile(null);
                                onCreateSubfolder();
                            }}
                        >
                            + Neuer Unterordner
                        </button>
                    </div>
                </div>
            ) : null}
        </div>
    );
}
