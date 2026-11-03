"use client";

import { useCallback, useEffect, useState } from "react";
import Link from "next/link";
import { usePathname, useRouter } from "next/navigation";
import { ChevronDown, ChevronRight, Folder } from "lucide-react";
import { useDroppable } from "@dnd-kit/core";
import { getSessionId, sessionHeaders } from "@/lib/session";

export type FolderTreeNode = {
    id: string;
    name: string;
    file_count?: number;
    children?: FolderTreeNode[];
};

function TreeNode({
    node,
    depth,
    collapsed,
    toggle,
    activeId,
    isCollapsedSidebar,
}: {
    node: FolderTreeNode;
    depth: number;
    collapsed: Record<string, boolean>;
    toggle: (id: string) => void;
    activeId: string | null;
    isCollapsedSidebar: boolean;
}) {
    const children = node.children || [];
    const hasKids = children.length > 0;
    const isOpen = !collapsed[node.id];
    const active = activeId === node.id;
    const { isOver, setNodeRef } = useDroppable({
        id: `tree-drop-${node.id}`,
        data: { type: "tree-folder", folderId: node.id, name: node.name },
    });

    if (isCollapsedSidebar) {
        return (
            <div ref={setNodeRef}>
                <Link
                    href={`/folder/${node.id}`}
                    title={node.name}
                    className={`flex items-center justify-center py-2 rounded-[10px] ${
                        active || isOver ? "bg-[#5B9DFF]/18 text-white" : "text-[#B8BEC9] hover:bg-white/[0.06]"
                    }`}
                >
                    <Folder size={16} className={active ? "text-[#5B9DFF]" : ""} />
                </Link>
                {hasKids && isOpen
                    ? children.map((c) => (
                          <TreeNode
                              key={c.id}
                              node={c}
                              depth={depth + 1}
                              collapsed={collapsed}
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
                className={`flex items-center gap-1 rounded-lg pr-1 ${
                    active || isOver ? "bg-[#5B9DFF]/15 text-white" : "text-[#B8BEC9] hover:bg-white/[0.06] hover:text-white"
                }`}
                style={{ paddingLeft: 6 + depth * 10 }}
            >
                {hasKids ? (
                    <button
                        type="button"
                        className="h-6 w-6 flex items-center justify-center shrink-0 text-gray-500 hover:text-white"
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
                    <Folder size={13} className={`shrink-0 ${active ? "text-[#5B9DFF]" : ""}`} />
                    <span className="truncate">{node.name}</span>
                    <span className="ml-auto shrink-0 text-[10px] text-gray-500">{node.file_count ?? 0}</span>
                </Link>
            </div>
            {hasKids && isOpen
                ? children.map((c) => (
                      <TreeNode
                          key={c.id}
                          node={c}
                          depth={depth + 1}
                          collapsed={collapsed}
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
    const [collapsed, setCollapsed] = useState<Record<string, boolean>>({});
    const [sectionOpen, setSectionOpen] = useState(true);

    const activeId = pathname?.startsWith("/folder/") ? pathname.split("/")[2] || null : null;

    const loadTree = useCallback(async () => {
        const username = typeof window !== "undefined" ? localStorage.getItem("username") || "" : "";
        if (!username) return;
        const sid = getSessionId() || "";
        try {
            const res = await fetch(
                `/api/folders/tree?username=${encodeURIComponent(username)}&session_id=${encodeURIComponent(sid)}`,
                { cache: "no-store", headers: sessionHeaders() }
            );
            if (!res.ok) return;
            const data = await res.json();
            setTree(Array.isArray(data) ? data : []);
        } catch {
            /* ignore */
        }
    }, []);

    useEffect(() => {
        void loadTree();
        const onChanged = () => void loadTree();
        window.addEventListener("blop_folders_changed", onChanged);
        return () => window.removeEventListener("blop_folders_changed", onChanged);
    }, [loadTree]);

    // Prefetch active path ancestors expanded
    useEffect(() => {
        if (!activeId || tree.length === 0) return;
        const path: string[] = [];
        const walk = (nodes: FolderTreeNode[], trail: string[]): boolean => {
            for (const n of nodes) {
                const next = [...trail, n.id];
                if (n.id === activeId) {
                    path.push(...trail);
                    return true;
                }
                if (walk(n.children || [], next)) {
                    path.push(...trail);
                    return true;
                }
            }
            return false;
        };
        walk(tree, []);
        if (path.length) {
            setCollapsed((prev) => {
                const next = { ...prev };
                for (const id of path) next[id] = false;
                return next;
            });
        }
    }, [activeId, tree]);

    const toggle = (id: string) => setCollapsed((prev) => ({ ...prev, [id]: !prev[id] }));

    if (tree.length === 0 && isCollapsedSidebar) return null;

    return (
        <div className="mt-3 pt-3 border-t border-white/10">
            {!isCollapsedSidebar ? (
                <button
                    type="button"
                    onClick={() => setSectionOpen((v) => !v)}
                    className="w-full flex items-center justify-between px-2 mb-1.5 text-[10px] font-semibold uppercase tracking-wider text-gray-500 hover:text-gray-300"
                >
                    <span>Meine Module &amp; Ordner</span>
                    {sectionOpen ? <ChevronDown size={12} /> : <ChevronRight size={12} />}
                </button>
            ) : null}
            {(sectionOpen || isCollapsedSidebar) && (
                <div className="space-y-0.5 max-h-[40vh] overflow-y-auto custom-scrollbar">
                    {tree.length === 0 ? (
                        !isCollapsedSidebar ? (
                            <p className="px-3 py-2 text-[11px] text-gray-600">Noch keine Ordner</p>
                        ) : null
                    ) : (
                        tree.map((n) => (
                            <TreeNode
                                key={n.id}
                                node={n}
                                depth={0}
                                collapsed={collapsed}
                                toggle={toggle}
                                activeId={activeId}
                                isCollapsedSidebar={isCollapsedSidebar}
                            />
                        ))
                    )}
                    {!isCollapsedSidebar ? (
                        <button
                            type="button"
                            onClick={() => router.push("/")}
                            className="w-full text-left px-3 py-1.5 text-[11px] text-[#93C5FD] hover:text-white"
                        >
                            Zum Dashboard
                        </button>
                    ) : null}
                </div>
            )}
        </div>
    );
}
