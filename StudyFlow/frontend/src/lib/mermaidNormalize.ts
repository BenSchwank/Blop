/**
 * Shared Mermaid source cleanup for fences + live render.
 * Critical: never put edge operators alone on a line (breaks layout → tiny/orphan nodes).
 * Also heal AI PAPs: `->` → `-->`, quote labels with ( ) / = * etc.
 */

const EDGE_SPLIT =
    /\s*(-->|---|==>|-\.->|-\.-+)\s*(?:\|([^|]*)\|\s*)?/g;

/** Labels that break Mermaid unless quoted. */
const LABEL_NEEDS_QUOTE = /[()[\]{}<>/=*+\\#,;:]|<br|\s/i;

/** Expand `A --> B --> C` into separate statements (one edge per line). */
export function expandMermaidEdgeChain(statement: string): string[] {
    const src = String(statement || "").trim();
    if (!src) return [];

    const nodes: string[] = [];
    const edges: { op: string; label?: string }[] = [];
    let lastIndex = 0;
    EDGE_SPLIT.lastIndex = 0;
    let m: RegExpExecArray | null;
    while ((m = EDGE_SPLIT.exec(src)) !== null) {
        nodes.push(src.slice(lastIndex, m.index).trim());
        edges.push({ op: m[1], label: m[2] });
        lastIndex = m.index + m[0].length;
    }
    nodes.push(src.slice(lastIndex).trim());

    if (edges.length === 0) return [src];

    const out: string[] = [];
    for (let i = 0; i < edges.length; i++) {
        const left = nodes[i];
        const right = nodes[i + 1];
        if (!left || !right) continue;
        const e = edges[i];
        out.push(e.label ? `${left} ${e.op}|${e.label}| ${right}` : `${left} ${e.op} ${right}`);
    }
    return out.length ? out : [src];
}

function quoteMermaidLabel(label: string): string {
    let L = String(label || "").trim();
    if (!L) return '""';
    // Already quoted
    if ((L.startsWith('"') && L.endsWith('"')) || (L.startsWith("'") && L.endsWith("'"))) {
        // Still scrub <br> inside quotes
        const inner = L.slice(1, -1).replace(/<br\s*\/?>/gi, " · ");
        return `"${inner.replace(/"/g, "'")}"`;
    }
    L = L.replace(/<br\s*\/?>/gi, " · ");
    L = L.replace(/"/g, "'");
    if (!LABEL_NEEDS_QUOTE.test(L)) return L;
    return `"${L}"`;
}

/**
 * Quote unsafe text inside Mermaid node shapes.
 * e.g. CalcA[a = (a + b) / 2] → CalcA["a = (a + b) / 2"]
 */
export function sanitizeMermaidNodeLabels(line: string): string {
    let s = line;

    // ((label)) stadium / circle
    s = s.replace(/(\b[A-Za-z][\w]*)\(\(([\s\S]*?)\)\)/g, (_m, id: string, lab: string) => {
        return `${id}((${quoteMermaidLabel(lab)}))`;
    });

    // [/label/] parallelogram (PAP input/output)
    s = s.replace(/(\b[A-Za-z][\w]*)\[\/([\s\S]*?)\/\]/g, (_m, id: string, lab: string) => {
        return `${id}[/${quoteMermaidLabel(lab)}/]`;
    });

    // [(label)] subroutine / rounded
    s = s.replace(/(\b[A-Za-z][\w]*)\[\(([\s\S]*?)\)\]/g, (_m, id: string, lab: string) => {
        return `${id}[(${quoteMermaidLabel(lab)})]`;
    });

    // {label} decision diamond
    s = s.replace(/(\b[A-Za-z][\w]*)\{([^{}]*?)\}/g, (_m, id: string, lab: string) => {
        return `${id}{${quoteMermaidLabel(lab)}}`;
    });

    // (label) round — not (( already handled; avoid matching (( by negative lookbehind via prior pass
    s = s.replace(/(\b[A-Za-z][\w]*)\((?![\(])([^)]*?)\)/g, (_m, id: string, lab: string) => {
        // Skip if this was already ((...)) residue — shouldn't happen
        return `${id}(${quoteMermaidLabel(lab)})`;
    });

    // [label] rectangle — skip [/ and [( and already-quoted ["
    s = s.replace(/(\b[A-Za-z][\w]*)\[(?![\/("'])([^\]]*?)\]/g, (_m, id: string, lab: string) => {
        return `${id}[${quoteMermaidLabel(lab)}]`;
    });

    // Edge labels |text| with special chars
    s = s.replace(/\|([^|\n]+)\|/g, (_m, lab: string) => {
        const q = quoteMermaidLabel(lab);
        // quoteMermaidLabel may add quotes; edge labels want bare or quoted without extra pipes issues
        const inner = q.startsWith('"') ? q.slice(1, -1) : q;
        return `|${inner}|`;
    });

    return s;
}

/** Close truncated shape brackets that AI often cuts off mid-formula. */
function closeTruncatedShapes(line: string): string {
    let s = line;
    const opens = (s.match(/\[/g) || []).length;
    const closes = (s.match(/\]/g) || []).length;
    if (opens > closes) s += "]".repeat(opens - closes);
    const openP = (s.match(/\(/g) || []).length;
    const closeP = (s.match(/\)/g) || []).length;
    // Only auto-close if clearly a node shape line with imbalance (heuristic)
    if (openP > closeP && /\b[A-Za-z][\w]*[\[{(]/.test(s)) {
        // Prefer not over-closing plain math; only if no ] yet closed for [
        // Skip aggressive paren close — quoting is enough for most cases
    }
    const openB = (s.match(/\{/g) || []).length;
    const closeB = (s.match(/\}/g) || []).length;
    if (openB > closeB) s += "}".repeat(openB - closeB);
    return s;
}

function expandDirectiveLine(line: string): string[] {
    const t = line.trim();
    if (!t) return [];
    const dir = /^(flowchart|graph)\s+(\w+)\s*(.*)$/i.exec(t);
    if (dir) {
        const head = `${dir[1]} ${dir[2]}`;
        const rest = (dir[3] || "").trim();
        if (!rest) return [head];
        return [head, ...expandMermaidEdgeChain(rest)];
    }
    if ((t.match(/-->|---|==>|-\.->/g) || []).length >= 2) {
        return expandMermaidEdgeChain(t);
    }
    return [t];
}

/** Normalize Mermaid body for render / fence repair. */
export function normalizeMermaidSource(raw: string): string {
    let src = String(raw || "").replace(/\r\n/g, "\n").trim();
    if (!src) return src;

    src = src.replace(/→/g, "-->");
    // Single-char arrow from LLMs: "Init -> Calc" → "Init --> Calc" (do not touch --> or --->)
    src = src.replace(/(^|[\s\]})])-+>(?=[\sA-Za-z[(])/gm, "$1-->");
    src = src.replace(/(^|[\s\]})])->(?!>)(?=[\sA-Za-z[(])/gm, "$1-->");

    // Soft-fix odd labeled arrows only: "-- label -->" (whitespace after -- so "-->" untouched)
    src = src.replace(/--\s+([^-→\n|>][^-\n]*?)\s*-{2,}\s*>/g, "-- $1 -->");
    src = src.replace(/--\s+([^-→\n|>][^-\n]*?)\s*-->/g, "-- $1 -->");

    // Global <br> outside shapes still hurts; replace early
    src = src.replace(/<br\s*\/?>/gi, " · ");

    const processLines = (lines: string[]): string[] => {
        const out: string[] = [];
        for (const line of lines) {
            const trimmed = line.trim();
            if (!trimmed) {
                out.push("");
                continue;
            }
            if (/^(style|classDef|class|linkStyle|click|%%)/i.test(trimmed)) {
                out.push(trimmed);
                continue;
            }
            if (/^(subgraph|end)\b/i.test(trimmed)) {
                out.push(trimmed);
                continue;
            }
            if (/^(flowchart|graph)\s/i.test(trimmed) && !/\s+\S+.*-->/.test(trimmed)) {
                out.push(...expandDirectiveLine(trimmed));
                continue;
            }
            const expanded = expandDirectiveLine(trimmed);
            for (let row of expanded) {
                if (/^(flowchart|graph)\s/i.test(row)) {
                    out.push(row);
                    continue;
                }
                row = closeTruncatedShapes(row);
                row = sanitizeMermaidNodeLabels(row);
                out.push(row);
            }
        }
        return out;
    };

    if (!src.includes("\n")) {
        const chunks = src
            .split(/;\s*/)
            .map((p) => p.trim())
            .filter(Boolean);
        return processLines(chunks).join("\n");
    }

    return processLines(src.split("\n")).join("\n").replace(/\n{3,}/g, "\n\n").trim();
}

/** True if fence body looks like a Mermaid flowchart/graph (incl. PAP aliases). */
export function looksLikeMermaidDiagram(body: string, lang?: string): boolean {
    const label = String(lang || "").trim().toLowerCase();
    if (label === "mermaid" || label === "pap" || label === "flowchart" || label === "flow") return true;
    const t = String(body || "").trim();
    return /^(flowchart|graph)\s/i.test(t);
}

/** Mermaid often returns an error SVG instead of throwing. */
export function isMermaidErrorSvg(svg: string): boolean {
    const s = String(svg || "");
    return /Syntax error in text/i.test(s) || (/mermaid version/i.test(s) && /Parse error/i.test(s));
}
