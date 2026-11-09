"use client";

import { useEffect, useLayoutEffect, useRef, useState } from "react";
import { createPortal } from "react-dom";
import { useDraggable } from "@dnd-kit/core";
import {
    MoreVertical,
    Edit,
    Archive,
    ArchiveRestore,
    Trash2,
    FolderInput,
    ExternalLink,
    Check,
    Download,
    Sparkles,
} from "lucide-react";
import {
    formatCompactDate,
    isAiContextEligible,
    isArchived,
    typeBadgeLabel,
    typeIconTone,
    CREATION_TYPES,
    type FolderFile,
} from "@/lib/folderFileUtils";

type Props = {
    file: FolderFile;
    icon: React.ReactNode;
    selected: boolean;
    aiActive: boolean;
    openMenuFileId: string | null;
    setOpenMenuFileId: (id: string | null) => void;
    onToggleSelect: (id: string, checked: boolean, ev?: React.MouseEvent | React.ChangeEvent) => void;
    onOpen: (file: FolderFile) => void;
    onRename: (file: FolderFile, newName: string) => Promise<void> | void;
    onArchive: (file: FolderFile, archived: boolean) => void;
    onDelete: (file: FolderFile) => void;
    onMoveMenu: (file: FolderFile) => void;
    canRename: boolean;
    youtubeId?: string | null;
    onOpenYoutube?: (id: string) => void;
    onRefine?: (file: FolderFile) => void;
    onDownload?: (file: FolderFile) => void;
};

type MenuPos = { top: number; left: number; openUp: boolean };

export default function FolderFileRow({
    file,
    icon,
    selected,
    aiActive,
    openMenuFileId,
    setOpenMenuFileId,
    onToggleSelect,
    onOpen,
    onRename,
    onArchive,
    onDelete,
    onMoveMenu,
    canRename,
    youtubeId,
    onOpenYoutube,
    onRefine,
    onDownload,
}: Props) {
    const { attributes, listeners, setNodeRef, isDragging } = useDraggable({
        id: `drag-${file.id}`,
        data: { type: "file", file },
    });
    const [editing, setEditing] = useState(false);
    const [draft, setDraft] = useState(file.name);
    const [menuPos, setMenuPos] = useState<MenuPos | null>(null);
    const inputRef = useRef<HTMLInputElement>(null);
    const menuBtnRef = useRef<HTMLButtonElement>(null);
    const menuRef = useRef<HTMLDivElement>(null);
    const archived = isArchived(file);
    const aiEligible = isAiContextEligible(file);
    const menuOpen = openMenuFileId === file.id;
    const checked = selected || (!archived && aiActive);
    const isCreation = CREATION_TYPES.has(file.type);

    useEffect(() => {
        if (editing) inputRef.current?.focus();
    }, [editing]);

    useLayoutEffect(() => {
        if (!menuOpen || !menuBtnRef.current) {
            setMenuPos(null);
            return;
        }
        const rect = menuBtnRef.current.getBoundingClientRect();
        const menuH = 220;
        const spaceBelow = window.innerHeight - rect.bottom;
        const openUp = spaceBelow < menuH;
        const top = openUp ? rect.top - 4 : rect.bottom + 4;
        const left = Math.min(rect.right - 192, window.innerWidth - 200);
        setMenuPos({ top, left: Math.max(8, left), openUp });
    }, [menuOpen]);

    useEffect(() => {
        if (!menuOpen) return;
        const onKey = (e: KeyboardEvent) => {
            if (e.key === "Escape") setOpenMenuFileId(null);
        };
        const onDown = (e: MouseEvent) => {
            const t = e.target as Node;
            if (menuRef.current?.contains(t) || menuBtnRef.current?.contains(t)) return;
            setOpenMenuFileId(null);
        };
        window.addEventListener("keydown", onKey);
        window.addEventListener("mousedown", onDown);
        return () => {
            window.removeEventListener("keydown", onKey);
            window.removeEventListener("mousedown", onDown);
        };
    }, [menuOpen, setOpenMenuFileId]);

    const commitRename = async () => {
        const next = draft.trim();
        setEditing(false);
        if (!next || next === file.name) {
            setDraft(file.name);
            return;
        }
        await onRename(file, next);
    };

    const toggle = (ev?: React.MouseEvent | React.ChangeEvent) => {
        onToggleSelect(file.id, !checked, ev);
    };

    const menuItem =
        "w-full min-h-9 flex items-center gap-2 px-3 py-2 text-sm text-gray-300 hover:bg-[#3A3D45] text-left";

    return (
        <div
            ref={setNodeRef}
            {...attributes}
            className={`group flex h-11 items-center gap-2.5 border-b border-white/[0.06] px-3 transition-colors overflow-visible ${
                isDragging ? "opacity-40" : ""
            } ${
                checked && !archived
                    ? "bg-[rgba(37,99,235,0.08)]"
                    : "hover:bg-white/[0.03]"
            }`}
            onClick={(e) => {
                const t = e.target as HTMLElement;
                if (t.closest("[data-row-open]") || t.closest("[data-row-menu]") || t.closest("input")) return;
                toggle(e);
            }}
        >
            <button
                type="button"
                role="checkbox"
                aria-checked={checked}
                aria-label="Datei auswählen"
                title={aiEligible ? "Auswahl & KI-Quelle" : "Auswahl"}
                onClick={(e) => {
                    e.stopPropagation();
                    toggle(e);
                }}
                onPointerDown={(e) => e.stopPropagation()}
                className={`shrink-0 flex !h-[18px] !w-[18px] !min-h-0 !p-0 items-center justify-center rounded-[5px] border-[1.5px] transition-colors ${
                    checked
                        ? "border-[#2563EB] bg-[#2563EB] text-white"
                        : "border-[#4B5563] bg-transparent text-transparent"
                }`}
            >
                {checked ? <Check size={12} strokeWidth={3} className="text-white" /> : null}
            </button>

            <div
                className={`h-7 w-7 rounded-md flex items-center justify-center shrink-0 cursor-grab ${typeIconTone(file.type)}`}
                {...listeners}
            >
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
                        data-row-open
                        className="min-w-0 text-left truncate text-[13px] font-medium text-white hover:underline !min-h-0 !p-0 !h-auto"
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
                        KI-Quelle
                    </span>
                ) : null}
                {canRename && !editing ? (
                    <button
                        type="button"
                        className="hidden group-hover:inline-flex items-center gap-1 shrink-0 text-[11px] text-gray-400 hover:text-white !min-h-0 !px-1 !py-0.5"
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

            <div
                className="relative shrink-0"
                data-row-menu
                onPointerDown={(e) => e.stopPropagation()}
                onClick={(e) => e.stopPropagation()}
            >
                <button
                    ref={menuBtnRef}
                    type="button"
                    onClick={() => setOpenMenuFileId(menuOpen ? null : file.id)}
                    className="!h-8 !w-8 !min-h-0 !p-0 flex items-center justify-center rounded-md text-[#9CA3AF] hover:text-[#F8FAFC] hover:bg-white/[0.08]"
                    aria-label="Dateiaktionen"
                    aria-expanded={menuOpen}
                >
                    <MoreVertical size={15} />
                </button>
                {menuOpen && menuPos && typeof document !== "undefined"
                    ? createPortal(
                          <div
                              ref={menuRef}
                              className="w-48 bg-[#23252A] border border-[#45484F] rounded-xl shadow-2xl z-[9999]"
                              style={{
                                  position: "fixed",
                                  top: menuPos.openUp ? undefined : menuPos.top,
                                  bottom: menuPos.openUp
                                      ? window.innerHeight - menuPos.top
                                      : undefined,
                                  left: menuPos.left,
                              }}
                              data-row-menu
                          >
                              <button
                                  type="button"
                                  onClick={() => {
                                      setOpenMenuFileId(null);
                                      onOpen(file);
                                  }}
                                  className={menuItem}
                              >
                                  <ExternalLink size={14} /> Öffnen
                              </button>
                              {isCreation && onRefine ? (
                                  <button
                                      type="button"
                                      onClick={() => {
                                          setOpenMenuFileId(null);
                                          onRefine(file);
                                      }}
                                      className={menuItem}
                                  >
                                      <Sparkles size={14} /> Mit Prompt anpassen…
                                  </button>
                              ) : null}
                              {youtubeId && onOpenYoutube ? (
                                  <button
                                      type="button"
                                      onClick={() => {
                                          setOpenMenuFileId(null);
                                          onOpenYoutube(youtubeId);
                                      }}
                                      className={menuItem}
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
                                      className={menuItem}
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
                                  className={menuItem}
                              >
                                  <FolderInput size={14} /> In Ordner verschieben…
                              </button>
                              {onDownload ? (
                                  <button
                                      type="button"
                                      onClick={() => {
                                          setOpenMenuFileId(null);
                                          onDownload(file);
                                      }}
                                      className={menuItem}
                                  >
                                      <Download size={14} /> Herunterladen
                                  </button>
                              ) : null}
                              <button
                                  type="button"
                                  onClick={() => {
                                      setOpenMenuFileId(null);
                                      onArchive(file, !archived);
                                  }}
                                  className={menuItem}
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
                                  className="w-full min-h-9 flex items-center gap-2 px-3 py-2 text-sm text-[#F87171] hover:bg-red-500/10 text-left"
                              >
                                  <Trash2 size={14} /> Löschen
                              </button>
                          </div>,
                          document.body,
                      )
                    : null}
            </div>
        </div>
    );
}
