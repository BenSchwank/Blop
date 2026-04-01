"use client";

import { useEffect, useState } from "react";
import { CheckCircle2, ChevronDown, ChevronUp, ListTodo, Loader2, XCircle } from "lucide-react";

type ActiveJob = { id: string; label: string; folderId: string; startedAt: number };
type RecentJob = { id: string; label: string; folderId: string; ok: boolean; finishedAt: number };
type QueueSnapshot = { updatedAt: number; active: ActiveJob[]; recent: RecentJob[] };

const EMPTY_QUEUE: QueueSnapshot = { updatedAt: 0, active: [], recent: [] };

export default function GlobalQueueStatus() {
    const [queue, setQueue] = useState<QueueSnapshot>(EMPTY_QUEUE);
    const [expanded, setExpanded] = useState(false);

    useEffect(() => {
        const readQueue = () => {
            try {
                const parsed = JSON.parse(localStorage.getItem("blop_global_queue") || "null");
                if (parsed && Array.isArray(parsed.active) && Array.isArray(parsed.recent)) {
                    setQueue({
                        updatedAt: Number(parsed.updatedAt || Date.now()),
                        active: parsed.active,
                        recent: parsed.recent.slice(0, 15),
                    });
                    return;
                }
            } catch {
                // ignore malformed localStorage values
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
                        {queue.active.length > 0 && (
                            <ul className="space-y-1.5 mb-2">
                                {queue.active.map((job) => (
                                    <li key={job.id} className="flex items-center gap-2 rounded-lg border border-[#2A2A40] bg-[#151525] px-2.5 py-2 text-sm text-gray-200">
                                        <Loader2 size={14} className="animate-spin text-amber-400 shrink-0" />
                                        <span className="truncate">{job.label}</span>
                                        <span className="ml-auto text-[10px] text-gray-500">#{job.folderId.slice(-6)}</span>
                                    </li>
                                ))}
                            </ul>
                        )}
                        {queue.recent.length > 0 && (
                            <ul className="space-y-1.5">
                                {queue.recent.slice(0, 8).map((job) => (
                                    <li key={job.id} className={`flex items-center gap-2 rounded-lg border px-2.5 py-2 text-sm ${job.ok ? "border-emerald-500/25 bg-emerald-500/10 text-emerald-200" : "border-red-500/25 bg-red-500/10 text-red-200"}`}>
                                        {job.ok ? <CheckCircle2 size={14} className="shrink-0" /> : <XCircle size={14} className="shrink-0" />}
                                        <span className="truncate">{job.label}</span>
                                        <span className="ml-auto text-[10px] opacity-80">{job.ok ? "Fertig" : "Fehler"}</span>
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

