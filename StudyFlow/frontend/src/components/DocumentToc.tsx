"use client";

import React, { useEffect, useState } from "react";
import { List } from "lucide-react";
import type { TocHeading } from "@/lib/markdownHeadings";

type Props = {
    headings: TocHeading[];
    scrollRoot?: HTMLElement | null;
    scrollRootRef?: React.RefObject<HTMLElement | null>;
};

export default function DocumentToc({ headings, scrollRoot, scrollRootRef }: Props) {
    const [open, setOpen] = useState(false);
    const [active, setActive] = useState<string>("");

    const usable = headings.filter((h) => h.level >= 1 && h.level <= 3);
    useEffect(() => {
        if (usable.length === 0) return;
        const root = scrollRoot || scrollRootRef?.current || null;
        const onScroll = () => {
            const ids = usable.map((h) => h.id);
            let current = ids[0] || "";
            for (const id of ids) {
                const el = document.getElementById(id);
                if (!el) continue;
                const top = el.getBoundingClientRect().top;
                if (top <= 120) current = id;
            }
            setActive(current);
        };
        const target: HTMLElement | Window = root || window;
        target.addEventListener("scroll", onScroll, { passive: true });
        onScroll();
        return () => target.removeEventListener("scroll", onScroll);
    }, [usable, scrollRoot, scrollRootRef]);

    if (usable.length < 2) return null;

    const jump = (id: string) => {
        const el = document.getElementById(id);
        if (!el) return;
        el.scrollIntoView({ behavior: "smooth", block: "start" });
        setActive(id);
        setOpen(false);
    };

    const list = (
        <nav aria-label="Inhaltsverzeichnis" className="space-y-0.5">
            {usable.map((h) => (
                <button
                    key={h.id}
                    type="button"
                    onClick={() => jump(h.id)}
                    className={`block w-full text-left rounded-lg px-2 py-1.5 text-xs leading-snug transition-colors ${
                        active === h.id ? "bg-[#5B9DFF]/15 text-[#9EC5FF]" : "text-gray-400 hover:text-white hover:bg-[#243044]"
                    } ${h.level === 1 ? "font-semibold" : ""} ${h.level === 3 ? "pl-5" : h.level === 2 ? "pl-3" : ""}`}
                >
                    {h.text}
                </button>
            ))}
        </nav>
    );

    return (
        <>
            <div className="lg:hidden no-print mb-4">
                <button
                    type="button"
                    onClick={() => setOpen((v) => !v)}
                    className="inline-flex items-center gap-2 px-3 py-2 rounded-xl border border-[#2A3140] bg-[#1A1F2A] text-sm text-gray-200"
                >
                    <List size={16} className="text-[#5B9DFF]" />
                    Inhaltsverzeichnis
                </button>
                {open ? (
                    <div className="mt-2 rounded-xl border border-[#2A3140] bg-[#1A1F2A] p-3 max-h-64 overflow-y-auto custom-scrollbar">
                        {list}
                    </div>
                ) : null}
            </div>
            <aside className="hidden lg:block no-print sticky top-4 self-start w-56 shrink-0 max-h-[calc(100vh-8rem)] overflow-y-auto custom-scrollbar rounded-2xl border border-[#2A3140] bg-[#1A1F2A]/90 p-3">
                <p className="text-[11px] uppercase tracking-widest text-[#5B9DFF] mb-2 flex items-center gap-1.5">
                    <List size={12} /> Inhalt
                </p>
                {list}
            </aside>
        </>
    );
}
