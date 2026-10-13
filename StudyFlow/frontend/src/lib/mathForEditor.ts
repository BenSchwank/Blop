import katex from "katex";

function escapeAttr(s: string): string {
    return s
        .replace(/&/g, "&amp;")
        .replace(/"/g, "&quot;")
        .replace(/</g, "&lt;")
        .replace(/>/g, "&gt;");
}

function unescapeAttr(s: string): string {
    return s
        .replace(/&quot;/g, '"')
        .replace(/&lt;/g, "<")
        .replace(/&gt;/g, ">")
        .replace(/&amp;/g, "&");
}

function renderKatex(latex: string, display: boolean): string {
    try {
        return katex.renderToString(latex, {
            throwOnError: false,
            strict: false,
            displayMode: display,
        });
    } catch {
        return escapeAttr(latex);
    }
}

function wrapBarePowers(md: string): string {
    let s = md || "";
    // Avoid touching content already inside $...$ or ``` fences by a simple pass:
    // first stash math, then wrap, then restore.
    const slots: string[] = [];
    s = s.replace(/\$\$([\s\S]+?)\$\$/g, (_m, inner: string) => {
        const i = slots.length;
        slots.push(`$$${String(inner).trim()}$$`);
        return `\uE000${i}\uE001`;
    });
    s = s.replace(/\$([^$\n]+?)\$/g, (_m, inner: string) => {
        const i = slots.length;
        slots.push(`$${String(inner).trim()}$`);
        return `\uE000${i}\uE001`;
    });
    s = s.replace(/```[\s\S]*?```/g, (block) => {
        const i = slots.length;
        slots.push(block);
        return `\uE000${i}\uE001`;
    });

    s = s.replace(
        /(?<![\\])([A-Za-z]|\\[A-Za-z]+|[)\]])\s*\^\s*(\{[^{}]+\}|[0-9]+|[A-Za-z]|\\[A-Za-z]+)(?!\w)/g,
        (_m, base: string, exp: string) => {
            const inner = exp.startsWith("{") ? exp.slice(1, -1) : exp;
            return `$${base}^{${inner}}$`;
        }
    );
    s = s.replace(
        /(?<![\\\w])([A-Za-z])([2-9]|1[0-2])(?![0-9A-Za-z_^{])/g,
        (_m, base: string, exp: string) => `$${base}^{${exp}}$`
    );

    s = s.replace(/\uE000(\d+)\uE001/g, (_m, idx: string) => slots[Number(idx)] ?? "");
    return s;
}

function mathToSpans(md: string): { text: string; slots: Array<{ latex: string; display: boolean }> } {
    const slots: Array<{ latex: string; display: boolean }> = [];
    let text = wrapBarePowers(md || "");
    text = text.replace(/\$\$([\s\S]+?)\$\$/g, (_m, inner: string) => {
        const i = slots.length;
        slots.push({ latex: String(inner).trim(), display: true });
        return `%%BLOPMATH${i}%%`;
    });
    text = text.replace(/\$([^$\n]+?)\$/g, (_m, inner: string) => {
        const i = slots.length;
        slots.push({ latex: String(inner).trim(), display: false });
        return `%%BLOPMATH${i}%%`;
    });
    return { text, slots };
}

function restoreSpans(html: string, slots: Array<{ latex: string; display: boolean }>): string {
    let out = html || "";
    slots.forEach((slot, i) => {
        const kind = slot.display ? "display" : "inline";
        const rendered = renderKatex(slot.latex, slot.display);
        const span =
            `<span class="blop-math-${kind}" data-math="${kind}" data-latex="${escapeAttr(slot.latex)}" contenteditable="false">${rendered}</span>`;
        out = out.split(`%%BLOPMATH${i}%%`).join(span);
    });
    return out;
}

/** Markdown (with $math$) → HTML for TipTap, preserving powers as KaTeX spans. */
export function markdownToEditorHtml(md: string, markedParse: (src: string) => string): string {
    const { text, slots } = mathToSpans(md);
    const html = markedParse(text) || "";
    return restoreSpans(html, slots);
}

/** Keep existing HTML; if it still contains $...$, upgrade those to spans. */
export function ensureMathSpansInHtml(html: string): string {
    if (!html) return html;
    if (!html.includes("$") && !html.includes("data-latex")) return html;
    if (/data-latex=/.test(html) && !/\$[^$]+\$/.test(html.replace(/<[^>]+>/g, ""))) {
        return html;
    }
    // Treat as markdown-ish fragment when raw dollars remain.
    if (/\$[^$]+\$|\$\$/.test(html) && !/<span[^>]*data-latex=/.test(html)) {
        return markdownToEditorHtml(html, (s) => s);
    }
    return html;
}

/** TipTap HTML → HTML with $...$ restored from data-latex (stable for round-trips / PDF). */
export function serializeEditorMathSpans(html: string): string {
    if (!html) return html;
    return html
        .replace(
            /<span[^>]*data-math="display"[^>]*data-latex="([^"]*)"[^>]*>[\s\S]*?<\/span>/gi,
            (_m, latex: string) => `$$${unescapeAttr(latex)}$$`
        )
        .replace(
            /<span[^>]*data-math="inline"[^>]*data-latex="([^"]*)"[^>]*>[\s\S]*?<\/span>/gi,
            (_m, latex: string) => `$${unescapeAttr(latex)}$`
        )
        // attribute order may swap
        .replace(
            /<span[^>]*data-latex="([^"]*)"[^>]*data-math="display"[^>]*>[\s\S]*?<\/span>/gi,
            (_m, latex: string) => `$$${unescapeAttr(latex)}$$`
        )
        .replace(
            /<span[^>]*data-latex="([^"]*)"[^>]*data-math="inline"[^>]*>[\s\S]*?<\/span>/gi,
            (_m, latex: string) => `$${unescapeAttr(latex)}$`
        );
}
