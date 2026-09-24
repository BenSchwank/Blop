"use client";

import React, { useCallback, useEffect, useState } from "react";
import { useParams, useRouter } from "next/navigation";
import { ArrowLeft, Loader2, Sparkles } from "lucide-react";
import SmartLearningView, {
    normalizeSmartLearningContent,
    type SmartLearningContent,
} from "@/components/SmartLearningView";

type FileRow = {
    id: string;
    name: string;
    type: string;
    content?: unknown;
};

export default function SmartLearningPage() {
    const params = useParams();
    const router = useRouter();
    const folderId = String(params?.id || "");
    const [loading, setLoading] = useState(true);
    const [generating, setGenerating] = useState(false);
    const [error, setError] = useState("");
    const [errorLink, setErrorLink] = useState<{ url: string; label: string } | null>(null);
    const [file, setFile] = useState<FileRow | null>(null);
    const [username, setUsername] = useState("");
    const [sessionId, setSessionId] = useState("");
    const [focus, setFocus] = useState("");
    const [examDate, setExamDate] = useState("");

    const load = useCallback(async () => {
        if (!folderId) return;
        setLoading(true);
        setError("");
        try {
            const u = localStorage.getItem("username") || "";
            const sid = localStorage.getItem("session_id") || "";
            setUsername(u);
            setSessionId(sid);
            if (!u) {
                setError("Nicht angemeldet.");
                return;
            }
            const res = await fetch(
                `/api/files/${folderId}?username=${encodeURIComponent(u)}&session_id=${encodeURIComponent(sid)}`
            );
            if (!res.ok) {
                throw new Error(`Dateien konnten nicht geladen werden (HTTP ${res.status}).`);
            }
            const data = await res.json();
            const files: FileRow[] = Array.isArray(data) ? data : data.files || [];
            const existingMeta =
                files.find((f) => f.id === `smart_main_${folderId}`) ||
                files.find((f) => f.type === "smart_learning") ||
                null;
            if (!existingMeta) {
                setFile(null);
                return;
            }
            if (existingMeta.content != null) {
                setFile(existingMeta);
                return;
            }
            const itemRes = await fetch(
                `/api/files/item/${encodeURIComponent(existingMeta.id)}?username=${encodeURIComponent(u)}&session_id=${encodeURIComponent(sid)}`,
                { headers: sid ? { "X-Session-Id": sid } : {} }
            );
            if (itemRes.ok) {
                setFile(await itemRes.json());
            } else {
                setFile(existingMeta);
            }
        } catch (e: any) {
            setError(e?.message || "Laden fehlgeschlagen.");
        } finally {
            setLoading(false);
        }
    }, [folderId]);

    useEffect(() => {
        void load();
    }, [load]);

    const generate = async () => {
        if (!username || generating) return;
        setGenerating(true);
        setError("");
        setErrorLink(null);
        try {
            const sid = sessionId || localStorage.getItem("session_id") || "";
            const res = await fetch(
                `/api/ai/smart-learning?session_id=${encodeURIComponent(sid)}`,
                {
                method: "POST",
                headers: {
                    "Content-Type": "application/json",
                    ...(sid ? { "X-Session-Id": sid } : {}),
                },
                body: JSON.stringify({
                    username,
                    folder_id: folderId,
                    focus: focus.trim() || undefined,
                    exam_date: examDate || undefined,
                }),
            });
            const data = await res.json().catch(() => ({}));
            if (!res.ok) {
                const detail = data.detail;
                const isAdmin =
                    localStorage.getItem("is_admin") === "true" ||
                    localStorage.getItem("username") === "admin_";
                if (detail && typeof detail === "object" && detail.admin_debug && isAdmin) {
                    setError(String(detail.debug || detail.message || `HTTP ${res.status}`));
                    if (detail.fix_url) {
                        setErrorLink({
                            url: String(detail.fix_url),
                            label: String(detail.fix_label || "Lösung öffnen"),
                        });
                    }
                } else if (typeof detail === "string") {
                    setError(detail);
                } else if (detail) {
                    setError(JSON.stringify(detail));
                } else {
                    setError(`HTTP ${res.status}`);
                }
                return;
            }
            const journey = normalizeSmartLearningContent(
                data.smart_learning || data.file?.content,
                data.file?.name || "Smart Learning"
            );
            setFile({
                id: data.file?.id || `smart_main_${folderId}`,
                name: data.file?.name || journey.title || "Smart Learning",
                type: "smart_learning",
                content: journey,
            });
        } catch (e: any) {
            setError(e?.message || "Generierung fehlgeschlagen.");
        } finally {
            setGenerating(false);
        }
    };

    const content: SmartLearningContent | null = file
        ? normalizeSmartLearningContent(file.content, file.name || "Smart Learning")
        : null;
    const hasChapters = Boolean(content && content.chapters.length > 0);

    return (
        <div className="min-h-screen bg-[#0B0B1A] text-white">
            <div className="max-w-3xl mx-auto px-4 py-6 space-y-6">
                <div className="flex items-center gap-3">
                    <button
                        type="button"
                        onClick={() => router.push(`/folder/${folderId}`)}
                        className="p-2 rounded-xl text-gray-400 hover:text-white hover:bg-[#1C1C33]"
                    >
                        <ArrowLeft size={20} />
                    </button>
                    <div>
                        <p className="text-[11px] uppercase tracking-[0.2em] text-[#8B89F0] flex items-center gap-1.5">
                            <Sparkles size={12} /> Smart Learning
                        </p>
                        <h1 className="text-xl font-semibold">Lernreise</h1>
                    </div>
                </div>

                {loading ? (
                    <div className="flex items-center justify-center py-20 text-gray-400 gap-2">
                        <Loader2 className="animate-spin" size={20} /> Laden…
                    </div>
                ) : hasChapters && content ? (
                    <SmartLearningView
                        content={content}
                        folderId={folderId}
                        username={username}
                        sessionId={sessionId}
                        onContentUpdated={(next) =>
                            setFile((prev) =>
                                prev ? { ...prev, content: next, name: next.title || prev.name } : prev
                            )
                        }
                        onStartPractice={(mode) => {
                            router.push(`/folder/${folderId}?practice=${mode}`);
                        }}
                        onClose={() => router.push(`/folder/${folderId}`)}
                    />
                ) : (
                    <div className="rounded-2xl border border-[#2A2A40] bg-[#151525] p-6 space-y-4">
                        <p className="text-sm text-gray-300">
                            {file
                                ? "Smart Learning ist noch leer. Generiere die Kapitel neu aus PDF/YouTube-Material."
                                : "Noch keine Smart-Learning-Reise in diesem Ordner. Lade PDF/YouTube hoch und generiere die Kapitel."}
                        </p>
                        <div>
                            <label className="block text-xs text-gray-500 mb-1">Fokus (optional)</label>
                            <input
                                value={focus}
                                onChange={(e) => setFocus(e.target.value)}
                                className="w-full bg-[#0B0B1A] border border-[#2A2A40] rounded-xl px-3 py-2 text-sm"
                                placeholder="z.B. Klausur Fokus"
                            />
                        </div>
                        <div>
                            <label className="block text-xs text-gray-500 mb-1">Prüfungsdatum (optional)</label>
                            <input
                                type="date"
                                value={examDate}
                                onChange={(e) => setExamDate(e.target.value)}
                                className="w-full bg-[#0B0B1A] border border-[#2A2A40] rounded-xl px-3 py-2 text-sm"
                            />
                        </div>
                        <button
                            type="button"
                            onClick={() => void generate()}
                            disabled={generating}
                            className="inline-flex items-center gap-2 px-4 py-2.5 rounded-xl bg-[#5E5CE6] hover:bg-[#4d4ac9] text-white font-semibold disabled:opacity-50"
                        >
                            {generating ? <Loader2 size={16} className="animate-spin" /> : <Sparkles size={16} />}
                            Smart Learning generieren
                        </button>
                    </div>
                )}

                {error ? (
                    <div className="rounded-xl border border-red-500/30 bg-red-500/10 text-red-300 text-sm px-4 py-3 space-y-2">
                        <p className="whitespace-pre-wrap">{error}</p>
                        {errorLink ? (
                            <a
                                href={errorLink.url}
                                target="_blank"
                                rel="noopener noreferrer"
                                className="inline-flex text-[#8B89F0] underline underline-offset-2 hover:text-white"
                            >
                                {errorLink.label}
                            </a>
                        ) : null}
                    </div>
                ) : null}
            </div>
        </div>
    );
}
