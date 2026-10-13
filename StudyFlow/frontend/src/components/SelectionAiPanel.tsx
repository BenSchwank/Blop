"use client";

import React, { useEffect, useRef } from "react";
import { Sparkles, X } from "lucide-react";
import { OVERLAY_SELECTION_AI_PANEL } from "@/constants/overlayLayout";

export interface SelectionAiPanelProps {
    open: boolean;
    selectedText: string;
    instruction: string;
    busy: boolean;
    error?: string;
    onInstructionChange: (value: string) => void;
    onClose: () => void;
    onApply: () => void;
    /** Optional: parent can attach a ref to detect focus inside the panel. */
    panelRef?: React.RefObject<HTMLDivElement | null>;
}

export default function SelectionAiPanel({
    open,
    selectedText,
    instruction,
    busy,
    error,
    onInstructionChange,
    onClose,
    onApply,
    panelRef,
}: SelectionAiPanelProps) {
    const localRef = useRef<HTMLDivElement | null>(null);
    const setRefs = (node: HTMLDivElement | null) => {
        localRef.current = node;
        if (panelRef) {
            (panelRef as React.MutableRefObject<HTMLDivElement | null>).current = node;
        }
    };

    useEffect(() => {
        if (!open || busy) return;
        const onKey = (ev: KeyboardEvent) => {
            if (ev.key === "Escape") onClose();
        };
        document.addEventListener("keydown", onKey);
        return () => document.removeEventListener("keydown", onKey);
    }, [open, busy, onClose]);

    if (!open) return null;

    const preview =
        selectedText.length > 280 ? `${selectedText.slice(0, 280).trim()}…` : selectedText;

    return (
        <div ref={setRefs} className={OVERLAY_SELECTION_AI_PANEL} role="dialog" aria-label="Markierung mit KI bearbeiten">
            <div className="flex items-center justify-between gap-2 px-4 py-3 border-b border-[#2A3140] bg-[#1A1F2A] shrink-0">
                <div className="flex items-center gap-2.5 min-w-0">
                    <div className="w-8 h-8 rounded-full bg-[#5B9DFF]/15 text-[#5B9DFF] flex items-center justify-center ring-1 ring-[#5B9DFF]/25 shrink-0">
                        <Sparkles size={16} />
                    </div>
                    <div className="min-w-0">
                        <h3 className="text-sm font-semibold text-white truncate">Markierung bearbeiten</h3>
                        <p className="text-[10px] text-gray-500 truncate">Rechts am Rand · nicht an der Maus</p>
                    </div>
                </div>
                <button
                    type="button"
                    onClick={onClose}
                    disabled={busy}
                    className="p-1.5 text-gray-400 hover:text-white hover:bg-[#243044] rounded-lg disabled:opacity-40"
                    aria-label="Schließen"
                >
                    <X size={16} />
                </button>
            </div>

            <div className="flex-1 overflow-y-auto p-4 space-y-3 custom-scrollbar min-h-0">
                <div>
                    <p className="text-[11px] uppercase tracking-wide text-gray-500 mb-1.5">Auswahl</p>
                    <div className="rounded-xl border border-[#2A3140] bg-[#0E1117] px-3 py-2.5 text-xs text-gray-300 leading-relaxed max-h-36 overflow-y-auto whitespace-pre-wrap">
                        {preview || "—"}
                    </div>
                </div>
                <div>
                    <p className="text-[11px] uppercase tracking-wide text-gray-500 mb-1.5">Anweisung</p>
                    <textarea
                        value={instruction}
                        onChange={(e) => onInstructionChange(e.target.value)}
                        placeholder="z. B. kürzer, einfacher, professioneller formulieren …"
                        className="w-full bg-[#0E1117] border border-[#2A3140] text-gray-200 rounded-xl px-3 py-2.5 text-sm min-h-[100px] focus:outline-none focus:ring-2 focus:ring-[#5B9DFF]/40 focus:border-[#5B9DFF]/50 resize-y"
                        disabled={busy}
                    />
                </div>
                {error ? <p className="text-xs text-red-400">{error}</p> : null}
            </div>

            <div className="flex gap-2 p-4 border-t border-[#2A3140] bg-[#1A1F2A] shrink-0">
                <button
                    type="button"
                    onClick={onClose}
                    disabled={busy}
                    className="flex-1 py-2.5 rounded-xl text-sm bg-[#243044] hover:bg-[#2E3A4F] text-gray-200 disabled:opacity-50"
                >
                    Schließen
                </button>
                <button
                    type="button"
                    onClick={onApply}
                    disabled={!instruction.trim() || busy || !selectedText.trim()}
                    className="flex-1 py-2.5 rounded-xl text-sm bg-[#5B9DFF] hover:bg-[#4A8AE6] text-white disabled:opacity-50 font-medium"
                >
                    {busy ? "Bearbeite…" : "Mit KI anwenden"}
                </button>
            </div>
        </div>
    );
}
