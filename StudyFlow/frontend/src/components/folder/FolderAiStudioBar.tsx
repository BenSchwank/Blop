"use client";

import { useState } from "react";
import {
    BrainCircuit,
    Sparkles,
    HelpCircle,
    Layers,
    FileOutput,
    FileText,
    Repeat,
    Mic,
    Video,
    Loader2,
    ChevronDown,
} from "lucide-react";

export type AiToolKey =
    | "plan"
    | "smart-learning"
    | "quiz"
    | "flashcards"
    | "summary"
    | "elaboration"
    | "repetition"
    | "podcast"
    | "learning-video";

type Props = {
    activeCount: number;
    eligibleCount: number;
    isGenerating: string[];
    savingAiContext?: boolean;
    onPreset: (preset: "all" | "sources" | "week" | "clear") => void;
    onTool: (key: AiToolKey) => void;
};

const TOOLS: { key: AiToolKey; label: string; icon: typeof BrainCircuit; genKey: string; tone: string }[] = [
    { key: "plan", label: "Lernplan", icon: BrainCircuit, genKey: "plan", tone: "bg-blue-500/10 text-blue-400 hover:bg-blue-500/20" },
    { key: "smart-learning", label: "Lernen", icon: Sparkles, genKey: "smart-learning", tone: "bg-indigo-500/10 text-indigo-300 hover:bg-indigo-500/20" },
    { key: "quiz", label: "Quiz", icon: HelpCircle, genKey: "quiz", tone: "bg-yellow-500/10 text-yellow-500 hover:bg-yellow-500/20" },
    { key: "flashcards", label: "Karten", icon: Layers, genKey: "flashcards", tone: "bg-green-500/10 text-green-400 hover:bg-green-500/20" },
    { key: "summary", label: "Zusammenfassung", icon: FileOutput, genKey: "summary", tone: "bg-blue-500/10 text-blue-400 hover:bg-blue-500/20" },
    { key: "elaboration", label: "Ausarbeitung", icon: FileText, genKey: "elaboration", tone: "bg-orange-500/10 text-orange-400 hover:bg-orange-500/20" },
    { key: "repetition", label: "Wiederholung", icon: Repeat, genKey: "repetition", tone: "bg-teal-500/10 text-teal-400 hover:bg-teal-500/20" },
    { key: "podcast", label: "Podcast", icon: Mic, genKey: "podcast", tone: "bg-blue-500/10 text-blue-300 hover:bg-blue-500/20" },
    { key: "learning-video", label: "Video", icon: Video, genKey: "learning-video", tone: "bg-cyan-500/10 text-cyan-300 hover:bg-cyan-500/20" },
];

export default function FolderAiStudioBar({
    activeCount,
    eligibleCount,
    isGenerating,
    savingAiContext,
    onPreset,
    onTool,
}: Props) {
    const [presetOpen, setPresetOpen] = useState(false);

    return (
        <div
            className="mb-5 flex flex-wrap items-center justify-between gap-2.5 rounded-xl border border-white/[0.08] px-3.5 py-2.5"
            style={{ background: "#181B22" }}
        >
            <div className="relative flex items-center gap-2 min-w-0">
                <span className="inline-flex items-center gap-1.5 rounded-full border border-[#5B9DFF]/25 bg-[#5B9DFF]/10 px-2.5 py-1 text-[12px] font-medium text-[#93C5FD] whitespace-nowrap">
                    Aktive KI-Quellen: {activeCount} von {eligibleCount} ausgewählt
                    {savingAiContext ? <Loader2 size={12} className="animate-spin" /> : null}
                </span>
                <button
                    type="button"
                    onClick={() => setPresetOpen((v) => !v)}
                    className="inline-flex items-center gap-1 h-8 px-2.5 rounded-lg text-[12px] text-gray-300 bg-white/[0.04] border border-white/[0.08] hover:bg-white/[0.08]"
                >
                    Quellen
                    <ChevronDown size={14} />
                </button>
                {presetOpen ? (
                    <div className="absolute left-0 top-full mt-1 z-40 w-56 rounded-xl border border-[#45484F] bg-[#23252A] shadow-2xl overflow-hidden">
                        {(
                            [
                                ["all", "Alle Quellen"],
                                ["sources", "Nur PDFs & Transkripte"],
                                ["week", "Nur diese Woche"],
                                ["clear", "Auswahl leeren"],
                            ] as const
                        ).map(([key, label]) => (
                            <button
                                key={key}
                                type="button"
                                onClick={() => {
                                    setPresetOpen(false);
                                    onPreset(key);
                                }}
                                className="w-full text-left px-3 py-2.5 text-sm text-gray-300 hover:bg-[#3A3D45] hover:text-white"
                            >
                                {label}
                            </button>
                        ))}
                    </div>
                ) : null}
            </div>

            <div className="flex flex-wrap items-center gap-1.5">
                {TOOLS.map((tool) => {
                    const Icon = tool.icon;
                    const busy = isGenerating.includes(tool.genKey);
                    return (
                        <button
                            key={tool.key}
                            type="button"
                            disabled={busy}
                            onClick={() => onTool(tool.key)}
                            className={`inline-flex items-center gap-1.5 h-8 px-2.5 rounded-lg text-[12px] font-medium transition-all disabled:opacity-50 ${tool.tone}`}
                            title={tool.label}
                        >
                            {busy ? <Loader2 size={14} className="animate-spin" /> : <Icon size={14} />}
                            {tool.label}
                        </button>
                    );
                })}
            </div>
        </div>
    );
}
