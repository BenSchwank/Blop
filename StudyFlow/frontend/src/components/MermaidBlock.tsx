"use client";

import { useEffect, useId, useRef } from "react";

export default function MermaidBlock({ code }: { code: string }) {
    const hostRef = useRef<HTMLDivElement>(null);
    const reactId = useId().replace(/:/g, "");

    useEffect(() => {
        const src = String(code || "").trim();
        if (!src || !hostRef.current) return;
        let cancelled = false;
        (async () => {
            const mermaid = (await import("mermaid")).default;
            mermaid.initialize({
                startOnLoad: false,
                theme: "dark",
                securityLevel: "strict",
                fontFamily: "inherit",
                themeVariables: {
                    primaryColor: "#1E3A5F",
                    primaryTextColor: "#F9FAFB",
                    primaryBorderColor: "#9CA3AF",
                    lineColor: "#E5E7EB",
                    secondaryColor: "#111827",
                    tertiaryColor: "#0F172A",
                    background: "#111827",
                },
            });
            try {
                const { svg } = await mermaid.render(`blop-mmd-${reactId}-${Math.random().toString(36).slice(2, 8)}`, src);
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

    return <div ref={hostRef} className="blop-mermaid not-prose my-4 overflow-x-auto [&_svg]:max-w-full" />;
}
