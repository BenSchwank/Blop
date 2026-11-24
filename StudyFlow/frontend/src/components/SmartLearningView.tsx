"use client";

import React, { useCallback, useEffect, useMemo, useRef, useState } from "react";
import {
    BookOpen,
    BrainCircuit,
    CheckCircle2,
    ChevronLeft,
    ChevronRight,
    ClipboardList,
    Flame,
    Heart,
    HelpCircle,
    Layers,
    Loader2,
    Lock,
    Pause,
    PenLine,
    Play,
    Settings2,
    SkipForward,
    Sparkles,
    Timer,
    Volume2,
    XCircle,
} from "lucide-react";
import MathMarkdown, {
    answerAccepted,
    answersMatch,
    keywordsAccepted,
    parseClozePrompt,
    splitAnswerAlternatives,
} from "@/components/MathMarkdown";
import { awardXp } from "@/lib/xpClient";
import DrawingAnswerPad, { type DrawingGradeResult } from "@/components/DrawingAnswerPad";

const HEARTS_MAX = 5;
const DAILY_GOAL_XP = 50;

const LEARN_VOICES = [
    { id: "charon", label: "Sam · ruhig erklärend" },
    { id: "aoede", label: "Alex · warm & neugierig" },
    { id: "kore", label: "Lea · nachdenklich & warm" },
    { id: "leda", label: "Mila · klar & jung" },
    { id: "puck", label: "Jonas · lebendig erzählend" },
    { id: "zephyr", label: "Nora · hell & direkt" },
] as const;

/** Strip markdown / LaTeX enough for natural TTS narration. */
function plainTextForTts(md: string): string {
    return String(md || "")
        .replace(/```[\s\S]*?```/g, " ")
        .replace(/\$\$([\s\S]*?)\$\$/g, " $1 ")
        .replace(/\$([^$\n]+)\$/g, " $1 ")
        .replace(/!\[[^\]]*\]\([^)]+\)/g, " ")
        .replace(/\[([^\]]+)\]\([^)]+\)/g, "$1")
        .replace(/[#>*_`~]/g, " ")
        .replace(/\s+/g, " ")
        .trim();
}

export type SmartLearningCheckType = "mc" | "true_false" | "cloze" | "short_answer" | "matching" | "drawing";

export type SmartLearningCheck = {
    type: SmartLearningCheckType;
    prompt: string;
    options: string[];
    answer: string;
    explanation?: string;
    blanks?: string[];
    /** Extra accepted forms (synonyms, plural, paraphrases). */
    accepted_answers?: string[];
    /** Grading criteria for drawing / diagram answers. */
    rubric?: string[];
    allow_upload?: boolean;
};

export type SmartLearningLesson = {
    id: string;
    teach: string;
    check: SmartLearningCheck;
    flashcards?: SmartLearningFlashcard[];
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
    accepted_answers?: string[];
    rubric?: string[];
    allow_upload?: boolean;
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
/** What the learn path uses before the knowledge check. */
export type SmartLearningContentMode = "concepts" | "flashcards" | "mix";
export type SmartLearningScope = "compact" | "standard" | "exam_week";

export type SmartLearningFlashcard = { front: string; back: string };

export type SmartLearningSettings = {
    difficulty: SmartLearningDifficulty;
    question_frequency: SmartLearningFrequency;
    focus_mode: SmartLearningFocusMode;
    custom_prompt?: string;
    exam_preset?: SmartLearningExamPreset;
    learn_content_mode?: SmartLearningContentMode;
    scope?: SmartLearningScope;
    scope_label?: string;
    days_until_exam?: number | null;
    unlock_all_chapters?: boolean;
    min_chapters?: number;
    max_chapters?: number;
};

export type SmartLearningSessionAwards = {
    lesson_ids: string[];
    exam_runs: string[];
};

export type DeepSessionStepType =
    | "teach"
    | "flashcards"
    | "exercise"
    | "note"
    | "recall"
    | "break"
    | "summary";

export type DeepSessionStep = {
    id: string;
    type: DeepSessionStepType;
    title: string;
    estimated_minutes: number;
    body?: string;
    flashcards?: SmartLearningFlashcard[];
    check?: SmartLearningCheck;
    note_prompt?: string;
};

export type DeepLearningSession = {
    id: string;
    title: string;
    target_minutes: number;
    steps: DeepSessionStep[];
    created_at?: string;
};

export type DeepSessionProgressEntry = {
    completed_step_ids?: string[];
    notes?: Record<string, string>;
    started_at?: string;
};

export type SmartLearningLearnCheckpoint = {
    lesson_ids: string[];
    teach_idx: number;
    card_idx: number;
    learn_phase: "teach" | "cards" | "assess" | "review";
    assess_lesson_ids?: string[];
    assess_idx?: number;
    hearts?: number;
    saved_at?: string;
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
        /** Lessons postponed to later in the path (not completed). */
        deferred_lesson_ids?: string[];
        /** Mid-path resume point when leaving to overview. */
        learn_checkpoint?: SmartLearningLearnCheckpoint | null;
        practice_sessions?: number;
        exam_best_score?: number | null;
        exam_completed_at?: string | null;
    };
    settings?: SmartLearningSettings;
    session_awards?: SmartLearningSessionAwards;
    deep_sessions?: DeepLearningSession[];
    active_deep_session_id?: string | null;
    deep_session_progress?: Record<string, DeepSessionProgressEntry>;
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
    /** If set, kick off a deep session generation once after mount (e.g. 45). */
    autoStartDeepMinutes?: 30 | 45 | 60 | 90 | null;
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
    learn_content_mode: "mix",
    scope: "standard",
    unlock_all_chapters: false,
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
    if (t === "matching" || t === "match" || t === "zuordnen" || t === "verbinden" || t === "connect" || t === "pairs")
        return "matching";
    if (
        t === "drawing" ||
        t === "draw" ||
        t === "sketch" ||
        t === "diagram" ||
        t === "pap" ||
        t === "flowchart" ||
        t === "bild" ||
        t === "zeichnen"
    )
        return "drawing";
    return "mc";
}

function normalizeSettings(raw: unknown): SmartLearningSettings {
    const o = raw && typeof raw === "object" && !Array.isArray(raw) ? (raw as Record<string, unknown>) : {};
    const difficulty = String(o.difficulty || "exam").toLowerCase();
    const frequency = String(o.question_frequency || "standard").toLowerCase();
    const focus = String(o.focus_mode || "balanced").toLowerCase();
    const preset = String(o.exam_preset || "standard").toLowerCase();
    const contentMode = String(o.learn_content_mode || "mix").toLowerCase();
    const scopeRaw = String(o.scope || "").toLowerCase();
    const scope = (["compact", "standard", "exam_week"].includes(scopeRaw)
        ? scopeRaw
        : "standard") as SmartLearningScope;
    const daysRaw = o.days_until_exam;
    const daysUntil =
        typeof daysRaw === "number" && Number.isFinite(daysRaw)
            ? Math.round(daysRaw)
            : daysRaw == null || daysRaw === ""
              ? null
              : Number.isFinite(Number(daysRaw))
                ? Math.round(Number(daysRaw))
                : null;
    const unlockAll =
        o.unlock_all_chapters === true ||
        scope === "exam_week" ||
        (daysUntil != null && daysUntil <= 7);
    const minCh = Number(o.min_chapters);
    const maxCh = Number(o.max_chapters);
    return {
        difficulty: (["basics", "exam", "hard"].includes(difficulty) ? difficulty : "exam") as SmartLearningDifficulty,
        question_frequency: (["standard", "compact", "chapter_end"].includes(frequency)
            ? frequency
            : "standard") as SmartLearningFrequency,
        focus_mode: (["theory", "balanced", "practice"].includes(focus) ? focus : "balanced") as SmartLearningFocusMode,
        custom_prompt: o.custom_prompt ? String(o.custom_prompt) : undefined,
        exam_preset: (["short", "standard", "full"].includes(preset) ? preset : "standard") as SmartLearningExamPreset,
        learn_content_mode: (["concepts", "flashcards", "mix"].includes(contentMode)
            ? contentMode
            : "mix") as SmartLearningContentMode,
        scope,
        scope_label: o.scope_label ? String(o.scope_label) : undefined,
        days_until_exam: daysUntil,
        unlock_all_chapters: unlockAll,
        min_chapters: Number.isFinite(minCh) ? minCh : undefined,
        max_chapters: Number.isFinite(maxCh) ? maxCh : undefined,
    };
}

function normalizeFlashcards(raw: unknown): SmartLearningFlashcard[] {
    if (!Array.isArray(raw)) return [];
    const out: SmartLearningFlashcard[] = [];
    for (const row of raw) {
        if (!row || typeof row !== "object" || Array.isArray(row)) continue;
        const o = row as Record<string, unknown>;
        const front = String(o.front || "").trim();
        const back = String(o.back || "").trim();
        if (front && back) out.push({ front, back });
    }
    return out;
}

/** Prefer AI cards; otherwise a short concept card from the teach text. */
function cardsForLesson(lesson: SmartLearningLesson): SmartLearningFlashcard[] {
    if (lesson.flashcards && lesson.flashcards.length > 0) return lesson.flashcards;
    const teach = String(lesson.teach || "").trim();
    if (!teach) return [];
    const plain = teach
        .replace(/```[\s\S]*?```/g, " ")
        .replace(/\$\$[\s\S]*?\$\$/g, " ")
        .replace(/\$[^$\n]+\$/g, " ")
        .replace(/[#>*_`~]/g, " ")
        .replace(/\s+/g, " ")
        .trim();
    if (!plain) return [];
    const sentences = plain.split(/(?<=[.!?])\s+/).filter((s) => s.length > 12);
    if (sentences.length >= 2) {
        return [
            { front: "Was ist die Kernidee?", back: sentences.slice(0, 2).join(" ") },
            ...(sentences[2] ? [{ front: "Merksatz / Detail?", back: sentences.slice(2, 4).join(" ") }] : []),
        ];
    }
    return [{ front: "Zusammenfassung", back: plain.slice(0, 420) }];
}

function normalizeAwards(raw: unknown): SmartLearningSessionAwards {
    const o = raw && typeof raw === "object" && !Array.isArray(raw) ? (raw as Record<string, unknown>) : {};
    return {
        lesson_ids: Array.isArray(o.lesson_ids) ? o.lesson_ids.map((x) => String(x)) : [],
        exam_runs: Array.isArray(o.exam_runs) ? o.exam_runs.map((x) => String(x)) : [],
    };
}

function normalizeAcceptedAnswers(raw: unknown): string[] | undefined {
    if (!Array.isArray(raw)) return undefined;
    const out = raw.map((x) => String(x).trim()).filter(Boolean);
    return out.length > 0 ? out : undefined;
}

function normalizeCheck(checkRaw: Record<string, unknown>): SmartLearningCheck | null {
    const prompt = String(checkRaw.prompt || "").trim();
    const answer = String(checkRaw.answer || "").trim();
    if (!prompt) return null;
    const type = normalizeCheckType(checkRaw.type);
    let blanks = Array.isArray(checkRaw.blanks)
        ? checkRaw.blanks.map((x) => String(x).trim()).filter(Boolean)
        : undefined;
    let options = Array.isArray(checkRaw.options)
        ? checkRaw.options.map((x) => String(x).trim()).filter(Boolean)
        : [];
    const accepted = normalizeAcceptedAnswers(checkRaw.accepted_answers);

    if (type === "matching") {
        const pairsRaw = Array.isArray(checkRaw.pairs) ? checkRaw.pairs : [];
        if (pairsRaw.length >= 2 && (options.length < 2 || !blanks || blanks.length < 2)) {
            const lefts: string[] = [];
            const rights: string[] = [];
            for (const p of pairsRaw) {
                if (!p || typeof p !== "object" || Array.isArray(p)) continue;
                const pr = p as Record<string, unknown>;
                const L = String(pr.left || pr.term || "").trim();
                const R = String(pr.right || pr.def || pr.definition || "").trim();
                if (L && R) {
                    lefts.push(L);
                    rights.push(R);
                }
            }
            if (lefts.length >= 2) {
                options = lefts;
                blanks = rights;
            }
        }
        if (options.length < 2 || !blanks || blanks.length < 2 || options.length !== blanks.length) return null;
        return {
            type: "matching",
            prompt: prompt || "Ordne die Paare zu.",
            options,
            blanks,
            answer: answer || blanks.join(" | "),
            explanation: checkRaw.explanation ? String(checkRaw.explanation) : undefined,
            accepted_answers: accepted,
        };
    }

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
            accepted_answers: accepted,
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
            accepted_answers: accepted,
        };
    }

    if (type === "drawing") {
        const rubric = Array.isArray(checkRaw.rubric)
            ? checkRaw.rubric.map((x) => String(x).trim()).filter(Boolean).slice(0, 8)
            : undefined;
        const modelAnswer =
            answer ||
            (rubric && rubric.length ? rubric.join("; ") : "") ||
            "Sinnvolle Skizze / Diagramm zur Aufgabenstellung";
        return {
            type: "drawing",
            prompt,
            options: [],
            answer: modelAnswer,
            explanation: checkRaw.explanation ? String(checkRaw.explanation) : undefined,
            accepted_answers: accepted,
            rubric: rubric && rubric.length ? rubric : undefined,
            allow_upload: checkRaw.allow_upload === false ? false : true,
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
        accepted_answers: accepted,
    };
}

function formatCorrectAnswerLabel(check: SmartLearningCheck): string {
    if (check.type === "cloze") {
        return (check.blanks || [])
            .map((b) => splitAnswerAlternatives(b)[0] || b)
            .filter(Boolean)
            .join(", ");
    }
    if (check.type === "matching") {
        return (check.options || [])
            .map((left, i) => `${left} → ${splitAnswerAlternatives(check.blanks?.[i] || "")[0] || "?"}`)
            .join("; ");
    }
    if (check.type === "drawing") {
        if (check.rubric?.length) return check.rubric.join("; ");
        return check.answer || "";
    }
    const primary = splitAnswerAlternatives(check.answer)[0] || check.answer;
    if (check.accepted_answers?.length) {
        const alts = check.accepted_answers
            .flatMap((a) => splitAnswerAlternatives(a))
            .filter((a) => a.toLowerCase() !== primary.toLowerCase())
            .slice(0, 3);
        if (alts.length) return `${primary} (auch: ${alts.join(", ")})`;
    }
    return primary;
}

function AnswerRevealBlock({
    check,
    correct,
    extraFeedback,
}: {
    check: SmartLearningCheck;
    correct: boolean;
    extraFeedback?: string;
}) {
    const solution = formatCorrectAnswerLabel(check);
    const solutionLabel = check.type === "drawing" ? "Muster / Kriterien" : "Richtige Antwort";
    return (
        <div className="text-sm text-gray-300 border border-[#2A3140] rounded-xl px-4 py-3 bg-[#12141A] space-y-2">
            {extraFeedback ? (
                <div>
                    <p className="text-[11px] uppercase tracking-wide text-gray-500 mb-1">KI-Feedback</p>
                    <p className="text-sm text-gray-200 whitespace-pre-wrap">{extraFeedback}</p>
                </div>
            ) : null}
            {!correct && solution ? (
                <div>
                    <p className="text-[11px] uppercase tracking-wide text-gray-500 mb-1">{solutionLabel}</p>
                    <MathMarkdown compact>{solution}</MathMarkdown>
                </div>
            ) : null}
            {check.explanation ? (
                <div>
                    <p className="text-[11px] uppercase tracking-wide text-gray-500 mb-1.5">Erklärung</p>
                    <MathMarkdown compact>{check.explanation}</MathMarkdown>
                </div>
            ) : !correct && solution ? null : solution ? (
                <div>
                    <p className="text-[11px] uppercase tracking-wide text-gray-500 mb-1">{solutionLabel}</p>
                    <MathMarkdown compact>{solution}</MathMarkdown>
                </div>
            ) : null}
        </div>
    );
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
    const flashcards = normalizeFlashcards(o.flashcards);
    return {
        id: String(o.id || `${chapterId}-l${i + 1}`).trim() || `${chapterId}-l${i + 1}`,
        teach,
        check,
        ...(flashcards.length ? { flashcards } : {}),
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
            accepted_answers: check.accepted_answers,
            rubric: check.rubric,
            allow_upload: check.allow_upload,
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
                deferred_lesson_ids: [],
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
    const deferredLessons = Array.isArray(progressObj.deferred_lesson_ids)
        ? progressObj.deferred_lesson_ids.map((x) => String(x))
        : [];
    const rawCheckpoint =
        progressObj.learn_checkpoint &&
        typeof progressObj.learn_checkpoint === "object" &&
        !Array.isArray(progressObj.learn_checkpoint)
            ? (progressObj.learn_checkpoint as Record<string, unknown>)
            : null;
    const learnCheckpoint: SmartLearningLearnCheckpoint | null = rawCheckpoint
        ? {
              lesson_ids: Array.isArray(rawCheckpoint.lesson_ids)
                  ? rawCheckpoint.lesson_ids.map((x) => String(x)).filter(Boolean)
                  : [],
              teach_idx: Math.max(0, Number(rawCheckpoint.teach_idx) || 0),
              card_idx: Math.max(0, Number(rawCheckpoint.card_idx) || 0),
              learn_phase:
                  rawCheckpoint.learn_phase === "cards" ||
                  rawCheckpoint.learn_phase === "assess" ||
                  rawCheckpoint.learn_phase === "review"
                      ? rawCheckpoint.learn_phase
                      : "teach",
              assess_lesson_ids: Array.isArray(rawCheckpoint.assess_lesson_ids)
                  ? rawCheckpoint.assess_lesson_ids.map((x) => String(x)).filter(Boolean)
                  : undefined,
              assess_idx: Math.max(0, Number(rawCheckpoint.assess_idx) || 0),
              hearts:
                  rawCheckpoint.hearts == null
                      ? undefined
                      : Math.max(0, Math.min(HEARTS_MAX, Number(rawCheckpoint.hearts) || 0)),
              saved_at: rawCheckpoint.saved_at ? String(rawCheckpoint.saved_at) : undefined,
          }
        : null;
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

    const deepSessions = normalizeDeepSessions(obj.deep_sessions);
    const deepProgressRaw =
        obj.deep_session_progress && typeof obj.deep_session_progress === "object" && !Array.isArray(obj.deep_session_progress)
            ? (obj.deep_session_progress as Record<string, unknown>)
            : {};
    const deepProgress: Record<string, DeepSessionProgressEntry> = {};
    for (const [k, v] of Object.entries(deepProgressRaw)) {
        if (!v || typeof v !== "object" || Array.isArray(v)) continue;
        const row = v as Record<string, unknown>;
        const notesRaw = row.notes && typeof row.notes === "object" && !Array.isArray(row.notes) ? (row.notes as Record<string, unknown>) : {};
        const notes: Record<string, string> = {};
        for (const [nk, nv] of Object.entries(notesRaw)) notes[nk] = String(nv ?? "");
        deepProgress[k] = {
            completed_step_ids: Array.isArray(row.completed_step_ids) ? row.completed_step_ids.map((x) => String(x)) : [],
            notes,
            started_at: row.started_at ? String(row.started_at) : undefined,
        };
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
            deferred_lesson_ids: deferredLessons.filter((id) => !completedLessons.includes(id)),
            learn_checkpoint:
                learnCheckpoint && learnCheckpoint.lesson_ids.length > 0 ? learnCheckpoint : null,
            practice_sessions: Math.max(0, Number(progressObj.practice_sessions) || 0),
            exam_best_score: Number.isFinite(best as number) ? clampReadiness(best as number) : null,
            exam_completed_at: progressObj.exam_completed_at ? String(progressObj.exam_completed_at) : null,
        },
        settings: normalizeSettings(obj.settings),
        session_awards: normalizeAwards(obj.session_awards),
        deep_sessions: deepSessions,
        active_deep_session_id: obj.active_deep_session_id ? String(obj.active_deep_session_id) : null,
        deep_session_progress: deepProgress,
    };
}

function normalizeDeepSessions(raw: unknown): DeepLearningSession[] {
    if (!Array.isArray(raw)) return [];
    const allowed: DeepSessionStepType[] = ["teach", "flashcards", "exercise", "note", "recall", "break", "summary"];
    const out: DeepLearningSession[] = [];
    for (const row of raw) {
        if (!row || typeof row !== "object" || Array.isArray(row)) continue;
        const o = row as Record<string, unknown>;
        const stepsRaw = Array.isArray(o.steps) ? o.steps : [];
        const steps: DeepSessionStep[] = [];
        for (let i = 0; i < stepsRaw.length; i++) {
            const st = stepsRaw[i];
            if (!st || typeof st !== "object" || Array.isArray(st)) continue;
            const s = st as Record<string, unknown>;
            const type = String(s.type || "").toLowerCase() as DeepSessionStepType;
            if (!allowed.includes(type)) continue;
            const check =
                s.check && typeof s.check === "object" && !Array.isArray(s.check)
                    ? normalizeCheck(s.check as Record<string, unknown>)
                    : undefined;
            const est = Number(s.estimated_minutes);
            steps.push({
                id: String(s.id || `s${i + 1}`).trim() || `s${i + 1}`,
                type,
                title: String(s.title || `Schritt ${i + 1}`).trim(),
                estimated_minutes: Number.isFinite(est) ? Math.max(1, Math.min(30, Math.round(est))) : 5,
                body: s.body ? String(s.body) : undefined,
                note_prompt: s.note_prompt ? String(s.note_prompt) : undefined,
                flashcards: normalizeFlashcards(s.flashcards),
                ...(check ? { check } : {}),
            });
        }
        if (steps.length === 0) continue;
        const tm = Number(o.target_minutes);
        out.push({
            id: String(o.id || `ds${out.length + 1}`).trim() || `ds${out.length + 1}`,
            title: String(o.title || "Tiefes Lernen").trim() || "Tiefes Lernen",
            target_minutes: Number.isFinite(tm) ? Math.max(15, Math.min(120, Math.round(tm))) : 60,
            steps,
            created_at: o.created_at ? String(o.created_at) : undefined,
        });
    }
    return out;
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

function gradeCloze(
    prompt: string,
    blanks: string[] | undefined,
    values: string[],
    accepted?: string[] | null
): boolean {
    const { answers } = parseClozePrompt(prompt, blanks);
    if (answers.length === 0) return false;
    // Order-independent when ≥2 blanks: each expected blank must match some unused given value
    if (answers.length >= 2) {
        const used = new Set<number>();
        return answers.every((a) => {
            const idx = values.findIndex(
                (v, i) => !used.has(i) && answerAccepted(a, v || "", accepted)
            );
            if (idx < 0) return false;
            used.add(idx);
            return true;
        });
    }
    return answerAccepted(answers[0], values[0] || "", accepted);
}

function gradeShortAnswer(check: SmartLearningCheck, given: string): boolean {
    if (answerAccepted(check.answer, given, check.accepted_answers)) return true;
    const keywords = check.blanks?.length ? check.blanks : [];
    if (keywords.length === 0) return false;
    return keywordsAccepted(keywords, given, check.accepted_answers);
}

function gradeMatching(check: SmartLearningCheck, assigned: string[]): boolean {
    const rights = check.blanks || [];
    if (rights.length === 0 || rights.length !== (check.options?.length || 0)) return false;
    return rights.every((r, i) => answerAccepted(r, assigned[i] || "", check.accepted_answers));
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
    const n = 3 + Math.floor(Math.random() * 2); // 3..4
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

/** Zuordnen / Verbinden: left column fixed, right column via shuffled selects. */
function MatchingExercise({
    prompt,
    lefts,
    rights,
    values,
    onChange,
    disabled,
    showResult,
}: {
    prompt: string;
    lefts: string[];
    rights: string[];
    values: string[];
    onChange: (v: string[]) => void;
    disabled?: boolean;
    showResult?: boolean;
}) {
    const shuffledRights = useMemo(() => {
        const copy = [...rights];
        shuffleInPlace(copy);
        return copy;
        // eslint-disable-next-line react-hooks/exhaustive-deps
    }, [rights.join("\u0001")]);

    useEffect(() => {
        if (values.length !== lefts.length) {
            onChange(Array.from({ length: lefts.length }, (_, i) => values[i] || ""));
        }
        // eslint-disable-next-line react-hooks/exhaustive-deps
    }, [lefts.length]);

    return (
        <div className="space-y-3">
            <MathMarkdown className="text-lg font-medium text-white">{prompt || "Ordne die Paare zu."}</MathMarkdown>
            <div className="space-y-2">
                {lefts.map((left, i) => {
                    const ok = showResult ? answersMatch(rights[i] || "", values[i] || "") : null;
                    return (
                        <div
                            key={`${i}-${left.slice(0, 24)}`}
                            className={`flex flex-col sm:flex-row sm:items-center gap-2 rounded-xl border px-3 py-2.5 ${
                                ok === true
                                    ? "border-green-500/40 bg-green-500/5"
                                    : ok === false
                                      ? "border-red-500/40 bg-red-500/5"
                                      : "border-[#2A3140] bg-[#12141A]"
                            }`}
                        >
                            <div className="flex-1 min-w-0 text-sm text-white">
                                <MathMarkdown compact>{left}</MathMarkdown>
                            </div>
                            <span className="hidden sm:inline text-gray-600 shrink-0">→</span>
                            <select
                                value={values[i] || ""}
                                disabled={disabled}
                                onChange={(e) => {
                                    const next = [...values];
                                    next[i] = e.target.value;
                                    onChange(next);
                                }}
                                className="sm:w-[55%] rounded-lg border border-[#3A4558] bg-[#0E1117] px-2.5 py-2 text-sm text-white focus:outline-none focus:ring-2 focus:ring-[#5B9DFF]/40 disabled:opacity-60"
                                aria-label={`Zuordnung für ${i + 1}`}
                            >
                                <option value="">— wählen —</option>
                                {shuffledRights.map((r) => (
                                    <option key={r} value={r}>
                                        {r.replace(/\$/g, "").slice(0, 80)}
                                    </option>
                                ))}
                            </select>
                        </div>
                    );
                })}
            </div>
            {showResult ? (
                <p className="text-xs text-gray-400">Richtige Paare: links ↔ rechts in der Listenreihenfolge.</p>
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

const DEEP_BUDGETS = [30, 45, 60, 90] as const;
export type DeepBudgetMinutes = (typeof DEEP_BUDGETS)[number];

export default function SmartLearningView({
    content,
    folderId,
    username,
    sessionId,
    busyPractice = false,
    onContentUpdated,
    onStartPractice,
    onClose,
    autoStartDeepMinutes = null,
}: Props) {
    const [local, setLocal] = useState<SmartLearningContent>(() => normalizeSmartLearningContent(content));
    const [mode, setMode] = useState<"overview" | "learn" | "exam" | "exam_result" | "exam_picker" | "deep">("overview");
    const [deepBudget, setDeepBudget] = useState<DeepBudgetMinutes>(60);
    const deepAutoStartedRef = useRef(false);
    const [deepBusy, setDeepBusy] = useState(false);
    const [deepStepIdx, setDeepStepIdx] = useState(0);
    const [deepNoteDraft, setDeepNoteDraft] = useState("");
    const [deepSelected, setDeepSelected] = useState<string | null>(null);
    const [deepCloze, setDeepCloze] = useState<string[]>([]);
    const [deepClozeDone, setDeepClozeDone] = useState(false);
    const [deepShort, setDeepShort] = useState("");
    const [deepCardIdx, setDeepCardIdx] = useState(0);
    const [deepCardFlipped, setDeepCardFlipped] = useState(false);
    const [learnPhase, setLearnPhase] = useState<"teach" | "cards" | "assess" | "review">("teach");
    const [teachQueue, setTeachQueue] = useState<FlatStep[]>([]);
    const [teachIdx, setTeachIdx] = useState(0);
    const [cardIdx, setCardIdx] = useState(0);
    const [cardFlipped, setCardFlipped] = useState(false);
    const [assessQueue, setAssessQueue] = useState<AssessItem[]>([]);
    const [assessIdx, setAssessIdx] = useState(0);
    const [batchTarget, setBatchTarget] = useState(0);
    const [reviewItem, setReviewItem] = useState<AssessItem | null>(null);
    const [deferCountByLessonId, setDeferCountByLessonId] = useState<Record<string, number>>({});
    const [learnVoice, setLearnVoice] = useState<string>("charon");
    const [ttsBusy, setTtsBusy] = useState(false);
    const [ttsPlaying, setTtsPlaying] = useState(false);
    const ttsAudioRef = useRef<HTMLAudioElement | null>(null);
    const ttsUrlRef = useRef<string | null>(null);
    const [selected, setSelected] = useState<string | null>(null);
    const [clozeValues, setClozeValues] = useState<string[]>([]);
    const [clozeSubmitted, setClozeSubmitted] = useState(false);
    const [shortAnswer, setShortAnswer] = useState("");
    const [failCounts, setFailCounts] = useState<Record<string, number>>({});
    const [drawingGrade, setDrawingGrade] = useState<DrawingGradeResult | null>(null);
    const [drawingBusy, setDrawingBusy] = useState(false);
    const [savingProgress, setSavingProgress] = useState(false);
    const [error, setError] = useState("");
    const MAX_DEFER_PER_LESSON = 2;
    const MAX_FAILS_BEFORE_REVEAL = 2;

    const [examIndex, setExamIndex] = useState(0);
    const [examSelected, setExamSelected] = useState<string | null>(null);
    const [examCloze, setExamCloze] = useState<string[]>([]);
    const [examClozeDone, setExamClozeDone] = useState(false);
    const [examShort, setExamShort] = useState("");
    const [examAnswers, setExamAnswers] = useState<Record<number, { correct: boolean; given: string }>>({});
    const [examSecondsLeft, setExamSecondsLeft] = useState<number | null>(null);
    const [lastExamScore, setLastExamScore] = useState<number | null>(null);
    const [activeExam, setActiveExam] = useState<SmartLearningExam | null>(null);
    const [examOrder, setExamOrder] = useState<number[]>([]);
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
    const deferredLessonSet = useMemo(
        () => new Set(local.progress?.deferred_lesson_ids || []),
        [local.progress?.deferred_lesson_ids]
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
                            deferred_lesson_ids: next.progress?.deferred_lesson_ids || [],
                            learn_checkpoint: next.progress?.learn_checkpoint ?? null,
                            practice_sessions: next.progress?.practice_sessions || 0,
                            exam_best_score: next.progress?.exam_best_score ?? null,
                            exam_completed_at: next.progress?.exam_completed_at ?? null,
                            readiness: computeReadiness(next),
                            settings: next.settings || DEFAULT_SETTINGS,
                            session_awards: next.session_awards || { lesson_ids: [], exam_runs: [] },
                            deep_session_progress: next.deep_session_progress || {},
                            active_deep_session_id: next.active_deep_session_id ?? null,
                        }),
                    }
                );
                const data = await res.json().catch(() => ({}));
                if (!res.ok) {
                    const detail = formatApiDetail(data.detail) || data.message || "";
                    throw new Error(
                        detail
                            ? `Fortschritt konnte nicht gespeichert werden: ${detail}`
                            : `Fortschritt konnte nicht gespeichert werden (HTTP ${res.status}).`
                    );
                }
                const updated = normalizeSmartLearningContent(data.smart_learning || next, next.title);
                setLocal(updated);
                onContentUpdated?.(updated);
            } catch (e: any) {
                // Keep local progress so learning is not blocked if the network/API hiccups.
                setLocal(next);
                onContentUpdated?.(next);
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
        const deferred = new Set(local.progress?.deferred_lesson_ids || []);
        const toMark = items.filter((it) => !deferred.has(it.lesson.id));
        const lessonsDone = new Set(local.progress?.completed_lesson_ids || []);
        for (const it of toMark) {
            lessonsDone.add(it.lesson.id);
            deferred.delete(it.lesson.id);
        }
        const chaptersDone = new Set(local.progress?.completed_chapter_ids || []);
        for (const ch of local.chapters) {
            if (ch.lessons.length > 0 && ch.lessons.every((l) => lessonsDone.has(l.id))) {
                chaptersDone.add(ch.id);
            }
        }
        const awards = normalizeAwards(local.session_awards);
        let xpGained = 0;
        for (const it of toMark) {
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
                deferred_lesson_ids: Array.from(deferred),
                learn_checkpoint: null,
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

    const clearLearnCheckpoint = async () => {
        if (!local.progress?.learn_checkpoint) return;
        const next: SmartLearningContent = {
            ...local,
            progress: {
                ...local.progress,
                learn_checkpoint: null,
            },
        };
        setLocal(next);
        await persistProgress(next);
    };

    const buildLearnCheckpoint = (): SmartLearningLearnCheckpoint | null => {
        if (mode !== "learn" || teachQueue.length === 0) return null;
        return {
            lesson_ids: teachQueue.map((t) => t.lesson.id),
            teach_idx: teachIdx,
            card_idx: cardIdx,
            learn_phase: learnPhase === "review" ? "assess" : learnPhase,
            assess_lesson_ids: assessQueue.map((a) => a.lesson.id),
            assess_idx: assessIdx,
            hearts,
            saved_at: new Date().toISOString(),
        };
    };

    const goToOverview = async () => {
        stopTeachTts();
        if (mode === "learn") {
            const cp = buildLearnCheckpoint();
            const next: SmartLearningContent = {
                ...local,
                progress: {
                    ...local.progress,
                    completed_chapter_ids: local.progress?.completed_chapter_ids || [],
                    completed_lesson_ids: local.progress?.completed_lesson_ids || [],
                    deferred_lesson_ids: local.progress?.deferred_lesson_ids || [],
                    learn_checkpoint: cp,
                    practice_sessions: local.progress?.practice_sessions || 0,
                    exam_best_score: local.progress?.exam_best_score ?? null,
                    exam_completed_at: local.progress?.exam_completed_at ?? null,
                },
                settings: local.settings || DEFAULT_SETTINGS,
            };
            next.readiness = computeReadiness(next);
            setLocal(next);
            setMode("overview");
            await persistProgress(next);
            return;
        }
        setMode("overview");
    };

    const resumeLearnCheckpoint = () => {
        const cp = local.progress?.learn_checkpoint;
        if (!cp?.lesson_ids?.length) return;
        const byId = new Map(flatSteps.map((s) => [s.lesson.id, s]));
        const queue = cp.lesson_ids.map((id) => byId.get(id)).filter(Boolean) as FlatStep[];
        if (!queue.length) {
            void clearLearnCheckpoint();
            return;
        }
        setTeachQueue(queue);
        setTeachIdx(Math.min(Math.max(0, cp.teach_idx), queue.length - 1));
        setBatchTarget(queue.length);
        setCardIdx(Math.max(0, cp.card_idx || 0));
        setCardFlipped(false);
        setHearts(cp.hearts == null ? HEARTS_MAX : Math.max(0, Math.min(HEARTS_MAX, cp.hearts)));
        setFailOpen(false);
        setCelebrate(null);
        setReviewItem(null);
        setDeferCountByLessonId({});
        setSelected(null);
        setClozeValues([]);
        setClozeSubmitted(false);
        setShortAnswer("");
        setDrawingGrade(null);
        setDrawingBusy(false);
        const phase = cp.learn_phase || "teach";
        if (phase === "assess" || phase === "review") {
            const assessIds = cp.assess_lesson_ids?.length
                ? cp.assess_lesson_ids
                : queue.map((q) => q.lesson.id);
            const assess = assessIds
                .map((id) => {
                    const step = byId.get(id);
                    return step ? { ...step, check: step.lesson.check } : null;
                })
                .filter(Boolean) as AssessItem[];
            setAssessQueue(assess);
            setAssessIdx(Math.min(Math.max(0, cp.assess_idx || 0), Math.max(0, assess.length - 1)));
            setLearnPhase(assess.length ? "assess" : "teach");
        } else {
            setAssessQueue([]);
            setAssessIdx(0);
            setLearnPhase(phase === "cards" ? "cards" : "teach");
        }
        setMode("learn");
    };

    const contentMode = (): SmartLearningContentMode =>
        settings.learn_content_mode || DEFAULT_SETTINGS.learn_content_mode || "mix";

    const initialLearnPhase = (): "teach" | "cards" =>
        contentMode() === "flashcards" ? "cards" : "teach";

    const beginBatchFrom = (startIndex: number, opts?: { includeDeferred?: boolean; preferLessonId?: string }) => {
        const includeDeferred = opts?.includeDeferred === true;
        const incomplete = flatSteps
            .map((s, i) => ({ s, i }))
            .filter(({ s }) => !completedLessonSet.has(s.lesson.id));
        const primary = incomplete.filter(({ s }) => !deferredLessonSet.has(s.lesson.id));
        const deferredOnly = incomplete.filter(({ s }) => deferredLessonSet.has(s.lesson.id));
        const pool = includeDeferred || primary.length === 0 ? [...primary, ...deferredOnly] : primary;
        const resetLearnInputs = () => {
            setAssessQueue([]);
            setAssessIdx(0);
            setReviewItem(null);
            setDeferCountByLessonId({});
            setCardIdx(0);
            setCardFlipped(false);
            setHearts(HEARTS_MAX);
            setFailOpen(false);
            setCelebrate(null);
            setSelected(null);
            setClozeValues([]);
            setClozeSubmitted(false);
            setShortAnswer("");
            setDrawingGrade(null);
            setDrawingBusy(false);
            setMode("learn");
        };
        if (pool.length === 0) {
            const fallback = flatSteps.slice(Math.max(0, startIndex), Math.max(0, startIndex) + 1);
            if (fallback.length === 0) return;
            setTeachQueue(fallback);
            setTeachIdx(0);
            setBatchTarget(1);
            setLearnPhase(initialLearnPhase());
            resetLearnInputs();
            return;
        }
        let ordered = pool.map((x) => x.s);
        if (opts?.preferLessonId) {
            const atPref = pool.findIndex(({ s }) => s.lesson.id === opts.preferLessonId);
            if (atPref >= 0) {
                ordered = [...pool.slice(atPref), ...pool.slice(0, atPref)].map((x) => x.s);
            }
        } else {
            const near = pool.find(({ i }) => i >= startIndex);
            if (near) {
                const at = pool.findIndex(({ i }) => i === near.i);
                ordered = [...pool.slice(at), ...pool.slice(0, at)].map((x) => x.s);
            }
        }
        const first = ordered[0];
        const remainingInChapter = ordered.filter((s) => s.chapterId === first.chapterId).length;
        const n = pickBatchSize(settings.question_frequency, remainingInChapter, ordered.length);
        const batch = ordered.slice(0, n);
        setTeachQueue(batch);
        setTeachIdx(0);
        setBatchTarget(batch.length);
        setLearnPhase(initialLearnPhase());
        resetLearnInputs();
        if (local.progress?.learn_checkpoint) {
            void clearLearnCheckpoint();
        }
    };

    const advanceAfterLearnUnit = () => {
        stopTeachTts();
        const mode = contentMode();
        const atEnd = teachIdx + 1 >= teachQueue.length;

        if (learnPhase === "teach") {
            if (mode === "mix" && cardsForLesson(teachQueue[teachIdx].lesson).length > 0) {
                setCardIdx(0);
                setCardFlipped(false);
                setLearnPhase("cards");
                return;
            }
            if (atEnd) {
                enterAssessPhase(teachQueue);
                return;
            }
            setTeachIdx((i) => i + 1);
            return;
        }

        if (learnPhase === "cards") {
            const cards = cardsForLesson(teachQueue[teachIdx]?.lesson);
            if (cardIdx + 1 < cards.length) {
                setCardIdx((i) => i + 1);
                setCardFlipped(false);
                return;
            }
            if (atEnd) {
                enterAssessPhase(teachQueue);
                return;
            }
            setTeachIdx((i) => i + 1);
            setCardIdx(0);
            setCardFlipped(false);
            setLearnPhase(mode === "flashcards" ? "cards" : "teach");
        }
    };

    const canRetreatLearn = (): boolean => {
        if (learnPhase === "review") return true;
        if (learnPhase === "assess") return assessIdx > 0 || teachQueue.length > 0;
        if (learnPhase === "cards") {
            if (cardIdx > 0) return true;
            if (contentMode() === "mix") return true;
            return teachIdx > 0;
        }
        if (learnPhase === "teach") return teachIdx > 0;
        return false;
    };

    const retreatLearnUnit = () => {
        stopTeachTts();
        const mode = contentMode();

        if (learnPhase === "review") {
            setReviewItem(null);
            setLearnPhase("assess");
            resetAssessInputs();
            return;
        }

        if (learnPhase === "assess") {
            if (assessIdx > 0) {
                setAssessIdx((i) => i - 1);
                resetAssessInputs();
                return;
            }
            const lastIdx = Math.max(0, teachQueue.length - 1);
            setTeachIdx(lastIdx);
            const lastLesson = teachQueue[lastIdx]?.lesson;
            const cards = lastLesson ? cardsForLesson(lastLesson) : [];
            if (mode === "flashcards" || (mode === "mix" && cards.length > 0)) {
                setLearnPhase("cards");
                setCardIdx(Math.max(0, cards.length - 1));
                setCardFlipped(false);
            } else {
                setLearnPhase("teach");
            }
            resetAssessInputs();
            return;
        }

        if (learnPhase === "cards") {
            if (cardIdx > 0) {
                setCardIdx((i) => i - 1);
                setCardFlipped(false);
                return;
            }
            if (mode === "mix") {
                setLearnPhase("teach");
                return;
            }
            if (teachIdx > 0) {
                const prev = teachQueue[teachIdx - 1];
                const prevCards = cardsForLesson(prev.lesson);
                setTeachIdx((i) => i - 1);
                setCardIdx(Math.max(0, prevCards.length - 1));
                setCardFlipped(false);
                setLearnPhase("cards");
            }
            return;
        }

        if (learnPhase === "teach" && teachIdx > 0) {
            const prev = teachQueue[teachIdx - 1];
            const prevCards = cardsForLesson(prev.lesson);
            setTeachIdx((i) => i - 1);
            if (mode === "mix" && prevCards.length > 0) {
                setLearnPhase("cards");
                setCardIdx(Math.max(0, prevCards.length - 1));
                setCardFlipped(false);
            } else if (mode === "flashcards") {
                setLearnPhase("cards");
                setCardIdx(Math.max(0, prevCards.length - 1));
                setCardFlipped(false);
            } else {
                setLearnPhase("teach");
            }
        }
    };

    const setDeferredLessonIds = async (ids: string[]) => {
        const done = new Set(local.progress?.completed_lesson_ids || []);
        const unique = Array.from(new Set(ids)).filter((id) => !done.has(id));
        const next: SmartLearningContent = {
            ...local,
            progress: {
                ...local.progress,
                completed_chapter_ids: local.progress?.completed_chapter_ids || [],
                completed_lesson_ids: local.progress?.completed_lesson_ids || [],
                deferred_lesson_ids: unique,
                practice_sessions: local.progress?.practice_sessions || 0,
                exam_best_score: local.progress?.exam_best_score ?? null,
                exam_completed_at: local.progress?.exam_completed_at ?? null,
            },
            settings: local.settings || DEFAULT_SETTINGS,
        };
        next.readiness = computeReadiness(next);
        setLocal(next);
        await persistProgress(next);
    };

    const deferLessonIds = async (lessonIds: string[]) => {
        const current = new Set(local.progress?.deferred_lesson_ids || []);
        for (const id of lessonIds) current.add(id);
        await setDeferredLessonIds(Array.from(current));
    };

    const undeferLessonIds = async (lessonIds: string[]) => {
        const remove = new Set(lessonIds);
        const current = (local.progress?.deferred_lesson_ids || []).filter((id) => !remove.has(id));
        await setDeferredLessonIds(current);
    };

    const deferCurrentLearnLesson = async () => {
        const current = teachQueue[teachIdx];
        if (!current) return;
        stopTeachTts();
        const lessonId = current.lesson.id;
        await deferLessonIds([lessonId]);
        const copy = teachQueue.filter((_, i) => i !== teachIdx);
        const assessCopy = assessQueue.filter((q) => q.lesson.id !== lessonId);
        setAssessQueue(assessCopy);
        setAssessIdx((i) => Math.min(i, Math.max(0, assessCopy.length - 1)));
        if (copy.length === 0) {
            setTeachQueue([]);
            setBatchTarget(0);
            setTeachIdx(0);
            setLearnPhase("teach");
            setMode("overview");
            void clearLearnCheckpoint();
            return;
        }
        const nextIdx = Math.min(teachIdx, copy.length - 1);
        setTeachQueue(copy);
        setTeachIdx(nextIdx);
        setBatchTarget(copy.length);
        setCardIdx(0);
        setCardFlipped(false);
        if (learnPhase === "assess" || learnPhase === "review") {
            if (assessCopy.length === 0) {
                setLearnPhase("assess");
            } else {
                setLearnPhase("assess");
                resetAssessInputs();
            }
            setReviewItem(null);
        } else {
            setLearnPhase(initialLearnPhase());
        }
    };

    const startLearn = () => {
        const firstPrimary = flatSteps.findIndex(
            (s) => !completedLessonSet.has(s.lesson.id) && !deferredLessonSet.has(s.lesson.id)
        );
        const firstAny = flatSteps.findIndex((s) => !completedLessonSet.has(s.lesson.id));
        beginBatchFrom(firstPrimary >= 0 ? firstPrimary : firstAny >= 0 ? firstAny : 0, {
            includeDeferred: firstPrimary < 0,
        });
    };

    const startChapter = (chapterId: string, opts?: { includeDeferred?: boolean }) => {
        const idx = flatSteps.findIndex(
            (s) =>
                s.chapterId === chapterId &&
                !completedLessonSet.has(s.lesson.id) &&
                (opts?.includeDeferred || !deferredLessonSet.has(s.lesson.id))
        );
        const fallbackDeferred = flatSteps.findIndex(
            (s) => s.chapterId === chapterId && !completedLessonSet.has(s.lesson.id)
        );
        const fallback = flatSteps.findIndex((s) => s.chapterId === chapterId);
        beginBatchFrom(idx >= 0 ? idx : fallbackDeferred >= 0 ? fallbackDeferred : Math.max(0, fallback), {
            includeDeferred: opts?.includeDeferred || idx < 0,
        });
    };

    const startLesson = async (lessonId: string) => {
        if (deferredLessonSet.has(lessonId)) {
            await undeferLessonIds([lessonId]);
        }
        const idx = flatSteps.findIndex((s) => s.lesson.id === lessonId);
        beginBatchFrom(Math.max(0, idx), { includeDeferred: true, preferLessonId: lessonId });
    };

    const resetAssessInputs = () => {
        setSelected(null);
        setClozeValues([]);
        setClozeSubmitted(false);
        setShortAnswer("");
        setDrawingGrade(null);
        setDrawingBusy(false);
    };

    const gradeDrawingAnswer = async (
        check: SmartLearningCheck,
        payload: { ink_png_base64?: string; image_base64?: string }
    ): Promise<DrawingGradeResult> => {
        setDrawingBusy(true);
        setError("");
        try {
            const res = await fetch(
                `/api/ai/smart-learning/grade-drawing?session_id=${encodeURIComponent(sessionId)}`,
                {
                    method: "POST",
                    headers: {
                        "Content-Type": "application/json",
                        "X-Session-Id": sessionId,
                    },
                    body: JSON.stringify({
                        username,
                        folder_id: folderId,
                        prompt: check.prompt,
                        answer: check.answer,
                        rubric: check.rubric || [],
                        explanation: check.explanation || "",
                        ink_png_base64: payload.ink_png_base64,
                        image_base64: payload.image_base64,
                    }),
                }
            );
            let data: Record<string, unknown> = await res.json().catch(() => ({}));
            if (res.status === 202 && data.job_id) {
                data = await pollSmartJob(String(data.job_id));
            }
            if ((!res.ok && res.status !== 202) && !data.grade) {
                throw new Error(formatApiDetail(data.detail) || `HTTP ${res.status}`);
            }
            const gradeRaw = (data.grade && typeof data.grade === "object" ? data.grade : data) as Record<
                string,
                unknown
            >;
            if (
                gradeRaw.score == null &&
                gradeRaw.correct == null &&
                !String(gradeRaw.feedback || "").trim()
            ) {
                throw new Error(formatApiDetail(data.detail) || "Keine Bewertung erhalten.");
            }
            const score = Math.max(0, Math.min(2, Number(gradeRaw.score) || 0));
            const result: DrawingGradeResult = {
                correct: score >= 2 || gradeRaw.correct === true,
                score,
                feedback: String(gradeRaw.feedback || "").trim() || (score >= 2 ? "Passt." : "Noch nicht ausreichend."),
                matched_rubric: Array.isArray(gradeRaw.matched_rubric)
                    ? gradeRaw.matched_rubric.map((x) => String(x))
                    : [],
            };
            setDrawingGrade(result);
            return result;
        } catch (err) {
            const msg = err instanceof Error ? err.message : "Auswertung fehlgeschlagen.";
            setError(`${msg} Du kannst überspringen oder erneut versuchen.`);
            throw err;
        } finally {
            setDrawingBusy(false);
        }
    };

    const stopTeachTts = () => {
        if (ttsAudioRef.current) {
            ttsAudioRef.current.pause();
            ttsAudioRef.current = null;
        }
        if (ttsUrlRef.current) {
            try {
                URL.revokeObjectURL(ttsUrlRef.current);
            } catch {
                /* ignore */
            }
            ttsUrlRef.current = null;
        }
        setTtsBusy(false);
        setTtsPlaying(false);
    };

    const playTeachTts = async (markdown: string) => {
        if (ttsBusy) return;
        const text = plainTextForTts(markdown);
        if (!text) {
            setError("Kein Text zum Vorlesen.");
            return;
        }
        stopTeachTts();
        setTtsBusy(true);
        setError("");
        try {
            const res = await fetch(`/api/ai/tts-preview?session_id=${encodeURIComponent(sessionId)}`, {
                method: "POST",
                headers: {
                    "Content-Type": "application/json",
                    "X-Session-Id": sessionId,
                },
                body: JSON.stringify({
                    username,
                    voice: learnVoice || "charon",
                    text: text.slice(0, 3800),
                }),
            });
            if (!res.ok) {
                let msg = `HTTP ${res.status}`;
                try {
                    const err = await res.json();
                    msg = formatApiDetail(err.detail) || msg;
                } catch {
                    /* ignore */
                }
                throw new Error(msg);
            }
            const blob = await res.blob();
            const url = URL.createObjectURL(blob);
            ttsUrlRef.current = url;
            const audio = new Audio(url);
            ttsAudioRef.current = audio;
            audio.onended = () => {
                setTtsPlaying(false);
                setTtsBusy(false);
            };
            audio.onerror = () => {
                setTtsPlaying(false);
                setTtsBusy(false);
                setError("Audio konnte nicht abgespielt werden.");
            };
            setTtsPlaying(true);
            await audio.play();
        } catch (e: any) {
            setError(e?.message || "Vorlesen fehlgeschlagen.");
            setTtsPlaying(false);
        } finally {
            setTtsBusy(false);
        }
    };

    useEffect(() => {
        return () => {
            if (ttsAudioRef.current) ttsAudioRef.current.pause();
            if (ttsUrlRef.current) {
                try {
                    URL.revokeObjectURL(ttsUrlRef.current);
                } catch {
                    /* ignore */
                }
            }
        };
    }, []);

    const enterAssessPhase = (batch: FlatStep[]) => {
        stopTeachTts();
        const items: AssessItem[] = batch.map((b) => ({ ...b, check: b.lesson.check }));
        shuffleInPlace(items);
        setAssessQueue(items);
        setAssessIdx(0);
        setReviewItem(null);
        setDeferCountByLessonId({});
        setFailCounts({});
        setLearnPhase("assess");
        resetAssessInputs();
        setHearts(HEARTS_MAX);
    };

    const activeDeepSession = useMemo(() => {
        const sessions = local.deep_sessions || [];
        const aid = local.active_deep_session_id;
        if (aid) {
            const found = sessions.find((s) => s.id === aid);
            if (found) return found;
        }
        return sessions.length ? sessions[sessions.length - 1] : null;
    }, [local.active_deep_session_id, local.deep_sessions]);

    const resetDeepExerciseInputs = () => {
        setDeepSelected(null);
        setDeepCloze([]);
        setDeepClozeDone(false);
        setDeepShort("");
        setDeepCardIdx(0);
        setDeepCardFlipped(false);
        setDrawingGrade(null);
        setDrawingBusy(false);
    };

    const startDeepSession = (session: DeepLearningSession) => {
        const prog = local.deep_session_progress?.[session.id];
        const done = new Set(prog?.completed_step_ids || []);
        let idx = session.steps.findIndex((s) => !done.has(s.id));
        if (idx < 0) idx = 0;
        setDeepStepIdx(idx);
        const note = prog?.notes?.[session.steps[idx]?.id || ""] || "";
        setDeepNoteDraft(note);
        resetDeepExerciseInputs();
        setMode("deep");
    };

    const generateDeepSession = async (minutes?: DeepBudgetMinutes) => {
        const budget = minutes || deepBudget;
        setDeepBudget(budget);
        setDeepBusy(true);
        setError("");
        try {
            const res = await fetch(
                `/api/ai/smart-learning/deep-session?session_id=${encodeURIComponent(sessionId)}`,
                {
                    method: "POST",
                    headers: {
                        "Content-Type": "application/json",
                        "X-Session-Id": sessionId,
                    },
                    body: JSON.stringify({
                        username,
                        folder_id: folderId,
                        target_minutes: budget,
                        focus: local.title || undefined,
                    }),
                }
            );
            let data: Record<string, unknown> = await res.json().catch(() => ({}));
            if (res.status === 202 && data.job_id) {
                data = await pollSmartJob(String(data.job_id));
            }
            if (!res.ok && !data.smart_learning && !data.deep_session) {
                throw new Error(formatApiDetail(data.detail) || `HTTP ${res.status}`);
            }
            const journeySource =
                data.smart_learning && typeof data.smart_learning === "object"
                    ? data.smart_learning
                    : data.deep_session && typeof data.deep_session === "object"
                      ? {
                            ...local,
                            deep_sessions: [...(local.deep_sessions || []), data.deep_session],
                            active_deep_session_id: (data.deep_session as { id?: string }).id,
                        }
                      : data;
            const updated = normalizeSmartLearningContent(journeySource, local.title);
            setLocal(updated);
            onContentUpdated?.(updated);
            const sessions = updated.deep_sessions || [];
            const sid = updated.active_deep_session_id || sessions[sessions.length - 1]?.id;
            const session = sessions.find((s) => s.id === sid) || sessions[sessions.length - 1];
            if (session) startDeepSession(session);
            else setError("Deep Session erzeugt, aber leer — bitte nochmal versuchen.");
        } catch (e: any) {
            setError(e?.message || "Tiefes Lernen fehlgeschlagen.");
        } finally {
            setDeepBusy(false);
        }
    };

    useEffect(() => {
        if (deepAutoStartedRef.current) return;
        let minutes: DeepBudgetMinutes | null = autoStartDeepMinutes || null;
        if (!minutes && typeof window !== "undefined") {
            const raw = sessionStorage.getItem("blop_smart_deep_minutes");
            const n = Number(raw);
            if (DEEP_BUDGETS.includes(n as DeepBudgetMinutes)) {
                minutes = n as DeepBudgetMinutes;
                sessionStorage.removeItem("blop_smart_deep_minutes");
            }
        }
        if (!minutes) return;
        deepAutoStartedRef.current = true;
        void generateDeepSession(minutes);
        // eslint-disable-next-line react-hooks/exhaustive-deps -- one-shot after open
    }, [autoStartDeepMinutes]);

    const persistDeepStep = async (sessionIdKey: string, stepId: string, noteText?: string) => {
        const prevEntry = local.deep_session_progress?.[sessionIdKey] || { completed_step_ids: [], notes: {} };
        const completed = Array.from(new Set([...(prevEntry.completed_step_ids || []), stepId]));
        const notes = { ...(prevEntry.notes || {}) };
        if (noteText != null) notes[stepId] = noteText;
        const next: SmartLearningContent = {
            ...local,
            active_deep_session_id: sessionIdKey,
            deep_session_progress: {
                ...(local.deep_session_progress || {}),
                [sessionIdKey]: {
                    ...prevEntry,
                    completed_step_ids: completed,
                    notes,
                },
            },
        };
        setLocal(next);
        await persistProgress(next);
    };

    const startIdkReview = (item: AssessItem) => {
        stopTeachTts();
        setReviewItem(item);
        setLearnPhase("review");
        resetAssessInputs();
    };

    const finishIdkReviewAndDefer = () => {
        stopTeachTts();
        if (!reviewItem) {
            setLearnPhase("assess");
            return;
        }
        const lessonId = reviewItem.lesson.id;
        const prevCount = deferCountByLessonId[lessonId] || 0;
        const nextCount = prevCount + 1;
        setDeferCountByLessonId((m) => ({ ...m, [lessonId]: nextCount }));
        void deferLessonIds([lessonId]);

        let removeAt = assessIdx;
        if (removeAt < 0 || removeAt >= assessQueue.length || assessQueue[removeAt]?.lesson.id !== lessonId) {
            removeAt = assessQueue.findIndex((q) => q.lesson.id === lessonId);
        }
        if (removeAt < 0) {
            setReviewItem(null);
            resetAssessInputs();
            setLearnPhase("assess");
            return;
        }
        const copy = [...assessQueue];
        const [moved] = copy.splice(removeAt, 1);
        if (moved && nextCount <= MAX_DEFER_PER_LESSON) {
            copy.push(moved);
        } else {
            // Permanently skip this batch — keep lesson deferred for later path
            setTeachQueue((q) => {
                const nextQ = q.filter((s) => s.lesson.id !== lessonId);
                setBatchTarget(nextQ.length);
                return nextQ;
            });
        }
        setAssessQueue(copy);
        setAssessIdx(copy.length === 0 ? 0 : Math.min(removeAt, copy.length - 1));
        setReviewItem(null);
        resetAssessInputs();
        setLearnPhase("assess");
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
        setExamOrder(built.questions.map((_, i) => i));
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
            <button
                type="button"
                onClick={() => void goToOverview()}
                className="inline-flex items-center gap-1.5 px-3 py-1.5 rounded-lg border border-[#2A3140] text-gray-200 hover:bg-[#243044] hover:text-white font-medium"
            >
                <ChevronLeft size={14} />
                Zur Übersicht
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
        const teachDone = learnPhase === "assess" || learnPhase === "review" ? batchTarget : teachIdx;
        const phaseTitle =
            learnPhase === "teach"
                ? "Lehrbuch"
                : learnPhase === "cards"
                  ? "Karteikarten"
                  : learnPhase === "review"
                    ? "Nochmal"
                    : "Wissenstest";
        return (
            <div className="space-y-2">
                <div className="flex items-center justify-between text-xs text-gray-500">
                    <span>
                        {phaseLabel} · Themen-Sammlung{" "}
                        {Math.min(
                            teachDone + (learnPhase === "teach" || learnPhase === "cards" ? 1 : 0),
                            batchTarget
                        )}
                        /{batchTarget}
                    </span>
                    <span className="text-[#9EC5FF]">{phaseTitle}</span>
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

    /* ——— Learn: review after „Ich weiß nicht“ ——— */
    if (mode === "learn" && learnPhase === "review" && reviewItem) {
        return (
            <div className="w-full max-w-3xl mx-auto space-y-5 pb-8">
                {hudBar}
                <BatchProgress phaseLabel={reviewItem.chapterTitle} />
                <div className="rounded-2xl border border-amber-500/30 bg-[#1A1F2A] p-6 sm:p-7 space-y-5 shadow-lg">
                    <div className="flex items-center gap-2">
                        <span className="inline-flex items-center gap-1.5 text-[11px] uppercase tracking-widest text-amber-300 font-semibold">
                            <BookOpen size={12} /> Nochmal erklären
                        </span>
                        <span className="text-[11px] text-gray-500">Frage kommt danach wieder</span>
                    </div>
                    <MathMarkdown className="text-white prose prose-invert prose-blop max-w-none leading-relaxed prose-p:my-3 prose-li:my-1">
                        {reviewItem.lesson.teach}
                    </MathMarkdown>
                    {reviewItem.check.explanation ? (
                        <div className="text-sm text-gray-300 border border-[#2A3140] rounded-xl px-4 py-3 bg-[#12141A]">
                            <p className="text-[11px] uppercase tracking-wide text-gray-500 mb-1.5">Tipp zur Frage</p>
                            <MathMarkdown compact>{reviewItem.check.explanation}</MathMarkdown>
                        </div>
                    ) : null}
                    <div className="flex flex-wrap items-center gap-2">
                        <button
                            type="button"
                            onClick={() => retreatLearnUnit()}
                            className="inline-flex items-center gap-2 px-4 py-3 rounded-xl border border-[#2A3140] text-gray-200 hover:bg-[#243044] font-medium min-h-[48px]"
                        >
                            <ChevronLeft size={16} />
                            Zurück zur Frage
                        </button>
                        <select
                            value={learnVoice}
                            onChange={(e) => setLearnVoice(e.target.value)}
                            className="rounded-xl border border-[#2A3140] bg-[#0E1117] px-3 py-2.5 text-xs text-gray-300 focus:outline-none focus:ring-2 focus:ring-[#5B9DFF]/40 min-h-[48px]"
                            aria-label="Vorlese-Stimme"
                        >
                            {LEARN_VOICES.map((v) => (
                                <option key={v.id} value={v.id}>
                                    {v.label}
                                </option>
                            ))}
                        </select>
                        <button
                            type="button"
                            disabled={ttsBusy && !ttsPlaying}
                            onClick={() => {
                                if (ttsPlaying) stopTeachTts();
                                else void playTeachTts(reviewItem.lesson.teach);
                            }}
                            className="inline-flex items-center gap-2 px-4 py-3 rounded-xl border border-[#5B9DFF]/35 text-[#9EC5FF] hover:bg-[#5B9DFF]/10 font-medium min-h-[48px] disabled:opacity-50"
                        >
                            {ttsBusy && !ttsPlaying ? (
                                <Loader2 size={16} className="animate-spin" />
                            ) : ttsPlaying ? (
                                <Pause size={16} />
                            ) : (
                                <Volume2 size={16} />
                            )}
                            {ttsPlaying ? "Stoppen" : "Vorlesen"}
                        </button>
                        <button
                            type="button"
                            onClick={finishIdkReviewAndDefer}
                            className="inline-flex items-center gap-2 px-5 py-3.5 rounded-xl bg-[#5B9DFF] hover:bg-[#4A8AE6] text-white font-semibold min-h-[48px]"
                        >
                            Später nochmal fragen
                            <ChevronRight size={18} />
                        </button>
                    </div>
                </div>
            </div>
        );
    }

    /* ——— Learn: teach batch ——— */
    if (mode === "learn" && learnPhase === "teach" && teachQueue[teachIdx]) {
        const current = teachQueue[teachIdx];
        const remainingAfter = Math.max(0, batchTarget - teachIdx - 1);
        return (
            <div className="w-full max-w-3xl mx-auto space-y-5 pb-8">
                {hudBar}
                <BatchProgress phaseLabel={current.chapterTitle} />
                <div className="rounded-2xl border border-[#2A3140] bg-[#1A1F2A] p-6 sm:p-8 space-y-5 shadow-lg">
                    <div className="flex flex-wrap items-center gap-2 justify-between">
                        <div className="flex items-center gap-2">
                            <span className="inline-flex items-center gap-1.5 text-[11px] uppercase tracking-widest text-[#5B9DFF] font-semibold">
                                <BookOpen size={12} /> Lehrkapitel
                            </span>
                            <span className="text-[11px] text-gray-500">
                                Lektion {teachIdx + 1}/{batchTarget}
                            </span>
                        </div>
                        <p className="text-[11px] text-gray-500">
                            {remainingAfter > 0
                                ? `Noch ${remainingAfter} Thema${remainingAfter === 1 ? "" : "en"}, danach Wissenstest`
                                : batchTarget > 1
                                  ? "Letztes Thema — danach Wissenstest"
                                  : "Danach kurzer Wissenstest"}
                        </p>
                    </div>
                    <MathMarkdown className="text-white prose prose-invert prose-blop max-w-none leading-relaxed prose-p:my-3 prose-li:my-1 prose-headings:text-white">
                        {current.lesson.teach}
                    </MathMarkdown>
                    <div className="flex flex-wrap items-center gap-2">
                        <button
                            type="button"
                            disabled={!canRetreatLearn()}
                            onClick={() => retreatLearnUnit()}
                            className="inline-flex items-center gap-2 px-4 py-3 rounded-xl border border-[#2A3140] text-gray-200 hover:bg-[#243044] font-medium min-h-[48px] disabled:opacity-40"
                        >
                            <ChevronLeft size={16} />
                            Zurück
                        </button>
                        <select
                            value={learnVoice}
                            onChange={(e) => setLearnVoice(e.target.value)}
                            className="rounded-xl border border-[#2A3140] bg-[#0E1117] px-3 py-2.5 text-xs text-gray-300 focus:outline-none focus:ring-2 focus:ring-[#5B9DFF]/40 min-h-[48px]"
                            aria-label="Vorlese-Stimme"
                        >
                            {LEARN_VOICES.map((v) => (
                                <option key={v.id} value={v.id}>
                                    {v.label}
                                </option>
                            ))}
                        </select>
                        <button
                            type="button"
                            disabled={ttsBusy && !ttsPlaying}
                            onClick={() => {
                                if (ttsPlaying) stopTeachTts();
                                else void playTeachTts(current.lesson.teach);
                            }}
                            className="inline-flex items-center gap-2 px-4 py-3 rounded-xl border border-[#5B9DFF]/35 text-[#9EC5FF] hover:bg-[#5B9DFF]/10 font-medium min-h-[48px] disabled:opacity-50"
                        >
                            {ttsBusy && !ttsPlaying ? (
                                <Loader2 size={16} className="animate-spin" />
                            ) : ttsPlaying ? (
                                <Pause size={16} />
                            ) : (
                                <Volume2 size={16} />
                            )}
                            {ttsPlaying ? "Stoppen" : "Vorlesen"}
                        </button>
                        <button
                            type="button"
                            onClick={() => void deferCurrentLearnLesson()}
                            className="inline-flex items-center gap-2 px-4 py-3 rounded-xl border border-amber-500/35 text-amber-100 hover:bg-amber-500/10 font-medium min-h-[48px]"
                        >
                            <SkipForward size={16} />
                            Später
                        </button>
                        <button
                            type="button"
                            onClick={() => advanceAfterLearnUnit()}
                            className="inline-flex items-center gap-2 px-5 py-3.5 rounded-xl bg-[#5B9DFF] hover:bg-[#4A8AE6] text-white font-semibold min-h-[48px]"
                        >
                            {contentMode() === "mix" && cardsForLesson(current.lesson).length > 0
                                ? "Zu den Karteikarten"
                                : teachIdx + 1 >= teachQueue.length
                                  ? batchTarget > 1
                                      ? "Zum Wissenstest"
                                      : "Verstanden — jetzt testen"
                                  : "Nächstes Thema"}
                            <ChevronRight size={18} />
                        </button>
                    </div>
                </div>
                {error ? <p className="text-sm text-red-400">{error}</p> : null}
            </div>
        );
    }

    /* ——— Learn: flashcards for current lesson ——— */
    if (mode === "learn" && learnPhase === "cards" && teachQueue[teachIdx]) {
        const current = teachQueue[teachIdx];
        const cards = cardsForLesson(current.lesson);
        if (cards.length === 0) {
            return (
                <div className="w-full max-w-2xl mx-auto space-y-5 pb-8">
                    {hudBar}
                    <BatchProgress phaseLabel={current.chapterTitle} />
                    <p className="text-sm text-gray-400">Keine Karten fuer dieses Thema.</p>
                    <button
                        type="button"
                        onClick={() => advanceAfterLearnUnit()}
                        className="inline-flex items-center gap-2 px-5 py-3 rounded-xl bg-[#5B9DFF] text-white font-semibold"
                    >
                        Weiter
                        <ChevronRight size={18} />
                    </button>
                </div>
            );
        }
        const card = cards[Math.min(cardIdx, cards.length - 1)];
        return (
            <div className="w-full max-w-2xl mx-auto space-y-5 pb-8">
                {hudBar}
                <BatchProgress phaseLabel={current.chapterTitle} />
                <div className="rounded-2xl border border-[#2A3140] bg-[#1A1F2A] p-6 sm:p-7 space-y-4 shadow-lg">
                    <div className="flex flex-wrap items-center justify-between gap-2">
                        <span className="inline-flex items-center gap-1.5 text-[11px] uppercase tracking-widest text-[#5B9DFF] font-semibold">
                            <Layers size={12} /> Karteikarten
                        </span>
                        <span className="text-[11px] text-gray-500">
                            Karte {cardIdx + 1}/{cards.length} · Lektion {teachIdx + 1}/{batchTarget}
                        </span>
                    </div>
                    <button
                        type="button"
                        onClick={() => setCardFlipped((f) => !f)}
                        className="w-full min-h-[180px] rounded-2xl border border-[#5B9DFF]/30 bg-[#12141A] px-5 py-8 text-left hover:border-[#5B9DFF]/55 transition-colors"
                    >
                        <p className="text-[11px] uppercase tracking-wide text-gray-500 mb-3">
                            {cardFlipped ? "Rückseite" : "Vorderseite"} · tippen zum Umdrehen
                        </p>
                        <MathMarkdown className="text-lg text-white leading-relaxed">
                            {cardFlipped ? card.back : card.front}
                        </MathMarkdown>
                    </button>
                    <div className="flex flex-wrap gap-2">
                        <button
                            type="button"
                            disabled={!canRetreatLearn()}
                            onClick={() => retreatLearnUnit()}
                            className="inline-flex items-center gap-2 px-4 py-3 rounded-xl border border-[#2A3140] text-gray-200 hover:bg-[#243044] min-h-[48px] disabled:opacity-40"
                        >
                            <ChevronLeft size={16} />
                            Zurück
                        </button>
                        <button
                            type="button"
                            onClick={() => setCardFlipped((f) => !f)}
                            className="inline-flex items-center gap-2 px-4 py-3 rounded-xl border border-[#2A3140] text-gray-200 hover:bg-[#243044] min-h-[48px]"
                        >
                            Umdrehen
                        </button>
                        <button
                            type="button"
                            onClick={() => void deferCurrentLearnLesson()}
                            className="inline-flex items-center gap-2 px-4 py-3 rounded-xl border border-amber-500/35 text-amber-100 hover:bg-amber-500/10 font-medium min-h-[48px]"
                        >
                            <SkipForward size={16} />
                            Später
                        </button>
                        <button
                            type="button"
                            onClick={() => advanceAfterLearnUnit()}
                            className="inline-flex items-center gap-2 px-5 py-3.5 rounded-xl bg-[#5B9DFF] hover:bg-[#4A8AE6] text-white font-semibold min-h-[48px]"
                        >
                            {cardIdx + 1 >= cards.length
                                ? teachIdx + 1 >= teachQueue.length
                                    ? "Zum Wissenstest"
                                    : contentMode() === "flashcards"
                                      ? "Nächste Lektion"
                                      : "Weiter lernen"
                                : "Nächste Karte"}
                            <ChevronRight size={18} />
                        </button>
                    </div>
                </div>
                {error ? <p className="text-sm text-red-400">{error}</p> : null}
            </div>
        );
    }

    /* ——— Learn: assess batch empty after defer ——— */
    if (mode === "learn" && learnPhase === "assess" && assessQueue.length === 0) {
        return (
            <div className="w-full max-w-2xl mx-auto space-y-5 pb-8">
                {hudBar}
                <div className="rounded-2xl border border-[#2A3140] bg-[#1A1F2A] p-6 text-center space-y-4">
                    <p className="text-sm text-gray-300">Keine offenen Fragen mehr in diesem Block.</p>
                    <button
                        type="button"
                        onClick={async () => {
                            const xp = await markLessonsDone(teachQueue);
                            setCelebrate({ title: "Block geschafft", xp: xp || 25 * teachQueue.length });
                        }}
                        className="inline-flex items-center gap-2 px-5 py-3 rounded-xl bg-[#5B9DFF] text-white font-semibold"
                    >
                        Block abschließen
                    </button>
                </div>
            </div>
        );
    }

    /* ——— Learn: assess batch ——— */
    if (mode === "learn" && learnPhase === "assess" && assessQueue[assessIdx]) {
        const current = assessQueue[assessIdx];
        const check = current.check;
        const isCloze = check.type === "cloze";
        const isShort = check.type === "short_answer";
        const isMatch = check.type === "matching";
        const isDrawing = check.type === "drawing";
        const clozeOk =
            isCloze && clozeSubmitted
                ? gradeCloze(check.prompt, check.blanks, clozeValues, check.accepted_answers)
                : false;
        const shortOk = isShort && clozeSubmitted ? gradeShortAnswer(check, shortAnswer) : false;
        const matchOk = isMatch && clozeSubmitted ? gradeMatching(check, clozeValues) : false;
        const drawingOk = isDrawing && drawingGrade != null && (drawingGrade.score >= 2 || drawingGrade.correct);
        const drawingPartial = isDrawing && drawingGrade != null && drawingGrade.score === 1 && !drawingOk;
        const mcOk =
            !isCloze &&
            !isShort &&
            !isMatch &&
            !isDrawing &&
            selected != null &&
            answerAccepted(check.answer, selected, check.accepted_answers);
        const correct = isCloze
            ? clozeOk
            : isShort
              ? shortOk
              : isMatch
                ? matchOk
                : isDrawing
                  ? drawingOk
                  : mcOk;
        const revealed = isCloze || isShort || isMatch ? clozeSubmitted : isDrawing ? drawingGrade != null : selected != null;
        const fails = failCounts[current.lesson.id] || 0;
        const forceReveal = !correct && revealed && fails >= MAX_FAILS_BEFORE_REVEAL;

        const bumpFail = () => {
            setFailCounts((prev) => ({
                ...prev,
                [current.lesson.id]: (prev[current.lesson.id] || 0) + 1,
            }));
        };

        const resetAssessInputs = () => {
            setSelected(null);
            setClozeValues([]);
            setClozeSubmitted(false);
            setShortAnswer("");
            setDrawingGrade(null);
            setDrawingBusy(false);
            setError("");
        };

        const onContinueAfterAssess = async () => {
            if (correct || forceReveal) {
                await undeferLessonIds([current.lesson.id]);
            }
            if (assessIdx + 1 < assessQueue.length) {
                setAssessIdx((i) => i + 1);
                resetAssessInputs();
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
                    <div className="flex items-center gap-2 flex-wrap">
                        <button
                            type="button"
                            disabled={!canRetreatLearn()}
                            onClick={() => retreatLearnUnit()}
                            className="inline-flex items-center gap-1.5 px-3 py-1.5 rounded-lg border border-[#2A3140] text-gray-300 hover:bg-[#243044] text-[11px] disabled:opacity-40"
                        >
                            <ChevronLeft size={12} />
                            Zurück
                        </button>
                        <span className="inline-flex items-center gap-1.5 text-[11px] uppercase tracking-widest text-amber-300/90 font-semibold">
                            {isCloze || isShort || isMatch || isDrawing ? <PenLine size={12} /> : <HelpCircle size={12} />}
                            {isCloze
                                ? "Lückentext"
                                : isShort
                                  ? "Freitext"
                                  : isMatch
                                    ? "Zuordnen"
                                    : isDrawing
                                      ? "Zeichnen"
                                      : "Quiz"}
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
                    ) : isMatch ? (
                        <MatchingExercise
                            prompt={check.prompt}
                            lefts={check.options}
                            rights={check.blanks || []}
                            values={clozeValues}
                            onChange={setClozeValues}
                            disabled={clozeSubmitted && matchOk}
                            showResult={clozeSubmitted}
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
                    ) : isDrawing ? (
                        <>
                            <MathMarkdown className="text-lg font-medium text-white">{check.prompt}</MathMarkdown>
                            {!drawingOk && !forceReveal ? (
                                <DrawingAnswerPad
                                    key={`assess-draw-${current.lesson.id}-${assessIdx}-${fails}`}
                                    allowUpload={check.allow_upload !== false}
                                    disabled={drawingBusy}
                                    busy={drawingBusy}
                                    onSubmit={(payload) => {
                                        void (async () => {
                                            try {
                                                const result = await gradeDrawingAnswer(check, payload);
                                                if (result.score >= 2 || result.correct) {
                                                    void grantXp(10, "drawing");
                                                } else {
                                                    bumpFail();
                                                    spendHeart();
                                                }
                                            } catch {
                                                /* soft fail — error banner + skip */
                                            }
                                        })();
                                    }}
                                />
                            ) : null}
                        </>
                    ) : (
                        <>
                            <MathMarkdown className="text-lg font-medium text-white">{check.prompt}</MathMarkdown>
                            {renderMcOptions(check, selected, (opt) => {
                                if (mcOk || forceReveal) return;
                                setSelected(opt);
                                if (answerAccepted(check.answer, opt, check.accepted_answers)) {
                                    void grantXp(10, "check");
                                } else {
                                    bumpFail();
                                    spendHeart();
                                }
                            })}
                        </>
                    )}

                    {(isCloze || isShort || isMatch) && !clozeSubmitted ? (
                        <div className="flex flex-wrap gap-2">
                            <button
                                type="button"
                                onClick={() => {
                                    setClozeSubmitted(true);
                                    const ok = isCloze
                                        ? gradeCloze(check.prompt, check.blanks, clozeValues, check.accepted_answers)
                                        : isMatch
                                          ? gradeMatching(check, clozeValues)
                                          : gradeShortAnswer(check, shortAnswer);
                                    if (ok)
                                        void grantXp(
                                            isCloze ? 5 : isMatch ? 8 : 8,
                                            isCloze ? "cloze" : isMatch ? "matching" : "short"
                                        );
                                    else {
                                        bumpFail();
                                        spendHeart();
                                    }
                                }}
                                disabled={
                                    isShort
                                        ? !shortAnswer.trim()
                                        : clozeValues.length === 0 ||
                                          clozeValues.some((v) => !String(v || "").trim())
                                }
                                className="inline-flex items-center gap-2 px-5 py-3.5 rounded-xl bg-[#5B9DFF] text-white font-semibold disabled:opacity-50 min-h-[48px]"
                            >
                                Antwort prüfen
                            </button>
                            <button
                                type="button"
                                onClick={() => startIdkReview(current)}
                                className="inline-flex items-center gap-2 px-5 py-3.5 rounded-xl border border-amber-500/35 text-amber-100 hover:bg-amber-500/10 font-medium min-h-[48px]"
                            >
                                <SkipForward size={16} />
                                Überspringen
                            </button>
                        </div>
                    ) : null}

                    {!isCloze && !isShort && !isMatch && !revealed ? (
                        <button
                            type="button"
                            onClick={() => startIdkReview(current)}
                            className="inline-flex items-center gap-2 px-5 py-3 rounded-xl border border-amber-500/35 text-amber-100 hover:bg-amber-500/10 font-medium min-h-[48px]"
                        >
                            <SkipForward size={16} />
                            Überspringen — weiß ich nicht
                        </button>
                    ) : null}

                    {revealed ? (
                        <div className="space-y-3 pt-1">
                            <div
                                className={`rounded-xl px-3 py-2 text-sm border ${
                                    correct
                                        ? "border-green-500/40 bg-green-500/10 text-green-300"
                                        : drawingPartial
                                          ? "border-amber-500/40 bg-amber-500/10 text-amber-200"
                                          : "border-red-500/40 bg-red-500/10 text-red-300"
                                }`}
                            >
                                {correct
                                    ? "Richtig — gut gemacht."
                                    : drawingPartial && !forceReveal
                                      ? "Teilweise richtig — ergänze die fehlenden Teile."
                                      : forceReveal
                                        ? "Versuch vorbei — hier die richtige Antwort."
                                        : hearts > 0
                                          ? `Leider falsch — noch ${hearts} ${hearts === 1 ? "Herz" : "Herzen"}.`
                                          : "Keine Herzen mehr."}
                            </div>
                            <AnswerRevealBlock
                                check={check}
                                correct={correct}
                                extraFeedback={isDrawing ? drawingGrade?.feedback : undefined}
                            />
                            {correct || forceReveal || hearts <= 0 ? (
                                <button
                                    type="button"
                                    onClick={() => void onContinueAfterAssess()}
                                    disabled={savingProgress}
                                    className="inline-flex items-center gap-2 px-5 py-3.5 rounded-xl bg-[#5B9DFF] text-white font-semibold disabled:opacity-50 min-h-[48px]"
                                >
                                    {savingProgress ? <Loader2 size={16} className="animate-spin" /> : null}
                                    {assessIdx + 1 >= assessQueue.length ? "Block abschließen" : "Weiter"}
                                    <ChevronRight size={18} />
                                </button>
                            ) : (
                                <div className="flex flex-wrap gap-2">
                                    <button
                                        type="button"
                                        onClick={resetAssessInputs}
                                        className="inline-flex items-center gap-2 px-5 py-3 rounded-xl border border-[#2A3140] text-gray-200 hover:bg-[#243044] min-h-[48px]"
                                    >
                                        Nochmal versuchen
                                    </button>
                                    <button
                                        type="button"
                                        onClick={() => startIdkReview(current)}
                                        className="inline-flex items-center gap-2 px-5 py-3 rounded-xl border border-amber-500/35 text-amber-100 hover:bg-amber-500/10 font-medium min-h-[48px]"
                                    >
                                        <SkipForward size={16} />
                                        Überspringen & später nochmal
                                    </button>
                                </div>
                            )}
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
        const order =
            examOrder.length === activeExam.questions.length
                ? examOrder
                : activeExam.questions.map((_, i) => i);
        const qRealIdx = order[examIndex] ?? examIndex;
        const q = activeExam.questions[qRealIdx];
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
        const isMatch = q.type === "matching";
        const isDrawing = q.type === "drawing";
        const revealed =
            isCloze || isShort || isMatch
                ? examClozeDone
                : isDrawing
                  ? drawingGrade != null
                  : examSelected != null;
        const mm = Math.floor((examSecondsLeft ?? 0) / 60);
        const ss = String((examSecondsLeft ?? 0) % 60).padStart(2, "0");
        const asCheck: SmartLearningCheck = {
            type: q.type,
            prompt: q.prompt,
            options: q.options,
            answer: q.answer,
            blanks: q.blanks,
            explanation: q.explanation,
            accepted_answers: q.accepted_answers,
            rubric: q.rubric,
            allow_upload: q.allow_upload,
        };
        const examCorrect = revealed
            ? isCloze
                ? gradeCloze(q.prompt, q.blanks, examCloze, q.accepted_answers)
                : isMatch
                  ? gradeMatching(asCheck, examCloze)
                  : isShort
                    ? gradeShortAnswer(asCheck, examShort)
                    : isDrawing
                      ? !!(drawingGrade && (drawingGrade.score >= 2 || drawingGrade.correct))
                      : answerAccepted(q.answer, examSelected || "", q.accepted_answers)
            : false;
        const resetExamInputs = () => {
            setExamSelected(null);
            setExamCloze([]);
            setExamClozeDone(false);
            setExamShort("");
            setDrawingGrade(null);
            setDrawingBusy(false);
            setError("");
        };

        const recordAndAdvance = (correct: boolean, given: string) => {
            const nextAnswers = { ...examAnswers, [qRealIdx]: { correct, given } };
            setExamAnswers(nextAnswers);
            if (examIndex + 1 >= order.length) {
                void finishExam(nextAnswers);
                return;
            }
            setExamIndex((i) => i + 1);
            resetExamInputs();
        };

        const skipExamQuestion = () => {
            if (examIndex >= order.length - 1) {
                // Last in queue: leave unanswered and finish if everything else done
                if (Object.keys(examAnswers).length >= order.length - 1) {
                    void finishExam({ ...examAnswers, [qRealIdx]: { correct: false, given: "" } });
                }
                return;
            }
            const next = [...order];
            const [moved] = next.splice(examIndex, 1);
            next.push(moved);
            setExamOrder(next);
            resetExamInputs();
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
                    {order.map((realIdx, pos) => {
                        const answered = examAnswers[realIdx] != null;
                        const current = pos === examIndex;
                        return (
                            <button
                                key={`${realIdx}-${pos}`}
                                type="button"
                                onClick={() => {
                                    if (revealed) return;
                                    setExamIndex(pos);
                                    resetExamInputs();
                                }}
                                className={`h-7 min-w-[1.75rem] px-1.5 rounded-md text-[11px] border ${
                                    current
                                        ? "border-[#5B9DFF] bg-[#5B9DFF]/20 text-[#9EC5FF]"
                                        : answered
                                          ? "border-green-500/40 bg-green-500/10 text-green-300"
                                          : "border-[#2A3140] text-gray-500"
                                }`}
                            >
                                {pos + 1}
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
                        {isCloze
                            ? " · Lückentext"
                            : isShort
                              ? " · Freitext"
                              : isMatch
                                ? " · Zuordnen"
                                : isDrawing
                                  ? " · Zeichnen"
                                  : " · Quiz"}
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
                    ) : isMatch ? (
                        <MatchingExercise
                            prompt={q.prompt}
                            lefts={q.options}
                            rights={q.blanks || []}
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
                    ) : isDrawing ? (
                        <>
                            <MathMarkdown className="text-lg font-medium text-white">{q.prompt}</MathMarkdown>
                            {!drawingGrade ? (
                                <DrawingAnswerPad
                                    key={`exam-draw-${qRealIdx}-${examIndex}`}
                                    allowUpload={q.allow_upload !== false}
                                    disabled={drawingBusy}
                                    busy={drawingBusy}
                                    onSubmit={(payload) => {
                                        void (async () => {
                                            try {
                                                await gradeDrawingAnswer(asCheck, payload);
                                            } catch {
                                                /* soft fail */
                                            }
                                        })();
                                    }}
                                />
                            ) : null}
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

                    {(isCloze || isShort || isMatch) && !examClozeDone ? (
                        <div className="flex flex-wrap gap-2">
                            <button
                                type="button"
                                onClick={() => setExamClozeDone(true)}
                                disabled={
                                    isShort
                                        ? !examShort.trim()
                                        : examCloze.length === 0 ||
                                          examCloze.some((v) => !String(v || "").trim())
                                }
                                className="px-5 py-3 rounded-xl bg-[#5B9DFF] text-white font-semibold disabled:opacity-50"
                            >
                                Antwort prüfen
                            </button>
                            <button
                                type="button"
                                onClick={skipExamQuestion}
                                className="inline-flex items-center gap-2 px-5 py-3 rounded-xl border border-amber-500/35 text-amber-100 hover:bg-amber-500/10 font-medium"
                            >
                                <SkipForward size={16} />
                                Überspringen
                            </button>
                        </div>
                    ) : null}

                    {!isCloze && !isShort && !isMatch && !revealed ? (
                        <button
                            type="button"
                            onClick={skipExamQuestion}
                            className="inline-flex items-center gap-2 px-5 py-3 rounded-xl border border-amber-500/35 text-amber-100 hover:bg-amber-500/10 font-medium"
                        >
                            <SkipForward size={16} />
                            Überspringen — später
                        </button>
                    ) : null}

                    {revealed ? (
                        <div className="space-y-3">
                            <div
                                className={`rounded-xl px-3 py-2 text-sm border ${
                                    examCorrect
                                        ? "border-green-500/40 bg-green-500/10 text-green-300"
                                        : isDrawing && drawingGrade?.score === 1
                                          ? "border-amber-500/40 bg-amber-500/10 text-amber-200"
                                          : "border-red-500/40 bg-red-500/10 text-red-300"
                                }`}
                            >
                                {examCorrect
                                    ? "Richtig."
                                    : isDrawing && drawingGrade?.score === 1
                                      ? "Teilweise — Lösung unten."
                                      : "Leider falsch — Lösung unten."}
                            </div>
                            <AnswerRevealBlock
                                check={asCheck}
                                correct={examCorrect}
                                extraFeedback={isDrawing ? drawingGrade?.feedback : undefined}
                            />
                            <div className="flex flex-wrap gap-2">
                                <button
                                    type="button"
                                    onClick={() => {
                                        if (isCloze) {
                                            recordAndAdvance(
                                                gradeCloze(q.prompt, q.blanks, examCloze, q.accepted_answers),
                                                examCloze.join(" | ")
                                            );
                                        } else if (isMatch) {
                                            recordAndAdvance(gradeMatching(asCheck, examCloze), examCloze.join(" | "));
                                        } else if (isShort) {
                                            recordAndAdvance(gradeShortAnswer(asCheck, examShort), examShort);
                                        } else if (isDrawing) {
                                            recordAndAdvance(
                                                examCorrect,
                                                drawingGrade?.feedback || (examCorrect ? "Zeichnung ok" : "Zeichnung")
                                            );
                                        } else {
                                            recordAndAdvance(examCorrect, examSelected || "");
                                        }
                                    }}
                                    className="inline-flex items-center gap-2 px-5 py-3 rounded-xl bg-[#5B9DFF] text-white font-semibold"
                                >
                                    {examIndex + 1 >= order.length ? "Ergebnis" : "Weiter"}
                                    <ChevronRight size={18} />
                                </button>
                                <button
                                    type="button"
                                    onClick={skipExamQuestion}
                                    className="inline-flex items-center gap-2 px-4 py-3 rounded-xl border border-amber-500/35 text-amber-100 hover:bg-amber-500/10 text-sm font-medium"
                                >
                                    <SkipForward size={16} />
                                    Überspringen
                                </button>
                                <button
                                    type="button"
                                    onClick={() => void finishExam({ ...examAnswers, [qRealIdx]: { correct: false, given: "" } })}
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

    /* ——— Deep learning session player ——— */
    if (mode === "deep" && activeDeepSession) {
        const session = activeDeepSession;
        const step = session.steps[deepStepIdx];
        const remainingMin = session.steps
            .slice(deepStepIdx)
            .reduce((sum, s) => sum + (s.estimated_minutes || 0), 0);
        const prog = local.deep_session_progress?.[session.id];
        const doneSet = new Set(prog?.completed_step_ids || []);

        const advanceDeep = async () => {
            if (!step) return;
            const noteVal = step.type === "note" ? deepNoteDraft : undefined;
            await persistDeepStep(session.id, step.id, noteVal);
            if (deepStepIdx + 1 >= session.steps.length) {
                setMode("overview");
                setCelebrate({ title: "Deep Session geschafft", xp: 40 });
                return;
            }
            const nextIdx = deepStepIdx + 1;
            setDeepStepIdx(nextIdx);
            const nextStep = session.steps[nextIdx];
            setDeepNoteDraft(prog?.notes?.[nextStep?.id || ""] || local.deep_session_progress?.[session.id]?.notes?.[nextStep?.id || ""] || "");
            resetDeepExerciseInputs();
        };

        if (!step) {
            return (
                <div className="text-center text-gray-400 py-10">
                    Keine Schritte.
                    <button type="button" className="block mx-auto mt-3 text-[#5B9DFF]" onClick={() => setMode("overview")}>
                        Zurück
                    </button>
                </div>
            );
        }

        const check = step.check;
        const isCloze = check?.type === "cloze";
        const isDrawing = check?.type === "drawing";
        const isShort =
            !isDrawing && (check?.type === "short_answer" || step.type === "recall");
        const isMatch = check?.type === "matching";
        const revealed =
            check &&
            (isCloze || isShort || isMatch
                ? deepClozeDone
                : isDrawing
                  ? drawingGrade != null
                  : deepSelected != null);
        const deepCorrect = revealed && check
            ? isCloze
                ? gradeCloze(check.prompt, check.blanks, deepCloze, check.accepted_answers)
                : isMatch
                  ? gradeMatching(check, deepCloze)
                  : isDrawing
                    ? !!(drawingGrade && (drawingGrade.score >= 2 || drawingGrade.correct))
                    : isShort
                      ? gradeShortAnswer(check, deepShort)
                      : answerAccepted(check.answer, deepSelected || "", check.accepted_answers)
            : false;

        return (
            <div className="w-full max-w-3xl mx-auto space-y-5 pb-10">
                {hudBar}
                <div className="flex items-center justify-between text-xs text-gray-400 gap-3">
                    <button type="button" onClick={() => setMode("overview")} className="hover:text-white">
                        ← Übersicht
                    </button>
                    <span className="truncate font-medium text-gray-300">{session.title}</span>
                    <span className="inline-flex items-center gap-1 text-[#5B9DFF] shrink-0">
                        <Timer size={12} /> ~{remainingMin} Min
                    </span>
                </div>
                <div className="h-1.5 rounded-full bg-[#23252A] border border-[#45484F] overflow-hidden">
                    <div
                        className="h-full bg-[#5B9DFF] transition-all"
                        style={{ width: `${((deepStepIdx + (doneSet.has(step.id) ? 1 : 0)) / Math.max(session.steps.length, 1)) * 100}%` }}
                    />
                </div>
                <div className="rounded-2xl border border-[#2A3140] bg-[#1A1F2A] p-6 sm:p-7 space-y-4">
                    <div className="flex items-center gap-2 flex-wrap">
                        <span className="text-[11px] uppercase tracking-widest text-[#9EC5FF] font-semibold">
                            {step.type} · Schritt {deepStepIdx + 1}/{session.steps.length}
                        </span>
                        <span className="text-[11px] text-gray-500">~{step.estimated_minutes} Min</span>
                    </div>
                    <h3 className="text-lg font-semibold text-white">{step.title}</h3>

                    {step.type === "teach" || step.type === "summary" || step.type === "break" ? (
                        <MathMarkdown className="text-white leading-relaxed">
                            {step.body || (step.type === "break" ? "Kurze Pause — atme durch, dann weiter." : "")}
                        </MathMarkdown>
                    ) : null}

                    {step.type === "note" ? (
                        <>
                            <p className="text-sm text-gray-300">{step.note_prompt || "Notiere in eigenen Worten."}</p>
                            <textarea
                                value={deepNoteDraft}
                                onChange={(e) => setDeepNoteDraft(e.target.value)}
                                rows={5}
                                className="w-full rounded-xl border border-[#2A3140] bg-[#0E1117] px-3 py-2 text-sm text-white focus:outline-none focus:ring-2 focus:ring-[#5B9DFF]/40"
                                placeholder="Deine Notiz…"
                            />
                        </>
                    ) : null}

                    {step.type === "flashcards" && step.flashcards?.length ? (
                        <div className="space-y-3">
                            <p className="text-xs text-gray-500">
                                Karte {deepCardIdx + 1}/{step.flashcards.length}
                            </p>
                            <button
                                type="button"
                                onClick={() => setDeepCardFlipped((f) => !f)}
                                className="w-full min-h-[140px] rounded-xl border border-[#2A3140] bg-[#12141A] p-5 text-left"
                            >
                                <MathMarkdown className="text-white">
                                    {deepCardFlipped
                                        ? step.flashcards[deepCardIdx]?.back || ""
                                        : step.flashcards[deepCardIdx]?.front || ""}
                                </MathMarkdown>
                                <p className="text-[11px] text-gray-500 mt-3">Tippen zum Umdrehen</p>
                            </button>
                            <div className="flex gap-2">
                                <button
                                    type="button"
                                    disabled={deepCardIdx <= 0}
                                    onClick={() => {
                                        setDeepCardIdx((i) => Math.max(0, i - 1));
                                        setDeepCardFlipped(false);
                                    }}
                                    className="px-3 py-2 rounded-lg border border-[#2A3140] text-gray-300 disabled:opacity-40"
                                >
                                    Zurück
                                </button>
                                <button
                                    type="button"
                                    disabled={deepCardIdx >= step.flashcards.length - 1}
                                    onClick={() => {
                                        setDeepCardIdx((i) => Math.min(step.flashcards!.length - 1, i + 1));
                                        setDeepCardFlipped(false);
                                    }}
                                    className="px-3 py-2 rounded-lg border border-[#2A3140] text-gray-300 disabled:opacity-40"
                                >
                                    Nächste Karte
                                </button>
                            </div>
                        </div>
                    ) : null}

                    {(step.type === "exercise" || step.type === "recall") && check ? (
                        <div className="space-y-3">
                            {isCloze ? (
                                <ClozeInputs
                                    prompt={check.prompt}
                                    blanks={check.blanks}
                                    values={deepCloze}
                                    onChange={setDeepCloze}
                                    disabled={deepClozeDone && deepCorrect}
                                    showResult={deepClozeDone && deepCorrect}
                                />
                            ) : isMatch ? (
                                <MatchingExercise
                                    prompt={check.prompt}
                                    lefts={check.options}
                                    rights={check.blanks || []}
                                    values={deepCloze}
                                    onChange={setDeepCloze}
                                    disabled={deepClozeDone && deepCorrect}
                                    showResult={deepClozeDone}
                                />
                            ) : isDrawing ? (
                                <>
                                    <MathMarkdown className="text-lg font-medium text-white">{check.prompt}</MathMarkdown>
                                    {!drawingGrade ? (
                                        <DrawingAnswerPad
                                            key={`deep-draw-${step.id}`}
                                            allowUpload={check.allow_upload !== false}
                                            disabled={drawingBusy}
                                            busy={drawingBusy}
                                            onSubmit={(payload) => {
                                                void (async () => {
                                                    try {
                                                        await gradeDrawingAnswer(check, payload);
                                                    } catch {
                                                        /* soft fail */
                                                    }
                                                })();
                                            }}
                                        />
                                    ) : null}
                                </>
                            ) : isShort ? (
                                <>
                                    <MathMarkdown className="text-lg font-medium text-white">{check.prompt}</MathMarkdown>
                                    <textarea
                                        value={deepShort}
                                        disabled={deepClozeDone && deepCorrect}
                                        onChange={(e) => setDeepShort(e.target.value)}
                                        rows={3}
                                        className="w-full rounded-xl border border-[#2A3140] bg-[#0E1117] px-3 py-2 text-sm text-white focus:outline-none focus:ring-2 focus:ring-[#5B9DFF]/40"
                                        placeholder="Antwort…"
                                    />
                                </>
                            ) : (
                                <>
                                    <MathMarkdown className="text-lg font-medium text-white">{check.prompt}</MathMarkdown>
                                    {renderMcOptions(check, deepSelected, (opt) => {
                                        if (deepSelected) return;
                                        setDeepSelected(opt);
                                    })}
                                </>
                            )}
                            {(isCloze || isShort || isMatch) && !deepClozeDone ? (
                                <button
                                    type="button"
                                    onClick={() => setDeepClozeDone(true)}
                                    disabled={
                                        isShort
                                            ? !deepShort.trim()
                                            : deepCloze.length === 0 || deepCloze.some((v) => !String(v || "").trim())
                                    }
                                    className="px-5 py-3 rounded-xl bg-[#5B9DFF] text-white font-semibold disabled:opacity-50"
                                >
                                    Antwort prüfen
                                </button>
                            ) : null}
                            {isDrawing && !revealed ? (
                                <button
                                    type="button"
                                    onClick={() =>
                                        setDrawingGrade({
                                            correct: false,
                                            score: 0,
                                            feedback: "Übersprungen.",
                                        })
                                    }
                                    className="inline-flex items-center gap-2 px-4 py-2 rounded-xl border border-amber-500/35 text-amber-100 hover:bg-amber-500/10 text-sm font-medium"
                                >
                                    <SkipForward size={14} />
                                    Überspringen
                                </button>
                            ) : null}
                            {revealed ? (
                                <AnswerRevealBlock
                                    check={check}
                                    correct={!!deepCorrect}
                                    extraFeedback={isDrawing ? drawingGrade?.feedback : undefined}
                                />
                            ) : null}
                        </div>
                    ) : null}

                    <div className="flex flex-wrap gap-2">
                        <button
                            type="button"
                            disabled={deepStepIdx <= 0}
                            onClick={() => {
                                const prevIdx = Math.max(0, deepStepIdx - 1);
                                setDeepStepIdx(prevIdx);
                                const prevStep = session.steps[prevIdx];
                                setDeepNoteDraft(prog?.notes?.[prevStep?.id || ""] || "");
                                resetDeepExerciseInputs();
                            }}
                            className="inline-flex items-center gap-2 px-4 py-3.5 rounded-xl border border-[#2A3140] text-gray-200 hover:bg-[#243044] min-h-[48px] disabled:opacity-40"
                        >
                            <ChevronLeft size={16} />
                            Zurück
                        </button>
                        <button
                            type="button"
                            onClick={() => void advanceDeep()}
                            disabled={
                                savingProgress ||
                                (step.type === "note" && !deepNoteDraft.trim()) ||
                                ((step.type === "exercise" || step.type === "recall") && check && !revealed)
                            }
                            className="inline-flex items-center gap-2 px-5 py-3.5 rounded-xl bg-[#5B9DFF] text-white font-semibold disabled:opacity-50 min-h-[48px]"
                        >
                            {savingProgress ? <Loader2 size={16} className="animate-spin" /> : null}
                            {deepStepIdx + 1 >= session.steps.length ? "Session abschließen" : "Weiter"}
                            <ChevronRight size={18} />
                        </button>
                    </div>
                </div>
                {error ? <p className="text-sm text-red-400">{error}</p> : null}
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

                    <div className="mt-5 rounded-xl border border-indigo-500/40 bg-indigo-500/10 p-4 space-y-3">
                        <div className="flex items-center gap-2">
                            <BrainCircuit size={18} className="text-indigo-300" />
                            <div>
                                <p className="text-sm font-semibold text-white">Intensive Lernsession</p>
                                <p className="text-[11px] text-indigo-200/80">Tiefes Lernen — halbe bis eineinhalb Stunden am Stück</p>
                            </div>
                        </div>
                        <p className="text-[11px] text-gray-400">
                            Wähle deine Lernzeit: die Session enthält Lehrtexte, Notizen, Übungen und Recall und dauert
                            ungefähr so lange. Erstellung kann ein paar Minuten brauchen.
                        </p>
                        <div className="flex flex-wrap gap-2">
                            {DEEP_BUDGETS.map((m) => (
                                <button
                                    key={m}
                                    type="button"
                                    onClick={() => setDeepBudget(m)}
                                    className={`px-3 py-2 rounded-lg text-xs font-medium border min-h-[40px] ${
                                        deepBudget === m
                                            ? "bg-indigo-500 text-white border-indigo-500"
                                            : "border-[#45484F] text-gray-300"
                                    }`}
                                >
                                    {m === 30 ? "½ Std" : m === 45 ? "¾ Std" : m === 60 ? "1 Std" : "1½ Std"} ({m} Min)
                                </button>
                            ))}
                        </div>
                        <div className="flex flex-wrap gap-2">
                            <button
                                type="button"
                                onClick={() => void generateDeepSession()}
                                disabled={deepBusy}
                                className="inline-flex items-center gap-2 px-4 py-2.5 rounded-xl bg-indigo-500 hover:bg-indigo-600 text-white text-sm font-semibold disabled:opacity-50 min-h-[44px]"
                            >
                                {deepBusy ? <Loader2 size={16} className="animate-spin" /> : <Sparkles size={16} />}
                                {deepBusy ? "Session wird erstellt…" : `${deepBudget}-Min-Session starten`}
                            </button>
                            {activeDeepSession ? (
                                <button
                                    type="button"
                                    onClick={() => startDeepSession(activeDeepSession)}
                                    className="inline-flex items-center gap-2 px-4 py-2.5 rounded-xl border border-indigo-400/40 text-indigo-200 text-sm font-medium min-h-[44px]"
                                >
                                    <Play size={16} />
                                    Letzte fortsetzen ({activeDeepSession.target_minutes} Min)
                                </button>
                            ) : null}
                        </div>
                    </div>

                    <div className="mt-4 flex flex-wrap gap-2">
                        {local.progress?.learn_checkpoint?.lesson_ids?.length ? (
                            <button
                                type="button"
                                onClick={() => resumeLearnCheckpoint()}
                                disabled={busyPractice}
                                className="inline-flex items-center justify-center gap-2 px-5 py-3 rounded-xl bg-amber-500/20 border border-amber-400/40 text-amber-100 hover:bg-amber-500/30 font-semibold disabled:opacity-50 transition-colors min-h-[48px]"
                            >
                                <Play size={18} />
                                Weiterlernen
                            </button>
                        ) : null}
                        <button
                            type="button"
                            onClick={startLearn}
                            disabled={busyPractice || flatSteps.length === 0}
                            className="inline-flex items-center justify-center gap-2 px-5 py-3 rounded-xl bg-[#5B9DFF] hover:bg-[#4A8AE6] text-white font-semibold disabled:opacity-50 transition-colors min-h-[48px]"
                        >
                            <Play size={18} />
                            {local.progress?.learn_checkpoint?.lesson_ids?.length
                                ? "Neu starten"
                                : "Kurzer Lernpfad"}
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
                    {settings.learn_content_mode === "concepts"
                        ? "Nur Lehrbuch → Wissenstest"
                        : settings.learn_content_mode === "flashcards"
                          ? "Nur Karteikarten → Wissenstest"
                          : "Lehrbuch + Karteikarten → Wissenstest"}
                    {" · "}
                    {settings.question_frequency === "compact"
                        ? "Fragen nach jedem Thema"
                        : settings.question_frequency === "chapter_end"
                          ? "Wissenstest am Kapitelende"
                          : "Wissenstest nach 3–4 Themen (gebündelt)"}
                    {settings.scope === "exam_week" || settings.unlock_all_chapters ? (
                        <>
                            {" · "}
                            {settings.scope_label || "Prüfungswoche"}
                            {settings.min_chapters && settings.max_chapters
                                ? ` · ${settings.min_chapters}–${settings.max_chapters} Themenblöcke`
                                : ""}
                            {" · alle Kapitel offen"}
                        </>
                    ) : settings.scope === "compact" ? (
                        <>{" · "}Kompakt</>
                    ) : null}
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
                        const locked = settings.unlock_all_chapters ? false : !prevDone;
                        const doneLessons = ch.lessons.filter((l) => completedLessonSet.has(l.id)).length;
                        const deferredInChapter = ch.lessons.filter(
                            (l) => deferredLessonSet.has(l.id) && !completedLessonSet.has(l.id)
                        ).length;
                        const chapterDone = ch.lessons.length > 0 && doneLessons === ch.lessons.length;
                        const openLessons = ch.lessons.filter((l) => !completedLessonSet.has(l.id));
                        return (
                            <div
                                key={ch.id}
                                className={`w-full text-left rounded-xl border px-4 py-3.5 ${
                                    locked
                                        ? "border-[#2A3140] bg-[#12141A] opacity-60"
                                        : "border-[#2A3140] bg-[#1A1F2A]"
                                }`}
                            >
                                <div className="flex items-start gap-3">
                                    {locked ? (
                                        <Lock size={16} className="text-gray-500 shrink-0 mt-0.5" />
                                    ) : (
                                        <BrainCircuit size={16} className="text-[#5B9DFF] shrink-0 mt-0.5" />
                                    )}
                                    <div className="flex-1 min-w-0 space-y-2">
                                        <div className="flex items-center gap-2 flex-wrap">
                                            <p className="text-sm font-medium text-white truncate flex-1 min-w-[8rem]">
                                                {ch.title}
                                            </p>
                                            <span className="flex items-center gap-1 shrink-0">
                                                {ch.lessons.map((l, li) => (
                                                    <span
                                                        key={l.id}
                                                        className={`h-1.5 w-1.5 rounded-full ${
                                                            completedLessonSet.has(l.id)
                                                                ? "bg-[#5B9DFF]"
                                                                : deferredLessonSet.has(l.id)
                                                                  ? "bg-amber-400"
                                                                  : "bg-[#3A4558]"
                                                        }`}
                                                        title={`Blatt ${li + 1}${
                                                            deferredLessonSet.has(l.id) ? " · später" : ""
                                                        }`}
                                                    />
                                                ))}
                                            </span>
                                            {chapterDone ? <CheckCircle2 size={16} className="text-green-400 shrink-0" /> : null}
                                        </div>
                                        <p className="text-[11px] text-gray-500">
                                            {doneLessons}/{ch.lessons.length || 0} Lektionen
                                            {deferredInChapter > 0 ? ` · ${deferredInChapter} später` : ""}
                                        </p>
                                        {ch.summary ? (
                                            <MathMarkdown compact className="text-xs text-gray-400 line-clamp-2">
                                                {ch.summary}
                                            </MathMarkdown>
                                        ) : null}
                                        {!locked && !chapterDone ? (
                                            <div className="flex flex-wrap gap-2 pt-1">
                                                <button
                                                    type="button"
                                                    onClick={() => startChapter(ch.id)}
                                                    className="inline-flex items-center gap-1.5 px-3 py-1.5 rounded-lg bg-[#5B9DFF]/15 border border-[#5B9DFF]/35 text-[#9EC5FF] text-xs font-medium hover:bg-[#5B9DFF]/25"
                                                >
                                                    <Play size={12} /> Lernen
                                                </button>
                                                {openLessons.length > 0 ? (
                                                    <button
                                                        type="button"
                                                        onClick={() =>
                                                            void deferLessonIds(openLessons.map((l) => l.id))
                                                        }
                                                        className="inline-flex items-center gap-1.5 px-3 py-1.5 rounded-lg border border-amber-500/35 text-amber-100 text-xs font-medium hover:bg-amber-500/10"
                                                    >
                                                        <SkipForward size={12} /> Thema später
                                                    </button>
                                                ) : null}
                                                {deferredInChapter > 0 ? (
                                                    <button
                                                        type="button"
                                                        onClick={() => {
                                                            void (async () => {
                                                                await undeferLessonIds(openLessons.map((l) => l.id));
                                                                startChapter(ch.id, { includeDeferred: true });
                                                            })();
                                                        }}
                                                        className="inline-flex items-center gap-1.5 px-3 py-1.5 rounded-lg border border-[#2A3140] text-gray-300 text-xs font-medium hover:bg-[#243044]"
                                                    >
                                                        Aufgeschobene jetzt
                                                    </button>
                                                ) : null}
                                            </div>
                                        ) : null}
                                        {!locked && ch.lessons.length > 0 ? (
                                            <ul className="space-y-1.5 pt-1 border-t border-[#2A3140]/80">
                                                {ch.lessons.map((l, li) => {
                                                    const done = completedLessonSet.has(l.id);
                                                    const deferred = deferredLessonSet.has(l.id) && !done;
                                                    const preview =
                                                        (l.teach || l.check?.prompt || `Blatt ${li + 1}`)
                                                            .replace(/\s+/g, " ")
                                                            .trim()
                                                            .slice(0, 72) || `Blatt ${li + 1}`;
                                                    return (
                                                        <li
                                                            key={l.id}
                                                            className="flex items-center gap-2 text-xs text-gray-400"
                                                        >
                                                            <span
                                                                className={`h-1.5 w-1.5 rounded-full shrink-0 ${
                                                                    done
                                                                        ? "bg-[#5B9DFF]"
                                                                        : deferred
                                                                          ? "bg-amber-400"
                                                                          : "bg-[#3A4558]"
                                                                }`}
                                                            />
                                                            <span className="flex-1 min-w-0 truncate text-gray-300">
                                                                {li + 1}. {preview}
                                                                {deferred ? " · später" : done ? " · fertig" : ""}
                                                            </span>
                                                            {!done ? (
                                                                <span className="flex items-center gap-1 shrink-0">
                                                                    <button
                                                                        type="button"
                                                                        onClick={() => void startLesson(l.id)}
                                                                        className="px-2 py-1 rounded-md border border-[#2A3140] text-[#9EC5FF] hover:bg-[#243044]"
                                                                    >
                                                                        Öffnen
                                                                    </button>
                                                                    {deferred ? (
                                                                        <button
                                                                            type="button"
                                                                            onClick={() => void undeferLessonIds([l.id])}
                                                                            className="px-2 py-1 rounded-md border border-[#2A3140] text-gray-300 hover:bg-[#243044]"
                                                                        >
                                                                            Zurück in Pfad
                                                                        </button>
                                                                    ) : (
                                                                        <button
                                                                            type="button"
                                                                            onClick={() => void deferLessonIds([l.id])}
                                                                            className="px-2 py-1 rounded-md border border-amber-500/30 text-amber-100 hover:bg-amber-500/10"
                                                                        >
                                                                            Später
                                                                        </button>
                                                                    )}
                                                                </span>
                                                            ) : null}
                                                        </li>
                                                    );
                                                })}
                                            </ul>
                                        ) : null}
                                    </div>
                                </div>
                            </div>
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
                                <p className="text-[11px] text-gray-500 mt-1">
                                    Einstellungen speichern oder Lernpfad neu generieren (längere Lehrbuch-Lektionen erst nach Neu-Generieren)
                                </p>
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
                                            ["standard", "Standard (3–4)"],
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
                            <div>
                                <p className="text-[11px] uppercase tracking-wide text-gray-500 mb-1.5">Lerninhalt</p>
                                <div className="flex flex-wrap gap-2">
                                    {(
                                        [
                                            ["concepts", "Nur Lehrbuch"],
                                            ["flashcards", "Nur Karteikarten"],
                                            ["mix", "Lehrbuch + Karten"],
                                        ] as const
                                    ).map(([k, label]) => (
                                        <button
                                            key={k}
                                            type="button"
                                            onClick={() => setDraftSettings((s) => ({ ...s, learn_content_mode: k }))}
                                            className={`px-3 py-1.5 rounded-lg text-xs border ${
                                                (draftSettings.learn_content_mode || "mix") === k
                                                    ? "bg-[#5B9DFF] text-white border-[#5B9DFF]"
                                                    : "border-[#45484F] text-gray-300"
                                            }`}
                                        >
                                            {label}
                                        </button>
                                    ))}
                                </div>
                                <p className="text-[11px] text-gray-500 mt-1.5">
                                    Steuert „Jetzt lernen“: erst Konzepte und/oder Karten, danach der Wissenstest.
                                </p>
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
