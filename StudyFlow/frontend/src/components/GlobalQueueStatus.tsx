"use client";

import { useEffect, useState } from "react";
import { CheckCircle2, ChevronDown, ChevronUp, ListTodo, Loader2, XCircle, ArrowUp, ArrowDown, X } from "lucide-react";
import { abortAiJob } from "@/lib/aiJobAbortRegistry";
import {
    readQueueSnapshot,
    reorderActiveJobs,
    removeActiveJobById,
    dismissRecentJobById,
    type QueueSnapshot,
} from "@/lib/globalQueueStorage";

const EMPTY_QUEUE: QueueSnapshot = { updatedAt: 0, active: [], recent: [] };

export default function GlobalQueueStatus() {
    const [queue, setQueue] = useState<QueueSnapshot>(EMPTY_QUEUE);
    const [expanded, setExpanded] = useState(false);

    useEffect(() => {
        const readQueue = () => {
            const parsed = readQueueSnapshot();
            if (parsed) {
                setQueue({
                    updatedAt: parsed.updatedAt,
                    active: parsed.active,
                    recent: parsed.recent.slice(0, 15),
                });
                return;
            }
            setQueue(EMPTY_QUEUE);
        };

        readQueue();
        window.addEventListener("storage", readQueue);
        window.addEventListener("blop_global_queue_updated", readQueue as EventListener);
        return () => {
            window.removeEventListener("storage", readQueue);
            window.removeEventListener("blop_global_queue_updated", readQueue as EventListener);
        };
    }, []);

    const handleCancelActive = (jobId: string) => {
        const aborted = abortAiJob(jobId);
        if (!aborted) removeActiveJobById(jobId);
    };

    const handleMoveActive = (fromIndex: number, delta: number) => {
        reorderActiveJobs(fromIndex, fromIndex + delta);
    };

    if (queue.active.length === 0 && queue.recent.length === 0) return null;

    return (
        <>
            {/* Topbar indicator with expandable panel */}
            <div className="fixed top-3 right-3 z-[130] no-print">
                <button
                    type="button"
                    onClick={() => setExpanded((v) => !v)}
                    className="flex items-center gap-2 rounded-xl border border-[#2A2A40] bg-[#0B0B1A]/95 px-3 py-2 text-sm text-white shadow-xl backdrop-blur-md hover:bg-[#151525]"
                >
                    <ListTodo size={15} className="text-[#5E5CE6]" />
                    <span>{queue.active.length} aktiv</span>
                    {expanded ? <ChevronUp size={15} className="text-gray-400" /> : <ChevronDown size={15} className="text-gray-400" />}
                </button>
                {expanded && (
                    <div className="mt-2 w-[min(92vw,24rem)] rounded-2xl border border-[#2A2A40] bg-[#0B0B1A]/95 p-3 shadow-2xl backdrop-blur-md">
                        <div className="text-[10px] uppercase tracking-wide text-gray-500 mb-2">Globale Warteschlange</div>
                        <p className="text-[10px] text-gray-600 mb-2 leading-snug">
                            Reihenfolge nur Anzeige; laufende Anfragen werden nicht automatisch umgestellt.
                        </p>
                        {queue.active.length > 0 && (
                            <ul className="space-y-1.5 mb-2">
                                {queue.active.map((job, index) => (
                                    <li
                                        key={job.id}
                                        className="flex items-center gap-1 rounded-lg border border-[#2A2A40] bg-[#151525] px-2 py-2 text-sm text-gray-200"
                                    >
                                        <Loader2 size={14} className="animate-spin text-amber-400 shrink-0" />
                                        <span className="truncate flex-1 min-w-0">{job.label}</span>
                                        <span className="text-[10px] text-gray-500 shrink-0">#{job.folderId.slice(-6)}</span>
                                        <div className="flex items-center gap-0.5 shrink-0">
                                            <button
                                                type="button"
                                                aria-label="Nach oben"
                                                title="Nach oben"
                                                disabled={index === 0}
                                                onClick={() => handleMoveActive(index, -1)}
                                                className="p-1 rounded text-gray-400 hover:bg-[#1C1C33] hover:text-white disabled:opacity-30 disabled:pointer-events-none"
                                            >
                                                <ArrowUp size={14} />
                                            </button>
                                            <button
                                                type="button"
                                                aria-label="Nach unten"
                                                title="Nach unten"
                                                disabled={index >= queue.active.length - 1}
                                                onClick={() => handleMoveActive(index, 1)}
                                                className="p-1 rounded text-gray-400 hover:bg-[#1C1C33] hover:text-white disabled:opacity-30 disabled:pointer-events-none"
                                            >
                                                <ArrowDown size={14} />
                                            </button>
                                            <button
                                                type="button"
                                                aria-label="Abbrechen"
                                                title="Abbrechen"
                                                onClick={() => handleCancelActive(job.id)}
                                                className="p-1 rounded text-red-400 hover:bg-red-500/15 hover:text-red-300"
                                            >
                                                <X size={14} />
                                            </button>
                                        </div>
                                    </li>
                                ))}
                            </ul>
                        )}
                        {queue.recent.length > 0 && (
                            <ul className="space-y-1.5">
                                {queue.recent.slice(0, 8).map((job) => (
                                    <li
                                        key={job.id}
                                        title={!job.ok && job.errorMessage ? job.errorMessage : undefined}
                                        className={`flex flex-col gap-0.5 rounded-lg border px-2.5 py-2 text-sm ${job.ok ? "border-emerald-500/25 bg-emerald-500/10 text-emerald-200" : "border-red-500/25 bg-red-500/10 text-red-200"}`}
                                    >
                                        <div className="flex items-center gap-2 min-w-0">
                                            {job.ok ? <CheckCircle2 size={14} className="shrink-0" /> : <XCircle size={14} className="shrink-0" />}
                                            <span className="truncate flex-1">{job.label}</span>
                                            <span className="shrink-0 text-[10px] opacity-80">{job.ok ? "Fertig" : "Fehler"}</span>
                                            <button
                                                type="button"
                                                aria-label="Eintrag entfernen"
                                                title="Entfernen"
                                                onClick={() => dismissRecentJobById(job.id)}
                                                className="p-1 rounded text-gray-500 hover:bg-[#1C1C33] hover:text-gray-300 shrink-0"
                                            >
                                                <X size={12} />
                                            </button>
                                        </div>
                                        {!job.ok && job.errorMessage && (
                                            <p className="text-[10px] leading-snug text-red-300/90 break-words line-clamp-3 pl-[22px]">
                                                {job.errorMessage}
                                            </p>
                                        )}
                                    </li>
                                ))}
                            </ul>
                        )}
                    </div>
                )}
            </div>

            {/* Floating compact widget */}
            <div className="fixed bottom-6 right-4 z-[120] no-print rounded-xl border border-[#2A2A40] bg-[#0B0B1A]/95 px-3 py-2 text-xs text-gray-200 shadow-2xl backdrop-blur-md">
                <div className="flex items-center gap-2">
                    <Loader2 size={13} className={queue.active.length > 0 ? "animate-spin text-[#5E5CE6]" : "text-gray-500"} />
                    <span>{queue.active.length} laufend</span>
                    <span className="text-gray-500">|</span>
                    <span>{queue.recent.length} zuletzt</span>
                </div>
            </div>
        </>
    );
}
