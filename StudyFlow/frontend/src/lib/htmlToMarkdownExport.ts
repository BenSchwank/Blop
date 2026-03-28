import TurndownService from 'turndown';

function textLooksLikeLatex(text: string): boolean {
    const t = text.trim();
    if (!t) return false;
    if (/\$[^$]+\$/.test(t) || t.includes('$$')) return true;
    if (/\\[a-zA-Z]+/.test(t)) return true;
    if (/\\[,;%#&_{}^]/.test(t)) return true;
    if (/\\frac|\\binom|\\sqrt|\\sum|\\int|\\cdot|\\times|\\leq|\\geq|\\neq|\\approx/.test(t)) return true;
    return false;
}

/**
 * TipTap HTML → Markdown for PDF/print via the same pipeline as read view (ReactMarkdown + KaTeX).
 * Custom rules run after defaults (Turndown matches last-added rules first) so LaTeX in <code> is not
 * wrapped in backticks — that would break KaTeX and strip backslashes.
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
            if (text.includes('\n')) {
                return `\n\n$$${text}$$\n\n`;
            }
            return `\n\n$${text}$\n\n`;
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
            return node.textContent || '';
        },
    });

    let md = td.turndown(html || '');
    md = md.replace(/\u00a0/g, ' ');
    md = md.replace(/\n{3,}/g, '\n\n').trim();
    return md;
}
