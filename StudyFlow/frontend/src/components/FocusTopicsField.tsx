"use client";

import React, { useRef } from "react";
import { FileText, ImageIcon, Loader2, Upload, X } from "lucide-react";

export type FocusTopicAttachment = {
    id: string;
    name: string;
    type: string;
};

type Props = {
    value: string;
    onChange: (next: string) => void;
    attachments: FocusTopicAttachment[];
    onAttachmentsChange: (next: FocusTopicAttachment[]) => void;
    onUploadFiles: (files: File[]) => Promise<void>;
    uploading?: boolean;
    placeholder?: string;
    /** Tailwind ring/focus accent, e.g. focus:ring-orange-500/50 */
    accentClassName?: string;
    rowsClassName?: string;
};

/** Text + optional PDF/image upload for „Kapitel / Themen / Fokus“. */
export default function FocusTopicsField({
    value,
    onChange,
    attachments,
    onAttachmentsChange,
    onUploadFiles,
    uploading = false,
    placeholder = "z.B. Kapitel 3–5, Zweipole, Kirchhoff — oder PDF/Bild mit Prüfungsthemen hochladen",
    accentClassName = "focus:ring-[#5B9DFF]/50 focus:border-[#5B9DFF]",
    rowsClassName = "h-24",
}: Props) {
    const inputRef = useRef<HTMLInputElement | null>(null);

    const remove = (id: string) => {
        onAttachmentsChange(attachments.filter((a) => a.id !== id));
    };

    return (
        <div className="space-y-2">
            <label className="block text-sm font-medium text-gray-300">Kapitel / Themen / Fokus (optional)</label>
            <textarea
                value={value}
                onChange={(e) => onChange(e.target.value)}
                placeholder={placeholder}
                disabled={uploading}
                className={`w-full ${rowsClassName} bg-[#353840] border border-[#45484F] text-white rounded-xl p-3 text-sm outline-none resize-none placeholder:text-gray-600 ${accentClassName}`}
            />
            <div className="flex flex-wrap items-center gap-2">
                <input
                    ref={inputRef}
                    type="file"
                    accept="application/pdf,image/png,image/jpeg,image/webp,image/gif,.pdf,.png,.jpg,.jpeg,.webp"
                    className="hidden"
                    multiple
                    onChange={(e) => {
                        const list = Array.from(e.target.files || []);
                        if (inputRef.current) inputRef.current.value = "";
                        if (list.length) void onUploadFiles(list);
                    }}
                />
                <button
                    type="button"
                    disabled={uploading}
                    onClick={() => inputRef.current?.click()}
                    className="inline-flex items-center gap-1.5 px-3 py-1.5 rounded-lg border border-[#45484F] text-xs text-gray-300 hover:bg-[#3A3D45] disabled:opacity-50"
                >
                    {uploading ? <Loader2 size={14} className="animate-spin" /> : <Upload size={14} />}
                    PDF / Bild mit Themen
                </button>
                <span className="text-[11px] text-gray-500">Text schreiben und/oder Datei — die KI stützt sich darauf</span>
            </div>
            {attachments.length > 0 ? (
                <ul className="flex flex-wrap gap-2">
                    {attachments.map((a) => (
                        <li
                            key={a.id}
                            className="inline-flex items-center gap-1.5 px-2.5 py-1 rounded-lg bg-[#1A1F2A] border border-[#45484F] text-[11px] text-gray-200"
                        >
                            {a.type === "image" ? <ImageIcon size={12} className="text-[#9EC5FF]" /> : <FileText size={12} className="text-orange-300" />}
                            <span className="max-w-[10rem] truncate">{a.name}</span>
                            <button
                                type="button"
                                onClick={() => remove(a.id)}
                                className="text-gray-500 hover:text-white p-0.5"
                                aria-label="Anhang entfernen"
                            >
                                <X size={12} />
                            </button>
                        </li>
                    ))}
                </ul>
            ) : null}
        </div>
    );
}

export function buildFocusWithAttachments(text: string, attachments: FocusTopicAttachment[]): string | undefined {
    const t = text.trim();
    const names = attachments.map((a) => a.name).filter(Boolean);
    if (!t && names.length === 0) return undefined;
    const fileNote = names.length
        ? `Prüfungsthemen-Anhänge (primärer Fokus): ${names.join(", ")}. Behandle genau diese Themen und stütze dich auf die zugehörigen Dateien im Kontext.`
        : "";
    return [fileNote, t].filter(Boolean).join("\n\n") || undefined;
}
