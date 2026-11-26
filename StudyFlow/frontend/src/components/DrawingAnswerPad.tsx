"use client";

import React, { useCallback, useEffect, useRef, useState } from "react";
import { Eraser, ImageIcon, Loader2, PenLine, Trash2, Upload } from "lucide-react";

export type DrawingGradeResult = {
    correct: boolean;
    score: number;
    feedback: string;
    matched_rubric?: string[];
};

export type DrawingSubmitPayload = { ink_png_base64?: string; image_base64?: string };

type Props = {
    allowUpload?: boolean;
    disabled?: boolean;
    busy?: boolean;
    /** When true, hide the sketch canvas and only offer photo upload (Freitext-Handschrift). */
    uploadOnly?: boolean;
    submitLabel?: string;
    uploadLabel?: string;
    onSubmit: (payload: DrawingSubmitPayload) => void;
};

const MAX_EDGE = 1280;
const TARGET_BYTES = 1.5 * 1024 * 1024;

async function compressImageFile(file: File): Promise<{ b64: string; mime: string; preview: string }> {
    if (!file.type.startsWith("image/") && !/\.(jpe?g|png|webp|heic|heif|gif)$/i.test(file.name)) {
        throw new Error("Bitte ein Bild (JPG/PNG/WebP) wählen.");
    }
    // Try decode via createImageBitmap / Image — HEIC may fail in some browsers.
    const objectUrl = URL.createObjectURL(file);
    try {
        const img = await new Promise<HTMLImageElement>((resolve, reject) => {
            const el = new Image();
            el.onload = () => resolve(el);
            el.onerror = () => reject(new Error("Bild konnte nicht gelesen werden (HEIC?). Bitte als JPG speichern."));
            el.src = objectUrl;
        });
        const w0 = img.naturalWidth || img.width;
        const h0 = img.naturalHeight || img.height;
        if (!w0 || !h0) throw new Error("Ungültiges Bild.");
        const scale = Math.min(1, MAX_EDGE / Math.max(w0, h0));
        let w = Math.max(1, Math.round(w0 * scale));
        let h = Math.max(1, Math.round(h0 * scale));
        const canvas = document.createElement("canvas");
        canvas.width = w;
        canvas.height = h;
        const ctx = canvas.getContext("2d");
        if (!ctx) throw new Error("Bildverarbeitung fehlgeschlagen.");
        ctx.fillStyle = "#ffffff";
        ctx.fillRect(0, 0, w, h);
        ctx.drawImage(img, 0, 0, w, h);

        let quality = 0.85;
        let dataUrl = canvas.toDataURL("image/jpeg", quality);
        while (dataUrl.length > TARGET_BYTES * 1.37 && quality > 0.45) {
            quality -= 0.1;
            dataUrl = canvas.toDataURL("image/jpeg", quality);
        }
        // Still huge → shrink further
        while (dataUrl.length > TARGET_BYTES * 1.37 && Math.max(w, h) > 640) {
            w = Math.max(1, Math.round(w * 0.75));
            h = Math.max(1, Math.round(h * 0.75));
            canvas.width = w;
            canvas.height = h;
            ctx.fillStyle = "#ffffff";
            ctx.fillRect(0, 0, w, h);
            ctx.drawImage(img, 0, 0, w, h);
            dataUrl = canvas.toDataURL("image/jpeg", 0.75);
        }
        if (dataUrl.length > TARGET_BYTES * 1.37) {
            throw new Error("Foto zu groß — bitte näher ran oder Ausschnitt wählen.");
        }
        const b64 = dataUrl.includes(",") ? dataUrl.split(",", 1)[1] : dataUrl;
        if (!b64) throw new Error("Bild konnte nicht kodiert werden.");
        return { b64, mime: "image/jpeg", preview: dataUrl };
    } finally {
        URL.revokeObjectURL(objectUrl);
    }
}

/** Canvas sketch pad + optional image upload for Smart Learning drawing / handwriting answers. */
export default function DrawingAnswerPad({
    allowUpload = true,
    disabled = false,
    busy = false,
    uploadOnly = false,
    submitLabel = "Zeichnung prüfen",
    uploadLabel = "Foto der Handschrift",
    onSubmit,
}: Props) {
    const canvasRef = useRef<HTMLCanvasElement | null>(null);
    const dprRef = useRef(1);
    const cssSizeRef = useRef({ w: 480, h: 260 });
    const hasInkRef = useRef(false);
    const drawing = useRef(false);
    const [tool, setTool] = useState<"pen" | "eraser">("pen");
    const [hasInk, setHasInk] = useState(false);
    const [uploadPreview, setUploadPreview] = useState<string | null>(null);
    const [uploadB64, setUploadB64] = useState<string | null>(null);
    const [localError, setLocalError] = useState("");
    const [uploadBusy, setUploadBusy] = useState(false);
    const fileRef = useRef<HTMLInputElement | null>(null);

    const markInk = (v: boolean) => {
        hasInkRef.current = v;
        setHasInk(v);
    };

    const paintBlank = useCallback(() => {
        const canvas = canvasRef.current;
        if (!canvas) return;
        const parent = canvas.parentElement;
        const cssW = Math.max(280, parent?.clientWidth || 480);
        const cssH = 260;
        const dpr = Math.min(window.devicePixelRatio || 1, 2);
        dprRef.current = dpr;
        cssSizeRef.current = { w: cssW, h: cssH };
        canvas.width = Math.floor(cssW * dpr);
        canvas.height = Math.floor(cssH * dpr);
        canvas.style.width = `${cssW}px`;
        canvas.style.height = `${cssH}px`;
        const ctx = canvas.getContext("2d");
        if (!ctx) return;
        ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
        ctx.fillStyle = "#0E1117";
        ctx.fillRect(0, 0, cssW, cssH);
        markInk(false);
    }, []);

    /** Resize without wiping strokes when dimensions barely change. */
    const softResize = useCallback(() => {
        const canvas = canvasRef.current;
        if (!canvas || uploadB64 || uploadOnly) return;
        const parent = canvas.parentElement;
        const cssW = Math.max(280, parent?.clientWidth || 480);
        const cssH = 260;
        const prev = cssSizeRef.current;
        if (Math.abs(prev.w - cssW) < 8 && Math.abs(prev.h - cssH) < 2) return;
        // Preserve bitmap
        const bak = document.createElement("canvas");
        bak.width = canvas.width;
        bak.height = canvas.height;
        const bctx = bak.getContext("2d");
        if (bctx) bctx.drawImage(canvas, 0, 0);
        const dpr = Math.min(window.devicePixelRatio || 1, 2);
        dprRef.current = dpr;
        cssSizeRef.current = { w: cssW, h: cssH };
        canvas.width = Math.floor(cssW * dpr);
        canvas.height = Math.floor(cssH * dpr);
        canvas.style.width = `${cssW}px`;
        canvas.style.height = `${cssH}px`;
        const ctx = canvas.getContext("2d");
        if (!ctx) return;
        ctx.setTransform(1, 0, 0, 1, 0, 0);
        ctx.fillStyle = "#0E1117";
        ctx.fillRect(0, 0, canvas.width, canvas.height);
        if (bctx) ctx.drawImage(bak, 0, 0, canvas.width, canvas.height);
        ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    }, [uploadB64, uploadOnly]);

    useEffect(() => {
        if (uploadOnly) return;
        paintBlank();
        const onResize = () => softResize();
        window.addEventListener("resize", onResize);
        return () => window.removeEventListener("resize", onResize);
    }, [paintBlank, softResize, uploadOnly]);

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
        const dpr = dprRef.current;
        ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
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
        if (tool === "pen") markInk(true);
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
        setLocalError("");
        setUploadPreview(null);
        setUploadB64(null);
        if (fileRef.current) fileRef.current.value = "";
        if (!uploadOnly) paintBlank();
    };

    const onPickFile = async (file: File | null) => {
        if (!file || !allowUpload) return;
        setLocalError("");
        setUploadBusy(true);
        try {
            const { b64, preview } = await compressImageFile(file);
            setUploadB64(b64);
            setUploadPreview(preview);
            markInk(false);
        } catch (err) {
            setLocalError(err instanceof Error ? err.message : "Upload fehlgeschlagen.");
        } finally {
            setUploadBusy(false);
            if (fileRef.current) fileRef.current.value = "";
        }
    };

    const exportInkPng = (): string | null => {
        const canvas = canvasRef.current;
        if (!canvas || !hasInkRef.current) return null;
        const exportCanvas = document.createElement("canvas");
        exportCanvas.width = canvas.width;
        exportCanvas.height = canvas.height;
        const ex = exportCanvas.getContext("2d");
        if (!ex) return null;
        ex.fillStyle = "#ffffff";
        ex.fillRect(0, 0, exportCanvas.width, exportCanvas.height);
        ex.drawImage(canvas, 0, 0);
        try {
            const img = ex.getImageData(0, 0, exportCanvas.width, exportCanvas.height);
            const d = img.data;
            for (let i = 0; i < d.length; i += 4) {
                const lum = (d[i] + d[i + 1] + d[i + 2]) / 3;
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

    const canSubmit = !disabled && !busy && !uploadBusy && (!!uploadB64 || (!uploadOnly && hasInk));

    const handleSubmit = () => {
        setLocalError("");
        if (uploadB64) {
            onSubmit({ image_base64: uploadB64 });
            return;
        }
        if (uploadOnly) {
            setLocalError("Bitte zuerst ein Foto der Handschrift hochladen.");
            return;
        }
        const ink = exportInkPng();
        if (!ink) {
            setLocalError("Bitte zuerst zeichnen oder ein Foto hochladen.");
            return;
        }
        onSubmit({ ink_png_base64: ink });
    };

    return (
        <div className="space-y-3">
            <div className="flex flex-wrap gap-2 items-center">
                {!uploadOnly ? (
                    <>
                        <button
                            type="button"
                            disabled={disabled || busy || !!uploadB64}
                            onClick={() => setTool("pen")}
                            className={`inline-flex items-center gap-1.5 px-3 py-1.5 rounded-lg text-xs border ${
                                tool === "pen"
                                    ? "bg-[#5B9DFF]/20 border-[#5B9DFF]/50 text-[#9EC5FF]"
                                    : "border-[#2A3140] text-gray-400"
                            }`}
                        >
                            <PenLine size={14} /> Stift
                        </button>
                        <button
                            type="button"
                            disabled={disabled || busy || !!uploadB64}
                            onClick={() => setTool("eraser")}
                            className={`inline-flex items-center gap-1.5 px-3 py-1.5 rounded-lg text-xs border ${
                                tool === "eraser"
                                    ? "bg-[#5B9DFF]/20 border-[#5B9DFF]/50 text-[#9EC5FF]"
                                    : "border-[#2A3140] text-gray-400"
                            }`}
                        >
                            <Eraser size={14} /> Radierer
                        </button>
                    </>
                ) : null}
                <button
                    type="button"
                    disabled={disabled || busy || uploadBusy}
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
                            accept="image/*,.jpg,.jpeg,.png,.webp,.heic,.heif"
                            className="hidden"
                            onChange={(e) => void onPickFile(e.target.files?.[0] || null)}
                        />
                        <button
                            type="button"
                            disabled={disabled || busy || uploadBusy}
                            onClick={() => fileRef.current?.click()}
                            className="inline-flex items-center gap-1.5 px-3 py-1.5 rounded-lg text-xs border border-[#2A3140] text-gray-300"
                        >
                            {uploadBusy ? <Loader2 size={14} className="animate-spin" /> : <Upload size={14} />}
                            {uploadLabel}
                        </button>
                    </>
                ) : null}
            </div>

            {uploadPreview ? (
                <div className="rounded-xl border border-[#2A3140] bg-[#0E1117] overflow-hidden p-2">
                    {/* eslint-disable-next-line @next/next/no-img-element */}
                    <img src={uploadPreview} alt="Upload" className="max-h-64 mx-auto object-contain" />
                    <p className="text-[11px] text-gray-500 text-center mt-1 flex items-center justify-center gap-1">
                        <ImageIcon size={12} /> Foto der Handschrift
                    </p>
                </div>
            ) : uploadOnly ? (
                <div className="rounded-xl border border-dashed border-[#2A3140] bg-[#0E1117] px-4 py-8 text-center text-sm text-gray-500">
                    Foto deiner handschriftlichen Lösung hochladen — die KI wertet es aus.
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

            {localError ? <p className="text-sm text-red-400">{localError}</p> : null}

            <button
                type="button"
                disabled={!canSubmit}
                onClick={handleSubmit}
                className="inline-flex items-center gap-2 px-5 py-3 rounded-xl bg-[#5B9DFF] text-white font-semibold disabled:opacity-50 min-h-[48px]"
            >
                {busy || uploadBusy ? <Loader2 size={16} className="animate-spin" /> : <PenLine size={16} />}
                {busy ? "Wird ausgewertet…" : submitLabel}
            </button>
        </div>
    );
}
