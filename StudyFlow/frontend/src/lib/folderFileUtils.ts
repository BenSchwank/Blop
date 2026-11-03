export type FolderFile = {
    id: string;
    name: string;
    type: string;
    created_at: string;
    archived?: boolean;
};

export type FileTypeTab = "all" | "sources" | "media" | "creations" | "archive";
export type FileSortMode = "newest" | "oldest" | "name" | "type";
export type FileGroupMode = "none" | "time" | "type";

export const AI_CONTEXT_FILE_TYPES = new Set([
    "transcript",
    "summary",
    "repetition",
    "elaboration",
    "plan",
    "quiz",
    "flashcards",
    "pdf",
    "smart_learning",
]);

export const AI_CONTEXT_SOURCE_TYPES = new Set(["pdf", "transcript"]);

export const SOURCE_SCRIPT_TYPES = new Set(["pdf", "transcript", "summary", "repetition", "elaboration"]);
export const MEDIA_TYPES = new Set(["audio", "video", "podcast", "image"]);
export const CREATION_TYPES = new Set([
    "plan",
    "quiz",
    "flashcards",
    "smart_learning",
    "summary",
    "repetition",
    "elaboration",
]);

export function isArchived(f: Pick<FolderFile, "archived">): boolean {
    return Boolean(f.archived);
}

export function isAiContextEligible(f: Pick<FolderFile, "type" | "archived">): boolean {
    return !isArchived(f) && AI_CONTEXT_FILE_TYPES.has(f.type);
}

export function isDefaultAiContextFile(f: Pick<FolderFile, "type" | "archived">): boolean {
    return !isArchived(f) && AI_CONTEXT_SOURCE_TYPES.has(f.type);
}

export function defaultAiContextIds(files: FolderFile[]): string[] {
    return files.filter(isDefaultAiContextFile).map((f) => f.id);
}

export function activeAiContextIds(files: FolderFile[], saved: string[] | null): string[] {
    if (saved === null) return defaultAiContextIds(files);
    const eligible = new Set(files.filter(isAiContextEligible).map((f) => f.id));
    return saved.filter((id) => eligible.has(id));
}

export function typeBadgeLabel(type: string): string {
    const map: Record<string, string> = {
        pdf: "PDF",
        transcript: "TRANSKRIPT",
        audio: "AUDIO",
        video: "VIDEO",
        podcast: "PODCAST",
        image: "BILD",
        summary: "ZUSAMMENFASSUNG",
        elaboration: "AUSARBEITUNG",
        repetition: "WIEDERHOLUNG",
        plan: "LERNPLAN",
        quiz: "QUIZ",
        flashcards: "KARTEN",
        smart_learning: "SMART LEARNING",
    };
    return map[type] || type.toUpperCase();
}

export function typeIconTone(type: string): string {
    if (type === "pdf" || type === "transcript") return "bg-red-500/15 text-red-400";
    if (type === "audio" || type === "podcast") return "bg-blue-500/15 text-blue-300";
    if (type === "video") return "bg-cyan-500/15 text-cyan-300";
    if (type === "flashcards") return "bg-green-500/15 text-green-400";
    if (type === "elaboration" || type === "smart_learning") return "bg-violet-500/15 text-violet-300";
    if (type === "quiz") return "bg-orange-500/15 text-orange-400";
    if (type === "plan") return "bg-blue-500/15 text-blue-400";
    if (type === "summary" || type === "repetition") return "bg-sky-500/15 text-sky-300";
    return "bg-[#3A3D45] text-[#5B9DFF]";
}

export function matchesTypeTab(f: FolderFile, tab: FileTypeTab): boolean {
    const archived = isArchived(f);
    if (tab === "archive") return archived;
    if (archived) return false;
    if (tab === "all") return true;
    if (tab === "sources") return SOURCE_SCRIPT_TYPES.has(f.type) || f.type === "pdf" || f.type === "transcript";
    if (tab === "media") return MEDIA_TYPES.has(f.type);
    if (tab === "creations") return CREATION_TYPES.has(f.type);
    return true;
}

export function countForTab(files: FolderFile[], tab: FileTypeTab): number {
    return files.filter((f) => matchesTypeTab(f, tab)).length;
}

export function sortFiles(files: FolderFile[], mode: FileSortMode): FolderFile[] {
    const copy = [...files];
    copy.sort((a, b) => {
        if (mode === "name") return a.name.localeCompare(b.name, "de", { sensitivity: "base" });
        if (mode === "type") return a.type.localeCompare(b.type) || a.name.localeCompare(b.name, "de");
        const ta = new Date(a.created_at).getTime() || 0;
        const tb = new Date(b.created_at).getTime() || 0;
        return mode === "oldest" ? ta - tb : tb - ta;
    });
    return copy;
}

export type FileGroup = { key: string; label: string; files: FolderFile[] };

function startOfLocalDay(d: Date): Date {
    return new Date(d.getFullYear(), d.getMonth(), d.getDate());
}

export function groupFiles(files: FolderFile[], mode: FileGroupMode): FileGroup[] {
    if (mode === "none") {
        return [{ key: "all", label: "Dateien", files }];
    }
    if (mode === "type") {
        const map = new Map<string, FolderFile[]>();
        for (const f of files) {
            const k = f.type || "other";
            if (!map.has(k)) map.set(k, []);
            map.get(k)!.push(f);
        }
        return [...map.entries()]
            .sort(([a], [b]) => a.localeCompare(b))
            .map(([key, list]) => ({ key, label: typeBadgeLabel(key), files: list }));
    }

    const now = new Date();
    const today = startOfLocalDay(now).getTime();
    const weekAgo = today - 6 * 24 * 60 * 60 * 1000;
    const monthAgo = today - 30 * 24 * 60 * 60 * 1000;
    const buckets: Record<string, FolderFile[]> = {
        week: [],
        month: [],
        older: [],
    };
    for (const f of files) {
        const t = new Date(f.created_at).getTime() || 0;
        if (t >= weekAgo) buckets.week.push(f);
        else if (t >= monthAgo) buckets.month.push(f);
        else buckets.older.push(f);
    }
    return [
        { key: "week", label: "Heute / Diese Woche", files: buckets.week },
        { key: "month", label: "Letzter Monat", files: buckets.month },
        { key: "older", label: "Älter als 30 Tage", files: buckets.older },
    ].filter((g) => g.files.length > 0);
}

export function filterSearch(files: FolderFile[], query: string): FolderFile[] {
    const q = query.trim().toLowerCase();
    if (!q) return files;
    return files.filter((f) => f.name.toLowerCase().includes(q) || f.type.toLowerCase().includes(q));
}

export function formatCompactDate(value?: string): string {
    if (!value) return "—";
    const date = new Date(value);
    if (Number.isNaN(date.getTime())) return value;
    return new Intl.DateTimeFormat("de-DE", {
        day: "2-digit",
        month: "2-digit",
        year: "numeric",
        hour: "2-digit",
        minute: "2-digit",
    }).format(date);
}

export function materialCount(files: FolderFile[]): number {
    return files.filter((f) => !isArchived(f)).length;
}

export function thisWeekEligibleIds(files: FolderFile[]): string[] {
    const today = startOfLocalDay(new Date()).getTime();
    const weekAgo = today - 6 * 24 * 60 * 60 * 1000;
    return files
        .filter(isAiContextEligible)
        .filter((f) => (new Date(f.created_at).getTime() || 0) >= weekAgo)
        .map((f) => f.id);
}
