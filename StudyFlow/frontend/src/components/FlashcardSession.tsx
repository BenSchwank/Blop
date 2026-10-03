"use client";

import React, { useEffect, useMemo, useState } from "react";
import { CheckCircle2, Edit, Layers, Maximize2, Shuffle } from "lucide-react";

export interface FlashcardRow {
    front: string;
    back: string;
}

export type FlashcardRating = "leicht" | "mittel" | "schwer";

type Props = {
    cards: FlashcardRow[];
    onEditCard?: (index: number) => void;
    onFullscreen?: (card: FlashcardRow) => void;
    onExportCsv?: () => void;
};

function shuffleIndices(n: number): number[] {
    const arr = Array.from({ length: n }, (_, i) => i);
    for (let i = arr.length - 1; i > 0; i--) {
        const j = Math.floor(Math.random() * (i + 1));
        [arr[i], arr[j]] = [arr[j], arr[i]];
    }
    return arr;
}

export default function FlashcardSession({ cards, onEditCard, onFullscreen, onExportCsv }: Props) {
    const [mode, setMode] = useState<"session" | "library">("session");
    const [queue, setQueue] = useState<number[]>([]);
    const [mastery, setMastery] = useState<Record<number, number>>({});
    const [progress, setProgress] = useState(0);
    const [flipped, setFlipped] = useState(false);
    const [flipMap, setFlipMap] = useState<Record<number, boolean>>({});
    const [done, setDone] = useState(false);

    const startSession = () => {
        setQueue(shuffleIndices(cards.length));
        setMastery({});
        setProgress(0);
        setFlipped(false);
        setDone(false);
        setMode("session");
    };

    useEffect(() => {
        if (cards.length > 0) startSession();
        // eslint-disable-next-line react-hooks/exhaustive-deps
    }, [cards.length]);

    const currentIndex = queue.length > 0 ? queue[0] : null;
    const current = currentIndex !== null ? cards[currentIndex] : null;

    const mastered = useMemo(
        () => Object.values(mastery).filter((s) => s >= 3).length,
        [mastery]
    );

    const rate = (rating: FlashcardRating) => {
        if (currentIndex === null) return;
        const currentScore = mastery[currentIndex] ?? 0;
        const nextScore =
            rating === "leicht"
                ? Math.min(3, currentScore + 2)
                : rating === "mittel"
                  ? Math.min(3, currentScore + 1)
                  : 0;
        const nextMastery = { ...mastery, [currentIndex]: nextScore };
        const rest = queue.slice(1);
        if (nextScore < 3) {
            if (rating === "schwer") {
                const insertAt = Math.min(1, rest.length);
                rest.splice(insertAt, 0, currentIndex);
                rest.push(currentIndex);
            } else if (rating === "mittel") {
                rest.push(currentIndex);
            }
        }
        setMastery(nextMastery);
        setQueue(rest);
        setProgress((p) => p + 1);
        setFlipped(false);
        if (rest.length === 0) setDone(true);
    };

    if (cards.length === 0) {
        return <p className="text-gray-400 text-sm">Fehlerhaftes oder leeres Karteikarten-Format.</p>;
    }

    return (
        <div className="flex flex-col h-full pb-8">
            <div className="flex flex-wrap gap-2 justify-between mb-4">
                <div className="text-xs text-gray-400 flex items-center gap-2">
                    <Layers size={14} />
                    {cards.length} Karte{cards.length === 1 ? "" : "n"}
                    {mode === "session" && !done ? ` · Fortschritt ${progress}` : null}
                </div>
                <div className="flex flex-wrap gap-2">
                    <button
                        type="button"
                        onClick={() => setMode(mode === "session" ? "library" : "session")}
                        className="bg-[#353840] hover:bg-[#3A3D45] border border-[#4A4E58] text-white px-3 py-2 rounded-xl text-sm"
                    >
                        {mode === "session" ? "Bibliothek" : "Übung"}
                    </button>
                    <button
                        type="button"
                        onClick={startSession}
                        className="bg-[#5B9DFF] hover:bg-[#4A8AE6] text-white px-4 py-2 rounded-xl text-sm font-medium flex items-center gap-2"
                    >
                        <Shuffle size={16} />
                        Übung starten
                    </button>
                    {onExportCsv ? (
                        <button
                            type="button"
                            onClick={onExportCsv}
                            className="bg-[#353840] hover:bg-[#3A3D45] border border-[#4A4E58] text-white px-3 py-2 rounded-xl text-sm"
                        >
                            Anki CSV
                        </button>
                    ) : null}
                </div>
            </div>

            {mode === "session" ? (
                done || !current ? (
                    <div className="rounded-2xl border border-[#45484F] bg-[#353840] p-8 text-center space-y-4 max-w-lg mx-auto">
                        <CheckCircle2 className="mx-auto text-green-400" size={36} />
                        <h3 className="text-xl font-semibold text-white">Session beendet</h3>
                        <p className="text-sm text-gray-400">
                            {mastered}/{cards.length} Karten sicher · {progress} Bewertungen
                        </p>
                        <button
                            type="button"
                            onClick={startSession}
                            className="inline-flex items-center gap-2 px-5 py-2.5 rounded-xl bg-[#5B9DFF] text-white font-semibold"
                        >
                            <Shuffle size={16} /> Nochmal
                        </button>
                    </div>
                ) : (
                    <div className="max-w-xl mx-auto w-full space-y-4">
                        <div className="h-1.5 rounded-full bg-[#23252A] border border-[#45484F] overflow-hidden">
                            <div
                                className="h-full bg-green-500/80 transition-all"
                                style={{ width: `${Math.min(100, (mastered / Math.max(cards.length, 1)) * 100)}%` }}
                            />
                        </div>
                        <p className="text-xs text-gray-500 text-center">
                            Queue {queue.length} · Karte {(currentIndex ?? 0) + 1}
                        </p>
                        <button
                            type="button"
                            onClick={() => setFlipped((f) => !f)}
                            className="w-full min-h-[220px] rounded-2xl border border-[#45484F] bg-[#353840] p-8 text-center shadow-lg transition-colors hover:border-[#5B9DFF]/40"
                        >
                            <p className="text-[11px] uppercase tracking-wide text-gray-500 mb-3">
                                {flipped ? "Rückseite" : "Vorderseite · tippen zum Umdrehen"}
                            </p>
                            <p className="text-lg text-white whitespace-pre-wrap leading-relaxed">
                                {flipped ? current.back : current.front}
                            </p>
                        </button>
                        {flipped ? (
                            <div className="flex flex-wrap gap-2 justify-center">
                                <button
                                    type="button"
                                    onClick={() => rate("schwer")}
                                    className="px-4 py-2.5 rounded-xl text-sm bg-red-500/20 border border-red-500/40 text-red-300 hover:bg-red-500/30"
                                >
                                    Wieder zeigen
                                </button>
                                <button
                                    type="button"
                                    onClick={() => rate("mittel")}
                                    className="px-4 py-2.5 rounded-xl text-sm bg-amber-500/20 border border-amber-500/40 text-amber-300 hover:bg-amber-500/30"
                                >
                                    Gewusst
                                </button>
                                <button
                                    type="button"
                                    onClick={() => rate("leicht")}
                                    className="px-4 py-2.5 rounded-xl text-sm bg-green-500/20 border border-green-500/40 text-green-300 hover:bg-green-500/30"
                                >
                                    Sicher
                                </button>
                            </div>
                        ) : (
                            <p className="text-center text-xs text-gray-500">Erst umdrehen, dann bewerten.</p>
                        )}
                    </div>
                )
            ) : (
                <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
                    {cards.map((c, i) => {
                        const isFlipped = !!flipMap[i];
                        return (
                            <div key={i} className="group relative">
                                <div className="absolute top-3 left-3 z-10 text-xs px-2 py-1 rounded-md bg-black/45 text-gray-300">
                                    Karte {i + 1}
                                </div>
                                <div className="absolute top-3 right-3 z-10 opacity-0 group-hover:opacity-100 transition-opacity flex items-center gap-1">
                                    {onEditCard ? (
                                        <button
                                            type="button"
                                            onClick={(e) => {
                                                e.stopPropagation();
                                                onEditCard(i);
                                            }}
                                            className="p-1.5 bg-black/40 hover:bg-black/80 rounded-lg text-gray-300"
                                            title="Bearbeiten"
                                        >
                                            <Edit size={16} />
                                        </button>
                                    ) : null}
                                    {onFullscreen ? (
                                        <button
                                            type="button"
                                            onClick={(e) => {
                                                e.stopPropagation();
                                                onFullscreen(c);
                                            }}
                                            className="p-1.5 bg-black/40 hover:bg-black/80 rounded-lg text-gray-300"
                                            title="Vollbild"
                                        >
                                            <Maximize2 size={16} />
                                        </button>
                                    ) : null}
                                </div>
                                <div
                                    className="h-48 w-full [perspective:1000px] cursor-pointer"
                                    onClick={() => setFlipMap((prev) => ({ ...prev, [i]: !prev[i] }))}
                                >
                                    <div
                                        className={`w-full h-full transition-all duration-500 [transform-style:preserve-3d] relative rounded-xl border border-[#45484F] bg-[#353840] ${
                                            isFlipped ? "[transform:rotateY(180deg)]" : ""
                                        }`}
                                    >
                                        <div className="absolute inset-0 rounded-xl [backface-visibility:hidden] flex items-center justify-center p-6 text-center text-white text-sm whitespace-pre-wrap overflow-y-auto">
                                            {c.front}
                                        </div>
                                        <div className="absolute inset-0 rounded-xl [backface-visibility:hidden] [transform:rotateY(180deg)] flex items-center justify-center p-6 text-center text-gray-300 bg-[#23252A] text-sm whitespace-pre-wrap overflow-y-auto">
                                            {c.back}
                                        </div>
                                    </div>
                                </div>
                            </div>
                        );
                    })}
                </div>
            )}
        </div>
    );
}
