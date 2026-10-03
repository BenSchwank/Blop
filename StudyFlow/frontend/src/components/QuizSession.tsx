"use client";

import React, { useMemo, useState } from "react";
import { CheckCircle2, ChevronRight, RotateCcw, XCircle } from "lucide-react";

export interface QuizQuestion {
    question: string;
    options?: string[];
    answer: string;
    explanation?: string;
}

type Props = {
    questions: QuizQuestion[];
};

export default function QuizSession({ questions }: Props) {
    const items = useMemo(
        () => (Array.isArray(questions) ? questions.filter((q) => q && String(q.question || "").trim()) : []),
        [questions]
    );
    const [index, setIndex] = useState(0);
    const [selected, setSelected] = useState<string | null>(null);
    const [answers, setAnswers] = useState<Record<number, string>>({});
    const [phase, setPhase] = useState<"question" | "done">("question");
    const [reviewWrongOnly, setReviewWrongOnly] = useState(false);

    const queue = useMemo(() => {
        if (!reviewWrongOnly) return items.map((_, i) => i);
        return items
            .map((_, i) => i)
            .filter((i) => answers[i] != null && answers[i] !== items[i]?.answer);
    }, [answers, items, reviewWrongOnly]);

    const qIndex = queue[index] ?? 0;
    const q = items[qIndex];
    const total = queue.length;
    const progress = total === 0 ? 0 : Math.min(100, Math.round(((index + (selected ? 1 : 0)) / total) * 100));

    const correctCount = useMemo(() => {
        return items.reduce((acc, item, i) => (answers[i] != null && answers[i] === item.answer ? acc + 1 : acc), 0);
    }, [answers, items]);

    const wrongCount = Object.keys(answers).length - correctCount;

    if (items.length === 0) {
        return <p className="text-gray-400 text-sm">Fehlerhaftes oder leeres Quiz-Format.</p>;
    }

    if (phase === "done") {
        const pct = Math.round((100 * correctCount) / items.length);
        return (
            <div className="max-w-xl mx-auto space-y-6 pb-12">
                <div className="rounded-2xl border border-[#45484F] bg-[#353840] p-8 text-center">
                    <p className="text-xs uppercase tracking-widest text-[#8B89F0] mb-2">Ergebnis</p>
                    <p className="text-5xl font-semibold text-white mb-2">{pct}%</p>
                    <p className="text-sm text-gray-400">
                        {correctCount} richtig · {wrongCount} falsch · {items.length} Fragen
                    </p>
                    <div className="mt-6 h-2 rounded-full bg-[#23252A] overflow-hidden border border-[#45484F]">
                        <div className="h-full bg-[#5B9DFF] transition-all" style={{ width: `${pct}%` }} />
                    </div>
                </div>
                <div className="flex flex-wrap gap-3 justify-center">
                    {wrongCount > 0 ? (
                        <button
                            type="button"
                            onClick={() => {
                                setReviewWrongOnly(true);
                                setIndex(0);
                                setSelected(null);
                                setPhase("question");
                            }}
                            className="inline-flex items-center gap-2 px-4 py-2.5 rounded-xl bg-amber-500/15 border border-amber-500/30 text-amber-200 text-sm font-medium hover:bg-amber-500/25"
                        >
                            <RotateCcw size={16} />
                            Falsche wiederholen
                        </button>
                    ) : null}
                    <button
                        type="button"
                        onClick={() => {
                            setReviewWrongOnly(false);
                            setAnswers({});
                            setIndex(0);
                            setSelected(null);
                            setPhase("question");
                        }}
                        className="inline-flex items-center gap-2 px-4 py-2.5 rounded-xl bg-[#5B9DFF] text-white text-sm font-semibold hover:bg-[#4A8AE6]"
                    >
                        Nochmal von vorn
                    </button>
                </div>
            </div>
        );
    }

    if (!q || total === 0) {
        return (
            <div className="text-center text-gray-400 text-sm py-8">
                Keine weiteren Fragen.
                <button
                    type="button"
                    className="block mx-auto mt-3 text-[#8B89F0] underline"
                    onClick={() => setPhase("done")}
                >
                    Zum Ergebnis
                </button>
            </div>
        );
    }

    const options = Array.isArray(q.options) && q.options.length > 0 ? q.options : [q.answer].filter(Boolean);
    const revealed = selected != null;

    const pick = (opt: string) => {
        if (revealed) return;
        setSelected(opt);
        setAnswers((prev) => ({ ...prev, [qIndex]: opt }));
    };

    const goNext = () => {
        if (index + 1 >= total) {
            setPhase("done");
            setSelected(null);
            return;
        }
        setIndex((i) => i + 1);
        setSelected(null);
    };

    return (
        <div className="max-w-2xl mx-auto space-y-5 pb-12">
            <div className="flex items-center justify-between gap-3 text-xs text-gray-400">
                <span>
                    Frage {Math.min(index + 1, total)} / {total}
                    {reviewWrongOnly ? " · Wiederholung" : ""}
                </span>
                <span>{progress}%</span>
            </div>
            <div className="h-1.5 rounded-full bg-[#23252A] border border-[#45484F] overflow-hidden">
                <div className="h-full bg-[#5B9DFF] transition-all duration-300" style={{ width: `${((index) / Math.max(total, 1)) * 100}%` }} />
            </div>

            <div className="rounded-2xl border border-[#45484F] bg-[#353840] p-6 shadow-md">
                <p className="text-lg font-medium text-white leading-relaxed mb-5">{q.question}</p>
                <div className="space-y-3">
                    {options.map((opt, idx) => {
                        const isSelected = selected === opt;
                        const isCorrect = opt === q.answer;
                        let styles = "border-[#4A4E58] hover:bg-[#3A3D45] cursor-pointer text-gray-300";
                        if (revealed) {
                            styles = "border-[#45484F] opacity-60 cursor-default";
                            if (isCorrect) styles = "border-green-500/50 bg-green-500/10 text-green-400 font-medium opacity-100";
                            if (isSelected && !isCorrect) styles = "border-red-500/50 bg-red-500/10 text-red-400 font-medium opacity-100";
                        }
                        return (
                            <button
                                key={idx}
                                type="button"
                                onClick={() => pick(opt)}
                                className={`w-full text-left p-3.5 rounded-xl border transition-all ${styles}`}
                            >
                                <div className="flex items-center justify-between gap-3">
                                    <span>{opt}</span>
                                    {revealed && isCorrect ? <CheckCircle2 size={18} className="text-green-500 shrink-0" /> : null}
                                    {revealed && isSelected && !isCorrect ? <XCircle size={18} className="text-red-500 shrink-0" /> : null}
                                </div>
                            </button>
                        );
                    })}
                </div>

                {revealed ? (
                    <div className="mt-5 space-y-4">
                        {q.explanation ? (
                            <div className="rounded-xl border border-[#45484F] bg-[#23252A] px-4 py-3 text-sm text-gray-300 leading-relaxed">
                                <p className="text-[11px] uppercase tracking-wide text-gray-500 mb-1">Erklärung</p>
                                {q.explanation}
                            </div>
                        ) : null}
                        <button
                            type="button"
                            onClick={goNext}
                            className="w-full sm:w-auto inline-flex items-center justify-center gap-2 px-5 py-3 rounded-xl bg-[#5B9DFF] hover:bg-[#4A8AE6] text-white font-semibold"
                        >
                            {index + 1 >= total ? "Ergebnis anzeigen" : "Weiter"}
                            <ChevronRight size={18} />
                        </button>
                    </div>
                ) : null}
            </div>
        </div>
    );
}
