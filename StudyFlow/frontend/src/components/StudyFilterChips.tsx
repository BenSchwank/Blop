"use client";

import React from "react";

export const STUDY_FILE_FILTERS = [
    { id: "all", label: "Alle" },
    { id: "materials", label: "Materialien" },
    { id: "creations", label: "Erstellungen" },
    { id: "untagged", label: "Ohne Tags" },
] as const;

export type StudyFileFilter = (typeof STUDY_FILE_FILTERS)[number]["id"];

export default function StudyFilterChips({
    value,
    onChange,
}: {
    value: StudyFileFilter;
    onChange: (next: StudyFileFilter) => void;
}) {
    return (
        <div className="flex flex-wrap gap-1.5" role="tablist" aria-label="Dateifilter">
            {STUDY_FILE_FILTERS.map((chip) => {
                const selected = value === chip.id;
                return (
                    <button
                        key={chip.id}
                        type="button"
                        role="tab"
                        aria-selected={selected}
                        onClick={() => onChange(chip.id)}
                        className={`min-h-9 px-3 rounded-full text-[12px] font-medium border transition-colors ${
                            selected
                                ? "bg-[#5B9DFF]/16 border-[#5B9DFF]/45 text-[#E8F1FF]"
                                : "bg-transparent border-white/12 text-[#B8BEC9] hover:border-white/25 hover:text-white"
                        }`}
                    >
                        {chip.label}
                    </button>
                );
            })}
        </div>
    );
}
