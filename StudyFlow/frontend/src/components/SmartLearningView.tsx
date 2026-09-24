"use client";

import React, { useCallback, useMemo, useState } from "react";
import {
    BookOpen,
    BrainCircuit,
    CheckCircle2,
    ChevronDown,
    ChevronUp,
    HelpCircle,
    Layers,
    Loader2,
    Play,
    Sparkles,
} from "lucide-react";

export type SmartLearningChapter = {
    id: string;
    title: string;
    summary: string;
    key_points: string[];
};

export type SmartLearningContent = {
    title: string;
    readiness: number;
    source_hint?: string;
    chapters: SmartLearningChapter[];
    progress?: {
        completed_chapter_ids?: string[];
        practice_sessions?: number;
    };
};

type PracticeMode = "quiz" | "flashcards";

type Props = {
    content: SmartLearningContent;
    folderId: string;
    username: string;
    sessionId: string;
    busyPractice?: boolean;
    onContentUpdated?: (next: SmartLearningContent) => void;
    onStartPractice: (mode: PracticeMode) => void;
    onClose?: () => void;
};

function clampReadiness(n: number): number {
    if (!Number.isFinite(n)) return 0;
    return Math.max(0, Math.min(100, Math.round(n)));
}

export function normalizeSmartLearningContent(raw: unknown, fallbackTitle = "Smart Learning"): SmartLearningContent {
    let data: unknown = raw;
    if (typeof data === "string") {
        try {
            data = JSON.parse(data);
        } catch {
            data = null;
        }
    }
    if (!data || typeof data !== "object" || Array.isArray(data)) {
        return { title: fallbackTitle, readiness: 0, chapters: [], progress: { completed_chapter_ids: [], practice_sessions: 0 } };
    }
    const obj = data as Record<string, unknown>;
    const chaptersRaw = Array.isArray(obj.chapters) ? obj.chapters : [];
    const chapters: SmartLearningChapter[] = chaptersRaw
        .filter((ch): ch is Record<string, unknown> => !!ch && typeof ch === "object" && !Array.isArray(ch))
        .map((ch, i) => {
            const kps = Array.isArray(ch.key_points)
                ? ch.key_points.map((p) => String(p).trim()).filter(Boolean)
                : [];
            return {
                id: String(ch.id || `ch${i + 1}`).trim() || `ch${i + 1}`,
                title: String(ch.title || `Kapitel ${i + 1}`).trim(),
                summary: String(ch.summary || "").trim(),
                key_points: kps,
            };
        });
    const progressObj =
        obj.progress && typeof obj.progress === "object" && !Array.isArray(obj.progress)
            ? (obj.progress as Record<string, unknown>)
            : {};
    const completed = Array.isArray(progressObj.completed_chapter_ids)
        ? progressObj.completed_chapter_ids.map((x) => String(x))
        : [];
    return {
        title: String(obj.title || fallbackTitle).trim() || fallbackTitle,
        readiness: clampReadiness(Number(obj.readiness) || 0),
        source_hint: obj.source_hint ? String(obj.source_hint) : undefined,
        chapters,
        progress: {
            completed_chapter_ids: completed,
            practice_sessions: Math.max(0, Number(progressObj.practice_sessions) || 0),
        },
    };
}

function computeReadiness(content: SmartLearningContent): number {
    const chapters = Array.isArray(content.chapters) ? content.chapters : [];
    const completed = content.progress?.completed_chapter_ids || [];
    const sessions = Number(content.progress?.practice_sessions || 0);
    if (chapters.length === 0) return clampReadiness(content.readiness || 0);
    const base = (100 * new Set(completed).size) / chapters.length;
    return clampReadiness(base + Math.min(20, sessions * 5));
}

function formatApiDetail(detail: unknown): string {
    if (detail == null) return "";
    if (typeof detail === "string") return detail;
    if (typeof detail === "object") return JSON.stringify(detail);
    return String(detail);
}

export default function SmartLearningView({
    content,
    folderId,
    username,
    sessionId,
    busyPractice = false,
    onContentUpdated,
    onStartPractice,
    onClose,
}: Props) {
    const [local, setLocal] = useState<SmartLearningContent>(() =>
        normalizeSmartLearningContent(content)
    );
    const [openChapterId, setOpenChapterId] = useState<string | null>(
        () => normalizeSmartLearningContent(content).chapters?.[0]?.id || null
    );
    const [practiceMode, setPracticeMode] = useState<PracticeMode>("quiz");
    const [savingProgress, setSavingProgress] = useState(false);
    const [error, setError] = useState("");

    React.useEffect(() => {
        setLocal(normalizeSmartLearningContent(content));
    }, [content]);

    const readiness = useMemo(() => computeReadiness(local), [local]);
    const completedSet = useMemo(
        () => new Set(local.progress?.completed_chapter_ids || []),
        [local.progress?.completed_chapter_ids]
    );

    const persistProgress = useCallback(
        async (next: SmartLearningContent) => {
            setSavingProgress(true);
            setError("");
            try {
                const res = await fetch(
                    `/api/ai/smart-learning/progress?session_id=${encodeURIComponent(sessionId)}`,
                    {
                        method: "PATCH",
                        headers: {
                            "Content-Type": "application/json",
                            "X-Session-Id": sessionId,
                        },
                        body: JSON.stringify({
                            username,
                            folder_id: folderId,
                            completed_chapter_ids: next.progress?.completed_chapter_ids || [],
                            practice_sessions: next.progress?.practice_sessions || 0,
                            readiness: computeReadiness(next),
                        }),
                    }
                );
                const data = await res.json().catch(() => ({}));
                if (!res.ok) {
                    throw new Error(formatApiDetail(data.detail) || "Fortschritt konnte nicht gespeichert werden.");
                }
                const updated = normalizeSmartLearningContent(data.smart_learning || next, next.title);
                setLocal(updated);
                onContentUpdated?.(updated);
            } catch (e: any) {
                setError(e?.message || "Fortschritt speichern fehlgeschlagen.");
            } finally {
                setSavingProgress(false);
            }
        },
        [folderId, onContentUpdated, sessionId, username]
    );

    const toggleChapterDone = async (chapterId: string) => {
        const current = new Set(local.progress?.completed_chapter_ids || []);
        if (current.has(chapterId)) current.delete(chapterId);
        else current.add(chapterId);
        const next: SmartLearningContent = {
            ...local,
            progress: {
                completed_chapter_ids: Array.from(current),
                practice_sessions: local.progress?.practice_sessions || 0,
            },
        };
        next.readiness = computeReadiness(next);
        setLocal(next);
        await persistProgress(next);
    };

    // Session count is bumped by the parent after quiz/flashcards actually start
    // (avoids counting cancelled config modals — closer to Lumivara "session started").
    const handleStartPractice = () => {
        onStartPractice(practiceMode);
    };

    const chapters = Array.isArray(local.chapters) ? local.chapters : [];

    return (
        <div className="w-full max-w-3xl mx-auto space-y-6">
            <div className="rounded-2xl border border-[#2A2A40] bg-[#151525] p-6 relative overflow-hidden">
                <div className="absolute inset-0 pointer-events-none bg-[radial-gradient(circle_at_30%_20%,rgba(94,92,230,0.18),transparent_55%)]" />
                <div className="relative">
                    <div className="flex items-start justify-between gap-4">
                        <div>
                            <p className="text-[11px] uppercase tracking-[0.2em] text-[#8B89F0] mb-2 flex items-center gap-1.5">
                                <Sparkles size={12} />
                                Smart Learning
                            </p>
                            <h2 className="text-2xl font-semibold text-white leading-tight">
                                {local.title || "Smart Learning"}
                            </h2>
                            {local.source_hint ? (
                                <p className="text-sm text-gray-400 mt-2">{local.source_hint}</p>
                            ) : null}
                        </div>
                        {onClose ? (
                            <button
                                type="button"
                                onClick={onClose}
                                className="text-xs text-gray-400 hover:text-white px-3 py-1.5 rounded-lg border border-[#2A2A40]"
                            >
                                Schließen
                            </button>
                        ) : null}
                    </div>

                    <div className="mt-6 flex flex-wrap items-end gap-4">
                        <div className="min-w-[140px]">
                            <p className="text-xs text-gray-500 mb-1">Bereitschaft</p>
                            <p className="text-3xl font-semibold text-[#8B89F0]">{readiness}%</p>
                        </div>
                        <div className="flex-1 min-w-[180px]">
                            <div className="h-2 rounded-full bg-[#0B0B1A] overflow-hidden border border-[#2A2A40]">
                                <div
                                    className="h-full bg-[#5E5CE6] transition-all duration-300"
                                    style={{ width: `${readiness}%` }}
                                />
                            </div>
                            <p className="text-[11px] text-gray-500 mt-1">
                                {completedSet.size}/{chapters.length} Kapitel ·{" "}
                                {local.progress?.practice_sessions || 0} Übungen
                                {savingProgress ? " · speichern…" : ""}
                            </p>
                        </div>
                    </div>

                    <div className="mt-6 flex flex-wrap gap-2">
                        <button
                            type="button"
                            onClick={() => setPracticeMode("quiz")}
                            className={`px-3 py-1.5 rounded-lg text-sm border transition-colors ${
                                practiceMode === "quiz"
                                    ? "bg-yellow-500/15 text-yellow-300 border-yellow-500/30"
                                    : "bg-[#0B0B1A] text-gray-400 border-[#2A2A40]"
                            }`}
                        >
                            <span className="inline-flex items-center gap-1.5">
                                <HelpCircle size={14} /> Multiple Choice
                            </span>
                        </button>
                        <button
                            type="button"
                            onClick={() => setPracticeMode("flashcards")}
                            className={`px-3 py-1.5 rounded-lg text-sm border transition-colors ${
                                practiceMode === "flashcards"
                                    ? "bg-green-500/15 text-green-300 border-green-500/30"
                                    : "bg-[#0B0B1A] text-gray-400 border-[#2A2A40]"
                            }`}
                        >
                            <span className="inline-flex items-center gap-1.5">
                                <Layers size={14} /> Karteikarten
                            </span>
                        </button>
                    </div>

                    <button
                        type="button"
                        onClick={() => void handleStartPractice()}
                        disabled={busyPractice || chapters.length === 0}
                        className="mt-4 w-full sm:w-auto inline-flex items-center justify-center gap-2 px-5 py-3 rounded-xl bg-[#5E5CE6] hover:bg-[#4d4ac9] text-white font-semibold disabled:opacity-50 transition-colors"
                    >
                        {busyPractice ? <Loader2 size={18} className="animate-spin" /> : <Play size={18} />}
                        Jetzt lernen
                    </button>
                </div>
            </div>

            {error ? (
                <div className="rounded-xl border border-red-500/30 bg-red-500/10 text-red-300 text-sm px-4 py-3">
                    {error}
                </div>
            ) : null}

            <div className="space-y-3">
                <h3 className="text-sm font-semibold text-gray-300 flex items-center gap-2">
                    <BookOpen size={16} /> Kapitel
                </h3>
                {chapters.length === 0 ? (
                    <div className="rounded-xl border border-dashed border-[#2A2A40] bg-[#151525] p-6 text-center text-gray-400 text-sm">
                        Noch keine Kapitel. Lade PDF/YouTube-Material hoch und generiere Smart Learning erneut.
                    </div>
                ) : (
                    chapters.map((ch) => {
                        const open = openChapterId === ch.id;
                        const done = completedSet.has(ch.id);
                        return (
                            <div
                                key={ch.id}
                                className="rounded-xl border border-[#2A2A40] bg-[#151525] overflow-hidden"
                            >
                                <button
                                    type="button"
                                    onClick={() => setOpenChapterId(open ? null : ch.id)}
                                    className="w-full flex items-center gap-3 px-4 py-3 text-left hover:bg-[#1C1C33] transition-colors"
                                >
                                    <BrainCircuit size={16} className="text-[#8B89F0] shrink-0" />
                                    <span className="flex-1 text-sm font-medium text-white">{ch.title}</span>
                                    {done ? <CheckCircle2 size={16} className="text-green-400 shrink-0" /> : null}
                                    {open ? (
                                        <ChevronUp size={16} className="text-gray-500" />
                                    ) : (
                                        <ChevronDown size={16} className="text-gray-500" />
                                    )}
                                </button>
                                {open ? (
                                    <div className="px-4 pb-4 border-t border-[#2A2A40] pt-3 space-y-3">
                                        <p className="text-sm text-gray-300 whitespace-pre-wrap leading-relaxed">
                                            {ch.summary}
                                        </p>
                                        {Array.isArray(ch.key_points) && ch.key_points.length > 0 ? (
                                            <ul className="space-y-1.5">
                                                {ch.key_points.map((kp, i) => (
                                                    <li
                                                        key={`${ch.id}-kp-${i}`}
                                                        className="text-sm text-gray-400 flex gap-2"
                                                    >
                                                        <span className="text-[#5E5CE6]">•</span>
                                                        <span>{kp}</span>
                                                    </li>
                                                ))}
                                            </ul>
                                        ) : null}
                                        <button
                                            type="button"
                                            onClick={() => void toggleChapterDone(ch.id)}
                                            className={`text-xs px-3 py-1.5 rounded-lg border transition-colors ${
                                                done
                                                    ? "border-green-500/40 text-green-300 bg-green-500/10"
                                                    : "border-[#2A2A40] text-gray-300 hover:bg-[#1C1C33]"
                                            }`}
                                        >
                                            {done ? "Kapitel erledigt" : "Kapitel als erledigt markieren"}
                                        </button>
                                    </div>
                                ) : null}
                            </div>
                        );
                    })
                )}
            </div>
        </div>
    );
}
