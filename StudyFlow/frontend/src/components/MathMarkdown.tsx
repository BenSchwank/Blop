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

/** Case/whitespace-tolerant blank compare for cloze answers. */
export function answersMatch(expected: string, given: string): boolean {
    const norm = (s: string) =>
        s
            .trim()
            .toLowerCase()
            .replace(/\s+/g, " ")
            .replace(/[„“"']/g, "");
    return norm(expected) === norm(given);
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
