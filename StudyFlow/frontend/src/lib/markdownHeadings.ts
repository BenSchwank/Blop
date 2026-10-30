export type TocHeading = {
    id: string;
    text: string;
    level: number;
};

function stripInlineMd(raw: string): string {
    return raw
        .replace(/`([^`]+)`/g, "$1")
        .replace(/\$\$[\s\S]*?\$\$/g, " ")
        .replace(/\$[^$]+\$/g, " ")
        .replace(/!\[[^\]]*\]\([^)]+\)/g, "")
        .replace(/\[([^\]]+)\]\([^)]+\)/g, "$1")
        .replace(/[*_~]+/g, "")
        .replace(/<[^>]+>/g, "")
        .replace(/\s+/g, " ")
        .trim();
}

export function slugifyHeading(text: string): string {
    const base = stripInlineMd(text)
        .toLowerCase()
        .replace(/ä/g, "ae")
        .replace(/ö/g, "oe")
        .replace(/ü/g, "ue")
        .replace(/ß/g, "ss")
        .normalize("NFKD")
        .replace(/[\u0300-\u036f]/g, "")
        .replace(/[^a-z0-9]+/g, "-")
        .replace(/-+/g, "-")
        .replace(/^-+|-+$/g, "");
    return base || "abschnitt";
}

/** Repair broken markdown hash-links and drop leftover inline TOC blocks. */
export function sanitizeDocumentMarkdown(markdown: string): string {
    let t = String(markdown || "");
    t = t.replace(/\]\(\s*#([^)]*)\)/g, (_m, inner: string) => {
        const slug = String(inner || "").replace(/[\s\r\n]+/g, "");
        return `](#${slug})`;
    });
    t = t.replace(/^#{1,3}\s+inhaltsverzeichnis\s*\n(?:[ \t]*(?:[-*+]|\d+[.)])[ \t]+.*\n?)+/gim, "\n");
    t = t.replace(/\n\s*[-*+]\s*$/g, "");
    return t.trim();
}

export function isTocIgnoredHeading(text: string): boolean {
    return /^inhaltsverzeichnis$/i.test(String(text || "").trim());
}

/** Body-word / heading check used for the incomplete-elaboration banner. */
export function isThinDocument(markdown: string): boolean {
    const t = sanitizeDocumentMarkdown(markdown);
    const headings = extractMarkdownHeadings(t).filter((h) => h.level >= 2 && !isTocIgnoredHeading(h.text));
    const prose = t
        .replace(/^#{1,6}\s+.*$/gm, " ")
        .replace(/\[([^\]]+)\]\([^)]*\)/g, "$1")
        .replace(/[`*_>#\-]+/g, " ")
        .replace(/\$\$[\s\S]*?\$\$/g, " ")
        .replace(/\$[^$]+\$/g, " ");
    const words = prose.split(/\s+/).filter(Boolean);
    if (headings.length === 0) return true;
    return words.length < 150;
}

/** Extract h1–h3 from markdown source for the document TOC. */
export function extractMarkdownHeadings(markdown: string): TocHeading[] {
    const used = new Map<string, number>();
    const out: TocHeading[] = [];
    const lines = String(markdown || "").split(/\r?\n/);
    let inFence = false;
    for (const line of lines) {
        if (/^\s*```/.test(line)) {
            inFence = !inFence;
            continue;
        }
        if (inFence) continue;
        const m = /^(#{1,3})\s+(.+?)\s*$/.exec(line);
        if (!m) continue;
        const text = stripInlineMd(m[2]);
        if (!text) continue;
        let id = slugifyHeading(text);
        const n = (used.get(id) || 0) + 1;
        used.set(id, n);
        if (n > 1) id = `${id}-${n}`;
        out.push({ id, text, level: m[1].length });
    }
    return out;
}

export function scrollToHeadingId(id: string): boolean {
    const raw = (id || "").replace(/^#/, "").trim();
    if (!raw) return false;
    const decoded = decodeURIComponent(raw);
    const el =
        document.getElementById(decoded) ||
        document.getElementById(slugifyHeading(decoded)) ||
        document.getElementById(raw);
    if (!el) return false;
    el.scrollIntoView({ behavior: "smooth", block: "start" });
    return true;
}
