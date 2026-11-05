"use client";

import React, { useMemo } from "react";
import ReactMarkdown from "react-markdown";
import remarkGfm from "remark-gfm";
import remarkMath from "remark-math";
import rehypeKatex from "rehype-katex";
import rehypeHighlight from "rehype-highlight";
import type { Root } from "mdast";
import type { Plugin } from "unified";
import "katex/dist/katex.min.css";
import "highlight.js/styles/github-dark.css";
import MermaidBlock from "@/components/MermaidBlock";
import CodeBlock from "@/components/CodeBlock";
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

/** Prevent 4-space / tab indented blocks from becoming <pre> (breaks #### c) headings). */
const remarkDisableIndentedCode: Plugin<[], Root> = function remarkDisableIndentedCode() {
    const data = this.data() as Record<string, unknown>;
    const micromarkExtensions = (data.micromarkExtensions as Array<Record<string, unknown>> | undefined) || [];
    micromarkExtensions.push({ disable: { null: ["codeIndented"] } });
    data.micromarkExtensions = micromarkExtensions;
};

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
                remarkPlugins={[remarkGfm, remarkMath, remarkDisableIndentedCode]}
                rehypePlugins={[
                    [rehypeKatex, { throwOnError: false, strict: false }],
                    [rehypeHighlight, { detect: false, ignoreMissing: true }],
                ]}
                components={{
                    h1: heading("h1"),
                    h2: heading("h2"),
                    h3: heading("h3"),
                    pre: ({ children: kids }) => <>{kids}</>,
                    code: ({ className, children: kids }) => {
                        const lang = /language-(\w+)/.exec(className || "")?.[1];
                        const plain = String(kids ?? "").replace(/\n$/, "");
                        if (lang === "mermaid") {
                            // Prefer raw text; highlighted children may already be nodes
                            const raw =
                                typeof kids === "string" || typeof kids === "number"
                                    ? String(kids).replace(/\n$/, "")
                                    : plainChildren(kids).replace(/\n$/, "") || plain;
                            return <MermaidBlock code={raw} />;
                        }
                        if (lang) {
                            return (
                                <CodeBlock language={lang} className={className}>
                                    {kids}
                                </CodeBlock>
                            );
                        }
                        return <code className={className}>{kids}</code>;
                    },
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
