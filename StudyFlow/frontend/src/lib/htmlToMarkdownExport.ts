import TurndownService from 'turndown';

/** Split markdown into alternating outside / inside ```fenced``` segments. */
function mapOutsideCodeFences(md: string, fn: (s: string) => string): string {
    const parts = md.split(/(```[\s\S]*?```)/g);
    return parts.map((p, i) => (i % 2 === 1 ? p : fn(p))).join('');
}

/** Transform text outside $...$ and $$...$$ (inline math delimiters). */
function mapOutsideInlineMath(md: string, fn: (s: string) => string): string {
    const out: string[] = [];
    let pos = 0;
    while (pos < md.length) {
        const d = md.indexOf('$', pos);
        if (d === -1) {
            out.push(fn(md.slice(pos)));
            break;
        }
        out.push(fn(md.slice(pos, d)));
        if (md[d + 1] === '$') {
            const close = md.indexOf('$$', d + 2);
            if (close === -1) {
                out.push(md.slice(d));
                break;
            }
            out.push(md.slice(d, close + 2));
            pos = close + 2;
        } else {
            const close = md.indexOf('$', d + 1);
            if (close === -1) {
                out.push(md.slice(d));
                break;
            }
            out.push(md.slice(d, close + 1));
            pos = close + 1;
        }
    }
    return out.join('');
}

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
    // Nach TipTap/marked oft ohne führendes \:
    if (/\bfrac\s*\{|\bcdot\b|\bsqrt\s*\{|\bbinom\b|\bimplies\b|\bbegin\s*\{|\bend\s*\{|\bint\s*[_^]|\bint\s*\(/i.test(t)) return true;
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

/**
 * Stellt fehlende \ vor typischen LaTeX-Befehlen wieder her (häufig nach HTML-Serialisierung / Turndown).
 * Läuft nur außerhalb von Code-Fences und Inline-$...$.
 */
function repairStrippedLatexCommands(text: string): string {
    let s = text;
    const cmds = [
        'frac',
        'dfrac',
        'tfrac',
        'sqrt',
        'cdot',
        'times',
        'binom',
        'implies',
        'leq',
        'geq',
        'neq',
        'approx',
        'equiv',
        'sum',
        'prod',
        'lim',
        'infty',
        'partial',
        'nabla',
        'ell',
        'left',
        'right',
        'bigl',
        'bigr',
        'Bigl',
        'Bigr',
        'begin',
        'end',
        'pmatrix',
        'bmatrix',
        'vmatrix',
        'matrix',
        'cases',
        'align',
        'mathrm',
        'mathbf',
        'mathit',
        'text',
        'operatorname',
        'vec',
        'hat',
        'bar',
        'tilde',
        'dot',
        'ddot',
        'overline',
        'underline',
        'cdots',
        'ldots',
        'dots',
        'vdots',
        'ddots',
        'forall',
        'exists',
        'neg',
        'wedge',
        'vee',
        'cap',
        'cup',
        'subset',
        'subseteq',
        'Rightarrow',
        'Leftrightarrow',
        'rightarrow',
        'leftarrow',
        'mapsto',
        'alpha',
        'beta',
        'gamma',
        'delta',
        'epsilon',
        'theta',
        'lambda',
        'mu',
        'sigma',
        'omega',
        'pi',
        'tau',
        'phi',
        'psi',
        'rho',
        'kappa',
        'nu',
        'xi',
        'zeta',
        'eta',
        'Gamma',
        'Delta',
        'Theta',
        'Lambda',
        'Sigma',
        'Omega',
        'Phi',
        'Psi',
    ];
    for (const cmd of cmds) {
        const re = new RegExp(`(?<![\\\\$])\\b(${cmd})\\b`, 'g');
        s = s.replace(re, '\\$1');
    }
    s = s.replace(/(?<!\\)\bint(?=\s*[_^]|\s*\()/gi, '\\int');
    return s;
}

function normalizeMarkdownForKatexExport(md: string): string {
    let out = mapOutsideCodeFences(md, (seg) => mapOutsideInlineMath(seg, repairStrippedLatexCommands));
    out = mapDisplayMathBlocks(out, (inner) =>
        normalizeDoubleBackslashBeforeLettersInDisplayMath(repairStrippedLatexCommands(inner)),
    );
    return out;
}

/**
 * TipTap HTML → Markdown for PDF/print via the same pipeline as read view (ReactMarkdown + KaTeX).
 * Custom rules run after defaults (Turndown matches last-added rules first) so LaTeX in <code> is not
 * wrapped in Markdown backticks — that would break KaTeX and strip backslashes.
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
