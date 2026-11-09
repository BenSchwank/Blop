/** Classify and sort study folders: modules first, year/empty last. */

export type SortableFolder = {
    id: string;
    name: string;
    file_count?: number;
    files?: unknown[];
    parent_id?: string | null;
    parentId?: string | null;
    parent_folder_id?: string | null;
    parentFolderId?: string | null;
    children?: SortableFolder[];
};

const PIN_KEY = "blop_pinned_folders";

export function loadPinnedFolderIds(): Set<string> {
    if (typeof window === "undefined") return new Set();
    try {
        const raw = localStorage.getItem(PIN_KEY);
        if (!raw) return new Set();
        const arr = JSON.parse(raw);
        return new Set(Array.isArray(arr) ? arr.map(String) : []);
    } catch {
        return new Set();
    }
}

export function savePinnedFolderIds(ids: Set<string>): void {
    if (typeof window === "undefined") return;
    localStorage.setItem(PIN_KEY, JSON.stringify([...ids]));
}

export function togglePinnedFolder(id: string): Set<string> {
    const next = loadPinnedFolderIds();
    if (next.has(id)) next.delete(id);
    else next.add(id);
    savePinnedFolderIds(next);
    return next;
}

export function isRootFolder(f: SortableFolder): boolean {
    const p =
        f.parent_id ?? f.parentId ?? f.parent_folder_id ?? f.parentFolderId ?? null;
    return p == null || p === "" || p === undefined;
}

/** Year / exam / test / empty legacy folders. */
export function isLegacyOrYearFolder(f: SortableFolder): boolean {
    const name = (f.name || "").trim();
    const lower = name.toLowerCase();
    const count = fileCount(f);
    if (/^\d{2,4}(\s|$|[-_.])/.test(name)) return true;
    if (/^(19|20)\d{2}$/.test(name)) return true;
    if (/\bprüfung\b/i.test(name)) return true;
    if (/^(test|tmp|temp|alt|archiv|old)\b/i.test(lower)) return true;
    if (count === 0 && !/[a-zäöüß]/i.test(name.replace(/\d/g, ""))) return true;
    return false;
}

export function isModuleFolder(f: SortableFolder): boolean {
    if (isLegacyOrYearFolder(f)) return false;
    const count = fileCount(f);
    // Named study modules (with or without files) that aren't year/test patterns.
    if (count > 0) return true;
    const lower = (f.name || "").toLowerCase();
    if (
        /vorlesung|mathe|informatik|englisch|physik|gerät|esp|et\b|phy\b|chemie|wirtschaft|recht/i.test(
            lower,
        )
    ) {
        return true;
    }
    // Non-empty-looking names that aren't pure digits
    return /[a-zäöüß]{3,}/i.test(f.name || "") && count >= 0 && !isLegacyOrYearFolder(f);
}

export function fileCount(f: SortableFolder): number {
    if (typeof f.file_count === "number") return f.file_count;
    if (Array.isArray(f.files)) return f.files.length;
    return 0;
}

export function partitionFolders<T extends SortableFolder>(
    folders: T[],
    pinned: Set<string>,
): { modules: T[]; legacy: T[] } {
    const roots = folders.filter(isRootFolder);
    const modules: T[] = [];
    const legacy: T[] = [];
    for (const f of roots) {
        if (pinned.has(f.id) || isModuleFolder(f)) modules.push(f);
        else legacy.push(f);
    }
    const sortFn = (a: T, b: T) => {
        const ap = pinned.has(a.id) ? 0 : 1;
        const bp = pinned.has(b.id) ? 0 : 1;
        if (ap !== bp) return ap - bp;
        const ac = fileCount(a);
        const bc = fileCount(b);
        if (bc !== ac) return bc - ac;
        return (a.name || "").localeCompare(b.name || "", "de", { sensitivity: "base" });
    };
    modules.sort(sortFn);
    legacy.sort((a, b) => {
        const ac = fileCount(a);
        const bc = fileCount(b);
        if (ac === 0 && bc !== 0) return 1;
        if (bc === 0 && ac !== 0) return -1;
        return (a.name || "").localeCompare(b.name || "", "de", { sensitivity: "base" });
    });
    return { modules, legacy };
}

export function sortFoldersSmart<T extends SortableFolder>(
    folders: T[],
    pinned: Set<string>,
): T[] {
    const { modules, legacy } = partitionFolders(folders, pinned);
    return [...modules, ...legacy];
}
