"use client";

import { useEffect, useRef } from "react";
import { createPortal } from "react-dom";
import DocumentMarkdown from "@/components/DocumentMarkdown";
import { sanitizeDocumentMarkdown } from "@/lib/markdownHeadings";

type Props = {
    markdown: string;
    title: string;
    folderName?: string;
    dateLabel?: string;
    onDone: () => void;
};

export default function MarkdownPrintPortal({ markdown, title, folderName, dateLabel, onDone }: Props) {
    const onDoneRef = useRef(onDone);
    onDoneRef.current = onDone;

    useEffect(() => {
        document.body.classList.add("blop-printing-markdown");
        let fallbackTimer: ReturnType<typeof setTimeout> | undefined;
        let printDelay: ReturnType<typeof setTimeout> | undefined;
        let raf1 = 0;
        let raf2 = 0;

        const finish = () => {
            if (fallbackTimer !== undefined) clearTimeout(fallbackTimer);
            fallbackTimer = undefined;
            window.removeEventListener("afterprint", finish);
            document.body.classList.remove("blop-printing-markdown");
            onDoneRef.current();
        };

        raf1 = requestAnimationFrame(() => {
            raf2 = requestAnimationFrame(() => {
                printDelay = setTimeout(() => {
                    window.addEventListener("afterprint", finish, { once: true });
                    fallbackTimer = setTimeout(finish, 120000);
                    window.print();
                }, 750);
            });
        });

        return () => {
            cancelAnimationFrame(raf1);
            cancelAnimationFrame(raf2);
            if (printDelay !== undefined) clearTimeout(printDelay);
            if (fallbackTimer !== undefined) clearTimeout(fallbackTimer);
            window.removeEventListener("afterprint", finish);
            document.body.classList.remove("blop-printing-markdown");
        };
    }, []);

    if (typeof document === "undefined") return null;

    const printedAt =
        dateLabel ||
        new Intl.DateTimeFormat("de-DE", { dateStyle: "medium", timeStyle: "short" }).format(new Date());

    return createPortal(
        <div
            id="blop-markdown-print-mount"
            className="fixed inset-0 z-[200000] overflow-auto bg-white text-gray-900 print:static print:overflow-visible"
        >
            <div className="no-print sticky top-0 z-10 border-b border-gray-200 bg-white/95 px-4 py-3 text-sm text-gray-600 backdrop-blur">
                Druckdialog öffnet sich gleich. Wähle <strong>Als PDF speichern</strong> als Ziel.
                <span className="mt-2 block text-xs text-gray-500">
                    In Chrome: unter <strong>Weitere Einstellungen</strong> die Option <strong>Kopf- und Fußzeilen</strong>{" "}
                    deaktivieren.
                </span>
            </div>
            <div className="blop-markdown-export-prose mx-auto max-w-3xl px-6 py-8 prose prose-slate max-w-none [&_.katex-display]:max-w-full">
                <header className="not-prose mb-6 border-b border-gray-200 pb-4">
                    <h1 className="text-2xl font-bold text-gray-900 mb-1">{title}</h1>
                    {folderName ? <p className="text-sm text-gray-600">{folderName}</p> : null}
                    <p className="text-xs text-gray-500 mt-1">{printedAt}</p>
                </header>
                <DocumentMarkdown>{sanitizeDocumentMarkdown(markdown)}</DocumentMarkdown>
            </div>
        </div>,
        document.body
    );
}
