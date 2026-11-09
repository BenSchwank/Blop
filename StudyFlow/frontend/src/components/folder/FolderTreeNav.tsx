"use client";

import { useCallback, useEffect, useMemo, useState } from "react";
import Link from "next/link";
import { usePathname, useRouter } from "next/navigation";
import { ChevronDown, ChevronRight, Folder, Star } from "lucide-react";
import { useDroppable } from "@dnd-kit/core";
import { getSessionId, sessionHeaders } from "@/lib/session";
import {
    isLegacyOrYearFolder,
    loadPinnedFolderIds,
    partitionFolders,
    type SortableFolder,
} from "@/lib/folderSort";

export type FolderTreeNode = {
    id: string;
    name: string;
    file_count?: number;
    children?: FolderTreeNode[];
};

function TreeNode({
    node,
    depth,
    expanded,
    toggle,
    activeId,
    isCollapsedSidebar,
}: {
    node: FolderTreeNode;
    depth: number;
    expanded: Record<string, boolean>;
    toggle: (id: string) => void;
    activeId: string | null;
    isCollapsedSidebar: boolean;
}) {
    const children = node.children || [];
    const hasKids = children.length > 0;
    const isOpen = Boolean(expanded[node.id]);
    const active = activeId === node.id;
    const { isOver, setNodeRef } = useDroppable({
        id: `tree-drop-${node.id}`,
        data: { type: "tree-folder", folderId: node.id, name: node.name },
    });

    const activeCls =
        active || isOver
            ? "bg-[rgba(59,130,246,0.14)] text-[#60A5FA] border-l-2 border-[#3B82F6]"
            : "text-[#B8BEC9] hover:bg-white/[0.06] hover:text-white border-l-2 border-transparent";

    if (isCollapsedSidebar) {
        return (
            <div ref={setNodeRef}>
                <Link
                    href={`/folder/${node.id}`}
                    title={node.name}
                    className={`flex items-center justify-center py-2 rounded-[10px] ${
                        active || isOver
                            ? "bg-[rgba(59,130,246,0.14)] text-[#60A5FA]"
                            : "text-[#B8BEC9] hover:bg-white/[0.06]"
                    }`}
                >
                    <Folder size={16} className={active ? "text-[#60A5FA]" : ""} />
                </Link>
                {hasKids && isOpen
                    ? children.map((c) => (
                          <TreeNode
                              key={c.id}
                              node={c}
                              depth={depth + 1}
                              expanded={expanded}
                              toggle={toggle}
                              activeId={activeId}
                              isCollapsedSidebar={isCollapsedSidebar}
                          />
                      ))
                    : null}
            </div>
        );
    }

    return (
        <div ref={setNodeRef}>
            <div
                className={`flex items-center gap-1 rounded-r-lg pr-1 min-w-0 ${activeCls}`}
                style={{ paddingLeft: 8 + Math.min(depth, 3) * 12 }}
            >
                {hasKids ? (
                    <button
                        type="button"
                        className="!h-6 !w-6 !min-h-0 !p-0 flex items-center justify-center shrink-0 text-gray-500 hover:text-white"
                        onClick={() => toggle(node.id)}
                        aria-label={isOpen ? "Zuklappen" : "Aufklappen"}
                    >
                        {isOpen ? <ChevronDown size={12} /> : <ChevronRight size={12} />}
                    </button>
                ) : (
                    <span className="w-6 shrink-0" />
                )}
                <Link
                    href={`/folder/${node.id}`}
                    className="flex min-w-0 flex-1 items-center gap-1.5 py-1.5 text-[13px]"
                    title={node.name}
                >
                    <Folder size={13} className={`shrink-0 ${active ? "text-[#60A5FA]" : ""}`} />
                    <span className="truncate min-w-0">{node.name}</span>
                    <span className="ml-auto shrink-0 text-[10px] text-gray-500">
                        {node.file_count ?? 0}
                    </span>
                </Link>
            </div>
            {hasKids && isOpen
                ? children.map((c) => (
                      <TreeNode
                          key={c.id}
                          node={c}
                          depth={depth + 1}
                          expanded={expanded}
                          toggle={toggle}
                          activeId={activeId}
                          isCollapsedSidebar={isCollapsedSidebar}
                      />
                  ))
                : null}
        </div>
    );
}

type Props = {
    isCollapsedSidebar: boolean;
};

export default function FolderTreeNav({ isCollapsedSidebar }: Props) {
    const pathname = usePathname();
    const router = useRouter();
    const [tree, setTree] = useState<FolderTreeNode[]>([]);
    const [expanded, setExpanded] = useState<Record<string, boolean>>({});
    const [modulesOpen, setModulesOpen] = useState(true);
    const [legacyOpen, setLegacyOpen] = useState(false);
    const [pinned, setPinned] = useState<Set<string>>(() => new Set());

    const activeId = pathname?.startsWith("/folder/") ? pathname.split("/")[2] || null : null;

    const loadTree = useCallback(async () => {
        const username = typeof window !== "undefined" ? localStorage.getItem("username") || "" : "";
        if (!username) return;
        const sid = getSessionId() || "";
        try {
            const res = await fetch(
                `/api/folders/tree?username=${encodeURIComponent(username)}&session_id=${encodeURIComponent(sid)}`,
                { cache: "no-store", headers: sessionHeaders() },
            );
            if (!res.ok) return;
            const data = await res.json();
            setTree(Array.isArray(data) ? data : []);
        } catch {
            /* ignore */
        }
    }, []);

    useEffect(() => {
        setPinned(loadPinnedFolderIds());
        void loadTree();
        const onChanged = () => {
            setPinned(loadPinnedFolderIds());
            void loadTree();
        };
        window.addEventListener("blop_folders_changed", onChanged);
        return () => window.removeEventListener("blop_folders_changed", onChanged);
    }, [loadTree]);

    useEffect(() => {
        if (!activeId || tree.length === 0) return;
        const ancestors: string[] = [];
        const walk = (nodes: FolderTreeNode[], trail: string[]): boolean => {
            for (const n of nodes) {
                if (n.id === activeId) {
                    ancestors.push(...trail);
                    return true;
                }
                if (walk(n.children || [], [...trail, n.id])) return true;
            }
            return false;
        };
        walk(tree, []);
        if (ancestors.length) {
            setExpanded((prev) => {
                const next = { ...prev };
                for (const id of ancestors) next[id] = true;
                return next;
            });
            // Open legacy section if active path lives there
            const rootId = ancestors[0] || activeId;
            const root = tree.find((n) => n.id === rootId);
            if (root && isLegacyOrYearFolder(root as SortableFolder) && !pinned.has(root.id)) {
                setLegacyOpen(true);
            }
        }
    }, [activeId, tree, pinned]);

    const { modules, legacy } = useMemo(() => {
        return partitionFolders(tree as SortableFolder[], pinned) as {
            modules: FolderTreeNode[];
            legacy: FolderTreeNode[];
        };
    }, [tree, pinned]);

    const toggle = (id: string) =>
        setExpanded((prev) => ({ ...prev, [id]: !prev[id] }));

    if (tree.length === 0 && isCollapsedSidebar) return null;

    const renderList = (nodes: FolderTreeNode[]) =>
        nodes.map((n) => (
            <TreeNode
                key={n.id}
                node={n}
                depth={0}
                expanded={expanded}
                toggle={toggle}
                activeId={activeId}
                isCollapsedSidebar={isCollapsedSidebar}
            />
        ));

    return (
        <div className="mt-3 pt-3 border-t border-white/10 min-w-0">
            {!isCollapsedSidebar ? (
                <>
                    <button
                        type="button"
                        onClick={() => setModulesOpen((v) => !v)}
                        className="w-full flex items-center justify-between px-2 mb-1.5 text-[10px] font-semibold uppercase tracking-wider text-gray-500 hover:text-gray-300 !min-h-0 !py-1 !px-2"
                    >
                        <span className="flex items-center gap-1 truncate">
                            <Star size={10} className="text-amber-400 shrink-0" /> MEINE MODULE
                        </span>
                        {modulesOpen ? <ChevronDown size={12} /> : <ChevronRight size={12} />}
                    </button>
                    {modulesOpen ? (
                        <div className="space-y-0.5 mb-2 max-h-[28vh] overflow-y-auto custom-scrollbar">
                            {modules.length === 0 ? (
                                <p className="px-3 py-2 text-[11px] text-gray-600">Noch keine Module</p>
                            ) : (
                                renderList(modules)
                            )}
                        </div>
                    ) : null}

                    {legacy.length > 0 ? (
                        <>
                            <button
                                type="button"
                                onClick={() => setLegacyOpen((v) => !v)}
                                className="w-full flex items-center justify-between px-2 mb-1.5 text-[10px] font-semibold uppercase tracking-wider text-gray-500 hover:text-gray-300 !min-h-0 !py-1 !px-2"
                            >
                                <span className="truncate">
                                    WEITERE / ALTE ORDNER ({legacy.length})
                                </span>
                                {legacyOpen ? <ChevronDown size={12} /> : <ChevronRight size={12} />}
                            </button>
                            {legacyOpen ? (
                                <div className="space-y-0.5 max-h-[20vh] overflow-y-auto custom-scrollbar">
                                    {renderList(legacy)}
                                </div>
                            ) : null}
                        </>
                    ) : null}

                    <button
                        type="button"
                        onClick={() => router.push("/")}
                        className="w-full text-left px-3 py-1.5 text-[11px] text-[#93C5FD] hover:text-white !min-h-0"
                    >
                        Zum Dashboard
                    </button>
                </>
            ) : (
                <div className="space-y-0.5 max-h-[40vh] overflow-y-auto custom-scrollbar">
                    {renderList([...modules, ...legacy])}
                </div>
            )}
        </div>
    );
}
