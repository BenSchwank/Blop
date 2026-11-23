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
        .replace(/[„“"'`´]/g, "")
        .replace(/[.,;:!?()[\]{}]/g, " ")
        .replace(/\s+/g, " ")
        .trim();
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

/** Split pipe-alternatives inside one blank: `Pointer|Zeiger`. */
export function splitAnswerAlternatives(raw: string): string[] {
    return String(raw || "")
        .split("|")
        .map((x) => x.trim())
        .filter(Boolean);
}

/**
 * Tolerant compare: exact-normalized match, pipe-alternatives, optional accepted list,
 * light DE plural stemming.
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
    const gStem = stemPluralDe(g);
    for (const cand of pool) {
        const e = normalizeAnswerText(cand);
        if (!e) continue;
        if (e === g || stemPluralDe(e) === gStem) return true;
    }
    return false;
}

/** Case/whitespace-tolerant blank compare (also accepts `a|b` alternatives in expected). */
export function answersMatch(expected: string, given: string): boolean {
    return answerAccepted(expected, given);
}

/** True if every required keyword (or alt) appears somewhere in given (order-independent). */
export function keywordsAccepted(keywords: string[], given: string, extras?: string[] | null): boolean {
    const g = normalizeAnswerText(given);
    if (!g || keywords.length === 0) return false;
    const bag = keywords.map((k) => splitAnswerAlternatives(k)).filter((alts) => alts.length > 0);
    const extraPool = (extras || []).flatMap((a) => splitAnswerAlternatives(a));
    return bag.every((alts) => {
        const combined = [...alts, ...extraPool];
        return combined.some((a) => {
            const e = normalizeAnswerText(a);
            if (!e) return false;
            if (g.includes(e)) return true;
            const stem = stemPluralDe(e);
            return stem.length >= 3 && (g.includes(stem) || stemPluralDe(g).includes(stem));
        });
    });
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
