"use client";

import React, { useCallback, useEffect, useMemo, useRef, useState } from "react";
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
    Settings2,
    Sparkles,
    Timer,
    XCircle,
} from "lucide-react";
import MathMarkdown, { answersMatch, parseClozePrompt } from "@/components/MathMarkdown";
import { awardXp } from "@/lib/xpClient";

const HEARTS_MAX = 5;
const DAILY_GOAL_XP = 50;

export type SmartLearningCheckType = "mc" | "true_false" | "cloze" | "short_answer";

export type SmartLearningCheck = {
    type: SmartLearningCheckType;
    prompt: string;
    options: string[];
    answer: string;
    explanation?: string;
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
    chapter_id?: string;
};

export type SmartLearningExam = {
    title: string;
    time_limit_sec?: number;
    questions: SmartLearningExamQuestion[];
};

export type SmartLearningDifficulty = "basics" | "exam" | "hard";
export type SmartLearningFrequency = "standard" | "compact" | "chapter_end";
export type SmartLearningFocusMode = "theory" | "balanced" | "practice";
export type SmartLearningExamPreset = "short" | "standard" | "full";

export type SmartLearningSettings = {
    difficulty: SmartLearningDifficulty;
    question_frequency: SmartLearningFrequency;
    focus_mode: SmartLearningFocusMode;
    custom_prompt?: string;
    exam_preset?: SmartLearningExamPreset;
};

export type SmartLearningSessionAwards = {
    lesson_ids: string[];
    exam_runs: string[];
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
    settings?: SmartLearningSettings;
    session_awards?: SmartLearningSessionAwards;
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

const EXAM_PRESETS: Record<
    SmartLearningExamPreset,
    { label: string; targetQ: number; timeSec: number; hint: string }
> = {
    short: { label: "Kurz (15 Min)", targetQ: 12, timeSec: 900, hint: "~12 Aufgaben" },
    standard: { label: "Standard (45 Min)", targetQ: 20, timeSec: 2700, hint: "~20 Aufgaben" },
    full: { label: "Vollständige Klausur (90 Min)", targetQ: 28, timeSec: 5400, hint: "~28 Aufgaben" },
};

const DEFAULT_SETTINGS: SmartLearningSettings = {
    difficulty: "exam",
    question_frequency: "standard",
    focus_mode: "balanced",
    exam_preset: "standard",
};

function clampReadiness(n: number): number {
    if (!Number.isFinite(n)) return 0;
    return Math.max(0, Math.min(100, Math.round(n)));
}

function normalizeCheckType(raw: unknown): SmartLearningCheckType {
    const t = String(raw || "mc").trim().toLowerCase();
    if (t === "true_false" || t === "truefalse" || t === "tf") return "true_false";
    if (t === "cloze" || t === "fill" || t === "lueckentext" || t === "lückentext") return "cloze";
    if (t === "short_answer" || t === "short" || t === "freitext" || t === "code" || t === "open") return "short_answer";
    return "mc";
}

function normalizeSettings(raw: unknown): SmartLearningSettings {
    const o = raw && typeof raw === "object" && !Array.isArray(raw) ? (raw as Record<string, unknown>) : {};
    const difficulty = String(o.difficulty || "exam").toLowerCase();
    const frequency = String(o.question_frequency || "standard").toLowerCase();
    const focus = String(o.focus_mode || "balanced").toLowerCase();
    const preset = String(o.exam_preset || "standard").toLowerCase();
    return {
        difficulty: (["basics", "exam", "hard"].includes(difficulty) ? difficulty : "exam") as SmartLearningDifficulty,
        question_frequency: (["standard", "compact", "chapter_end"].includes(frequency)
            ? frequency
            : "standard") as SmartLearningFrequency,
        focus_mode: (["theory", "balanced", "practice"].includes(focus) ? focus : "balanced") as SmartLearningFocusMode,
        custom_prompt: o.custom_prompt ? String(o.custom_prompt) : undefined,
        exam_preset: (["short", "standard", "full"].includes(preset) ? preset : "standard") as SmartLearningExamPreset,
    };
}

function normalizeAwards(raw: unknown): SmartLearningSessionAwards {
    const o = raw && typeof raw === "object" && !Array.isArray(raw) ? (raw as Record<string, unknown>) : {};
    return {
        lesson_ids: Array.isArray(o.lesson_ids) ? o.lesson_ids.map((x) => String(x)) : [],
        exam_runs: Array.isArray(o.exam_runs) ? o.exam_runs.map((x) => String(x)) : [],
    };
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

    if (type === "short_answer") {
        if (!answer) return null;
        return {
            type: "short_answer",
            prompt,
            options: [],
            answer,
            blanks,
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
        const chapter_id = qr.chapter_id || qr.topic_id ? String(qr.chapter_id || qr.topic_id) : undefined;
        questions.push({
            id: String(qr.id || `ex-q${i + 1}`).trim() || `ex-q${i + 1}`,
            type: check.type,
            prompt: check.prompt,
            options: check.options,
            answer: check.answer,
            blanks: check.blanks,
            explanation: check.explanation,
            chapter_id,
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
            settings: { ...DEFAULT_SETTINGS },
            session_awards: { lesson_ids: [], exam_runs: [] },
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
                    chapter_id: ch.id,
                });
                if (qs.length >= 20) break;
            }
            if (qs.length >= 20) break;
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
        settings: normalizeSettings(obj.settings),
        session_awards: normalizeAwards(obj.session_awards),
    };
}

function allLessonIds(content: SmartLearningContent): string[] {
    return content.chapters.flatMap((ch) => ch.lessons.map((l) => l.id));
}

function computeReadiness(content: SmartLearningContent): number {
    const lessons = allLessonIds(content);
    const completedLessons = new Set(content.progress?.completed_lesson_ids || []);
    const sessions = Number(content.progress?.practice_sessions || 0);
    const examBonus =
        content.progress?.exam_best_score != null
            ? Math.min(10, Math.round((content.progress.exam_best_score || 0) / 20))
            : 0;
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

function gradeCloze(prompt: string, blanks: string[] | undefined, values: string[]): boolean {
    const { answers } = parseClozePrompt(prompt, blanks);
    if (answers.length === 0) return false;
    return answers.every((a, i) => answersMatch(a, values[i] || ""));
}

function gradeShortAnswer(check: SmartLearningCheck, given: string): boolean {
    if (answersMatch(check.answer, given)) return true;
    const keywords = check.blanks?.length ? check.blanks : [];
    if (keywords.length === 0) return false;
    const norm = given.trim().toLowerCase();
    return keywords.every((k) => norm.includes(String(k).trim().toLowerCase()));
}

function shuffleInPlace<T>(arr: T[]): T[] {
    for (let i = arr.length - 1; i > 0; i--) {
        const j = Math.floor(Math.random() * (i + 1));
        [arr[i], arr[j]] = [arr[j], arr[i]];
    }
    return arr;
}

function pickBatchSize(
    frequency: SmartLearningFrequency,
    remainingInChapter: number,
    remainingTotal: number
): number {
    if (remainingTotal <= 0) return 0;
    if (frequency === "compact") return 1;
    if (frequency === "chapter_end") return Math.max(1, Math.min(remainingInChapter || remainingTotal, remainingTotal));
    const n = 2 + Math.floor(Math.random() * 4); // 2..5
    return Math.max(1, Math.min(n, remainingTotal));
}

type FlatStep = { chapterId: string; chapterTitle: string; lesson: SmartLearningLesson };

type AssessItem = FlatStep & { check: SmartLearningCheck };

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
    onChange: (v: string[]) => void;
    disabled?: boolean;
    showResult?: boolean;
}) {
    const { segments, answers } = parseClozePrompt(prompt, blanks);
    useEffect(() => {
        if (values.length !== answers.length) {
            onChange(Array.from({ length: answers.length }, (_, i) => values[i] || ""));
        }
        // eslint-disable-next-line react-hooks/exhaustive-deps
    }, [answers.length]);

    return (
        <div className="space-y-3">
            <div className="text-lg font-medium text-white leading-relaxed flex flex-wrap items-baseline gap-y-2">
                {segments.map((seg, i) => (
                    <React.Fragment key={i}>
                        {seg ? <MathMarkdown compact className="inline [&_p]:inline [&_p]:my-0">{seg}</MathMarkdown> : null}
                        {i < answers.length ? (
                            <span className="inline-flex mx-1 align-baseline">
                                <input
                                    type="text"
                                    value={values[i] || ""}
                                    disabled={disabled}
                                    onChange={(e) => {
                                        const next = [...values];
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

function buildExamForPreset(
    base: SmartLearningExam | null | undefined,
    chapters: SmartLearningChapter[],
    preset: SmartLearningExamPreset
): SmartLearningExam {
    const cfg = EXAM_PRESETS[preset];
    const pool: SmartLearningExamQuestion[] = [];
    const seen = new Set<string>();
    const push = (q: SmartLearningExamQuestion) => {
        const key = `${q.type}|${q.prompt}`;
        if (seen.has(key)) return;
        seen.add(key);
        pool.push(q);
    };
    for (const q of base?.questions || []) push(q);
    for (const ch of chapters) {
        for (const les of ch.lessons) {
            push({
                id: `ex-${les.id}`,
                type: les.check.type,
                prompt: les.check.prompt,
                options: les.check.options,
                answer: les.check.answer,
                blanks: les.check.blanks,
                explanation: les.check.explanation,
                chapter_id: ch.id,
            });
        }
    }
    const questions = pool.slice(0, Math.max(cfg.targetQ, Math.min(30, pool.length)));
    // Prefer up to targetQ; if fewer available, use all
    const sliced = questions.length > cfg.targetQ ? questions.slice(0, cfg.targetQ) : questions;
    return {
        title: base?.title || "Probeklausur",
        time_limit_sec: cfg.timeSec,
        questions: sliced.length > 0 ? sliced : questions,
    };
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
    const [mode, setMode] = useState<"overview" | "learn" | "exam" | "exam_result" | "exam_picker">("overview");
    const [learnPhase, setLearnPhase] = useState<"teach" | "assess">("teach");
    const [teachQueue, setTeachQueue] = useState<FlatStep[]>([]);
    const [teachIdx, setTeachIdx] = useState(0);
    const [assessQueue, setAssessQueue] = useState<AssessItem[]>([]);
    const [assessIdx, setAssessIdx] = useState(0);
    const [batchTarget, setBatchTarget] = useState(0);
    const [selected, setSelected] = useState<string | null>(null);
    const [clozeValues, setClozeValues] = useState<string[]>([]);
    const [clozeSubmitted, setClozeSubmitted] = useState(false);
    const [shortAnswer, setShortAnswer] = useState("");
    const [savingProgress, setSavingProgress] = useState(false);
    const [error, setError] = useState("");

    const [examIndex, setExamIndex] = useState(0);
    const [examSelected, setExamSelected] = useState<string | null>(null);
    const [examCloze, setExamCloze] = useState<string[]>([]);
    const [examClozeDone, setExamClozeDone] = useState(false);
    const [examShort, setExamShort] = useState("");
    const [examAnswers, setExamAnswers] = useState<Record<number, { correct: boolean; given: string }>>({});
    const [examSecondsLeft, setExamSecondsLeft] = useState<number | null>(null);
    const [lastExamScore, setLastExamScore] = useState<number | null>(null);
    const [activeExam, setActiveExam] = useState<SmartLearningExam | null>(null);
    const [examPreset, setExamPreset] = useState<SmartLearningExamPreset>("standard");
    const [readinessBeforeExam, setReadinessBeforeExam] = useState(0);
    const examFinishingRef = useRef(false);

    const [hearts, setHearts] = useState(HEARTS_MAX);
    const [failOpen, setFailOpen] = useState(false);
    const [celebrate, setCelebrate] = useState<{ title: string; xp: number } | null>(null);
    const [hudXp, setHudXp] = useState(0);
    const [hudStreak, setHudStreak] = useState(0);
    const [dailyXp, setDailyXp] = useState(0);

    const [customizeOpen, setCustomizeOpen] = useState(false);
    const [customizeBusy, setCustomizeBusy] = useState(false);
    const [examRegenBusy, setExamRegenBusy] = useState(false);
    const [draftSettings, setDraftSettings] = useState<SmartLearningSettings>(DEFAULT_SETTINGS);

    useEffect(() => {
        // Avoid mid-session overwrite from parent refresh
        if (mode !== "overview") return;
        setLocal(normalizeSmartLearningContent(content));
    }, [content, mode]);

    useEffect(() => {
        const loadMe = async () => {
            try {
                const res = await fetch(
                    `/api/me?session_id=${encodeURIComponent(sessionId)}&username=${encodeURIComponent(username)}`,
                    { headers: { "X-Session-Id": sessionId } }
                );
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
    const settings = local.settings || DEFAULT_SETTINGS;
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
                            settings: next.settings || DEFAULT_SETTINGS,
                            session_awards: next.session_awards || { lesson_ids: [], exam_runs: [] },
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

    const pollSmartJob = async (jobId: string): Promise<Record<string, unknown>> => {
        for (let i = 0; i < 120; i++) {
            await new Promise((r) => setTimeout(r, 2000));
            const pr = await fetch(
                `/api/ai/jobs/status/${encodeURIComponent(jobId)}?username=${encodeURIComponent(username)}&session_id=${encodeURIComponent(sessionId)}`,
                { headers: { "X-Session-Id": sessionId } }
            );
            const pd = await pr.json().catch(() => ({}));
            if (!pr.ok) throw new Error(formatApiDetail(pd.detail) || `HTTP ${pr.status}`);
            if (pd.status === "done") return pd as Record<string, unknown>;
            if (pd.status === "error") throw new Error(formatApiDetail(pd.detail) || "Generierung fehlgeschlagen.");
        }
        throw new Error("Zeitüberschreitung — bitte später in den Dateien nachsehen.");
    };

    const markLessonsDone = async (items: FlatStep[]) => {
        const lessonsDone = new Set(local.progress?.completed_lesson_ids || []);
        for (const it of items) lessonsDone.add(it.lesson.id);
        const chaptersDone = new Set(local.progress?.completed_chapter_ids || []);
        for (const ch of local.chapters) {
            if (ch.lessons.length > 0 && ch.lessons.every((l) => lessonsDone.has(l.id))) {
                chaptersDone.add(ch.id);
            }
        }
        const awards = normalizeAwards(local.session_awards);
        let xpGained = 0;
        for (const it of items) {
            if (!awards.lesson_ids.includes(it.lesson.id)) {
                awards.lesson_ids.push(it.lesson.id);
                await grantXp(25, "lesson");
                xpGained += 25;
            }
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
            session_awards: awards,
            settings: local.settings || DEFAULT_SETTINGS,
        };
        next.readiness = computeReadiness(next);
        setLocal(next);
        await persistProgress(next);
        return xpGained;
    };

    const beginBatchFrom = (startIndex: number) => {
        const incomplete = flatSteps
            .map((s, i) => ({ s, i }))
            .filter(({ s }) => !completedLessonSet.has(s.lesson.id));
        if (incomplete.length === 0) {
            const fallback = flatSteps.slice(Math.max(0, startIndex), Math.max(0, startIndex) + 1);
            if (fallback.length === 0) return;
            setTeachQueue(fallback);
            setTeachIdx(0);
            setBatchTarget(1);
            setAssessQueue([]);
            setAssessIdx(0);
            setLearnPhase("teach");
            setHearts(HEARTS_MAX);
            setFailOpen(false);
            setCelebrate(null);
            setSelected(null);
            setClozeValues([]);
            setClozeSubmitted(false);
            setShortAnswer("");
            setMode("learn");
            return;
        }
        // Prefer continuing near startIndex
        let ordered = incomplete.map((x) => x.s);
        const near = incomplete.find(({ i }) => i >= startIndex);
        if (near) {
            const at = incomplete.findIndex(({ i }) => i === near.i);
            ordered = [...incomplete.slice(at), ...incomplete.slice(0, at)].map((x) => x.s);
        }
        const first = ordered[0];
        const remainingInChapter = ordered.filter((s) => s.chapterId === first.chapterId).length;
        const n = pickBatchSize(settings.question_frequency, remainingInChapter, ordered.length);
        const batch = ordered.slice(0, n);
        setTeachQueue(batch);
        setTeachIdx(0);
        setBatchTarget(batch.length);
        setAssessQueue([]);
        setAssessIdx(0);
        setLearnPhase("teach");
        setHearts(HEARTS_MAX);
        setFailOpen(false);
        setCelebrate(null);
        setSelected(null);
        setClozeValues([]);
        setClozeSubmitted(false);
        setShortAnswer("");
        setMode("learn");
    };

    const startLearn = () => {
        const firstIncomplete = flatSteps.findIndex((s) => !completedLessonSet.has(s.lesson.id));
        beginBatchFrom(firstIncomplete >= 0 ? firstIncomplete : 0);
    };

    const startChapter = (chapterId: string) => {
        const idx = flatSteps.findIndex((s) => s.chapterId === chapterId && !completedLessonSet.has(s.lesson.id));
        const fallback = flatSteps.findIndex((s) => s.chapterId === chapterId);
        beginBatchFrom(idx >= 0 ? idx : Math.max(0, fallback));
    };

    const enterAssessPhase = (batch: FlatStep[]) => {
        const items: AssessItem[] = batch.map((b) => ({ ...b, check: b.lesson.check }));
        shuffleInPlace(items);
        setAssessQueue(items);
        setAssessIdx(0);
        setLearnPhase("assess");
        setSelected(null);
        setClozeValues([]);
        setClozeSubmitted(false);
        setShortAnswer("");
        setHearts(HEARTS_MAX);
    };

    const openExamPicker = () => {
        setExamPreset(settings.exam_preset || "standard");
        setMode("exam_picker");
    };

    const startExamWithPreset = async (preset: SmartLearningExamPreset, preferRegen: boolean) => {
        setExamPreset(preset);
        const nextSettings = { ...(local.settings || DEFAULT_SETTINGS), exam_preset: preset };
        let journey: SmartLearningContent = { ...local, settings: nextSettings };
        setLocal(journey);
        void persistProgress(journey);

        let built = buildExamForPreset(journey.exam, journey.chapters, preset);
        const need = EXAM_PRESETS[preset].targetQ;
        if (preferRegen || built.questions.length < Math.min(need, 10)) {
            setExamRegenBusy(true);
            try {
                const res = await fetch(
                    `/api/ai/smart-learning/exam?session_id=${encodeURIComponent(sessionId)}`,
                    {
                        method: "POST",
                        headers: {
                            "Content-Type": "application/json",
                            "X-Session-Id": sessionId,
                        },
                        body: JSON.stringify({
                            username,
                            folder_id: folderId,
                            exam_preset: preset,
                            difficulty: nextSettings.difficulty,
                            custom_prompt: nextSettings.custom_prompt,
                        }),
                    }
                );
                let data: Record<string, unknown> = await res.json().catch(() => ({}));
                if (res.status === 202 && data.job_id) {
                    data = await pollSmartJob(String(data.job_id));
                }
                if (res.ok || data.smart_learning || data.exam) {
                    const updated = normalizeSmartLearningContent(data.smart_learning || data, journey.title);
                    journey = updated;
                    setLocal(updated);
                    onContentUpdated?.(updated);
                    built = buildExamForPreset(updated.exam, updated.chapters, preset);
                }
            } catch {
                /* use local pad */
            } finally {
                setExamRegenBusy(false);
            }
        }

        if (!built.questions.length) {
            setError("Keine Klausurfragen verfügbar.");
            setMode("overview");
            return;
        }
        setActiveExam(built);
        setExamIndex(0);
        setExamSelected(null);
        setExamCloze([]);
        setExamClozeDone(false);
        setExamShort("");
        setExamAnswers({});
        setLastExamScore(null);
        setReadinessBeforeExam(computeReadiness(journey));
        examFinishingRef.current = false;
        setExamSecondsLeft(built.time_limit_sec ?? EXAM_PRESETS[preset].timeSec);
        setMode("exam");
    };

    const finishExam = useCallback(
        async (answers: Record<number, { correct: boolean; given: string }>) => {
            const currentExam = activeExam;
            if (!currentExam || examFinishingRef.current) return;
            examFinishingRef.current = true;
            const total = currentExam.questions.length;
            const correct = Object.values(answers).filter((a) => a.correct).length;
            const score = total > 0 ? clampReadiness((100 * correct) / total) : 0;
            setLastExamScore(score);
            const prevBest = local.progress?.exam_best_score;
            const best = prevBest == null ? score : Math.max(prevBest, score);
            const awards = normalizeAwards(local.session_awards);
            const runKey = `exam-${new Date().toISOString().slice(0, 10)}-${examPreset}`;
            let awarded = false;
            if (!awards.exam_runs.includes(runKey)) {
                awards.exam_runs.push(runKey);
                awarded = true;
                await grantXp(50, "exam");
            }
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
                session_awards: awards,
                settings: { ...(local.settings || DEFAULT_SETTINGS), exam_preset: examPreset },
            };
            next.readiness = computeReadiness(next);
            setLocal(next);
            setMode("exam_result");
            setExamSecondsLeft(null);
            await persistProgress(next);
            if (!awarded) {
                /* XP already granted earlier today for this preset */
            }
        },
        [activeExam, examPreset, local, persistProgress]
    );

    useEffect(() => {
        if (mode === "exam" && examSecondsLeft === 0) {
            void finishExam(examAnswers);
        }
        // eslint-disable-next-line react-hooks/exhaustive-deps
    }, [examSecondsLeft, mode]);

    const saveSettingsOnly = async (nextSettings: SmartLearningSettings) => {
        const next = { ...local, settings: nextSettings };
        setLocal(next);
        await persistProgress(next);
    };

    const regenerateWithSettings = async (nextSettings: SmartLearningSettings) => {
        setCustomizeBusy(true);
        setError("");
        try {
            await saveSettingsOnly(nextSettings);
            const res = await fetch(`/api/ai/smart-learning?session_id=${encodeURIComponent(sessionId)}`, {
                method: "POST",
                headers: {
                    "Content-Type": "application/json",
                    "X-Session-Id": sessionId,
                },
                body: JSON.stringify({
                    username,
                    folder_id: folderId,
                    focus: nextSettings.custom_prompt || undefined,
                    custom_prompt: nextSettings.custom_prompt || undefined,
                    difficulty: nextSettings.difficulty,
                    question_frequency: nextSettings.question_frequency,
                    focus_mode: nextSettings.focus_mode,
                    exam_preset: nextSettings.exam_preset || "standard",
                }),
            });
            let data: Record<string, unknown> = await res.json().catch(() => ({}));
            if (res.status === 202 && data.job_id) {
                data = await pollSmartJob(String(data.job_id));
            } else if (!res.ok) {
                throw new Error(formatApiDetail(data.detail) || "Lernpfad konnte nicht angepasst werden.");
            }
            const updated = normalizeSmartLearningContent(
                data.smart_learning || (data.file as any)?.content || data,
                local.title
            );
            updated.settings = { ...updated.settings, ...nextSettings };
            setLocal(updated);
            onContentUpdated?.(updated);
            setCustomizeOpen(false);
        } catch (e: any) {
            setError(e?.message || "Anpassung fehlgeschlagen.");
        } finally {
            setCustomizeBusy(false);
        }
    };

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

    const BatchProgress = ({ phaseLabel }: { phaseLabel: string }) => {
        const teachDone = learnPhase === "assess" ? batchTarget : teachIdx;
        return (
            <div className="space-y-2">
                <div className="flex items-center justify-between text-xs text-gray-500">
                    <span>
                        {phaseLabel} · Themen-Sammlung {Math.min(teachDone + (learnPhase === "teach" ? 1 : 0), batchTarget)}/
                        {batchTarget}
                    </span>
                    <span className="text-[#9EC5FF]">
                        {learnPhase === "teach" ? "Lernen" : "Wissenstest"}
                    </span>
                </div>
                <div className="relative h-1.5 rounded-full bg-[#23252A] border border-[#45484F]">
                    <div
                        className="h-full bg-[#5B9DFF] transition-all rounded-full"
                        style={{
                            width: `${
                                learnPhase === "teach"
                                    ? (teachIdx / Math.max(batchTarget, 1)) * 70
                                    : 70 + (assessIdx / Math.max(assessQueue.length, 1)) * 30
                            }%`,
                        }}
                    />
                    <div className="absolute inset-0 flex justify-between px-0.5 pointer-events-none">
                        {Array.from({ length: Math.max(batchTarget, 1) }).map((_, i) => (
                            <span
                                key={i}
                                className={`mt-[-2px] h-2.5 w-2.5 rounded-full border ${
                                    i < teachDone ? "bg-[#5B9DFF] border-[#5B9DFF]" : "bg-[#12141A] border-[#45484F]"
                                }`}
                            />
                        ))}
                    </div>
                </div>
            </div>
        );
    };

    /* ——— Learn: teach batch ——— */
    if (mode === "learn" && learnPhase === "teach" && teachQueue[teachIdx]) {
        const current = teachQueue[teachIdx];
        return (
            <div className="w-full max-w-2xl mx-auto space-y-5 pb-8">
                {hudBar}
                <BatchProgress phaseLabel={current.chapterTitle} />
                <div className="rounded-2xl border border-[#2A3140] bg-[#1A1F2A] p-6 sm:p-7 space-y-5 shadow-lg">
                    <div className="flex items-center gap-2">
                        <span className="inline-flex items-center gap-1.5 text-[11px] uppercase tracking-widest text-[#5B9DFF] font-semibold">
                            <BookOpen size={12} /> Erklären
                        </span>
                        <span className="text-[11px] text-gray-500">
                            Lektion {teachIdx + 1}/{batchTarget}
                        </span>
                    </div>
                    <MathMarkdown className="text-white">{current.lesson.teach}</MathMarkdown>
                    <button
                        type="button"
                        onClick={() => {
                            if (teachIdx + 1 >= teachQueue.length) {
                                enterAssessPhase(teachQueue);
                            } else {
                                setTeachIdx((i) => i + 1);
                            }
                        }}
                        className="inline-flex items-center gap-2 px-5 py-3.5 rounded-xl bg-[#5B9DFF] hover:bg-[#4A8AE6] text-white font-semibold min-h-[48px]"
                    >
                        {teachIdx + 1 >= teachQueue.length
                            ? batchTarget > 1
                                ? "Zum Wissenstest"
                                : "Verstanden — jetzt testen"
                            : "Nächstes Thema"}
                        <ChevronRight size={18} />
                    </button>
                </div>
                {error ? <p className="text-sm text-red-400">{error}</p> : null}
            </div>
        );
    }

    /* ——— Learn: assess batch ——— */
    if (mode === "learn" && learnPhase === "assess" && assessQueue[assessIdx]) {
        const current = assessQueue[assessIdx];
        const check = current.check;
        const isCloze = check.type === "cloze";
        const isShort = check.type === "short_answer";
        const clozeOk = isCloze && clozeSubmitted ? gradeCloze(check.prompt, check.blanks, clozeValues) : false;
        const shortOk = isShort && clozeSubmitted ? gradeShortAnswer(check, shortAnswer) : false;
        const mcOk = !isCloze && !isShort && selected != null && selected === check.answer;
        const correct = isCloze ? clozeOk : isShort ? shortOk : mcOk;
        const revealed = isCloze || isShort ? clozeSubmitted : selected != null;

        const onCorrectContinue = async () => {
            if (assessIdx + 1 < assessQueue.length) {
                setAssessIdx((i) => i + 1);
                setSelected(null);
                setClozeValues([]);
                setClozeSubmitted(false);
                setShortAnswer("");
                return;
            }
            const xp = await markLessonsDone(teachQueue);
            const moreLeft = flatSteps.some((s) => !completedLessonSet.has(s.lesson.id) && !teachQueue.some((t) => t.lesson.id === s.lesson.id));
            // Also check after mark — use teachQueue ids
            const doneIds = new Set([...(local.progress?.completed_lesson_ids || []), ...teachQueue.map((t) => t.lesson.id)]);
            const stillLeft = flatSteps.some((s) => !doneIds.has(s.lesson.id));
            setCelebrate({
                title: stillLeft || moreLeft ? "Block geschafft" : "Lernreise abgeschlossen",
                xp: xp || 25 * teachQueue.length,
            });
        };

        return (
            <div className="w-full max-w-2xl mx-auto space-y-5 pb-8">
                {hudBar}
                <BatchProgress phaseLabel={current.chapterTitle} />
                <div className="rounded-2xl border border-[#2A3140] bg-[#1A1F2A] p-6 sm:p-7 space-y-4 shadow-lg">
                    <div className="flex items-center gap-2">
                        <span className="inline-flex items-center gap-1.5 text-[11px] uppercase tracking-widest text-amber-300/90 font-semibold">
                            {isCloze ? <PenLine size={12} /> : isShort ? <PenLine size={12} /> : <HelpCircle size={12} />}
                            {isCloze ? "Lückentext" : isShort ? "Freitext" : "Prüfen"}
                        </span>
                        <span className="text-[11px] text-gray-500">
                            Frage {assessIdx + 1}/{assessQueue.length}
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
                    ) : isShort ? (
                        <>
                            <MathMarkdown className="text-lg font-medium text-white">{check.prompt}</MathMarkdown>
                            <textarea
                                value={shortAnswer}
                                disabled={clozeSubmitted && shortOk}
                                onChange={(e) => setShortAnswer(e.target.value)}
                                rows={3}
                                className="w-full rounded-xl border border-[#2A3140] bg-[#0E1117] px-3 py-2 text-sm text-white focus:outline-none focus:ring-2 focus:ring-[#5B9DFF]/40"
                                placeholder="Kurzantwort…"
                            />
                        </>
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

                    {(isCloze || isShort) && !clozeSubmitted ? (
                        <button
                            type="button"
                            onClick={() => {
                                setClozeSubmitted(true);
                                const ok = isCloze
                                    ? gradeCloze(check.prompt, check.blanks, clozeValues)
                                    : gradeShortAnswer(check, shortAnswer);
                                if (ok) void grantXp(isCloze ? 5 : 8, isCloze ? "cloze" : "short");
                                else spendHeart();
                            }}
                            disabled={
                                isCloze
                                    ? clozeValues.every((v) => !String(v || "").trim())
                                    : !shortAnswer.trim()
                            }
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
                                    {assessIdx + 1 >= assessQueue.length ? "Block abschließen" : "Weiter"}
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
                {error ? <p className="text-sm text-red-400">{error}</p> : null}

                {failOpen ? (
                    <div className="fixed inset-0 z-[140] bg-black/70 flex items-center justify-center p-4">
                        <div className="w-full max-w-sm rounded-2xl border border-[#2A3140] bg-[#12141A] p-6 text-center space-y-4">
                            <Heart className="mx-auto text-red-400" />
                            <h3 className="text-lg font-semibold text-white">Keine Herzen mehr</h3>
                            <p className="text-sm text-gray-400">Der Wissenstest bricht ab. Starte den Block neu.</p>
                            <div className="flex gap-2">
                                <button type="button" onClick={() => setMode("overview")} className="flex-1 py-2.5 rounded-xl border border-[#2A3140] text-gray-300">
                                    Übersicht
                                </button>
                                <button
                                    type="button"
                                    onClick={() => enterAssessPhase(teachQueue)}
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
                        <div className="w-full max-w-sm rounded-2xl border border-[#5B9DFF]/40 bg-[#12141A] p-6 text-center space-y-3">
                            <Sparkles className="mx-auto text-[#5B9DFF]" size={32} />
                            <h3 className="text-lg font-semibold text-white">{celebrate.title}</h3>
                            <p className="text-[#9EC5FF] font-medium">+{celebrate.xp} XP</p>
                            <button
                                type="button"
                                onClick={() => {
                                    setCelebrate(null);
                                    const doneIds = new Set([
                                        ...(local.progress?.completed_lesson_ids || []),
                                        ...teachQueue.map((t) => t.lesson.id),
                                    ]);
                                    const nextIdx = flatSteps.findIndex((s) => !doneIds.has(s.lesson.id));
                                    if (nextIdx < 0) {
                                        setMode("overview");
                                        return;
                                    }
                                    beginBatchFrom(nextIdx);
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

    /* ——— Exam picker ——— */
    if (mode === "exam_picker") {
        return (
            <div className="w-full max-w-xl mx-auto space-y-5 pb-10">
                <button type="button" onClick={() => setMode("overview")} className="text-xs text-gray-400 hover:text-white">
                    ← Übersicht
                </button>
                <div className="rounded-2xl border border-[#2A3140] bg-[#1A1F2A] p-6 space-y-4">
                    <h3 className="text-lg font-semibold text-white flex items-center gap-2">
                        <ClipboardList size={18} className="text-[#5B9DFF]" /> Probeklausur starten
                    </h3>
                    <p className="text-sm text-gray-400">Wähle die Klausurlänge. Fehlende Fragen werden aus dem Lernpfad ergänzt.</p>
                    <div className="space-y-2">
                        {(Object.keys(EXAM_PRESETS) as SmartLearningExamPreset[]).map((key) => {
                            const p = EXAM_PRESETS[key];
                            const active = examPreset === key;
                            return (
                                <button
                                    key={key}
                                    type="button"
                                    onClick={() => setExamPreset(key)}
                                    className={`w-full text-left rounded-xl border px-4 py-3 transition-colors ${
                                        active
                                            ? "border-[#5B9DFF]/50 bg-[#5B9DFF]/10"
                                            : "border-[#2A3140] hover:border-[#5B9DFF]/30"
                                    }`}
                                >
                                    <p className="text-sm font-medium text-white">{p.label}</p>
                                    <p className="text-[11px] text-gray-500">{p.hint}</p>
                                </button>
                            );
                        })}
                    </div>
                    <button
                        type="button"
                        disabled={examRegenBusy}
                        onClick={() => void startExamWithPreset(examPreset, true)}
                        className="w-full inline-flex items-center justify-center gap-2 py-3 rounded-xl bg-[#5B9DFF] text-white font-semibold disabled:opacity-50 min-h-[48px]"
                    >
                        {examRegenBusy ? <Loader2 size={16} className="animate-spin" /> : <Play size={16} />}
                        Klausur starten
                    </button>
                    {error ? <p className="text-sm text-red-400">{error}</p> : null}
                </div>
            </div>
        );
    }

    /* ——— Exam result ——— */
    if (mode === "exam_result" && activeExam) {
        const score = lastExamScore ?? 0;
        const weak: { title: string; rate: number }[] = [];
        const byChapter: Record<string, { ok: number; total: number; title: string }> = {};
        activeExam.questions.forEach((q, i) => {
            const cid = q.chapter_id || "all";
            const title = chapters.find((c) => c.id === cid)?.title || cid;
            if (!byChapter[cid]) byChapter[cid] = { ok: 0, total: 0, title };
            byChapter[cid].total += 1;
            if (examAnswers[i]?.correct) byChapter[cid].ok += 1;
        });
        for (const row of Object.values(byChapter)) {
            const rate = row.total ? row.ok / row.total : 1;
            if (rate < 0.7) weak.push({ title: row.title, rate });
        }
        weak.sort((a, b) => a.rate - b.rate);
        const delta = readiness - readinessBeforeExam;

        return (
            <div className="w-full max-w-xl mx-auto space-y-6 pb-10">
                <div className="rounded-2xl border border-[#2A3140] bg-[#1A1F2A] p-8 text-center space-y-3">
                    <p className="text-[11px] uppercase tracking-widest text-[#5B9DFF]">Probeklausur</p>
                    <p className="text-5xl font-semibold text-white">{score}%</p>
                    <p className="text-[#9EC5FF] font-medium">
                        {Object.values(examAnswers).filter((a) => a.correct).length} / {activeExam.questions.length} richtig
                    </p>
                    <p className="text-sm text-gray-400">
                        Bereitschaft {readiness}%
                        {delta !== 0 ? ` (${delta > 0 ? "+" : ""}${delta})` : ""}
                        {local.progress?.exam_best_score != null ? ` · Klausur-Best ${local.progress.exam_best_score}%` : ""}
                    </p>
                    <div className="h-2 rounded-full bg-[#12141A] overflow-hidden border border-[#2A3140]">
                        <div className="h-full bg-[#5B9DFF]" style={{ width: `${score}%` }} />
                    </div>
                </div>
                {weak.length > 0 ? (
                    <div className="rounded-xl border border-[#2A3140] bg-[#1A1F2A] p-4 space-y-2">
                        <p className="text-xs uppercase tracking-wide text-gray-500">Schwache Themen</p>
                        {weak.slice(0, 5).map((w) => (
                            <div key={w.title} className="flex justify-between text-sm text-gray-300">
                                <span className="truncate">{w.title}</span>
                                <span className="text-amber-200">{Math.round(w.rate * 100)}%</span>
                            </div>
                        ))}
                    </div>
                ) : (
                    <p className="text-center text-sm text-gray-500">Keine auffälligen Schwachstellen — stark!</p>
                )}
                <div className="flex flex-wrap gap-3 justify-center">
                    <button
                        type="button"
                        onClick={() => void startExamWithPreset(examPreset, false)}
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
    if (mode === "exam" && activeExam) {
        const q = activeExam.questions[examIndex];
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
        const isShort = q.type === "short_answer";
        const revealed = isCloze || isShort ? examClozeDone : examSelected != null;
        const mm = Math.floor((examSecondsLeft ?? 0) / 60);
        const ss = String((examSecondsLeft ?? 0) % 60).padStart(2, "0");

        const recordAndAdvance = (correct: boolean, given: string) => {
            const nextAnswers = { ...examAnswers, [examIndex]: { correct, given } };
            setExamAnswers(nextAnswers);
            if (examIndex + 1 >= activeExam.questions.length) {
                void finishExam(nextAnswers);
                return;
            }
            setExamIndex((i) => i + 1);
            setExamSelected(null);
            setExamCloze([]);
            setExamClozeDone(false);
            setExamShort("");
        };

        return (
            <div className="w-full max-w-3xl mx-auto space-y-5 pb-10">
                <div className="flex items-center justify-between text-xs text-gray-400 gap-3">
                    <button type="button" onClick={() => setMode("overview")} className="hover:text-white">
                        ← Abbrechen
                    </button>
                    <span className="truncate">{activeExam.title}</span>
                    {examSecondsLeft != null ? (
                        <span className="inline-flex items-center gap-1 text-[#5B9DFF] shrink-0">
                            <Timer size={12} /> {mm}:{ss}
                        </span>
                    ) : (
                        <span>
                            {examIndex + 1}/{activeExam.questions.length}
                        </span>
                    )}
                </div>

                <div className="flex flex-wrap gap-1.5">
                    {activeExam.questions.map((_, i) => {
                        const answered = examAnswers[i] != null;
                        const current = i === examIndex;
                        return (
                            <button
                                key={i}
                                type="button"
                                onClick={() => {
                                    if (revealed) return;
                                    setExamIndex(i);
                                    setExamSelected(null);
                                    setExamCloze([]);
                                    setExamClozeDone(false);
                                    setExamShort("");
                                }}
                                className={`h-7 min-w-[1.75rem] px-1.5 rounded-md text-[11px] border ${
                                    current
                                        ? "border-[#5B9DFF] bg-[#5B9DFF]/20 text-[#9EC5FF]"
                                        : answered
                                          ? "border-green-500/40 bg-green-500/10 text-green-300"
                                          : "border-[#2A3140] text-gray-500"
                                }`}
                            >
                                {i + 1}
                            </button>
                        );
                    })}
                </div>

                <div className="h-1.5 rounded-full bg-[#23252A] border border-[#45484F] overflow-hidden">
                    <div
                        className="h-full bg-[#5B9DFF] transition-all"
                        style={{
                            width: `${(Object.keys(examAnswers).length / Math.max(activeExam.questions.length, 1)) * 100}%`,
                        }}
                    />
                </div>

                <div className="rounded-2xl border border-[#2A3140] bg-[#1A1F2A] p-6 space-y-4">
                    <p className="text-[11px] uppercase tracking-widest text-gray-500">
                        Frage {examIndex + 1} / {activeExam.questions.length}
                        {isCloze ? " · Lückentext" : isShort ? " · Freitext" : ""}
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
                    ) : isShort ? (
                        <>
                            <MathMarkdown className="text-lg font-medium text-white">{q.prompt}</MathMarkdown>
                            <textarea
                                value={examShort}
                                disabled={examClozeDone}
                                onChange={(e) => setExamShort(e.target.value)}
                                rows={3}
                                className="w-full rounded-xl border border-[#2A3140] bg-[#0E1117] px-3 py-2 text-sm text-white focus:outline-none focus:ring-2 focus:ring-[#5B9DFF]/40"
                                placeholder="Antwort…"
                            />
                        </>
                    ) : (
                        <>
                            <MathMarkdown className="text-lg font-medium text-white">{q.prompt}</MathMarkdown>
                            {renderMcOptions(q, examSelected, (opt) => {
                                if (examSelected) return;
                                setExamSelected(opt);
                            })}
                        </>
                    )}

                    {(isCloze || isShort) && !examClozeDone ? (
                        <button
                            type="button"
                            onClick={() => setExamClozeDone(true)}
                            disabled={isCloze ? examCloze.every((v) => !String(v || "").trim()) : !examShort.trim()}
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
                            <div className="flex flex-wrap gap-2">
                                <button
                                    type="button"
                                    onClick={() => {
                                        if (isCloze) {
                                            recordAndAdvance(gradeCloze(q.prompt, q.blanks, examCloze), examCloze.join(" | "));
                                        } else if (isShort) {
                                            recordAndAdvance(gradeShortAnswer(q, examShort), examShort);
                                        } else {
                                            recordAndAdvance(examSelected === q.answer, examSelected || "");
                                        }
                                    }}
                                    className="inline-flex items-center gap-2 px-5 py-3 rounded-xl bg-[#5B9DFF] text-white font-semibold"
                                >
                                    {examIndex + 1 >= activeExam.questions.length ? "Ergebnis" : "Weiter"}
                                    <ChevronRight size={18} />
                                </button>
                                <button
                                    type="button"
                                    onClick={() => void finishExam({ ...examAnswers, [examIndex]: { correct: false, given: "" } })}
                                    className="px-4 py-3 rounded-xl border border-[#2A3140] text-gray-400 text-sm"
                                >
                                    Abgeben
                                </button>
                            </div>
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
                        <div className="flex items-center gap-2 shrink-0">
                            <button
                                type="button"
                                onClick={() => {
                                    setDraftSettings(settings);
                                    setCustomizeOpen(true);
                                }}
                                className="text-xs text-[#9EC5FF] hover:text-white px-3 py-1.5 rounded-lg border border-[#5B9DFF]/30 inline-flex items-center gap-1.5"
                            >
                                <Settings2 size={12} /> Mit KI anpassen
                            </button>
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
                                <span
                                    className="block h-full bg-[#5B9DFF]"
                                    style={{ width: `${Math.min(100, Math.round((100 * dailyXp) / DAILY_GOAL_XP))}%` }}
                                />
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
                                onClick={openExamPicker}
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
                <p className="text-[11px] text-gray-500">
                    {settings.question_frequency === "compact"
                        ? "Fragen nach jedem Thema"
                        : settings.question_frequency === "chapter_end"
                          ? "Wissenstest am Kapitelende"
                          : "Wissenstest nach 2–5 Themen (gebündelt)"}
                </p>
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
                                            <span className="flex items-center gap-1 shrink-0">
                                                {ch.lessons.map((l, li) => (
                                                    <span
                                                        key={l.id}
                                                        className={`h-1.5 w-1.5 rounded-full ${
                                                            completedLessonSet.has(l.id) ? "bg-[#5B9DFF]" : "bg-[#3A4558]"
                                                        }`}
                                                        title={`Meilenstein ${li + 1}`}
                                                    />
                                                ))}
                                            </span>
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
                                    </div>
                                </div>
                            </button>
                        );
                    })
                )}
            </div>

            {customizeOpen ? (
                <div className="fixed inset-0 z-[150] bg-black/70 backdrop-blur-sm flex items-center justify-center p-4">
                    <div className="w-full max-w-lg rounded-2xl border border-[#45484F] bg-[#23252A] shadow-2xl overflow-hidden">
                        <div className="p-5 border-b border-[#45484F] flex items-center justify-between gap-3">
                            <div>
                                <h3 className="text-base font-semibold text-white flex items-center gap-2">
                                    <Sparkles size={16} className="text-[#5B9DFF]" /> Mit KI anpassen
                                </h3>
                                <p className="text-[11px] text-gray-500 mt-1">Einstellungen speichern oder Lernpfad neu generieren</p>
                            </div>
                            <button
                                type="button"
                                disabled={customizeBusy}
                                onClick={() => setCustomizeOpen(false)}
                                className="text-gray-400 hover:text-white text-sm"
                            >
                                Schließen
                            </button>
                        </div>
                        <div className="p-5 space-y-4 max-h-[70vh] overflow-y-auto">
                            <div>
                                <label className="text-[11px] uppercase tracking-wide text-gray-500">Freitext-Prompt</label>
                                <textarea
                                    value={draftSettings.custom_prompt || ""}
                                    onChange={(e) => setDraftSettings((s) => ({ ...s, custom_prompt: e.target.value }))}
                                    rows={3}
                                    placeholder='z. B. "Fokussiere mehr auf C-Pointer und Speicherverwaltung"'
                                    className="mt-1.5 w-full rounded-xl border border-[#45484F] bg-[#353840] px-3 py-2 text-sm text-white focus:outline-none focus:ring-2 focus:ring-[#5B9DFF]/40"
                                />
                            </div>
                            <div>
                                <p className="text-[11px] uppercase tracking-wide text-gray-500 mb-1.5">Schwierigkeit</p>
                                <div className="flex flex-wrap gap-2">
                                    {(
                                        [
                                            ["basics", "Grundlagen"],
                                            ["exam", "Klausurniveau"],
                                            ["hard", "Schwer"],
                                        ] as const
                                    ).map(([k, label]) => (
                                        <button
                                            key={k}
                                            type="button"
                                            onClick={() => setDraftSettings((s) => ({ ...s, difficulty: k }))}
                                            className={`px-3 py-1.5 rounded-lg text-xs border ${
                                                draftSettings.difficulty === k
                                                    ? "bg-[#5B9DFF] text-white border-[#5B9DFF]"
                                                    : "border-[#45484F] text-gray-300"
                                            }`}
                                        >
                                            {label}
                                        </button>
                                    ))}
                                </div>
                            </div>
                            <div>
                                <p className="text-[11px] uppercase tracking-wide text-gray-500 mb-1.5">Fragen-Frequenz</p>
                                <div className="flex flex-wrap gap-2">
                                    {(
                                        [
                                            ["standard", "Standard (2–5)"],
                                            ["compact", "Kompakt (jedes Thema)"],
                                            ["chapter_end", "Kapitelende"],
                                        ] as const
                                    ).map(([k, label]) => (
                                        <button
                                            key={k}
                                            type="button"
                                            onClick={() => setDraftSettings((s) => ({ ...s, question_frequency: k }))}
                                            className={`px-3 py-1.5 rounded-lg text-xs border ${
                                                draftSettings.question_frequency === k
                                                    ? "bg-[#5B9DFF] text-white border-[#5B9DFF]"
                                                    : "border-[#45484F] text-gray-300"
                                            }`}
                                        >
                                            {label}
                                        </button>
                                    ))}
                                </div>
                            </div>
                            <div>
                                <p className="text-[11px] uppercase tracking-wide text-gray-500 mb-1.5">Fokus</p>
                                <div className="flex flex-wrap gap-2">
                                    {(
                                        [
                                            ["theory", "Theorie-lastig"],
                                            ["balanced", "Ausgewogen"],
                                            ["practice", "Praxis-lastig"],
                                        ] as const
                                    ).map(([k, label]) => (
                                        <button
                                            key={k}
                                            type="button"
                                            onClick={() => setDraftSettings((s) => ({ ...s, focus_mode: k }))}
                                            className={`px-3 py-1.5 rounded-lg text-xs border ${
                                                draftSettings.focus_mode === k
                                                    ? "bg-[#5B9DFF] text-white border-[#5B9DFF]"
                                                    : "border-[#45484F] text-gray-300"
                                            }`}
                                        >
                                            {label}
                                        </button>
                                    ))}
                                </div>
                            </div>
                        </div>
                        <div className="p-5 border-t border-[#45484F] flex flex-col sm:flex-row gap-2">
                            <button
                                type="button"
                                disabled={customizeBusy}
                                onClick={async () => {
                                    await saveSettingsOnly(draftSettings);
                                    setCustomizeOpen(false);
                                }}
                                className="flex-1 py-2.5 rounded-xl border border-[#45484F] text-gray-300 text-sm font-medium"
                            >
                                Nur speichern
                            </button>
                            <button
                                type="button"
                                disabled={customizeBusy}
                                onClick={() => void regenerateWithSettings(draftSettings)}
                                className="flex-1 py-2.5 rounded-xl bg-[#5B9DFF] text-white text-sm font-semibold inline-flex items-center justify-center gap-2 disabled:opacity-50"
                            >
                                {customizeBusy ? <Loader2 size={15} className="animate-spin" /> : <Sparkles size={15} />}
                                Lernpfad neu generieren
                            </button>
                        </div>
                    </div>
                </div>
            ) : null}
        </div>
    );
}
