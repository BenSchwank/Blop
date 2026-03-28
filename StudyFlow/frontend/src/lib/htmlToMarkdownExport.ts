import TurndownService from 'turndown';

/**
 * TipTap HTML → Markdown for PDF/print via the same pipeline as read view (ReactMarkdown + KaTeX).
 * Preserves $...$ / $$...$$ that lived in paragraph text.
 */
export function htmlToMarkdownForPdfExport(html: string): string {
    const td = new TurndownService({
        headingStyle: 'atx',
        codeBlockStyle: 'fenced',
        bulletListMarker: '-',
        emDelimiter: '*',
    });

    let md = td.turndown(html || '');
    md = md.replace(/\u00a0/g, ' ');
    md = md.replace(/\n{3,}/g, '\n\n').trim();
    return md;
}
