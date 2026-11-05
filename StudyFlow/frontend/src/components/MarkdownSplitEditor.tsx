"use client";

import React, { useCallback, useEffect, useMemo, useRef, useState } from "react";
import { Save, X, Loader2, Check, Bold, Heading2, Sigma, Code2, GitBranch } from "lucide-react";
import DocumentMarkdown from "@/components/DocumentMarkdown";

type Props = {
    initialContent: string;
    title: string;
    onSave: (content: string, opts?: { close?: boolean }) => Promise<void>;
    onClose: () => void;
    contentShiftClass?: string;
    youtubeVideoId?: string | null;
};

type InsertKind = "h2" | "bold" | "inlineMath" | "displayMath" | "mermaid" | "code";

function insertSnippet(
    value: string,
    start: number,
    end: number,
    kind: InsertKind
): { next: string; selStart: number; selEnd: number } {
    const selected = value.slice(start, end);
    const wrap = (before: string, after: string, placeholder: string) => {
        const body = selected || placeholder;
        const next = value.slice(0, start) + before + body + after + value.slice(end);
        const selStart = start + before.length;
        return { next, selStart, selEnd: selStart + body.length };
    };
    switch (kind) {
        case "h2":
            return wrap("\n## ", "\n", "Überschrift");
        case "bold":
            return wrap("**", "**", "fett");
        case "inlineMath":
            return wrap("$", "$", "x");
        case "displayMath":
            return wrap("\n$$\n", "\n$$\n", "\\frac{a}{b}");
        case "mermaid":
            return wrap("\n```mermaid\n", "\n```\n", "flowchart TD\n  A --> B");
        case "code":
            return wrap("\n```c\n", "\n```\n", "// code");
        default:
            return { next: value, selStart: start, selEnd: end };
    }
}

export default function MarkdownSplitEditor({
    initialContent,
    title,
    onSave,
    onClose,
    contentShiftClass = "",
    youtubeVideoId,
}: Props) {
    const [value, setValue] = useState(() => String(initialContent || ""));
    const [isSaving, setIsSaving] = useState(false);
    const [saveStatus, setSaveStatus] = useState<"idle" | "saving" | "saved">("idle");
    const taRef = useRef<HTMLTextAreaElement | null>(null);
    const baseline = useRef(String(initialContent || ""));

    useEffect(() => {
        const next = String(initialContent || "");
        setValue(next);
        baseline.current = next;
    }, [initialContent]);

    const dirty = value !== baseline.current;

    const preview = useMemo(() => value, [value]);

    const applyInsert = useCallback((kind: InsertKind) => {
        const el = taRef.current;
        const start = el?.selectionStart ?? value.length;
        const end = el?.selectionEnd ?? value.length;
        const { next, selStart, selEnd } = insertSnippet(value, start, end, kind);
        setValue(next);
        requestAnimationFrame(() => {
            if (!taRef.current) return;
            taRef.current.focus();
            taRef.current.setSelectionRange(selStart, selEnd);
        });
    }, [value]);

    const handleSave = async (close: boolean) => {
        setIsSaving(true);
        setSaveStatus("saving");
        try {
            await onSave(value, { close });
            baseline.current = value;
            setSaveStatus("saved");
            window.setTimeout(() => setSaveStatus("idle"), 1600);
        } catch {
            setSaveStatus("idle");
        } finally {
            setIsSaving(false);
        }
    };

    const requestClose = () => {
        if (dirty && !window.confirm("Ungespeicherte Änderungen verwerfen?")) return;
        onClose();
    };

    const toolBtn =
        "inline-flex h-8 items-center gap-1.5 rounded-lg px-2.5 text-[12px] text-gray-300 hover:bg-white/5 hover:text-white";

    return (
        <div
            className={`fixed inset-0 z-[100] flex flex-col overflow-hidden bg-[#1A1916] animate-in fade-in duration-200 ${contentShiftClass}`}
        >
            <div className="relative z-20 flex h-14 shrink-0 items-center justify-between gap-3 border-b border-white/[0.08] bg-[#23252A] px-3 sm:px-4">
                <div className="flex min-w-0 items-center gap-2">
                    <button
                        type="button"
                        onClick={requestClose}
                        className="shrink-0 rounded-xl p-2 text-gray-400 hover:bg-[#3A3D45] hover:text-white"
                        aria-label="Schließen"
                    >
                        <X size={18} />
                    </button>
                    <p className="truncate text-[13px] font-medium text-white">{title || "Dokument"}</p>
                    <span className="hidden sm:inline text-[11px] text-gray-500">Markdown-Quelle</span>
                </div>
                <div className="flex shrink-0 items-center gap-2">
                    {saveStatus === "saved" ? (
                        <span className="inline-flex items-center gap-1 text-[12px] text-emerald-400">
                            <Check size={14} /> Gespeichert
                        </span>
                    ) : null}
                    <button
                        type="button"
                        disabled={isSaving}
                        onClick={() => void handleSave(false)}
                        className="inline-flex h-[34px] items-center gap-1.5 rounded-lg border border-white/10 bg-[#27272A] px-3 text-[13px] text-white hover:bg-[#323232] disabled:opacity-50"
                    >
                        {isSaving ? <Loader2 size={14} className="animate-spin" /> : <Save size={14} />}
                        Speichern
                    </button>
                    <button
                        type="button"
                        disabled={isSaving}
                        onClick={() => void handleSave(true)}
                        className="inline-flex h-[34px] items-center gap-1.5 rounded-lg bg-[#2563EB] px-3 text-[13px] font-medium text-white hover:bg-[#1D4ED8] disabled:opacity-50"
                    >
                        Speichern &amp; Schließen
                    </button>
                </div>
            </div>

            <div className="flex shrink-0 flex-wrap items-center gap-1 border-b border-white/[0.06] bg-[#1E222B] px-3 py-1.5">
                <button type="button" className={toolBtn} onClick={() => applyInsert("h2")} title="Überschrift">
                    <Heading2 size={14} /> H2
                </button>
                <button type="button" className={toolBtn} onClick={() => applyInsert("bold")} title="Fett">
                    <Bold size={14} /> Fett
                </button>
                <button type="button" className={toolBtn} onClick={() => applyInsert("inlineMath")} title="Inline-Math">
                    <Sigma size={14} /> $…$
                </button>
                <button type="button" className={toolBtn} onClick={() => applyInsert("displayMath")} title="Display-Math">
                    <Sigma size={14} /> $$
                </button>
                <button type="button" className={toolBtn} onClick={() => applyInsert("mermaid")} title="Mermaid">
                    <GitBranch size={14} /> Mermaid
                </button>
                <button type="button" className={toolBtn} onClick={() => applyInsert("code")} title="Code">
                    <Code2 size={14} /> Code
                </button>
            </div>

            {youtubeVideoId ? (
                <div className="shrink-0 border-b border-white/[0.06] bg-black px-4 py-3">
                    <div className="mx-auto aspect-video max-h-[220px] w-full max-w-3xl overflow-hidden rounded-xl border border-[#45484F]">
                        <iframe
                            title="YouTube"
                            className="h-full w-full"
                            src={`https://www.youtube-nocookie.com/embed/${youtubeVideoId}`}
                            allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture"
                            allowFullScreen
                        />
                    </div>
                </div>
            ) : null}

            <div className="flex min-h-0 flex-1 flex-col lg:flex-row">
                <div className="flex min-h-0 flex-1 flex-col border-b border-white/[0.06] lg:border-b-0 lg:border-r lg:border-white/[0.06]">
                    <div className="shrink-0 px-3 py-1.5 text-[11px] uppercase tracking-wide text-gray-500">Quelle</div>
                    <textarea
                        ref={taRef}
                        value={value}
                        onChange={(e) => setValue(e.target.value)}
                        spellCheck={false}
                        className="min-h-0 flex-1 resize-none bg-[#12141A] px-4 py-3 font-mono text-[13px] leading-[1.6] text-[#E5E7EB] outline-none"
                        style={{
                            fontFamily:
                                "JetBrains Mono, Fira Code, ui-monospace, SFMono-Regular, Menlo, Consolas, monospace",
                        }}
                    />
                </div>
                <div className="flex min-h-0 flex-1 flex-col overflow-hidden bg-[#1a1a1a]">
                    <div className="shrink-0 px-3 py-1.5 text-[11px] uppercase tracking-wide text-gray-500">Vorschau</div>
                    <div className="min-h-0 flex-1 overflow-y-auto p-4 sm:p-6">
                        <div className="prose prose-invert prose-blop mx-auto max-w-[760px]">
                            <DocumentMarkdown>{preview}</DocumentMarkdown>
                        </div>
                    </div>
                </div>
            </div>
        </div>
    );
}
