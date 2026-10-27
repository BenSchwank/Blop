"use client";

import React, { useMemo } from "react";
import ReactMarkdown from "react-markdown";
import remarkGfm from "remark-gfm";
import remarkMath from "remark-math";
import rehypeKatex from "rehype-katex";
import "katex/dist/katex.min.css";
import { extractMarkdownHeadings, slugifyHeading, type TocHeading } from "@/lib/markdownHeadings";

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

export default function DocumentMarkdown({ children, className = "" }: Props) {
    const text = typeof children === "string" ? children : String(children ?? "");
    const headings = useMemo(() => extractMarkdownHeadings(text), [text]);
    const cursor = { i: 0 };

    const heading = (Tag: "h1" | "h2" | "h3") =>
        function Heading({ children: kids }: { children?: React.ReactNode }) {
            const fallback = slugifyHeading(plainChildren(kids));
            const id = headings[cursor.i]?.id || fallback;
            cursor.i += 1;
            return (
                <Tag id={id} className="scroll-mt-24">
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
                }}
            >
                {text}
            </ReactMarkdown>
        </div>
    );
}

export function headingsFromMarkdown(markdown: string): TocHeading[] {
    return extractMarkdownHeadings(markdown);
}
