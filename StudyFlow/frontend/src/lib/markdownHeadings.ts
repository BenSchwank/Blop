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
        .normalize("NFKD")
        .replace(/[\u0300-\u036f]/g, "")
        .replace(/[^a-z0-9äöüß]+/gi, "-")
        .replace(/^-+|-+$/g, "");
    return base || "abschnitt";
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
