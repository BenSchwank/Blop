"use client";

import { useState, type ReactNode } from "react";
import { Check, Copy } from "lucide-react";

type Props = {
    language?: string;
    children: ReactNode;
    className?: string;
};

export default function CodeBlock({ language, children, className }: Props) {
    const [copied, setCopied] = useState(false);
    const label = (language || "code").toLowerCase();

    const handleCopy = async (e: React.MouseEvent<HTMLButtonElement>) => {
        const root = e.currentTarget.closest(".blop-code-block");
        const text = root?.querySelector("code")?.textContent ?? "";
        if (!text) return;
        try {
            await navigator.clipboard.writeText(text);
            setCopied(true);
            window.setTimeout(() => setCopied(false), 1600);
        } catch {
            /* ignore */
        }
    };

    return (
        <div className="blop-code-block not-prose my-5 overflow-hidden rounded-[10px] border border-white/[0.08] bg-[#12141A]">
            <div className="flex h-8 items-center justify-between border-b border-white/[0.06] bg-[#1E222B] px-3">
                <span className="text-[11px] font-medium uppercase tracking-wide text-gray-400">{label}</span>
                <button
                    type="button"
                    onClick={handleCopy}
                    className="inline-flex h-6 items-center gap-1 rounded px-1.5 text-[11px] text-gray-400 hover:bg-white/5 hover:text-white"
                    title="Kopieren"
                >
                    {copied ? <Check size={12} className="text-emerald-400" /> : <Copy size={12} />}
                    <span>{copied ? "Kopiert" : "Kopieren"}</span>
                </button>
            </div>
            <pre className="m-0 overflow-x-auto p-4" style={{ whiteSpace: "pre" }}>
                <code
                    className={[
                        className,
                        "hljs block font-mono text-[13px] leading-[1.6] text-[#E5E7EB]",
                        "[font-family:JetBrains_Mono,Fira_Code,ui-monospace,SFMono-Regular,Menlo,Consolas,monospace]",
                    ]
                        .filter(Boolean)
                        .join(" ")}
                >
                    {children}
                </code>
            </pre>
        </div>
    );
}
