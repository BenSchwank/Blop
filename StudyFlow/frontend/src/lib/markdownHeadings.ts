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

function stashSegments(src: string, pattern: RegExp): { text: string; slots: string[] } {
    const slots: string[] = [];
    const text = src.replace(pattern, (block) => {
        const i = slots.length;
        slots.push(block);
        return `\uE000${i}\uE001`;
    });
    return { text, slots };
}

function restoreSlots(text: string, slots: string[]): string {
    return text.replace(/\uE000(\d+)\uE001/g, (_m, idx: string) => slots[Number(idx)] ?? "");
}

function wrapInlineDisplaystyle(inner: string): string {
    const t = inner.trim();
    if (!t || /\\displaystyle/.test(t)) return t;
    if (/\\frac|\\sum|\\oint|\\int\b/.test(t)) return `\\displaystyle ${t}`;
    return t;
}

/** Repair LLM markdown so KaTeX/remark-math can parse it. */
export function sanitizeDocumentMarkdown(markdown: string): string {
    let t = String(markdown || "").replace(/\r\n/g, "\n");
    t = t.replace(/\]\(\s*#([^)]*)\)/g, (_m, inner: string) => {
        const slug = String(inner || "").replace(/[\s\r\n]+/g, "");
        return `](#${slug})`;
    });
    t = t.replace(/^#{1,3}\s+inhaltsverzeichnis\s*\n(?:[ \t]*(?:[-*+]|\d+[.)])[ \t]+.*\n?)+/gim, "\n");
    t = t.replace(/\n\s*[-*+]\s*$/g, "");

    const fences = stashSegments(t, /```[\s\S]*?```/g);
    t = fences.text;

    // Isolate display math (possibly without newlines), then force delimiter line breaks.
    t = t.replace(/\$\$([\s\S]*?)\$\$/g, (_m, inner: string) => {
        const body = String(inner).trim();
        return `\n$$\n${body}\n$$\n`;
    });
    t = t.replace(/([^\n])\$\$/g, "$1\n$$");
    t = t.replace(/\$\$([^\n])/g, "$$\n$1");

    const display = stashSegments(t, /\$\$[\s\S]*?\$\$/g);
    t = display.text;

    t = t.replace(/\$([^$\n]+?)\$/g, (_m, inner: string) => `$${wrapInlineDisplaystyle(inner)}$`);
    const inline = stashSegments(t, /\$[^$\n]+\$/g);
    t = inline.text;

    t = t
        .split("\n")
        .map((line) => {
            if (/^\s*\|/.test(line) || /^\s*-{3,}\s*$/.test(line)) return line;
            let s = line.replace(/([^\n#])(#{2,4}\s)/g, "$1\n\n$2");
            s = s.replace(/(\S)(---)/g, "$1\n\n$2");
            return s;
        })
        .join("\n");
    t = t.replace(/\n{3,}/g, "\n\n");

    t = restoreSlots(t, inline.slots);
    t = restoreSlots(t, display.slots);
    t = restoreSlots(t, fences.slots);
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

export function stripLeadingTaskEmoji(text: string): string {
    return String(text || "").replace(/^[✅☑️✔️\s]+/u, "").trim();
}

export function splitTaskTime(text: string): { body: string; minutes: number | null } {
    const raw = stripLeadingTaskEmoji(text);
    const m = raw.match(/^(.*?)(?:\s*[\(（]\s*(?:ca\.?\s*)?(\d+)\s*(?:min(?:uten)?|m)\s*[\)）])\s*$/i);
    if (!m) return { body: raw, minutes: null };
    return { body: m[1].trim(), minutes: Number(m[2]) };
}
