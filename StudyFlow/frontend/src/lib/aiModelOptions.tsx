"use client";

import React, { useEffect, useMemo, useState } from "react";

/** Curated Blop internal ids (mapped to OpenRouter in the backend). */
export const AI_MODEL_CHOICES = [
    { value: "", label: "Automatisch: passendes Modell (empfohlen)" },
    { value: "gemini-3.7-flash", label: "Gemini 3.7 Flash — niedriger Tokenverbrauch" },
    { value: "gemini-3-flash", label: "Gemini 3 Flash — niedriger Tokenverbrauch" },
    { value: "gemini-2.5-flash", label: "Gemini 2.5 Flash — niedriger Tokenverbrauch" },
    { value: "gemini-2.5-flash-lite", label: "Gemini 2.5 Flash Lite — sehr niedriger Tokenverbrauch" },
    { value: "claude-sonnet-5.5", label: "Claude Sonnet 5.5 — teurer" },
    { value: "gpt-6.1-sol", label: "GPT-6.1 Sol — teurer" },
    { value: "gemini-3.1-pro", label: "Gemini 3.1 Pro — teurer" },
    { value: "gemini-2.5-pro", label: "Gemini 2.5 Pro — teurer" },
] as const;

export type OpenRouterCatalogModel = {
    id: string;
    name: string;
    prompt_per_1k: number;
    completion_per_1k: number;
};

let catalogCache: OpenRouterCatalogModel[] | null = null;
let catalogPromise: Promise<OpenRouterCatalogModel[]> | null = null;

export function fetchOpenRouterCatalog(): Promise<OpenRouterCatalogModel[]> {
    if (catalogCache) {
        return Promise.resolve(catalogCache);
    }
    if (!catalogPromise) {
        catalogPromise = fetch("/api/ai/models")
            .then((res) => (res.ok ? res.json() : { models: [] }))
            .then((data) => {
                const models = Array.isArray(data?.models) ? data.models : [];
                catalogCache = models;
                return models;
            })
            .catch(() => {
                catalogCache = [];
                return [];
            });
    }
    return catalogPromise;
}

function isKnownChoice(value: string): boolean {
    if (!value) {
        return true;
    }
    return AI_MODEL_CHOICES.some((option) => option.value === value);
}

export function AiModelOptions({ current }: { current?: string }) {
    const value = current || "";
    const known = isKnownChoice(value);
    const [catalog, setCatalog] = useState<OpenRouterCatalogModel[]>(catalogCache || []);

    useEffect(() => {
        let active = true;
        fetchOpenRouterCatalog().then((models) => {
            if (active) {
                setCatalog(models);
            }
        });
        return () => {
            active = false;
        };
    }, []);

    const curatedIds = useMemo(
        () => new Set<string>(AI_MODEL_CHOICES.map((c) => c.value).filter(Boolean)),
        [],
    );
    const openRouterExtras = useMemo(
        () => catalog.filter((m) => m.id && !curatedIds.has(m.id)).slice(0, 80),
        [catalog, curatedIds],
    );

    return (
        <>
            {AI_MODEL_CHOICES.map((option) => (
                <option key={option.value || "auto"} value={option.value}>
                    {option.label}
                </option>
            ))}
            {openRouterExtras.length > 0 ? (
                <optgroup label="OpenRouter">
                    {openRouterExtras.map((m) => (
                        <option key={m.id} value={m.id}>
                            {m.name} — in {m.prompt_per_1k} / out {m.completion_per_1k} pro 1k
                        </option>
                    ))}
                </optgroup>
            ) : null}
            {value && !known && !catalog.some((m) => m.id === value) ? (
                <option value={value}>{value} (bisherige Wahl)</option>
            ) : null}
        </>
    );
}

type AiModelSelectorProps = {
    value: string;
    onChange: (value: string) => void;
    className?: string;
    id?: string;
    showCustomHint?: boolean;
};

/** Select + optional custom OpenRouter slug (provider/model). */
export function AiModelSelector({
    value,
    onChange,
    className = "",
    id,
    showCustomHint = true,
}: AiModelSelectorProps) {
    const [customOpen, setCustomOpen] = useState(false);
    const [customSlug, setCustomSlug] = useState("");
    const inCatalogOrCurated =
        !value || isKnownChoice(value);

    useEffect(() => {
        if (value && !inCatalogOrCurated && value.includes("/")) {
            setCustomOpen(true);
            setCustomSlug(value);
        }
    }, [value, inCatalogOrCurated]);

    return (
        <div className="space-y-2">
            <select
                id={id}
                value={inCatalogOrCurated || !customOpen ? value : "__custom__"}
                onChange={(e) => {
                    const next = e.target.value;
                    if (next === "__custom__") {
                        setCustomOpen(true);
                        if (customSlug) {
                            onChange(customSlug);
                        }
                        return;
                    }
                    setCustomOpen(false);
                    onChange(next);
                }}
                className={className}
            >
                <option value="__custom__">Eigene OpenRouter-Modell-ID…</option>
                <AiModelOptions current={customOpen ? customSlug : value} />
            </select>
            {customOpen ? (
                <input
                    type="text"
                    value={customSlug}
                    onChange={(e) => {
                        const slug = e.target.value;
                        setCustomSlug(slug);
                        onChange(slug);
                    }}
                    placeholder="z. B. anthropic/claude-sonnet-4 or openai/gpt-4o"
                    className="w-full bg-[#353840] border border-[#45484F] text-gray-200 rounded-xl px-4 py-2 text-sm font-mono"
                    spellCheck={false}
                />
            ) : null}
            {showCustomHint ? (
                <p className="text-xs text-gray-500">
                    Automatisch wählt das Backend passend zu Aufgabe, Materialumfang und Abo. Feste Modelle oder
                    eigene OpenRouter-IDs gelten für alle KI-Funktionen.
                </p>
            ) : null}
        </div>
    );
}