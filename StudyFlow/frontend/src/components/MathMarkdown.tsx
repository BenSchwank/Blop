"use client";

import React from "react";
import ReactMarkdown from "react-markdown";
import remarkGfm from "remark-gfm";
import remarkMath from "remark-math";
import rehypeKatex from "rehype-katex";
import "katex/dist/katex.min.css";
import { sanitizeDocumentMarkdown } from "@/lib/markdownHeadings";

type Props = {
    children: string;
    className?: string;
    /** Compact inline-ish prose (option labels, short prompts). */
    compact?: boolean;
};

/**
 * Shared Markdown + KaTeX renderer for StudyFlow learning surfaces.
 * Same stack as folder read mode / FloatingChat so $...$ powers render correctly.
 */
export default function MathMarkdown({ children, className = "", compact = false }: Props) {
    const text = sanitizeDocumentMarkdown(typeof children === "string" ? children : String(children ?? ""));
    if (!text.trim()) return null;

    const prose = compact
        ? "prose prose-invert prose-blop max-w-none text-sm leading-relaxed prose-p:my-0 prose-p:inline prose-headings:my-1 [&_.katex]:text-[1em] [&_.katex-display]:my-2 [&_.katex-display]:overflow-x-auto"
        : "prose prose-invert prose-blop max-w-none text-base leading-relaxed prose-p:my-2 prose-ul:my-2 prose-ol:my-2 prose-li:my-0.5 prose-headings:text-white [&_.katex-display]:my-3 [&_.katex-display]:overflow-x-auto";

    return (
        <div className={`${prose} ${className}`.trim()}>
            <ReactMarkdown
                remarkPlugins={[remarkGfm, remarkMath]}
                rehypePlugins={[[rehypeKatex, { throwOnError: false, strict: false }]]}
            >
                {text}
            </ReactMarkdown>
        </div>
    );
}

/** Normalize for tolerant answer compare (case, quotes, light punctuation). */
export function normalizeAnswerText(s: string): string {
    return String(s || "")
        .trim()
        .toLowerCase()
        .normalize("NFD")
        .replace(/[\u0300-\u036f]/g, "")
        .replace(/ß/g, "ss")
        .replace(/[„“"'`´]/g, "")
        .replace(/[.,;:!?()[\]{}]/g, " ")
        .replace(/\s+/g, " ")
        .trim();
}

/** Same as normalize, but spaces/hyphens removed (ganze Zahlen ≈ ganzezahlen). */
export function compactAnswerText(s: string): string {
    return normalizeAnswerText(s).replace(/[\s\-_/+]+/g, "");
}

/** Strip common German plural endings for light singular/plural tolerance. */
function stemPluralDe(s: string): string {
    const n = normalizeAnswerText(s);
    if (n.length < 4) return n;
    if (n.endsWith("en") && n.length > 4) return n.slice(0, -2);
    if (n.endsWith("er") && n.length > 4) return n.slice(0, -2);
    if (n.endsWith("e") && n.length > 3) return n.slice(0, -1);
    if (n.endsWith("n") && n.length > 3) return n.slice(0, -1);
    if (n.endsWith("s") && n.length > 3) return n.slice(0, -1);
    return n;
}

function editDistance(a: string, b: string): number {
    if (a === b) return 0;
    const m = a.length;
    const n = b.length;
    if (m === 0) return n;
    if (n === 0) return m;
    // Cap cost for very long strings — compare only if lengths are close
    if (Math.abs(m - n) > 4) return Math.abs(m - n);
    const prev = new Array<number>(n + 1);
    const cur = new Array<number>(n + 1);
    for (let j = 0; j <= n; j++) prev[j] = j;
    for (let i = 1; i <= m; i++) {
        cur[0] = i;
        const ca = a.charCodeAt(i - 1);
        for (let j = 1; j <= n; j++) {
            const cost = ca === b.charCodeAt(j - 1) ? 0 : 1;
            cur[j] = Math.min(cur[j - 1] + 1, prev[j] + 1, prev[j - 1] + cost);
        }
        for (let j = 0; j <= n; j++) prev[j] = cur[j];
    }
    return prev[n];
}

function fuzzyAllowance(len: number): number {
    if (len <= 3) return 0;
    if (len <= 5) return 1;
    if (len <= 10) return 2;
    return Math.min(3, Math.floor(len / 5));
}

/** Core tolerant equality for one expected candidate vs given. */
export function answersFuzzyEqual(expected: string, given: string): boolean {
    const e = normalizeAnswerText(expected);
    const g = normalizeAnswerText(given);
    if (!e || !g) return false;
    if (e === g) return true;
    if (stemPluralDe(e) === stemPluralDe(g)) return true;

    const ec = compactAnswerText(expected);
    const gc = compactAnswerText(given);
    if (!ec || !gc) return false;
    if (ec === gc) return true;

    const eStem = stemPluralDe(ec);
    const gStem = stemPluralDe(gc);
    if (eStem === gStem) return true;

    // Containment for longer phrases (order-independent keyword-ish), min length 4
    if (ec.length >= 4 && gc.length >= 4) {
        if (ec.includes(gc) || gc.includes(ec)) {
            const ratio = Math.min(ec.length, gc.length) / Math.max(ec.length, gc.length);
            if (ratio >= 0.7) return true;
        }
        if (eStem.length >= 4 && gStem.length >= 4 && (eStem.includes(gStem) || gStem.includes(eStem))) {
            const ratio = Math.min(eStem.length, gStem.length) / Math.max(eStem.length, gStem.length);
            if (ratio >= 0.7) return true;
        }
    }

    const maxLen = Math.max(ec.length, gc.length);
    const minLen = Math.min(ec.length, gc.length);
    if (minLen < 3) return false;
    if (maxLen - minLen > fuzzyAllowance(maxLen) + 1) return false;
    const dist = editDistance(ec, gc);
    if (dist <= fuzzyAllowance(maxLen)) return true;
    // Also try stemmed compact forms
    if (eStem.length >= 4 && gStem.length >= 4) {
        const stemMax = Math.max(eStem.length, gStem.length);
        if (editDistance(eStem, gStem) <= fuzzyAllowance(stemMax)) return true;
    }
    return false;
}

/** Split pipe-alternatives inside one blank: `Pointer|Zeiger`. */
export function splitAnswerAlternatives(raw: string): string[] {
    return String(raw || "")
        .split("|")
        .map((x) => x.trim())
        .filter(Boolean);
}

/**
 * Tolerant compare: case, spaces, light typos, pipe-alternatives, optional accepted list,
 * DE plural stemming. E.g. "ganzzahlen" / "ganzezahlen" ≈ "ganze Zahlen".
 */
export function answerAccepted(
    expected: string,
    given: string,
    alternatives?: string[] | null
): boolean {
    const g = normalizeAnswerText(given);
    if (!g) return false;
    const pool = [
        ...splitAnswerAlternatives(expected),
        ...(alternatives || []).flatMap((a) => splitAnswerAlternatives(a)),
    ].filter(Boolean);
    if (pool.length === 0) return false;
    return pool.some((cand) => answersFuzzyEqual(cand, given));
}

/** Case/whitespace/typo-tolerant blank compare (also accepts `a|b` alternatives in expected). */
export function answersMatch(expected: string, given: string): boolean {
    return answerAccepted(expected, given);
}

/** True if every required keyword (or alt) appears somewhere in given (order-independent). */
export function keywordsAccepted(keywords: string[], given: string, extras?: string[] | null): boolean {
    const g = normalizeAnswerText(given);
    const gc = compactAnswerText(given);
    if (!g || keywords.length === 0) return false;
    const bag = keywords.map((k) => splitAnswerAlternatives(k)).filter((alts) => alts.length > 0);
    const extraPool = (extras || []).flatMap((a) => splitAnswerAlternatives(a));
    return bag.every((alts) => {
        const combined = [...alts, ...extraPool];
        return combined.some((a) => {
            const e = normalizeAnswerText(a);
            if (!e) return false;
            if (g.includes(e) || gc.includes(compactAnswerText(a))) return true;
            if (answersFuzzyEqual(a, given)) return true;
            const stem = stemPluralDe(e);
            return stem.length >= 3 && (g.includes(stem) || stemPluralDe(g).includes(stem));
        });
    });
}

/** Similarity 0..1 between two answer strings (compact + edit distance). */
export function answerSimilarity(expected: string, given: string): number {
    const e = compactAnswerText(expected);
    const g = compactAnswerText(given);
    if (!e || !g) return 0;
    if (e === g || stemPluralDe(e) === stemPluralDe(g)) return 1;
    if (answersFuzzyEqual(expected, given)) return 0.92;
    const maxLen = Math.max(e.length, g.length);
    if (maxLen === 0) return 0;
    const dist = editDistance(e, g);
    const editSim = Math.max(0, 1 - dist / maxLen);
    let contain = 0;
    if (e.length >= 3 && g.length >= 3) {
        if (e.includes(g) || g.includes(e)) {
            contain = Math.min(e.length, g.length) / maxLen;
        }
    }
    // Token overlap (space-normalized)
    const et = new Set(normalizeAnswerText(expected).split(" ").filter((t) => t.length >= 2));
    const gt = new Set(normalizeAnswerText(given).split(" ").filter((t) => t.length >= 2));
    let token = 0;
    if (et.size && gt.size) {
        let hit = 0;
        for (const t of et) if (gt.has(t) || [...gt].some((x) => answersFuzzyEqual(t, x))) hit += 1;
        token = hit / et.size;
    }
    return Math.max(editSim, contain, token * 0.85);
}

export function maxAnswerSimilarity(
    expected: string,
    given: string,
    alternatives?: string[] | null
): number {
    const pool = [
        ...splitAnswerAlternatives(expected),
        ...(alternatives || []).flatMap((a) => splitAnswerAlternatives(a)),
    ].filter(Boolean);
    if (!pool.length) return 0;
    return Math.max(...pool.map((p) => answerSimilarity(p, given)));
}

export type AnswerGradeVerdict = "correct" | "wrong" | "uncertain";

/**
 * Local-first grading gate for free-text:
 * - correct: fuzzy/keyword match
 * - wrong: empty, or clearly dissimilar short answer
 * - uncertain: local miss but plausible paraphrase / partial keywords / mid similarity → ask AI
 */
export function classifyShortAnswerVerdict(
    expected: string,
    given: string,
    alternatives?: string[] | null,
    keywords?: string[] | null
): AnswerGradeVerdict {
    const raw = String(given || "").trim();
    if (!raw) return "wrong";
    if (answerAccepted(expected, given, alternatives)) return "correct";
    if (keywords?.length && keywordsAccepted(keywords, given, alternatives)) return "correct";

    const sim = maxAnswerSimilarity(expected, given, alternatives);
    const kw = (keywords || []).map((k) => splitAnswerAlternatives(k)).filter((a) => a.length);
    let kwHits = 0;
    if (kw.length) {
        for (const alts of kw) {
            if (alts.some((a) => answerSimilarity(a, given) >= 0.55 || normalizeAnswerText(given).includes(normalizeAnswerText(a)))) {
                kwHits += 1;
            }
        }
    }
    const compactLen = compactAnswerText(given).length;

    // Partial keywords → likely related wording
    if (kw.length >= 2 && kwHits > 0 && kwHits < kw.length) return "uncertain";
    if (kw.length === 1 && kwHits === 1 && sim < 0.92) return "uncertain";

    // Mid similarity band
    if (sim >= 0.22 && sim < 0.92) return "uncertain";

    // Longer free-text often paraphrases the model answer with low string overlap
    if (compactLen >= 12) return "uncertain";

    // Very short and far away → clearly wrong, skip AI
    return "wrong";
}

/**
 * Split a cloze prompt with `{{answer}}` markers into text segments and blank answers.
 * If `blanks` is provided, those override the text inside the braces for grading.
 */
export function parseClozePrompt(
    prompt: string,
    blanks?: string[]
): { segments: string[]; answers: string[] } {
    const segments: string[] = [];
    const answers: string[] = [];
    const re = /\{\{([^}]*)\}\}/g;
    let last = 0;
    let m: RegExpExecArray | null;
    let bi = 0;
    while ((m = re.exec(prompt)) !== null) {
        segments.push(prompt.slice(last, m.index));
        const fromBrace = String(m[1] || "").trim();
        const fromList = blanks && blanks[bi] != null ? String(blanks[bi]).trim() : "";
        answers.push(fromList || fromBrace);
        bi += 1;
        last = m.index + m[0].length;
    }
    segments.push(prompt.slice(last));
    if (answers.length === 0 && blanks && blanks.length > 0) {
        return { segments: [prompt, ""], answers: blanks.map((b) => String(b).trim()).filter(Boolean) };
    }
    return { segments, answers };
}
