"use client";

import React, { useCallback, useEffect, useMemo, useState } from "react";
import {
    BookOpen,
    BrainCircuit,
    CheckCircle2,
    ChevronRight,
    HelpCircle,
    Layers,
    Loader2,
    Play,
    Sparkles,
    XCircle,
} from "lucide-react";

export type SmartLearningCheck = {
    type: "mc" | "true_false";
    prompt: string;
    options: string[];
    answer: string;
    explanation?: string;
};

export type SmartLearningLesson = {
    id: string;
    teach: string;
    check: SmartLearningCheck;
};

export type SmartLearningChapter = {
    id: string;
    title: string;
    summary: string;
    key_points: string[];
    lessons: SmartLearningLesson[];
};

export type SmartLearningContent = {
    title: string;
    readiness: number;
    source_hint?: string;
    chapters: SmartLearningChapter[];
    progress?: {
        completed_chapter_ids?: string[];
        completed_lesson_ids?: string[];
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

function fallbackLessonsFromKeyPoints(chapterId: string, keyPoints: string[]): SmartLearningLesson[] {
    return keyPoints.slice(0, 4).map((kp, i) => ({
        id: `${chapterId}-l${i + 1}`,
        teach: kp,
        check: {
            type: "true_false" as const,
            prompt: `Stimmt das? ${kp}`,
            options: ["Richtig", "Falsch"],
            answer: "Richtig",
            explanation: kp,
        },
    }));
}

function normalizeLesson(raw: unknown, chapterId: string, i: number): SmartLearningLesson | null {
    if (!raw || typeof raw !== "object" || Array.isArray(raw)) return null;
    const o = raw as Record<string, unknown>;
    const teach = String(o.teach || "").trim();
    const checkRaw = o.check && typeof o.check === "object" && !Array.isArray(o.check) ? (o.check as Record<string, unknown>) : null;
    if (!teach || !checkRaw) return null;
    const options = Array.isArray(checkRaw.options)
        ? checkRaw.options.map((x) => String(x).trim()).filter(Boolean)
        : [];
    const answer = String(checkRaw.answer || "").trim();
    const prompt = String(checkRaw.prompt || "").trim();
    if (!prompt || !answer) return null;
    const opts = options.length >= 2 ? options : answer === "Richtig" || answer === "Falsch" ? ["Richtig", "Falsch"] : [answer, "Andere Antwort"];
    return {
        id: String(o.id || `${chapterId}-l${i + 1}`).trim() || `${chapterId}-l${i + 1}`,
        teach,
        check: {
            type: String(checkRaw.type || "mc") === "true_false" ? "true_false" : "mc",
            prompt,
            options: opts,
            answer,
            explanation: checkRaw.explanation ? String(checkRaw.explanation) : undefined,
        },
    };
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
        return {
            title: fallbackTitle,
            readiness: 0,
            chapters: [],
            progress: { completed_chapter_ids: [], completed_lesson_ids: [], practice_sessions: 0 },
        };
    }
    const obj = data as Record<string, unknown>;
    const chaptersRaw = Array.isArray(obj.chapters) ? obj.chapters : [];
    const chapters: SmartLearningChapter[] = chaptersRaw
        .filter((ch): ch is Record<string, unknown> => !!ch && typeof ch === "object" && !Array.isArray(ch))
        .map((ch, i) => {
            const cid = String(ch.id || `ch${i + 1}`).trim() || `ch${i + 1}`;
            const kps = Array.isArray(ch.key_points)
                ? ch.key_points.map((p) => String(p).trim()).filter(Boolean)
                : [];
            const lessonsRaw = Array.isArray(ch.lessons) ? ch.lessons : [];
            let lessons = lessonsRaw
                .map((l, li) => normalizeLesson(l, cid, li))
                .filter((l): l is SmartLearningLesson => !!l);
            if (lessons.length === 0 && kps.length > 0) {
                lessons = fallbackLessonsFromKeyPoints(cid, kps);
            }
            return {
                id: cid,
                title: String(ch.title || `Kapitel ${i + 1}`).trim(),
                summary: String(ch.summary || "").trim(),
                key_points: kps,
                lessons,
            };
        });
    const progressObj =
        obj.progress && typeof obj.progress === "object" && !Array.isArray(obj.progress)
            ? (obj.progress as Record<string, unknown>)
            : {};
    const completed = Array.isArray(progressObj.completed_chapter_ids)
        ? progressObj.completed_chapter_ids.map((x) => String(x))
        : [];
    const completedLessons = Array.isArray(progressObj.completed_lesson_ids)
        ? progressObj.completed_lesson_ids.map((x) => String(x))
        : [];
    return {
        title: String(obj.title || fallbackTitle).trim() || fallbackTitle,
        readiness: clampReadiness(Number(obj.readiness) || 0),
        source_hint: obj.source_hint ? String(obj.source_hint) : undefined,
        chapters,
        progress: {
            completed_chapter_ids: completed,
            completed_lesson_ids: completedLessons,
            practice_sessions: Math.max(0, Number(progressObj.practice_sessions) || 0),
        },
    };
}

function allLessonIds(content: SmartLearningContent): string[] {
    return content.chapters.flatMap((ch) => ch.lessons.map((l) => l.id));
}

function computeReadiness(content: SmartLearningContent): number {
    const lessons = allLessonIds(content);
    const completedLessons = new Set(content.progress?.completed_lesson_ids || []);
    const sessions = Number(content.progress?.practice_sessions || 0);
    if (lessons.length > 0) {
        const base = (100 * [...completedLessons].filter((id) => lessons.includes(id)).length) / lessons.length;
        return clampReadiness(base + Math.min(15, sessions * 3));
    }
    const chapters = content.chapters;
    const completed = content.progress?.completed_chapter_ids || [];
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

type FlatStep = { chapterId: string; chapterTitle: string; lesson: SmartLearningLesson };

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
    const [local, setLocal] = useState<SmartLearningContent>(() => normalizeSmartLearningContent(content));
    const [mode, setMode] = useState<"overview" | "learn">("overview");
    const [stepIndex, setStepIndex] = useState(0);
    const [stepPhase, setStepPhase] = useState<"teach" | "check">("teach");
    const [selected, setSelected] = useState<string | null>(null);
    const [savingProgress, setSavingProgress] = useState(false);
    const [error, setError] = useState("");

    useEffect(() => {
        setLocal(normalizeSmartLearningContent(content));
    }, [content]);

    const readiness = useMemo(() => computeReadiness(local), [local]);
    const completedLessonSet = useMemo(
        () => new Set(local.progress?.completed_lesson_ids || []),
        [local.progress?.completed_lesson_ids]
    );

    const flatSteps: FlatStep[] = useMemo(() => {
        const out: FlatStep[] = [];
        for (const ch of local.chapters) {
            for (const lesson of ch.lessons) {
                out.push({ chapterId: ch.id, chapterTitle: ch.title, lesson });
            }
        }
        return out;
    }, [local.chapters]);

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
                            completed_lesson_ids: next.progress?.completed_lesson_ids || [],
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

    const markLessonDone = async (lessonId: string, chapterId: string) => {
        const lessonsDone = new Set(local.progress?.completed_lesson_ids || []);
        lessonsDone.add(lessonId);
        const chapter = local.chapters.find((c) => c.id === chapterId);
        const chaptersDone = new Set(local.progress?.completed_chapter_ids || []);
        if (chapter && chapter.lessons.every((l) => lessonsDone.has(l.id))) {
            chaptersDone.add(chapterId);
        }
        const next: SmartLearningContent = {
            ...local,
            progress: {
                completed_chapter_ids: Array.from(chaptersDone),
                completed_lesson_ids: Array.from(lessonsDone),
                practice_sessions: local.progress?.practice_sessions || 0,
            },
        };
        next.readiness = computeReadiness(next);
        setLocal(next);
        await persistProgress(next);
    };

    const startLearn = () => {
        const firstIncomplete = flatSteps.findIndex((s) => !completedLessonSet.has(s.lesson.id));
        setStepIndex(firstIncomplete >= 0 ? firstIncomplete : 0);
        setStepPhase("teach");
        setSelected(null);
        setMode("learn");
    };

    const current = flatSteps[stepIndex];

    const advanceAfterCheck = async () => {
        if (current) {
            await markLessonDone(current.lesson.id, current.chapterId);
        }
        if (stepIndex + 1 >= flatSteps.length) {
            setMode("overview");
            setSelected(null);
            return;
        }
        setStepIndex((i) => i + 1);
        setStepPhase("teach");
        setSelected(null);
    };

    const chapters = local.chapters;

    if (mode === "learn" && current) {
        const check = current.lesson.check;
        const revealed = selected != null;
        return (
            <div className="w-full max-w-2xl mx-auto space-y-5">
                <div className="flex items-center justify-between text-xs text-gray-400">
                    <button type="button" onClick={() => setMode("overview")} className="hover:text-white">
                        ← Übersicht
                    </button>
                    <span>
                        Schritt {stepIndex + 1} / {flatSteps.length} · {current.chapterTitle}
                    </span>
                </div>
                <div className="h-1.5 rounded-full bg-[#0B0B1A] border border-[#2A2A40] overflow-hidden">
                    <div
                        className="h-full bg-[#5E5CE6] transition-all"
                        style={{ width: `${(stepIndex / Math.max(flatSteps.length, 1)) * 100}%` }}
                    />
                </div>

                {stepPhase === "teach" ? (
                    <div className="rounded-2xl border border-[#2A2A40] bg-[#151525] p-6 space-y-5">
                        <p className="text-[11px] uppercase tracking-widest text-[#8B89F0]">Erklären</p>
                        <p className="text-base text-white leading-relaxed whitespace-pre-wrap">{current.lesson.teach}</p>
                        <button
                            type="button"
                            onClick={() => {
                                setSelected(null);
                                setStepPhase("check");
                            }}
                            className="inline-flex items-center gap-2 px-5 py-3 rounded-xl bg-[#5E5CE6] hover:bg-[#4d4ac9] text-white font-semibold"
                        >
                            Verstanden — jetzt testen
                            <ChevronRight size={18} />
                        </button>
                    </div>
                ) : (
                    <div className="rounded-2xl border border-[#2A2A40] bg-[#151525] p-6 space-y-4">
                        <p className="text-[11px] uppercase tracking-widest text-amber-300/90">Check</p>
                        <p className="text-lg text-white font-medium">{check.prompt}</p>
                        <div className="space-y-2">
                            {check.options.map((opt, i) => {
                                const isSel = selected === opt;
                                const isOk = opt === check.answer;
                                let styles = "border-[#3B3B55] hover:bg-[#1C1C33] text-gray-300";
                                if (revealed) {
                                    styles = "border-[#2A2A40] opacity-60";
                                    if (isOk) styles = "border-green-500/50 bg-green-500/10 text-green-400 opacity-100";
                                    if (isSel && !isOk) styles = "border-red-500/50 bg-red-500/10 text-red-400 opacity-100";
                                }
                                return (
                                    <button
                                        key={i}
                                        type="button"
                                        disabled={revealed}
                                        onClick={() => setSelected(opt)}
                                        className={`w-full text-left p-3 rounded-xl border transition-all ${styles}`}
                                    >
                                        <span className="flex items-center justify-between gap-2">
                                            {opt}
                                            {revealed && isOk ? <CheckCircle2 size={16} /> : null}
                                            {revealed && isSel && !isOk ? <XCircle size={16} /> : null}
                                        </span>
                                    </button>
                                );
                            })}
                        </div>
                        {revealed ? (
                            <div className="space-y-3">
                                {check.explanation ? (
                                    <p className="text-sm text-gray-400 border border-[#2A2A40] rounded-xl px-3 py-2 bg-[#0B0B1A]">
                                        {check.explanation}
                                    </p>
                                ) : null}
                                <button
                                    type="button"
                                    onClick={() => void advanceAfterCheck()}
                                    disabled={savingProgress}
                                    className="inline-flex items-center gap-2 px-5 py-3 rounded-xl bg-[#5E5CE6] text-white font-semibold disabled:opacity-50"
                                >
                                    {savingProgress ? <Loader2 size={16} className="animate-spin" /> : null}
                                    {stepIndex + 1 >= flatSteps.length ? "Fertig" : "Weiter"}
                                    <ChevronRight size={18} />
                                </button>
                            </div>
                        ) : null}
                    </div>
                )}
                {error ? <p className="text-sm text-red-400">{error}</p> : null}
            </div>
        );
    }

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
                                <div className="h-full bg-[#5E5CE6] transition-all duration-300" style={{ width: `${readiness}%` }} />
                            </div>
                            <p className="text-[11px] text-gray-500 mt-1">
                                {completedLessonSet.size}/{flatSteps.length} Lektionen ·{" "}
                                {local.progress?.practice_sessions || 0} Extra-Übungen
                                {savingProgress ? " · speichern…" : ""}
                            </p>
                        </div>
                    </div>

                    <button
                        type="button"
                        onClick={startLearn}
                        disabled={busyPractice || flatSteps.length === 0}
                        className="mt-5 w-full sm:w-auto inline-flex items-center justify-center gap-2 px-5 py-3 rounded-xl bg-[#5E5CE6] hover:bg-[#4d4ac9] text-white font-semibold disabled:opacity-50 transition-colors"
                    >
                        <Play size={18} />
                        Jetzt lernen
                    </button>

                    <div className="mt-4 flex flex-wrap gap-2 items-center">
                        <span className="text-xs text-gray-500 mr-1">Vorhandenes öffnen:</span>
                        <button
                            type="button"
                            onClick={() => onStartPractice("quiz")}
                            className="px-3 py-1.5 rounded-lg text-sm border border-[#2A2A40] text-gray-300 hover:bg-[#1C1C33] inline-flex items-center gap-1.5"
                        >
                            <HelpCircle size={14} /> Quiz
                        </button>
                        <button
                            type="button"
                            onClick={() => onStartPractice("flashcards")}
                            className="px-3 py-1.5 rounded-lg text-sm border border-[#2A2A40] text-gray-300 hover:bg-[#1C1C33] inline-flex items-center gap-1.5"
                        >
                            <Layers size={14} /> Karteikarten
                        </button>
                    </div>
                </div>
            </div>

            {error ? (
                <div className="rounded-xl border border-red-500/30 bg-red-500/10 text-red-300 text-sm px-4 py-3">{error}</div>
            ) : null}

            <div className="space-y-3">
                <h3 className="text-sm font-semibold text-gray-300 flex items-center gap-2">
                    <BookOpen size={16} /> Kapitel
                </h3>
                {chapters.length === 0 ? (
                    <div className="rounded-xl border border-dashed border-[#2A2A40] bg-[#151525] p-6 text-center text-gray-400 text-sm">
                        Noch keine Kapitel. Material hochladen und Smart Learning erneut generieren.
                    </div>
                ) : (
                    chapters.map((ch) => {
                        const doneLessons = ch.lessons.filter((l) => completedLessonSet.has(l.id)).length;
                        const chapterDone = ch.lessons.length > 0 && doneLessons === ch.lessons.length;
                        return (
                            <div key={ch.id} className="rounded-xl border border-[#2A2A40] bg-[#151525] px-4 py-3">
                                <div className="flex items-center gap-3">
                                    <BrainCircuit size={16} className="text-[#8B89F0] shrink-0" />
                                    <div className="flex-1 min-w-0">
                                        <p className="text-sm font-medium text-white truncate">{ch.title}</p>
                                        <p className="text-[11px] text-gray-500">
                                            {doneLessons}/{ch.lessons.length || 0} Lektionen
                                            {ch.summary ? ` · ${ch.summary.slice(0, 80)}${ch.summary.length > 80 ? "…" : ""}` : ""}
                                        </p>
                                    </div>
                                    {chapterDone ? <CheckCircle2 size={16} className="text-green-400 shrink-0" /> : null}
                                </div>
                            </div>
                        );
                    })
                )}
            </div>
        </div>
    );
}
