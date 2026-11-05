"use client";

import { useEffect, useId, useRef } from "react";

/** Soft-normalize one-line diagrams so Mermaid can parse them. */
function normalizeMermaidSource(raw: string): string {
    const src = String(raw || "").trim();
    if (!src || src.includes("\n")) return src;
    return src
        .replace(/;\s*/g, ";\n")
        .replace(/\s*(-->|---|-.->|==>|==)\s*/g, "\n$1 ")
        .trim();
}

export default function MermaidBlock({ code }: { code: string }) {
    const hostRef = useRef<HTMLDivElement>(null);
    const reactId = useId().replace(/:/g, "");

    useEffect(() => {
        const src = normalizeMermaidSource(code);
        if (!src || !hostRef.current) return;
        let cancelled = false;
        (async () => {
            const mermaid = (await import("mermaid")).default;
            mermaid.initialize({
                startOnLoad: false,
                theme: "dark",
                securityLevel: "loose",
                fontFamily: "inherit",
                themeVariables: {
                    primaryColor: "#1E3A5F",
                    primaryTextColor: "#F9FAFB",
                    primaryBorderColor: "#9CA3AF",
                    lineColor: "#E5E7EB",
                    secondaryColor: "#111827",
                    tertiaryColor: "#0F172A",
                    background: "#181B22",
                },
            });
            try {
                const { svg } = await mermaid.render(
                    `blop-mmd-${reactId}-${Math.random().toString(36).slice(2, 8)}`,
                    src
                );
                if (!cancelled && hostRef.current) hostRef.current.innerHTML = svg;
            } catch {
                if (!cancelled && hostRef.current) {
                    hostRef.current.textContent = src;
                }
            }
        })();
        return () => {
            cancelled = true;
        };
    }, [code, reactId]);

    return (
        <div className="blop-mermaid not-prose my-5 overflow-hidden rounded-[10px] border border-white/[0.08] bg-[#181B22] p-5">
            <div ref={hostRef} className="overflow-x-auto [&_svg]:max-w-full" />
        </div>
    );
}
