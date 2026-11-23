"use client";

import { useEffect, useId, useRef } from "react";
import { isMermaidErrorSvg, normalizeMermaidSource } from "@/lib/mermaidNormalize";

let mermaidInitPromise: Promise<typeof import("mermaid").default> | null = null;

async function getMermaid() {
    if (!mermaidInitPromise) {
        mermaidInitPromise = import("mermaid").then((mod) => {
            const mermaid = mod.default;
            mermaid.initialize({
                startOnLoad: false,
                theme: "dark",
                securityLevel: "loose",
                fontFamily: "ui-sans-serif, system-ui, sans-serif",
                flowchart: {
                    htmlLabels: true,
                    curve: "basis",
                    padding: 8,
                    nodeSpacing: 28,
                    rankSpacing: 36,
                    useMaxWidth: false,
                },
                themeVariables: {
                    fontSize: "15px",
                    primaryColor: "#1E3A5F",
                    primaryTextColor: "#F9FAFB",
                    primaryBorderColor: "#9CA3AF",
                    lineColor: "#E5E7EB",
                    secondaryColor: "#111827",
                    tertiaryColor: "#0F172A",
                    background: "#181B22",
                    mainBkg: "#1E3A5F",
                    nodeBorder: "#9CA3AF",
                    clusterBkg: "#111827",
                    titleColor: "#F9FAFB",
                    edgeLabelBackground: "#181B22",
                },
            });
            return mermaid;
        });
    }
    return mermaidInitPromise;
}

/**
 * Crop empty Mermaid canvas via getBBox, then display fluidly in the card
 * (width 100%, height auto, max-height) so PAPs stay compact and readable.
 */
function fitSvg(host: HTMLElement) {
    const svg = host.querySelector("svg") as SVGSVGElement | null;
    if (!svg) return;

    const maxH = Math.min(Math.round((typeof window !== "undefined" ? window.innerHeight : 900) * 0.72), 780);

    try {
        const bbox = svg.getBBox();
        if (bbox.width > 1 && bbox.height > 1) {
            const pad = 10;
            svg.setAttribute(
                "viewBox",
                `${bbox.x - pad} ${bbox.y - pad} ${bbox.width + pad * 2} ${bbox.height + pad * 2}`
            );
        }
    } catch {
        /* getBBox can throw if not in DOM yet */
    }

    svg.removeAttribute("width");
    svg.removeAttribute("height");
    svg.setAttribute("preserveAspectRatio", "xMidYMin meet");
    svg.style.display = "block";
    svg.style.width = "100%";
    svg.style.maxWidth = "100%";
    svg.style.height = "auto";
    svg.style.maxHeight = `${maxH}px`;
    svg.style.marginInline = "auto";
}

function showFallback(host: HTMLElement, src: string, detail?: string) {
    host.innerHTML = "";
    const wrap = document.createElement("div");
    wrap.className = "space-y-2";
    const msg = document.createElement("p");
    msg.className = "text-sm text-amber-200/90";
    msg.textContent = detail || "Diagramm konnte nicht gerendert werden (Syntax). Rohtext:";
    const pre = document.createElement("pre");
    pre.className = "text-xs text-gray-400 whitespace-pre-wrap font-mono max-h-48 overflow-auto";
    pre.textContent = src;
    wrap.appendChild(msg);
    wrap.appendChild(pre);
    host.appendChild(wrap);
}

export default function MermaidBlock({ code }: { code: string }) {
    const hostRef = useRef<HTMLDivElement>(null);
    const reactId = useId().replace(/:/g, "");

    useEffect(() => {
        const src = normalizeMermaidSource(code);
        if (!src || !hostRef.current) return;
        let cancelled = false;
        (async () => {
            try {
                const mermaid = await getMermaid();
                const { svg } = await mermaid.render(
                    `blop-mmd-${reactId}-${Math.random().toString(36).slice(2, 8)}`,
                    src
                );
                if (cancelled || !hostRef.current) return;
                if (isMermaidErrorSvg(svg)) {
                    showFallback(hostRef.current, src, "Mermaid-Syntaxfehler — Diagramm übersprungen:");
                    return;
                }
                hostRef.current.innerHTML = svg;
                // Double rAF: layout/paint so getBBox sees real geometry
                requestAnimationFrame(() => {
                    requestAnimationFrame(() => {
                        if (!cancelled && hostRef.current) fitSvg(hostRef.current);
                    });
                });
            } catch {
                if (!cancelled && hostRef.current) {
                    showFallback(hostRef.current, src);
                }
            }
        })();
        return () => {
            cancelled = true;
        };
    }, [code, reactId]);

    return (
        <div className="blop-mermaid not-prose my-5 max-h-[min(75vh,820px)] overflow-auto rounded-[10px] border border-white/[0.08] bg-[#181B22] p-4 sm:p-5">
            <div ref={hostRef} className="min-w-0 [&_svg]:max-w-full" />
        </div>
    );
}
