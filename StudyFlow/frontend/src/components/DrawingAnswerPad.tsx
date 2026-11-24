"use client";

import React, { useCallback, useEffect, useRef, useState } from "react";
import { Eraser, ImageIcon, Loader2, PenLine, Trash2, Upload } from "lucide-react";

export type DrawingGradeResult = {
    correct: boolean;
    score: number;
    feedback: string;
    matched_rubric?: string[];
};

type Props = {
    allowUpload?: boolean;
    disabled?: boolean;
    busy?: boolean;
    onSubmit: (payload: { ink_png_base64?: string; image_base64?: string }) => void;
};

function fileToBase64(file: File): Promise<string> {
    return new Promise((resolve, reject) => {
        const reader = new FileReader();
        reader.onload = () => {
            const raw = String(reader.result || "");
            const b64 = raw.includes(",") ? raw.split(",", 1)[1] : raw;
            resolve(b64 || "");
        };
        reader.onerror = () => reject(new Error("Datei lesen fehlgeschlagen"));
        reader.readAsDataURL(file);
    });
}

/** Canvas sketch pad + optional image upload for Smart Learning drawing answers. */
export default function DrawingAnswerPad({
    allowUpload = true,
    disabled = false,
    busy = false,
    onSubmit,
}: Props) {
    const canvasRef = useRef<HTMLCanvasElement | null>(null);
    const drawing = useRef(false);
    const [tool, setTool] = useState<"pen" | "eraser">("pen");
    const [hasInk, setHasInk] = useState(false);
    const [uploadPreview, setUploadPreview] = useState<string | null>(null);
    const [uploadB64, setUploadB64] = useState<string | null>(null);
    const fileRef = useRef<HTMLInputElement | null>(null);

    const resizeCanvas = useCallback(() => {
        const canvas = canvasRef.current;
        if (!canvas) return;
        const parent = canvas.parentElement;
        const cssW = Math.max(280, parent?.clientWidth || 480);
        const cssH = 260;
        const dpr = Math.min(window.devicePixelRatio || 1, 2);
        canvas.width = Math.floor(cssW * dpr);
        canvas.height = Math.floor(cssH * dpr);
        canvas.style.width = `${cssW}px`;
        canvas.style.height = `${cssH}px`;
        const ctx = canvas.getContext("2d");
        if (!ctx) return;
        ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
        ctx.fillStyle = "#0E1117";
        ctx.fillRect(0, 0, cssW, cssH);
        setHasInk(false);
    }, []);

    useEffect(() => {
        resizeCanvas();
        const onResize = () => resizeCanvas();
        window.addEventListener("resize", onResize);
        return () => window.removeEventListener("resize", onResize);
    }, [resizeCanvas]);

    const pos = (e: React.PointerEvent<HTMLCanvasElement>) => {
        const canvas = canvasRef.current!;
        const rect = canvas.getBoundingClientRect();
        return { x: e.clientX - rect.left, y: e.clientY - rect.top };
    };

    const onPointerDown = (e: React.PointerEvent<HTMLCanvasElement>) => {
        if (disabled || busy || uploadB64) return;
        const canvas = canvasRef.current;
        const ctx = canvas?.getContext("2d");
        if (!canvas || !ctx) return;
        canvas.setPointerCapture(e.pointerId);
        drawing.current = true;
        const p = pos(e);
        ctx.beginPath();
        ctx.moveTo(p.x, p.y);
        ctx.lineCap = "round";
        ctx.lineJoin = "round";
        ctx.lineWidth = tool === "eraser" ? 16 : 2.5;
        ctx.strokeStyle = tool === "eraser" ? "#0E1117" : "#E8EEF8";
        ctx.globalCompositeOperation = "source-over";
    };

    const onPointerMove = (e: React.PointerEvent<HTMLCanvasElement>) => {
        if (!drawing.current || disabled || busy || uploadB64) return;
        const ctx = canvasRef.current?.getContext("2d");
        if (!ctx) return;
        const p = pos(e);
        ctx.lineTo(p.x, p.y);
        ctx.stroke();
        if (tool === "pen") setHasInk(true);
    };

    const onPointerUp = (e: React.PointerEvent<HTMLCanvasElement>) => {
        if (!drawing.current) return;
        drawing.current = false;
        try {
            canvasRef.current?.releasePointerCapture(e.pointerId);
        } catch {
            /* ignore */
        }
    };

    const clearCanvas = () => {
        resizeCanvas();
        setUploadPreview(null);
        setUploadB64(null);
        if (fileRef.current) fileRef.current.value = "";
    };

    const onPickFile = async (file: File | null) => {
        if (!file || !allowUpload) return;
        if (!file.type.startsWith("image/")) return;
        if (file.size > 2 * 1024 * 1024) return;
        const b64 = await fileToBase64(file);
        setUploadB64(b64);
        setUploadPreview(`data:${file.type};base64,${b64}`);
        setHasInk(false);
    };

    const exportInkPng = (): string | null => {
        const canvas = canvasRef.current;
        if (!canvas || !hasInk) return null;
        // Export white background / dark ink for vision models
        const exportCanvas = document.createElement("canvas");
        exportCanvas.width = canvas.width;
        exportCanvas.height = canvas.height;
        const ex = exportCanvas.getContext("2d");
        if (!ex) return null;
        ex.fillStyle = "#ffffff";
        ex.fillRect(0, 0, exportCanvas.width, exportCanvas.height);
        ex.drawImage(canvas, 0, 0);
        // invert dark strokes → black on white roughly via destination-over already white
        // Re-draw: get image data and map near-dark pixels
        try {
            const img = ex.getImageData(0, 0, exportCanvas.width, exportCanvas.height);
            const d = img.data;
            for (let i = 0; i < d.length; i += 4) {
                const r = d[i];
                const g = d[i + 1];
                const b = d[i + 2];
                // source canvas is dark bg with light strokes — after drawImage light-ish strokes on white
                const lum = (r + g + b) / 3;
                if (lum > 40 && lum < 230) {
                    d[i] = 20;
                    d[i + 1] = 20;
                    d[i + 2] = 20;
                    d[i + 3] = 255;
                } else if (lum <= 40) {
                    d[i] = 255;
                    d[i + 1] = 255;
                    d[i + 2] = 255;
                }
            }
            ex.putImageData(img, 0, 0);
        } catch {
            /* keep as-is */
        }
        const dataUrl = exportCanvas.toDataURL("image/png");
        return dataUrl.includes(",") ? dataUrl.split(",", 1)[1] : dataUrl;
    };

    const canSubmit = !disabled && !busy && (!!uploadB64 || hasInk);

    return (
        <div className="space-y-3">
            <div className="flex flex-wrap gap-2 items-center">
                <button
                    type="button"
                    disabled={disabled || busy || !!uploadB64}
                    onClick={() => setTool("pen")}
                    className={`inline-flex items-center gap-1.5 px-3 py-1.5 rounded-lg text-xs border ${
                        tool === "pen" ? "bg-[#5B9DFF]/20 border-[#5B9DFF]/50 text-[#9EC5FF]" : "border-[#2A3140] text-gray-400"
                    }`}
                >
                    <PenLine size={14} /> Stift
                </button>
                <button
                    type="button"
                    disabled={disabled || busy || !!uploadB64}
                    onClick={() => setTool("eraser")}
                    className={`inline-flex items-center gap-1.5 px-3 py-1.5 rounded-lg text-xs border ${
                        tool === "eraser" ? "bg-[#5B9DFF]/20 border-[#5B9DFF]/50 text-[#9EC5FF]" : "border-[#2A3140] text-gray-400"
                    }`}
                >
                    <Eraser size={14} /> Radierer
                </button>
                <button
                    type="button"
                    disabled={disabled || busy}
                    onClick={clearCanvas}
                    className="inline-flex items-center gap-1.5 px-3 py-1.5 rounded-lg text-xs border border-[#2A3140] text-gray-400"
                >
                    <Trash2 size={14} /> Leeren
                </button>
                {allowUpload ? (
                    <>
                        <input
                            ref={fileRef}
                            type="file"
                            accept="image/png,image/jpeg,image/webp"
                            className="hidden"
                            onChange={(e) => void onPickFile(e.target.files?.[0] || null)}
                        />
                        <button
                            type="button"
                            disabled={disabled || busy}
                            onClick={() => fileRef.current?.click()}
                            className="inline-flex items-center gap-1.5 px-3 py-1.5 rounded-lg text-xs border border-[#2A3140] text-gray-300"
                        >
                            <Upload size={14} /> Bild hochladen
                        </button>
                    </>
                ) : null}
            </div>

            {uploadPreview ? (
                <div className="rounded-xl border border-[#2A3140] bg-[#0E1117] overflow-hidden p-2">
                    {/* eslint-disable-next-line @next/next/no-img-element */}
                    <img src={uploadPreview} alt="Upload" className="max-h-64 mx-auto object-contain" />
                    <p className="text-[11px] text-gray-500 text-center mt-1 flex items-center justify-center gap-1">
                        <ImageIcon size={12} /> Hochgeladenes Bild
                    </p>
                </div>
            ) : (
                <div className="rounded-xl border border-[#2A3140] overflow-hidden touch-none">
                    <canvas
                        ref={canvasRef}
                        className="w-full block cursor-crosshair"
                        onPointerDown={onPointerDown}
                        onPointerMove={onPointerMove}
                        onPointerUp={onPointerUp}
                        onPointerCancel={onPointerUp}
                    />
                </div>
            )}

            <button
                type="button"
                disabled={!canSubmit}
                onClick={() => {
                    if (uploadB64) {
                        onSubmit({ image_base64: uploadB64 });
                        return;
                    }
                    const ink = exportInkPng();
                    if (ink) onSubmit({ ink_png_base64: ink });
                }}
                className="inline-flex items-center gap-2 px-5 py-3 rounded-xl bg-[#5B9DFF] text-white font-semibold disabled:opacity-50 min-h-[48px]"
            >
                {busy ? <Loader2 size={16} className="animate-spin" /> : <PenLine size={16} />}
                {busy ? "Wird ausgewertet…" : "Zeichnung prüfen"}
            </button>
        </div>
    );
}
