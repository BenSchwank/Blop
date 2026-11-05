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
        .replace(/(\d+)\\\./g, "$1.")
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

/** Isolated ASCII placeholders — never share indices across fence/math layers. */
function stashWithPrefix(
    src: string,
    pattern: RegExp,
    prefix: "BLOPFENCE" | "BLOPDISP" | "BLOPINL"
): { text: string; slots: string[] } {
    const slots: string[] = [];
    const text = src.replace(pattern, (block) => {
        const i = slots.length;
        slots.push(block);
        return `%%${prefix}${i}%%`;
    });
    return { text, slots };
}

function restoreWithPrefix(
    text: string,
    slots: string[],
    prefix: "BLOPFENCE" | "BLOPDISP" | "BLOPINL"
): string {
    const re = new RegExp(`%%${prefix}(\\d+)%%`, "g");
    return text.replace(re, (_m, idx: string) => slots[Number(idx)] ?? "");
}

function wrapInlineDisplaystyle(inner: string): string {
    const t = inner.trim();
    if (!t || /\\displaystyle/.test(t)) return t;
    if (/\\frac|\\sum|\\oint|\\int\b/.test(t)) return `\\displaystyle ${t}`;
    return t;
}

/**
 * Heal docs where a prior slot-collision replaced ```lang fences with KaTeX $A^{10}$ / A¹⁰.
 * Only rewrite clear fence-open/close markers — leave real math alone.
 */
function repairCorruptedCodeFences(src: string): string {
    let t = src;
    // Opening fence: $A^{10}$mermaid / A¹⁰c / A10python
    t = t.replace(/(?:\$A\^\{10\}\$|A[¹1][⁰0]|\$A\^\{10\})(\w+)/g, "\n```$1\n");
    // Closing fence on its own (after a language-open repair left body + trailing marker)
    t = t.replace(/(^|\n)(?:\$A\^\{10\}\$|A[¹1][⁰0])(?=\s*(?:\n|$))/g, "$1\n```\n");
    return t;
}

/** Undo prose damage from repairStrippedLatexCommands (anschließ\end → anschließend). */
function repairStrippedCommandInProse(src: string): string {
    // Letter + \end + letter (German compound words corrupted by \b(end)\b injection)
    return src.replace(/(?<=\p{L})\\+end(?=\p{L})/gu, "end");
}

function repairEscapedHeadingDots(src: string): string {
    return src
        .split("\n")
        .map((line) => {
            if (/^\s*#{1,6}\s/.test(line)) {
                return line.replace(/(\d+)\\\.\s*/g, "$1. ");
            }
            return line;
        })
        .join("\n");
}

/** Wrap bare cases environments in $$ … $$ and clean \\\, row breaks. */
function repairBareCasesBlocks(src: string): string {
    return src.replace(/(\\begin\{cases\}[\s\S]*?\\end\{cases\})/g, (block, _full, offset: number) => {
        const before = src.slice(Math.max(0, offset - 4), offset);
        const after = src.slice(offset + block.length, offset + block.length + 4);
        const alreadyWrapped = /\$\$\s*$/.test(before) || /^\s*\$\$/.test(after);
        let body = block.replace(/\\\\\\,/g, "\\\\ ").replace(/\\{2},/g, "\\\\ ");
        if (alreadyWrapped) return body;
        return `\n$$\n${body}\n$$\n`;
    });
}

function normalizeMermaidFenceBody(body: string): string {
    let b = body.trim();
    b = b.replace(/→/g, "-->");
    b = b.replace(/--\s*([^-→\n]+?)\s*-+\s*-->/g, "-- $1 -->");
    b = b.replace(/--\s*([^-→\n]+?)\s*-->/g, "-- $1 -->");
    if (!b.includes("\n")) {
        b = b
            .replace(/;\s*/g, ";\n")
            .replace(/\s*(-->|---|-.->|==>|==)\s*/g, "\n$1 ")
            .trim();
    }
    return b;
}

/** Tag flowchart/graph fences as mermaid and fix Unicode arrows. */
function repairMermaidFences(src: string): string {
    return src.replace(/```([^\n`]*)\n([\s\S]*?)```/g, (_m, lang: string, body: string) => {
        const trimmed = String(body || "").trim();
        const looksMermaid = /^(flowchart|graph)\s/i.test(trimmed);
        let label = String(lang || "").trim();
        if (looksMermaid && (!label || label.toLowerCase() === "text" || label.toLowerCase() === "plain")) {
            label = "mermaid";
        }
        if (label.toLowerCase() === "mermaid" || looksMermaid) {
            return `\`\`\`mermaid\n${normalizeMermaidFenceBody(trimmed)}\n\`\`\``;
        }
        return `\`\`\`${lang}\n${body}\`\`\``;
    });
}

/** Repair LLM markdown so KaTeX/remark-math can parse it. */
export function sanitizeDocumentMarkdown(markdown: string): string {
    let t = String(markdown || "").replace(/\r\n/g, "\n");
    t = repairCorruptedCodeFences(t);
    t = repairStrippedCommandInProse(t);
    t = repairEscapedHeadingDots(t);
    t = repairBareCasesBlocks(t);
    t = repairMermaidFences(t);
    t = t.replace(/\]\(\s*#([^)]*)\)/g, (_m, inner: string) => {
        const slug = String(inner || "").replace(/[\s\r\n]+/g, "");
        return `](#${slug})`;
    });
    t = t.replace(/^#{1,3}\s+inhaltsverzeichnis\s*\n(?:[ \t]*(?:[-*+]|\d+[.)])[ \t]+.*\n?)+/gim, "\n");
    t = t.replace(/\n\s*[-*+]\s*$/g, "");

    // Fences first — restored last, byte-identical, never touched by math/heading passes.
    const fences = stashWithPrefix(t, /```[\s\S]*?```/g, "BLOPFENCE");
    t = fences.text;

    // Isolate display math (possibly without newlines), then force delimiter line breaks.
    t = t.replace(/\$\$([\s\S]*?)\$\$/g, (_m, inner: string) => {
        const body = String(inner).trim();
        return `\n$$\n${body}\n$$\n`;
    });
    t = t.replace(/([^\n])\$\$/g, "$1\n$$");
    t = t.replace(/\$\$([^\n])/g, "$$\n$1");

    const display = stashWithPrefix(t, /\$\$[\s\S]*?\$\$/g, "BLOPDISP");
    t = display.text;

    t = t.replace(/\$([^$\n]+?)\$/g, (_m, inner: string) => `$${wrapInlineDisplaystyle(inner)}$`);
    const inline = stashWithPrefix(t, /\$[^$\n]+\$/g, "BLOPINL");
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

    t = restoreWithPrefix(t, inline.slots, "BLOPINL");
    t = restoreWithPrefix(t, display.slots, "BLOPDISP");
    t = restoreWithPrefix(t, fences.slots, "BLOPFENCE");
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
