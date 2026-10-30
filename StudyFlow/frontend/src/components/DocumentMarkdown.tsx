"use client";

import React, { useMemo } from "react";
import ReactMarkdown from "react-markdown";
import remarkGfm from "remark-gfm";
import remarkMath from "remark-math";
import rehypeKatex from "rehype-katex";
import "katex/dist/katex.min.css";
import {
    extractMarkdownHeadings,
    sanitizeDocumentMarkdown,
    scrollToHeadingId,
    slugifyHeading,
    type TocHeading,
} from "@/lib/markdownHeadings";

type Props = {
    children: string;
    className?: string;
};

function plainChildren(node: React.ReactNode): string {
    if (node == null || typeof node === "boolean") return "";
    if (typeof node === "string" || typeof node === "number") return String(node);
    if (Array.isArray(node)) return node.map(plainChildren).join("");
    if (React.isValidElement(node)) {
        const kids = (node.props as { children?: React.ReactNode }).children;
        return plainChildren(kids);
    }
    return "";
}

const headingClass: Record<"h1" | "h2" | "h3", string> = {
    h1: "scroll-mt-20 text-3xl font-semibold text-white tracking-tight",
    h2: "scroll-mt-20 mt-[2.2em] mb-[0.8em] pb-[0.3em] border-b border-white/[0.08] text-xl font-semibold text-white",
    h3: "scroll-mt-20 mt-8 mb-3 text-lg font-medium text-slate-100",
};

export default function DocumentMarkdown({ children, className = "" }: Props) {
    const text = sanitizeDocumentMarkdown(typeof children === "string" ? children : String(children ?? ""));
    const headings = useMemo(() => extractMarkdownHeadings(text), [text]);
    const cursor = { i: 0 };

    const heading = (Tag: "h1" | "h2" | "h3") =>
        function Heading({ children: kids }: { children?: React.ReactNode }) {
            const fallback = slugifyHeading(plainChildren(kids));
            const id = headings[cursor.i]?.id || fallback;
            cursor.i += 1;
            return (
                <Tag id={id} className={headingClass[Tag]}>
                    {kids}
                </Tag>
            );
        };

    return (
        <div className={className}>
            <ReactMarkdown
                remarkPlugins={[remarkGfm, remarkMath]}
                rehypePlugins={[[rehypeKatex, { throwOnError: false, strict: false }]]}
                components={{
                    h1: heading("h1"),
                    h2: heading("h2"),
                    h3: heading("h3"),
                    a: ({ href, children: kids, ...rest }) => {
                        const isHash = typeof href === "string" && href.startsWith("#");
                        return (
                            <a
                                href={href}
                                className="text-[#60A5FA] no-underline hover:underline"
                                onClick={
                                    isHash
                                        ? (e) => {
                                              e.preventDefault();
                                              scrollToHeadingId(href || "");
                                          }
                                        : undefined
                                }
                                {...rest}
                            >
                                {kids}
                            </a>
                        );
                    },
                }}
            >
                {text}
            </ReactMarkdown>
        </div>
    );
}

export function headingsFromMarkdown(markdown: string): TocHeading[] {
    return extractMarkdownHeadings(sanitizeDocumentMarkdown(markdown));
}
