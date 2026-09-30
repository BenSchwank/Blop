import React from "react";

/** Customer-facing OpenRouter choices. Rates follow MODEL_TOKEN_RATES_PER_1K. */
export const AI_MODEL_CHOICES = [
    { value: "", label: "Automatisch: bestes Modell pro Aufgabe" },
    { value: "claude-sonnet-5.5", label: "Claude Sonnet 5.5 — höherer Tokenverbrauch" },
    { value: "gpt-6.1-sol", label: "GPT-6.1 Sol — höherer Tokenverbrauch" },
    { value: "gemini-3.7-flash", label: "Gemini 3.7 Flash — niedriger Tokenverbrauch" },
] as const;

export function AiModelOptions({ current }: { current?: string }) {
    const value = current || "";
    const known = AI_MODEL_CHOICES.some((option) => option.value === value);
    return (
        <>
            {AI_MODEL_CHOICES.map((option) => (
                <option key={option.value || "auto"} value={option.value}>
                    {option.label}
                </option>
            ))}
            {value && !known ? (
                <option value={value}>{value} (bisherige Wahl)</option>
            ) : null}
        </>
    );
}
