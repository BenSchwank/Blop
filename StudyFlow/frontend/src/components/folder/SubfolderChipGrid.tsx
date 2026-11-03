"use client";

import { useDroppable } from "@dnd-kit/core";
import { Edit, Trash2, Folder } from "lucide-react";

export type SubfolderRow = {
    id: string;
    name: string;
    parent_id?: string | null;
    file_count?: number;
};

function SubfolderChip({
    subfolder,
    onClick,
    onRename,
    onDelete,
}: {
    subfolder: SubfolderRow;
    onClick: () => void;
    onRename: (sf: SubfolderRow) => void;
    onDelete: (sf: SubfolderRow) => void;
}) {
    const { isOver, setNodeRef } = useDroppable({
        id: `drop-${subfolder.id}`,
        data: { type: "subfolder", subfolder },
    });

    return (
        <div
            ref={setNodeRef}
            className={`group flex h-[42px] items-center gap-2 rounded-lg border px-3 transition-all ${
                isOver
                    ? "border-[#5B9DFF] bg-[#5B9DFF]/15 shadow-md shadow-[#5B9DFF]/20"
                    : "border-[#45484F] bg-[#353840] hover:border-[#5B9DFF]/40 hover:bg-[#3A3D45]"
            }`}
        >
            <button type="button" onClick={onClick} className="flex min-w-0 flex-1 items-center gap-2 text-left h-full">
                <Folder size={15} className="shrink-0 text-[#5B9DFF]" />
                <span className="truncate text-[13px] font-medium text-gray-200 group-hover:text-white">{subfolder.name}</span>
                <span className="shrink-0 text-[11px] text-gray-500">({subfolder.file_count ?? 0})</span>
            </button>
            <div
                className="ml-auto hidden items-center gap-0.5 shrink-0 group-hover:flex"
                onPointerDown={(e) => e.stopPropagation()}
                onClick={(e) => e.stopPropagation()}
            >
                <button
                    type="button"
                    onClick={() => onRename(subfolder)}
                    className="h-7 w-7 flex items-center justify-center rounded-md text-gray-400 hover:text-white hover:bg-[#23233b]"
                    aria-label="Unterordner umbenennen"
                >
                    <Edit size={13} />
                </button>
                <button
                    type="button"
                    onClick={() => onDelete(subfolder)}
                    className="h-7 w-7 flex items-center justify-center rounded-md text-gray-400 hover:text-red-300 hover:bg-red-500/10"
                    aria-label="Unterordner löschen"
                >
                    <Trash2 size={13} />
                </button>
            </div>
        </div>
    );
}

type Props = {
    subfolders: SubfolderRow[];
    onOpen: (sf: SubfolderRow) => void;
    onRename: (sf: SubfolderRow) => void;
    onDelete: (sf: SubfolderRow) => void;
};

export default function SubfolderChipGrid({ subfolders, onOpen, onRename, onDelete }: Props) {
    return (
        <div className="mb-5">
            <h3 className="mb-2 text-[11px] font-semibold uppercase tracking-wider text-gray-500">Unterordner</h3>
            {subfolders.length === 0 ? (
                <p className="text-xs text-gray-600 italic">Noch keine Unterordner</p>
            ) : (
                <div
                    className="grid gap-2.5"
                    style={{ gridTemplateColumns: "repeat(auto-fill, minmax(200px, 1fr))" }}
                >
                    {subfolders.map((sf) => (
                        <SubfolderChip
                            key={sf.id}
                            subfolder={sf}
                            onClick={() => onOpen(sf)}
                            onRename={onRename}
                            onDelete={onDelete}
                        />
                    ))}
                </div>
            )}
        </div>
    );
}
