"use client";

import React, { useEffect, useMemo, useState } from "react";
import { List } from "lucide-react";
import { isTocIgnoredHeading, type TocHeading } from "@/lib/markdownHeadings";

type Props = {
    headings: TocHeading[];
    scrollRoot?: HTMLElement | null;
    scrollRootRef?: React.RefObject<HTMLElement | null>;
};

export default function DocumentToc({ headings, scrollRoot, scrollRootRef }: Props) {
    const [open, setOpen] = useState(false);
    const [active, setActive] = useState<string>("");

    const usable = useMemo(
        () =>
            headings.filter(
                (h) => h.level >= 2 && h.level <= 3 && !isTocIgnoredHeading(h.text)
            ),
        [headings]
    );

    useEffect(() => {
        if (usable.length === 0) return;
        const root = scrollRoot || scrollRootRef?.current || null;
        const observer = new IntersectionObserver(
            (entries) => {
                const visible = entries
                    .filter((e) => e.isIntersecting)
                    .sort((a, b) => a.boundingClientRect.top - b.boundingClientRect.top);
                const id = visible[0]?.target?.id;
                if (id) setActive(id);
            },
            { root, rootMargin: "-88px 0px -55% 0px", threshold: [0, 0.15, 1] }
        );
        usable.forEach((h) => {
            const el = document.getElementById(h.id);
            if (el) observer.observe(el);
        });
        if (!active && usable[0]) setActive(usable[0].id);
        return () => observer.disconnect();
        // eslint-disable-next-line react-hooks/exhaustive-deps
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
        <nav aria-label="Inhaltsverzeichnis" className="border-l border-white/[0.08]">
            {usable.map((h) => {
                const isActive = active === h.id;
                return (
                    <button
                        key={h.id}
                        type="button"
                        onClick={() => jump(h.id)}
                        className={`block w-full text-left line-clamp-2 text-[13px] leading-[1.4] py-1.5 px-3 transition-colors ${
                            h.level === 3 ? "pl-6" : ""
                        } ${
                            isActive
                                ? "border-l-2 border-[#3B82F6] -ml-px bg-[rgba(59,130,246,0.08)] text-[#F8FAFC] font-medium"
                                : "border-l-2 border-transparent -ml-px text-[#9CA3AF] hover:text-[#E5E7EB]"
                        }`}
                    >
                        {h.text}
                    </button>
                );
            })}
        </nav>
    );

    return (
        <>
            <div className="lg:hidden no-print mb-4">
                <button
                    type="button"
                    onClick={() => setOpen((v) => !v)}
                    className="inline-flex items-center gap-2 px-3 py-2 rounded-xl border border-white/10 bg-[#1A1F2A] text-sm text-gray-200"
                >
                    <List size={16} className="text-[#5B9DFF]" />
                    Inhalt
                </button>
                {open ? (
                    <div className="mt-2 rounded-xl border border-white/10 bg-[#1A1F2A] p-3 max-h-64 overflow-y-auto custom-scrollbar">
                        {list}
                    </div>
                ) : null}
            </div>
            <aside className="hidden lg:block no-print sticky top-[88px] self-start w-[252px] shrink-0 max-h-[calc(100vh-120px)] overflow-y-auto custom-scrollbar pr-1">
                <p className="text-[11px] uppercase tracking-widest text-[#5B9DFF] mb-3 flex items-center gap-1.5">
                    <List size={12} /> Inhalt
                </p>
                {list}
            </aside>
        </>
    );
}
