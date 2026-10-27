"use client";

import React, { useCallback, useEffect, useMemo, useState } from "react";
import {
    BookOpen,
    BrainCircuit,
    CheckCircle2,
    ChevronRight,
    ClipboardList,
    Flame,
    Heart,
    HelpCircle,
    Layers,
    Loader2,
    Lock,
    PenLine,
    Play,
    Sparkles,
    Timer,
    XCircle,
} from "lucide-react";
import MathMarkdown, { answersMatch, parseClozePrompt } from "@/components/MathMarkdown";
import { awardXp } from "@/lib/xpClient";

const HEARTS_MAX = 5;
const DAILY_GOAL_XP = 50;

export type SmartLearningCheckType = "mc" | "true_false" | "cloze";

export type SmartLearningCheck = {
    type: SmartLearningCheckType;
    prompt: string;
    options: string[];
    answer: string;
    explanation?: string;
    /** Expected cloze answers (parallel to {{…}} slots). */
    blanks?: string[];
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

export type SmartLearningExamQuestion = {
    id: string;
    type: SmartLearningCheckType;
    prompt: string;
    options: string[];
    answer: string;
    explanation?: string;
    blanks?: string[];
};

export type SmartLearningExam = {
    title: string;
    time_limit_sec?: number;
    questions: SmartLearningExamQuestion[];
};

export type SmartLearningContent = {
    title: string;
    readiness: number;
    source_hint?: string;
    chapters: SmartLearningChapter[];
    exam?: SmartLearningExam | null;
    progress?: {
        completed_chapter_ids?: string[];
        completed_lesson_ids?: string[];
        practice_sessions?: number;
        exam_best_score?: number | null;
        exam_completed_at?: string | null;
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

function normalizeCheckType(raw: unknown): SmartLearningCheckType {
    const t = String(raw || "mc").trim().toLowerCase();
    if (t === "true_false" || t === "truefalse" || t === "tf") return "true_false";
    if (t === "cloze" || t === "fill" || t === "lueckentext" || t === "lückentext") return "cloze";
    return "mc";
}

function normalizeCheck(checkRaw: Record<string, unknown>): SmartLearningCheck | null {
    const prompt = String(checkRaw.prompt || "").trim();
    const answer = String(checkRaw.answer || "").trim();
    if (!prompt) return null;
    const type = normalizeCheckType(checkRaw.type);
    const blanks = Array.isArray(checkRaw.blanks)
        ? checkRaw.blanks.map((x) => String(x).trim()).filter(Boolean)
        : undefined;
    let options = Array.isArray(checkRaw.options)
        ? checkRaw.options.map((x) => String(x).trim()).filter(Boolean)
        : [];

    if (type === "cloze") {
        const parsed = parseClozePrompt(prompt, blanks);
        const resolvedBlanks = parsed.answers.length > 0 ? parsed.answers : blanks || (answer ? [answer] : []);
        if (resolvedBlanks.length === 0) return null;
        return {
            type: "cloze",
            prompt,
            options: [],
            answer: answer || resolvedBlanks.join(" | "),
            blanks: resolvedBlanks,
            explanation: checkRaw.explanation ? String(checkRaw.explanation) : undefined,
        };
    }

    if (!answer) return null;
    if (options.length < 2) {
        options =
            answer === "Richtig" || answer === "Falsch"
                ? ["Richtig", "Falsch"]
                : [answer, "Andere Antwort"];
    }
    return {
        type,
        prompt,
        options,
        answer,
        explanation: checkRaw.explanation ? String(checkRaw.explanation) : undefined,
    };
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
    const checkRaw =
        o.check && typeof o.check === "object" && !Array.isArray(o.check)
            ? (o.check as Record<string, unknown>)
            : null;
    if (!teach || !checkRaw) return null;
    const check = normalizeCheck(checkRaw);
    if (!check) return null;
    return {
        id: String(o.id || `${chapterId}-l${i + 1}`).trim() || `${chapterId}-l${i + 1}`,
        teach,
        check,
    };
}

function normalizeExam(raw: unknown): SmartLearningExam | null {
    if (!raw || typeof raw !== "object" || Array.isArray(raw)) return null;
    const o = raw as Record<string, unknown>;
    const questionsRaw = Array.isArray(o.questions) ? o.questions : [];
    const questions: SmartLearningExamQuestion[] = [];
    for (let i = 0; i < questionsRaw.length; i++) {
        const q = questionsRaw[i];
        if (!q || typeof q !== "object" || Array.isArray(q)) continue;
        const qr = q as Record<string, unknown>;
        const check = normalizeCheck(qr);
        if (!check) continue;
        questions.push({
            id: String(qr.id || `ex-q${i + 1}`).trim() || `ex-q${i + 1}`,
            type: check.type,
            prompt: check.prompt,
            options: check.options,
            answer: check.answer,
            blanks: check.blanks,
            explanation: check.explanation,
        });
    }
    if (questions.length === 0) return null;
    const limit = Number(o.time_limit_sec);
    return {
        title: String(o.title || "Probeklausur").trim() || "Probeklausur",
        time_limit_sec: Number.isFinite(limit) && limit > 0 ? Math.round(limit) : undefined,
        questions,
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
            exam: null,
            progress: {
                completed_chapter_ids: [],
                completed_lesson_ids: [],
                practice_sessions: 0,
                exam_best_score: null,
                exam_completed_at: null,
            },
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
    const best =
        progressObj.exam_best_score == null || progressObj.exam_best_score === ""
            ? null
            : Number(progressObj.exam_best_score);
    let exam = normalizeExam(obj.exam);
    if (!exam) {
        // Synthesize a short practice exam from existing lesson checks (older journeys).
        const qs: SmartLearningExamQuestion[] = [];
        for (const ch of chapters) {
            for (const les of ch.lessons) {
                qs.push({
                    id: `ex-${les.id}`,
                    type: les.check.type,
                    prompt: les.check.prompt,
                    options: les.check.options,
                    answer: les.check.answer,
                    blanks: les.check.blanks,
                    explanation: les.check.explanation,
                });
                if (qs.length >= 10) break;
            }
            if (qs.length >= 10) break;
        }
        if (qs.length > 0) {
            exam = { title: "Probeklausur", time_limit_sec: 900, questions: qs };
        }
    }

    return {
        title: String(obj.title || fallbackTitle).trim() || fallbackTitle,
        readiness: clampReadiness(Number(obj.readiness) || 0),
        source_hint: obj.source_hint ? String(obj.source_hint) : undefined,
        chapters,
        exam,
        progress: {
            completed_chapter_ids: completed,
            completed_lesson_ids: completedLessons,
            practice_sessions: Math.max(0, Number(progressObj.practice_sessions) || 0),
            exam_best_score: Number.isFinite(best as number) ? clampReadiness(best as number) : null,
            exam_completed_at: progressObj.exam_completed_at ? String(progressObj.exam_completed_at) : null,
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
    const examBonus = content.progress?.exam_best_score != null ? Math.min(10, Math.round((content.progress.exam_best_score || 0) / 20)) : 0;
    if (lessons.length > 0) {
        const base = (100 * [...completedLessons].filter((id) => lessons.includes(id)).length) / lessons.length;
        return clampReadiness(base + Math.min(15, sessions * 3) + examBonus);
    }
    const chapters = content.chapters;
    const completed = content.progress?.completed_chapter_ids || [];
    if (chapters.length === 0) return clampReadiness(content.readiness || 0);
    const base = (100 * new Set(completed).size) / chapters.length;
    return clampReadiness(base + Math.min(20, sessions * 5) + examBonus);
}

function formatApiDetail(detail: unknown): string {
    if (detail == null) return "";
    if (typeof detail === "string") return detail;
    if (typeof detail === "object") return JSON.stringify(detail);
    return String(detail);
}

type FlatStep = { chapterId: string; chapterTitle: string; lesson: SmartLearningLesson };

function ClozeInputs({
    prompt,
    blanks,
    values,
    onChange,
    disabled,
    showResult,
}: {
    prompt: string;
    blanks?: string[];
    values: string[];
    onChange: (next: string[]) => void;
    disabled?: boolean;
    showResult?: boolean;
}) {
    const { segments, answers } = useMemo(() => parseClozePrompt(prompt, blanks), [prompt, blanks]);
    return (
        <div className="text-base text-white leading-relaxed space-y-3">
            <p className="text-[11px] uppercase tracking-wide text-gray-500">Lückentext</p>
            <div className="rounded-xl border border-[#4A4E58] bg-[#2A2E38] px-4 py-3 leading-8">
                {segments.map((seg, i) => (
                    <React.Fragment key={i}>
                        {seg ? <MathMarkdown compact className="inline [&_p]:inline">{seg}</MathMarkdown> : null}
                        {i < answers.length ? (
                            <span className="inline-flex items-center mx-1 align-middle">
                                <input
                                    type="text"
                                    value={values[i] ?? ""}
                                    disabled={disabled}
                                    onChange={(e) => {
                                        const next = [...values];
                                        while (next.length < answers.length) next.push("");
                                        next[i] = e.target.value;
                                        onChange(next);
                                    }}
                                    className={`min-w-[6rem] max-w-[12rem] mx-0.5 px-2 py-1 rounded-lg border text-sm text-white bg-[#1A1916] focus:outline-none focus:ring-2 focus:ring-[#5B9DFF]/40 ${
                                        showResult
                                            ? answersMatch(answers[i], values[i] || "")
                                                ? "border-green-500/60"
                                                : "border-red-500/60"
                                            : "border-[#4A4E58]"
                                    }`}
                                    placeholder={`Lücke ${i + 1}`}
                                    aria-label={`Lücke ${i + 1}`}
                                />
                            </span>
                        ) : null}
                    </React.Fragment>
                ))}
            </div>
            {showResult ? (
                <p className="text-xs text-gray-400">
                    Erwartet:{" "}
                    {answers.map((a, i) => (
                        <span key={i} className="inline-block mr-2">
                            <MathMarkdown compact>{a}</MathMarkdown>
                        </span>
                    ))}
                </p>
            ) : null}
        </div>
    );
}

function gradeCloze(prompt: string, blanks: string[] | undefined, values: string[]): boolean {
    const { answers } = parseClozePrompt(prompt, blanks);
    if (answers.length === 0) return false;
    return answers.every((a, i) => answersMatch(a, values[i] || ""));
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
    const [local, setLocal] = useState<SmartLearningContent>(() => normalizeSmartLearningContent(content));
    const [mode, setMode] = useState<"overview" | "learn" | "exam" | "exam_result">("overview");
    const [stepIndex, setStepIndex] = useState(0);
    const [stepPhase, setStepPhase] = useState<"teach" | "check">("teach");
    const [selected, setSelected] = useState<string | null>(null);
    const [clozeValues, setClozeValues] = useState<string[]>([]);
    const [clozeSubmitted, setClozeSubmitted] = useState(false);
    const [savingProgress, setSavingProgress] = useState(false);
    const [error, setError] = useState("");

    // Exam state
    const [examIndex, setExamIndex] = useState(0);
    const [examSelected, setExamSelected] = useState<string | null>(null);
    const [examCloze, setExamCloze] = useState<string[]>([]);
    const [examClozeDone, setExamClozeDone] = useState(false);
    const [examAnswers, setExamAnswers] = useState<Record<number, { correct: boolean; given: string }>>({});
    const [examSecondsLeft, setExamSecondsLeft] = useState<number | null>(null);
    const [lastExamScore, setLastExamScore] = useState<number | null>(null);
    const examFinishingRef = React.useRef(false);
    const [hearts, setHearts] = useState(HEARTS_MAX);
    const [failOpen, setFailOpen] = useState(false);
    const [celebrate, setCelebrate] = useState<{ title: string; xp: number } | null>(null);
    const [hudXp, setHudXp] = useState(0);
    const [hudStreak, setHudStreak] = useState(0);
    const [dailyXp, setDailyXp] = useState(0);

    useEffect(() => {
        setLocal(normalizeSmartLearningContent(content));
    }, [content]);

    useEffect(() => {
        const loadMe = async () => {
            try {
                const res = await fetch(`/api/me?session_id=${encodeURIComponent(sessionId)}&username=${encodeURIComponent(username)}`, {
                    headers: { "X-Session-Id": sessionId },
                });
                if (!res.ok) return;
                const data = await res.json();
                setHudXp(Number(data.xp) || 0);
                setHudStreak(Number(data.streak_days) || 0);
                setDailyXp(Number(data.daily_xp) || 0);
            } catch {
                /* ignore */
            }
        };
        if (username) void loadMe();
    }, [sessionId, username]);

    const grantXp = async (amount: number, reason: string) => {
        const data = await awardXp(amount, reason);
        if (data?.xp != null) setHudXp(data.xp);
        if (data?.streak_days != null) setHudStreak(data.streak_days);
        if (data?.daily_xp != null) setDailyXp(data.daily_xp);
        return data;
    };

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

    const exam = local.exam;

    useEffect(() => {
        if (mode !== "exam" || examSecondsLeft == null) return;
        if (examSecondsLeft <= 0) return;
        const t = window.setTimeout(() => setExamSecondsLeft((s) => (s == null ? s : Math.max(0, s - 1))), 1000);
        return () => clearTimeout(t);
    }, [mode, examSecondsLeft]);

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
                            exam_best_score: next.progress?.exam_best_score ?? null,
                            exam_completed_at: next.progress?.exam_completed_at ?? null,
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
                ...local.progress,
                completed_chapter_ids: Array.from(chaptersDone),
                completed_lesson_ids: Array.from(lessonsDone),
                practice_sessions: local.progress?.practice_sessions || 0,
                exam_best_score: local.progress?.exam_best_score ?? null,
                exam_completed_at: local.progress?.exam_completed_at ?? null,
            },
        };
        next.readiness = computeReadiness(next);
        setLocal(next);
        await persistProgress(next);
    };

    const beginLessonAt = (index: number) => {
        setStepIndex(Math.max(0, index));
        setStepPhase("teach");
        setSelected(null);
        setClozeValues([]);
        setClozeSubmitted(false);
        setHearts(HEARTS_MAX);
        setFailOpen(false);
        setCelebrate(null);
        setMode("learn");
    };

    const startLearn = () => {
        const firstIncomplete = flatSteps.findIndex((s) => !completedLessonSet.has(s.lesson.id));
        beginLessonAt(firstIncomplete >= 0 ? firstIncomplete : 0);
    };

    const startChapter = (chapterId: string) => {
        const idx = flatSteps.findIndex((s) => s.chapterId === chapterId && !completedLessonSet.has(s.lesson.id));
        const fallback = flatSteps.findIndex((s) => s.chapterId === chapterId);
        beginLessonAt(idx >= 0 ? idx : Math.max(0, fallback));
    };

    const startExam = () => {
        if (!exam || exam.questions.length === 0) return;
        setExamIndex(0);
        setExamSelected(null);
        setExamCloze([]);
        setExamClozeDone(false);
        setExamAnswers({});
        setLastExamScore(null);
        examFinishingRef.current = false;
        setExamSecondsLeft(exam.time_limit_sec ?? null);
        setMode("exam");
    };

    const finishExam = useCallback(
        async (answers: Record<number, { correct: boolean; given: string }>) => {
            if (!exam || examFinishingRef.current) return;
            examFinishingRef.current = true;
            const total = exam.questions.length;
            const correct = Object.values(answers).filter((a) => a.correct).length;
            const score = total > 0 ? clampReadiness((100 * correct) / total) : 0;
            setLastExamScore(score);
            const prevBest = local.progress?.exam_best_score;
            const best = prevBest == null ? score : Math.max(prevBest, score);
            const next: SmartLearningContent = {
                ...local,
                progress: {
                    ...local.progress,
                    completed_chapter_ids: local.progress?.completed_chapter_ids || [],
                    completed_lesson_ids: local.progress?.completed_lesson_ids || [],
                    practice_sessions: (local.progress?.practice_sessions || 0) + 1,
                    exam_best_score: best,
                    exam_completed_at: new Date().toISOString(),
                },
            };
            next.readiness = computeReadiness(next);
            setLocal(next);
            setMode("exam_result");
            setExamSecondsLeft(null);
            await persistProgress(next);
            void grantXp(50, "exam");
        },
        [exam, local, persistProgress]
    );

    useEffect(() => {
        if (mode === "exam" && examSecondsLeft === 0) {
            void finishExam(examAnswers);
        }
        // Only fire when timer hits zero
        // eslint-disable-next-line react-hooks/exhaustive-deps
    }, [examSecondsLeft, mode]);

    const current = flatSteps[stepIndex];

    const chapters = local.chapters;

    const renderMcOptions = (
        check: { options: string[]; answer: string },
        selectedOpt: string | null,
        onPick: (opt: string) => void
    ) => {
        const revealed = selectedOpt != null;
        return (
            <div className="space-y-2.5">
                {check.options.map((opt, i) => {
                    const isSel = selectedOpt === opt;
                    const isOk = opt === check.answer;
                    let styles = "border-[#4A4E58] hover:bg-[#3A3D45] text-gray-200 min-h-[48px]";
                    if (revealed) {
                        styles = "border-[#45484F] opacity-60 min-h-[48px]";
                        if (isOk) styles = "border-green-500/50 bg-green-500/10 text-green-300 opacity-100 min-h-[48px]";
                        if (isSel && !isOk) styles = "border-red-500/50 bg-red-500/10 text-red-300 opacity-100 min-h-[48px]";
                    }
                    return (
                        <button
                            key={i}
                            type="button"
                            disabled={revealed}
                            onClick={() => onPick(opt)}
                            className={`w-full text-left px-4 py-3.5 rounded-xl border transition-colors ${styles}`}
                        >
                            <span className="flex items-center justify-between gap-3">
                                <MathMarkdown compact className="flex-1 text-left [&_p]:my-0">
                                    {opt}
                                </MathMarkdown>
                                {revealed && isOk ? <CheckCircle2 size={18} className="shrink-0 text-green-400" /> : null}
                                {revealed && isSel && !isOk ? <XCircle size={18} className="shrink-0 text-red-400" /> : null}
                            </span>
                        </button>
                    );
                })}
            </div>
        );
    };

    const spendHeart = () => {
        setHearts((h) => {
            const n = h - 1;
            if (n <= 0) setFailOpen(true);
            return Math.max(0, n);
        });
    };

    const hudBar = (
        <div className="flex items-center justify-between gap-3 text-xs text-gray-400">
            <button type="button" onClick={() => setMode("overview")} className="hover:text-white">
                ← Übersicht
            </button>
            <div className="flex items-center gap-2">
                <span className="inline-flex items-center gap-0.5 text-red-400">
                    {Array.from({ length: HEARTS_MAX }).map((_, i) => (
                        <Heart key={i} size={14} className={i < hearts ? "fill-red-400" : "opacity-25"} />
                    ))}
                </span>
                <span className="inline-flex items-center gap-1 text-[#9EC5FF]">
                    <Sparkles size={12} /> {hudXp}
                </span>
                <span className="inline-flex items-center gap-1 text-amber-200">
                    <Flame size={12} /> {hudStreak}
                </span>
            </div>
        </div>
    );

    /* ——— Learn mode ——— */
    if (mode === "learn" && current) {
        const check = current.lesson.check;
        const isCloze = check.type === "cloze";
        const clozeOk = isCloze && clozeSubmitted ? gradeCloze(check.prompt, check.blanks, clozeValues) : false;
        const mcOk = !isCloze && selected != null && selected === check.answer;
        const correct = isCloze ? clozeOk : mcOk;
        const revealed = isCloze ? clozeSubmitted : selected != null;

        const onCorrectContinue = async () => {
            await markLessonDone(current.lesson.id, current.chapterId);
            await grantXp(25, "lesson");
            setCelebrate({
                title: stepIndex + 1 >= flatSteps.length ? "Lernreise abgeschlossen" : "Lektion geschafft",
                xp: 25,
            });
        };

        return (
            <div className="w-full max-w-2xl mx-auto space-y-5 pb-8">
                {hudBar}
                <div className="flex items-center justify-between text-xs text-gray-500">
                    <span>
                        Schritt {stepIndex + 1} / {flatSteps.length} · {current.chapterTitle}
                    </span>
                </div>
                <div className="h-1.5 rounded-full bg-[#23252A] border border-[#45484F] overflow-hidden">
                    <div
                        className="h-full bg-[#5B9DFF] transition-all"
                        style={{ width: `${(stepIndex / Math.max(flatSteps.length, 1)) * 100}%` }}
                    />
                </div>

                {stepPhase === "teach" ? (
                    <div className="rounded-2xl border border-[#2A3140] bg-[#1A1F2A] p-6 sm:p-7 space-y-5 shadow-lg">
                        <div className="flex items-center gap-2">
                            <span className="inline-flex items-center gap-1.5 text-[11px] uppercase tracking-widest text-[#5B9DFF] font-semibold">
                                <BookOpen size={12} /> Erklären
                            </span>
                        </div>
                        <MathMarkdown className="text-white">{current.lesson.teach}</MathMarkdown>
                        <button
                            type="button"
                            onClick={() => {
                                setSelected(null);
                                setClozeValues([]);
                                setClozeSubmitted(false);
                                setStepPhase("check");
                            }}
                            className="inline-flex items-center gap-2 px-5 py-3.5 rounded-xl bg-[#5B9DFF] hover:bg-[#4A8AE6] text-white font-semibold min-h-[48px]"
                        >
                            Verstanden — jetzt testen
                            <ChevronRight size={18} />
                        </button>
                    </div>
                ) : (
                    <div className="rounded-2xl border border-[#2A3140] bg-[#1A1F2A] p-6 sm:p-7 space-y-4 shadow-lg">
                        <div className="flex items-center gap-2">
                            <span className="inline-flex items-center gap-1.5 text-[11px] uppercase tracking-widest text-amber-300/90 font-semibold">
                                {isCloze ? <PenLine size={12} /> : <HelpCircle size={12} />}
                                {isCloze ? "Lückentext" : "Prüfen"}
                            </span>
                        </div>

                        {isCloze ? (
                            <ClozeInputs
                                prompt={check.prompt}
                                blanks={check.blanks}
                                values={clozeValues}
                                onChange={setClozeValues}
                                disabled={clozeSubmitted && clozeOk}
                                showResult={clozeSubmitted && clozeOk}
                            />
                        ) : (
                            <>
                                <MathMarkdown className="text-lg font-medium text-white">{check.prompt}</MathMarkdown>
                                {renderMcOptions(check, selected, (opt) => {
                                    if (mcOk) return;
                                    setSelected(opt);
                                    if (opt === check.answer) {
                                        void grantXp(10, "check");
                                    } else {
                                        spendHeart();
                                    }
                                })}
                            </>
                        )}

                        {isCloze && !clozeSubmitted ? (
                            <button
                                type="button"
                                onClick={() => {
                                    setClozeSubmitted(true);
                                    const ok = gradeCloze(check.prompt, check.blanks, clozeValues);
                                    if (ok) void grantXp(5, "cloze");
                                    else spendHeart();
                                }}
                                disabled={clozeValues.every((v) => !String(v || "").trim())}
                                className="inline-flex items-center gap-2 px-5 py-3.5 rounded-xl bg-[#5B9DFF] text-white font-semibold disabled:opacity-50 min-h-[48px]"
                            >
                                Antwort prüfen
                            </button>
                        ) : null}

                        {revealed ? (
                            <div className="space-y-3 pt-1">
                                <div
                                    className={`rounded-xl px-3 py-2 text-sm border ${
                                        correct
                                            ? "border-green-500/40 bg-green-500/10 text-green-300"
                                            : "border-red-500/40 bg-red-500/10 text-red-300"
                                    }`}
                                >
                                    {correct
                                        ? "Richtig — gut gemacht."
                                        : hearts > 0
                                          ? `Leider falsch — noch ${hearts} ${hearts === 1 ? "Herz" : "Herzen"}.`
                                          : "Keine Herzen mehr."}
                                </div>
                                {correct && check.explanation ? (
                                    <div className="text-sm text-gray-300 border border-[#2A3140] rounded-xl px-4 py-3 bg-[#12141A]">
                                        <p className="text-[11px] uppercase tracking-wide text-gray-500 mb-1.5">Erklärung</p>
                                        <MathMarkdown compact>{check.explanation}</MathMarkdown>
                                    </div>
                                ) : null}
                                {correct ? (
                                    <button
                                        type="button"
                                        onClick={() => void onCorrectContinue()}
                                        disabled={savingProgress}
                                        className="inline-flex items-center gap-2 px-5 py-3.5 rounded-xl bg-[#5B9DFF] text-white font-semibold disabled:opacity-50 min-h-[48px]"
                                    >
                                        {savingProgress ? <Loader2 size={16} className="animate-spin" /> : null}
                                        {stepIndex + 1 >= flatSteps.length ? "Fertig" : "Weiter"}
                                        <ChevronRight size={18} />
                                    </button>
                                ) : hearts > 0 ? (
                                    <button
                                        type="button"
                                        onClick={() => {
                                            setSelected(null);
                                            setClozeSubmitted(false);
                                        }}
                                        className="inline-flex items-center gap-2 px-5 py-3 rounded-xl border border-[#2A3140] text-gray-200 hover:bg-[#243044] min-h-[48px]"
                                    >
                                        Nochmal versuchen
                                    </button>
                                ) : null}
                            </div>
                        ) : null}
                    </div>
                )}
                {error ? <p className="text-sm text-red-400">{error}</p> : null}

                {failOpen ? (
                    <div className="fixed inset-0 z-[140] bg-black/70 flex items-center justify-center p-4">
                        <div className="w-full max-w-sm rounded-2xl border border-[#2A3140] bg-[#12141A] p-6 text-center space-y-4">
                            <Heart className="mx-auto text-red-400" />
                            <h3 className="text-lg font-semibold text-white">Keine Herzen mehr</h3>
                            <p className="text-sm text-gray-400">Die Lektion bricht ab. Starte sie neu, wenn du bereit bist.</p>
                            <div className="flex gap-2">
                                <button type="button" onClick={() => setMode("overview")} className="flex-1 py-2.5 rounded-xl border border-[#2A3140] text-gray-300">
                                    Übersicht
                                </button>
                                <button
                                    type="button"
                                    onClick={() => beginLessonAt(stepIndex)}
                                    className="flex-1 py-2.5 rounded-xl bg-[#5B9DFF] text-white font-semibold"
                                >
                                    Nochmal
                                </button>
                            </div>
                        </div>
                    </div>
                ) : null}

                {celebrate ? (
                    <div className="fixed inset-0 z-[140] bg-black/70 flex items-center justify-center p-4">
                        <div className="w-full max-w-sm rounded-2xl border border-[#5B9DFF]/40 bg-[#12141A] p-6 text-center space-y-3 animate-in zoom-in duration-200">
                            <Sparkles className="mx-auto text-[#5B9DFF]" size={32} />
                            <h3 className="text-lg font-semibold text-white">{celebrate.title}</h3>
                            <p className="text-[#9EC5FF] font-medium">+{celebrate.xp} XP</p>
                            <p className="text-xs text-gray-500">Streak {hudStreak} Tage</p>
                            <button
                                type="button"
                                onClick={() => {
                                    const last = stepIndex + 1 >= flatSteps.length;
                                    setCelebrate(null);
                                    if (last) {
                                        setMode("overview");
                                        return;
                                    }
                                    setStepIndex((i) => i + 1);
                                    setStepPhase("teach");
                                    setSelected(null);
                                    setClozeValues([]);
                                    setClozeSubmitted(false);
                                    setHearts(HEARTS_MAX);
                                }}
                                className="w-full py-2.5 rounded-xl bg-[#5B9DFF] text-white font-semibold"
                            >
                                Weiter
                            </button>
                        </div>
                    </div>
                ) : null}
            </div>
        );
    }

    /* ——— Exam result ——— */
    if (mode === "exam_result" && exam) {
        const score = lastExamScore ?? 0;
        return (
            <div className="w-full max-w-xl mx-auto space-y-6 pb-10">
                <div className="rounded-2xl border border-[#2A3140] bg-[#1A1F2A] p-8 text-center space-y-3">
                    <p className="text-[11px] uppercase tracking-widest text-[#5B9DFF]">Probeklausur</p>
                    <p className="text-5xl font-semibold text-white">{score}%</p>
                    <p className="text-[#9EC5FF] font-medium">+50 XP</p>
                    <p className="text-xs text-gray-500">Streak {hudStreak} Tage</p>
                    <p className="text-sm text-gray-400">
                        {Object.values(examAnswers).filter((a) => a.correct).length} / {exam.questions.length} richtig
                        {local.progress?.exam_best_score != null
                            ? ` · Bestwert ${local.progress.exam_best_score}%`
                            : ""}
                    </p>
                    <div className="h-2 rounded-full bg-[#12141A] overflow-hidden border border-[#2A3140]">
                        <div className="h-full bg-[#5B9DFF]" style={{ width: `${score}%` }} />
                    </div>
                </div>
                <div className="flex flex-wrap gap-3 justify-center">
                    <button
                        type="button"
                        onClick={startExam}
                        className="px-5 py-3 rounded-xl bg-[#5B9DFF] text-white font-semibold"
                    >
                        Nochmal versuchen
                    </button>
                    <button
                        type="button"
                        onClick={() => setMode("overview")}
                        className="px-5 py-3 rounded-xl border border-[#2A3140] text-gray-300 hover:bg-[#243044]"
                    >
                        Zur Übersicht
                    </button>
                </div>
            </div>
        );
    }

    /* ——— Exam mode ——— */
    if (mode === "exam" && exam) {
        const q = exam.questions[examIndex];
        if (!q) {
            return (
                <div className="text-center text-gray-400 py-8">
                    Keine Fragen.
                    <button type="button" className="block mx-auto mt-3 text-[#5B9DFF]" onClick={() => setMode("overview")}>
                        Zurück
                    </button>
                </div>
            );
        }
        const isCloze = q.type === "cloze";
        const revealed = isCloze ? examClozeDone : examSelected != null;
        const mm = Math.floor((examSecondsLeft ?? 0) / 60);
        const ss = String((examSecondsLeft ?? 0) % 60).padStart(2, "0");

        const recordAndAdvance = (correct: boolean, given: string) => {
            const nextAnswers = { ...examAnswers, [examIndex]: { correct, given } };
            setExamAnswers(nextAnswers);
            if (examIndex + 1 >= exam.questions.length) {
                void finishExam(nextAnswers);
                return;
            }
            setExamIndex((i) => i + 1);
            setExamSelected(null);
            setExamCloze([]);
            setExamClozeDone(false);
        };

        return (
            <div className="w-full max-w-2xl mx-auto space-y-5 pb-10">
                <div className="flex items-center justify-between text-xs text-gray-400 gap-3">
                    <button type="button" onClick={() => setMode("overview")} className="hover:text-white">
                        ← Abbrechen
                    </button>
                    <span className="truncate">{exam.title}</span>
                    {examSecondsLeft != null ? (
                        <span className="inline-flex items-center gap-1 text-[#5B9DFF] shrink-0">
                            <Timer size={12} /> {mm}:{ss}
                        </span>
                    ) : (
                        <span>
                            {examIndex + 1}/{exam.questions.length}
                        </span>
                    )}
                </div>
                <div className="h-1.5 rounded-full bg-[#23252A] border border-[#45484F] overflow-hidden">
                    <div
                        className="h-full bg-[#5B9DFF] transition-all"
                        style={{ width: `${(examIndex / Math.max(exam.questions.length, 1)) * 100}%` }}
                    />
                </div>

                <div className="rounded-2xl border border-[#2A3140] bg-[#1A1F2A] p-6 space-y-4">
                    <p className="text-[11px] uppercase tracking-widest text-gray-500">
                        Frage {examIndex + 1} / {exam.questions.length}
                        {isCloze ? " · Lückentext" : ""}
                    </p>

                    {isCloze ? (
                        <ClozeInputs
                            prompt={q.prompt}
                            blanks={q.blanks}
                            values={examCloze}
                            onChange={setExamCloze}
                            disabled={examClozeDone}
                            showResult={examClozeDone}
                        />
                    ) : (
                        <>
                            <MathMarkdown className="text-lg font-medium text-white">{q.prompt}</MathMarkdown>
                            {renderMcOptions(q, examSelected, (opt) => {
                                if (examSelected) return;
                                setExamSelected(opt);
                            })}
                        </>
                    )}

                    {isCloze && !examClozeDone ? (
                        <button
                            type="button"
                            onClick={() => setExamClozeDone(true)}
                            disabled={examCloze.every((v) => !String(v || "").trim())}
                            className="px-5 py-3 rounded-xl bg-[#5B9DFF] text-white font-semibold disabled:opacity-50"
                        >
                            Antwort prüfen
                        </button>
                    ) : null}

                    {revealed ? (
                        <div className="space-y-3">
                            {q.explanation ? (
                                <div className="text-sm text-gray-300 border border-[#2A3140] rounded-xl px-4 py-3 bg-[#12141A]">
                                    <MathMarkdown compact>{q.explanation}</MathMarkdown>
                                </div>
                            ) : null}
                            <button
                                type="button"
                                onClick={() => {
                                    if (isCloze) {
                                        recordAndAdvance(gradeCloze(q.prompt, q.blanks, examCloze), examCloze.join(" | "));
                                    } else {
                                        recordAndAdvance(examSelected === q.answer, examSelected || "");
                                    }
                                }}
                                className="inline-flex items-center gap-2 px-5 py-3 rounded-xl bg-[#5B9DFF] text-white font-semibold"
                            >
                                {examIndex + 1 >= exam.questions.length ? "Ergebnis" : "Weiter"}
                                <ChevronRight size={18} />
                            </button>
                        </div>
                    ) : null}
                </div>
            </div>
        );
    }

    /* ——— Overview ——— */
    return (
        <div className="w-full max-w-3xl mx-auto space-y-6 pb-8">
            <div className="rounded-2xl border border-[#2A3140] bg-[#1A1F2A] p-6 relative overflow-hidden">
                <div className="absolute inset-0 pointer-events-none bg-[radial-gradient(circle_at_30%_20%,rgba(91,157,255,0.14),transparent_55%)]" />
                <div className="relative">
                    <div className="flex items-start justify-between gap-4">
                        <div>
                            <p className="text-[11px] uppercase tracking-[0.2em] text-[#5B9DFF] mb-2 flex items-center gap-1.5">
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
                                className="text-xs text-gray-400 hover:text-white px-3 py-1.5 rounded-lg border border-[#2A3140]"
                            >
                                Schließen
                            </button>
                        ) : null}
                    </div>

                    <div className="mt-4 flex flex-wrap gap-2 text-xs">
                        <span className="inline-flex items-center gap-1 px-2 py-1 rounded-lg bg-[#5B9DFF]/10 text-[#9EC5FF]">
                            <Sparkles size={12} /> {hudXp.toLocaleString()} XP
                        </span>
                        <span className="inline-flex items-center gap-1 px-2 py-1 rounded-lg bg-amber-500/10 text-amber-200">
                            <Flame size={12} /> {hudStreak} Tage Streak
                        </span>
                        <span className="inline-flex items-center gap-1.5 px-2 py-1 rounded-lg border border-[#2A3140] text-gray-400">
                            Tagesziel {dailyXp}/{DAILY_GOAL_XP} XP
                            <span className="w-16 h-1.5 rounded-full bg-[#12141A] overflow-hidden">
                                <span className="block h-full bg-[#5B9DFF]" style={{ width: `${Math.min(100, Math.round((100 * dailyXp) / DAILY_GOAL_XP))}%` }} />
                            </span>
                        </span>
                    </div>

                    <div className="mt-6 flex flex-wrap items-end gap-4">
                        <div className="min-w-[140px]">
                            <p className="text-xs text-gray-500 mb-1">Bereitschaft</p>
                            <p className="text-3xl font-semibold text-[#5B9DFF]">{readiness}%</p>
                        </div>
                        <div className="flex-1 min-w-[180px]">
                            <div className="h-2 rounded-full bg-[#12141A] overflow-hidden border border-[#2A3140]">
                                <div className="h-full bg-[#5B9DFF] transition-all duration-300" style={{ width: `${readiness}%` }} />
                            </div>
                            <p className="text-[11px] text-gray-500 mt-1">
                                {completedLessonSet.size}/{flatSteps.length} Lektionen ·{" "}
                                {local.progress?.practice_sessions || 0} Extra-Übungen
                                {local.progress?.exam_best_score != null
                                    ? ` · Klausur-Best ${local.progress.exam_best_score}%`
                                    : ""}
                                {savingProgress ? " · speichern…" : ""}
                            </p>
                        </div>
                    </div>

                    <div className="mt-5 flex flex-wrap gap-2">
                        <button
                            type="button"
                            onClick={startLearn}
                            disabled={busyPractice || flatSteps.length === 0}
                            className="inline-flex items-center justify-center gap-2 px-5 py-3 rounded-xl bg-[#5B9DFF] hover:bg-[#4A8AE6] text-white font-semibold disabled:opacity-50 transition-colors min-h-[48px]"
                        >
                            <Play size={18} />
                            Jetzt lernen
                        </button>
                        {exam && exam.questions.length > 0 ? (
                            <button
                                type="button"
                                onClick={startExam}
                                className="inline-flex items-center justify-center gap-2 px-5 py-3 rounded-xl border border-[#5B9DFF]/40 text-[#9EC5FF] hover:bg-[#5B9DFF]/10 font-semibold min-h-[48px]"
                            >
                                <ClipboardList size={18} />
                                Probeklausur
                            </button>
                        ) : null}
                    </div>

                    <div className="mt-4 flex flex-wrap gap-2 items-center">
                        <span className="text-xs text-gray-500 mr-1">Vorhandenes öffnen:</span>
                        <button
                            type="button"
                            onClick={() => onStartPractice("quiz")}
                            className="px-3 py-2 rounded-lg text-sm border border-[#2A3140] text-gray-300 hover:bg-[#243044] inline-flex items-center gap-1.5 min-h-[40px]"
                        >
                            <HelpCircle size={14} /> Quiz
                        </button>
                        <button
                            type="button"
                            onClick={() => onStartPractice("flashcards")}
                            className="px-3 py-2 rounded-lg text-sm border border-[#2A3140] text-gray-300 hover:bg-[#243044] inline-flex items-center gap-1.5 min-h-[40px]"
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
                    <BookOpen size={16} /> Lernpfad
                </h3>
                {chapters.length === 0 ? (
                    <div className="rounded-xl border border-dashed border-[#2A3140] bg-[#1A1F2A] p-6 text-center text-gray-400 text-sm">
                        Noch keine Kapitel. Material hochladen und Smart Learning erneut generieren.
                    </div>
                ) : (
                    chapters.map((ch, chi) => {
                        const prevDone =
                            chi === 0 ||
                            chapters.slice(0, chi).every((p) => p.lessons.length > 0 && p.lessons.every((l) => completedLessonSet.has(l.id)));
                        const locked = !prevDone;
                        const doneLessons = ch.lessons.filter((l) => completedLessonSet.has(l.id)).length;
                        const chapterDone = ch.lessons.length > 0 && doneLessons === ch.lessons.length;
                        const hasCloze = ch.lessons.some((l) => l.check.type === "cloze");
                        return (
                            <button
                                key={ch.id}
                                type="button"
                                disabled={locked}
                                onClick={() => startChapter(ch.id)}
                                className={`w-full text-left rounded-xl border px-4 py-3.5 transition-colors ${
                                    locked
                                        ? "border-[#2A3140] bg-[#12141A] opacity-60 cursor-not-allowed"
                                        : "border-[#2A3140] bg-[#1A1F2A] hover:border-[#5B9DFF]/40"
                                }`}
                            >
                                <div className="flex items-start gap-3">
                                    {locked ? (
                                        <Lock size={16} className="text-gray-500 shrink-0 mt-0.5" />
                                    ) : (
                                        <BrainCircuit size={16} className="text-[#5B9DFF] shrink-0 mt-0.5" />
                                    )}
                                    <div className="flex-1 min-w-0 space-y-1">
                                        <div className="flex items-center gap-2">
                                            <p className="text-sm font-medium text-white truncate flex-1">{ch.title}</p>
                                            {hasCloze ? (
                                                <span className="text-[10px] px-1.5 py-0.5 rounded bg-[#5B9DFF]/15 text-[#9EC5FF] shrink-0">
                                                    Lückentext
                                                </span>
                                            ) : null}
                                            {chapterDone ? <CheckCircle2 size={16} className="text-green-400 shrink-0" /> : null}
                                        </div>
                                        <p className="text-[11px] text-gray-500">
                                            {doneLessons}/{ch.lessons.length || 0} Lektionen
                                        </p>
                                        {ch.summary ? (
                                            <MathMarkdown compact className="text-xs text-gray-400 line-clamp-2">
                                                {ch.summary}
                                            </MathMarkdown>
                                        ) : null}
                                        {ch.key_points.length > 0 ? (
                                            <ul className="mt-1 space-y-0.5">
                                                {ch.key_points.slice(0, 3).map((kp, i) => (
                                                    <li key={i} className="text-[11px] text-gray-500 flex gap-1.5">
                                                        <span className="text-[#5B9DFF]">·</span>
                                                        <MathMarkdown compact className="flex-1 [&_p]:my-0">
                                                            {kp}
                                                        </MathMarkdown>
                                                    </li>
                                                ))}
                                            </ul>
                                        ) : null}
                                    </div>
                                </div>
                            </button>
                        );
                    })
                )}
            </div>
        </div>
    );
}
