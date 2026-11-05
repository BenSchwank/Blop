import TurndownService from 'turndown';

/** Inside $$...$$ only: \\frac → \frac (typisch nach HTML/JSON-Doppelung), ohne Zeilen-\\ in Matrizen zu zerstören. */
function normalizeDoubleBackslashBeforeLettersInDisplayMath(body: string): string {
    return body.replace(/\\{2}(?=[A-Za-z])/g, '\\');
}

function mapDisplayMathBlocks(md: string, fn: (inner: string) => string): string {
    return md.replace(/\$\$([\s\S]*?)\$\$/g, (_full, inner: string) => `$$${fn(inner)}$$`);
}

function textLooksLikeLatex(text: string): boolean {
    const t = text.trim();
    if (!t) return false;
    if (/\$[^$]+\$/.test(t) || t.includes('$$')) return true;
    if (/\\[a-zA-Z]+/.test(t)) return true;
    if (/\\[,;%#&_{}^]/.test(t)) return true;
    if (/\\frac|\\binom|\\sqrt|\\sum|\\int|\\cdot|\\times|\\leq|\\geq|\\neq|\\approx/.test(t)) return true;
    if (/\bfrac\s*\{|\bsqrt\s*\{|\bbinom\b|\bbegin\s*\{/i.test(t)) return true;
    if (/[_^]\s*\{/.test(t) && /[a-zA-Z]/.test(t)) return true;
    return false;
}

/** Wrap editor/inline LaTeX so remark-math + rehype-katex see it (plain \frac in a paragraph is ignored). */
function wrapForInlineMath(tex: string): string {
    let t = tex.trim();
    if (!t) return '';
    if ((t.startsWith('$') && t.endsWith('$') && t.length > 1) || (t.startsWith('$$') && t.endsWith('$$'))) {
        return t;
    }
    t = t.replace(/(?<!\\)\$/g, '\\$');
    if (t.includes('\n')) {
        const inner = normalizeDoubleBackslashBeforeLettersInDisplayMath(t);
        return `\n\n$$${inner}$$\n\n`;
    }
    return `$${t}$`;
}

/** Light normalize only inside $$…$$ — never inject \ before bare words in prose. */
function normalizeMarkdownForKatexExport(md: string): string {
    return mapDisplayMathBlocks(md, (inner) => normalizeDoubleBackslashBeforeLettersInDisplayMath(inner));
}

/**
 * TipTap HTML → Markdown for legacy HTML docs / PDF from TipTap.
 * Does NOT run repairStrippedLatexCommands (that destroyed German prose like anschließ\end).
 */
export function htmlToMarkdownForPdfExport(html: string): string {
    const td = new TurndownService({
        headingStyle: 'atx',
        codeBlockStyle: 'fenced',
        bulletListMarker: '-',
        emDelimiter: '*',
    });

    td.addRule('preCodeLatex', {
        filter(node) {
            if (node.nodeName !== 'PRE') return false;
            const text = (node.textContent || '').trim();
            return textLooksLikeLatex(text);
        },
        replacement(_content, node) {
            const text = (node.textContent || '').trim();
            return wrapForInlineMath(text);
        },
    });

    td.addRule('inlineCodeLatex', {
        filter(node) {
            if (node.nodeName !== 'CODE') return false;
            const parent = node.parentNode as HTMLElement | null;
            if (parent?.nodeName === 'PRE') return false;
            const text = node.textContent || '';
            return textLooksLikeLatex(text);
        },
        replacement(_content, node) {
            return wrapForInlineMath(node.textContent || '');
        },
    });

    let md = td.turndown(html || '');
    md = md.replace(/\u00a0/g, ' ');
    md = md.replace(/\n{3,}/g, '\n\n').trim();
    md = normalizeMarkdownForKatexExport(md);
    return md;
}
