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
const MAX_RAW_FALLBACK = 3.5 * 1024 * 1024;

function fileToDataUrl(file: File): Promise<string> {
    return new Promise((resolve, reject) => {
        const reader = new FileReader();
        reader.onload = () => {
            const raw = String(reader.result || "");
            if (!raw.startsWith("data:")) {
                reject(new Error("Datei konnte nicht gelesen werden."));
                return;
            }
            resolve(raw);
        };
        reader.onerror = () => reject(new Error("Datei lesen fehlgeschlagen."));
        reader.readAsDataURL(file);
    });
}

function splitDataUrl(dataUrl: string): { mime: string; b64: string } | null {
    const raw = dataUrl.trim();
    const marker = ";base64,";
    const idx = raw.indexOf(marker);
    if (idx < 0) return null;
    const header = raw.slice(0, idx);
    const b64 = raw.slice(idx + marker.length).replace(/\s/g, "");
    if (!b64) return null;
    const mimeMatch = /^data:([^;,]+)/.exec(header);
    return { mime: mimeMatch?.[1] || "image/jpeg", b64 };
}

function canvasToJpegDataUrl(canvas: HTMLCanvasElement, quality: number): string {
    try {
        return canvas.toDataURL("image/jpeg", quality);
    } catch {
        try {
            return canvas.toDataURL("image/png");
        } catch {
            return "";
        }
    }
}

async function loadImageElement(src: string): Promise<HTMLImageElement> {
    return new Promise((resolve, reject) => {
        const el = new Image();
        el.onload = () => resolve(el);
        el.onerror = () => reject(new Error("Bild konnte nicht gelesen werden. Bitte JPG oder PNG verwenden."));
        el.src = src;
    });
}

async function compressImageFile(file: File): Promise<{ b64: string; mime: string; preview: string }> {
    const nameOk = /\.(jpe?g|png|webp|gif|bmp|heic|heif)$/i.test(file.name);
    if (file.type && !file.type.startsWith("image/") && !nameOk) {
        throw new Error("Bitte ein Bild (JPG/PNG/WebP) wählen.");
    }
    if (!file.size) throw new Error("Leere Datei.");

    // 1) Always get a raw data-URL first (works for most camera JPEGs without canvas).
    let rawDataUrl = "";
    try {
        rawDataUrl = await fileToDataUrl(file);
    } catch {
        /* continue — try object URL path */
    }

    // 2) Try canvas downscale for large photos.
    const objectUrl = URL.createObjectURL(file);
    try {
        let img: HTMLImageElement | null = null;
        try {
            img = await loadImageElement(objectUrl);
        } catch {
            if (rawDataUrl) img = await loadImageElement(rawDataUrl);
        }
        if (!img) throw new Error("Bild konnte nicht geladen werden.");

        const w0 = img.naturalWidth || img.width;
        const h0 = img.naturalHeight || img.height;
        if (!w0 || !h0) {
            // Some HEIC/odd formats report 0 — fall back to raw bytes if small enough.
            if (rawDataUrl && file.size <= MAX_RAW_FALLBACK) {
                const parts = splitDataUrl(rawDataUrl);
                if (parts?.b64) {
                    return { b64: parts.b64, mime: parts.mime, preview: rawDataUrl };
                }
            }
            throw new Error("Ungültiges Bild — bitte als JPG speichern und erneut hochladen.");
        }

        const scale = Math.min(1, MAX_EDGE / Math.max(w0, h0));
        let w = Math.max(1, Math.round(w0 * scale));
        let h = Math.max(1, Math.round(h0 * scale));
        // Cap pixel count for mobile Safari canvas limits
        while (w * h > 2_500_000) {
            w = Math.max(1, Math.round(w * 0.85));
            h = Math.max(1, Math.round(h * 0.85));
        }

        const canvas = document.createElement("canvas");
        canvas.width = w;
        canvas.height = h;
        const ctx = canvas.getContext("2d", { alpha: false });
        if (!ctx) throw new Error("Bildverarbeitung fehlgeschlagen.");
        ctx.fillStyle = "#ffffff";
        ctx.fillRect(0, 0, w, h);
        ctx.drawImage(img, 0, 0, w, h);

        let quality = 0.82;
        let dataUrl = canvasToJpegDataUrl(canvas, quality);
        while (dataUrl && dataUrl.length > 2_000_000 && quality > 0.4) {
            quality -= 0.12;
            dataUrl = canvasToJpegDataUrl(canvas, quality);
        }
        while ((!dataUrl || dataUrl.length > 2_000_000) && Math.max(w, h) > 480) {
            w = Math.max(1, Math.round(w * 0.7));
            h = Math.max(1, Math.round(h * 0.7));
            canvas.width = w;
            canvas.height = h;
            ctx.fillStyle = "#ffffff";
            ctx.fillRect(0, 0, w, h);
            ctx.drawImage(img, 0, 0, w, h);
            dataUrl = canvasToJpegDataUrl(canvas, 0.72);
        }

        const parts = dataUrl ? splitDataUrl(dataUrl) : null;
        if (parts?.b64) {
            return { b64: parts.b64, mime: parts.mime || "image/jpeg", preview: dataUrl };
        }

        // 3) Canvas failed → raw FileReader fallback
        if (rawDataUrl && file.size <= MAX_RAW_FALLBACK) {
            const rawParts = splitDataUrl(rawDataUrl);
            if (rawParts?.b64) {
                return { b64: rawParts.b64, mime: rawParts.mime, preview: rawDataUrl };
            }
        }
        throw new Error("Bild konnte nicht verarbeitet werden. Bitte als JPG (unter 4 MB) speichern.");
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
    const [dragOver, setDragOver] = useState(false);
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

    const softResize = useCallback(() => {
        const canvas = canvasRef.current;
        if (!canvas || uploadB64 || uploadOnly) return;
        const parent = canvas.parentElement;
        const cssW = Math.max(280, parent?.clientWidth || 480);
        const cssH = 260;
        const prev = cssSizeRef.current;
        if (Math.abs(prev.w - cssW) < 8 && Math.abs(prev.h - cssH) < 2) return;
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
        // Defer one frame so parent width is laid out
        const t = window.setTimeout(() => paintBlank(), 0);
        const onResize = () => softResize();
        window.addEventListener("resize", onResize);
        return () => {
            window.clearTimeout(t);
            window.removeEventListener("resize", onResize);
        };
    }, [paintBlank, softResize, uploadOnly]);

    const pos = (e: React.PointerEvent<HTMLCanvasElement>) => {
        const canvas = canvasRef.current!;
        const rect = canvas.getBoundingClientRect();
        return { x: e.clientX - rect.left, y: e.clientY - rect.top };
    };

    const onPointerDown = (e: React.PointerEvent<HTMLCanvasElement>) => {
        if (disabled || busy || uploadB64) return;
        e.preventDefault();
        const canvas = canvasRef.current;
        const ctx = canvas?.getContext("2d");
        if (!canvas || !ctx) return;
        try {
            canvas.setPointerCapture(e.pointerId);
        } catch {
            /* ignore */
        }
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
        // Dot so a tap counts as ink
        ctx.lineTo(p.x + 0.01, p.y);
        ctx.stroke();
        if (tool === "pen") markInk(true);
    };

    const onPointerMove = (e: React.PointerEvent<HTMLCanvasElement>) => {
        if (!drawing.current || disabled || busy || uploadB64) return;
        e.preventDefault();
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
            if (!b64) throw new Error("Bild konnte nicht verarbeitet werden.");
            setUploadB64(b64);
            setUploadPreview(preview);
            markInk(false);
        } catch (err) {
            setUploadB64(null);
            setUploadPreview(null);
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
        try {
            const dataUrl = exportCanvas.toDataURL("image/png");
            const parts = splitDataUrl(dataUrl);
            return parts?.b64 || null;
        } catch {
            return null;
        }
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

    const openPicker = () => {
        if (disabled || busy || uploadBusy) return;
        fileRef.current?.click();
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
                            accept="image/*,image/jpeg,image/png,image/webp,.jpg,.jpeg,.png,.webp"
                            capture="environment"
                            className="hidden"
                            onChange={(e) => void onPickFile(e.target.files?.[0] || null)}
                        />
                        <button
                            type="button"
                            disabled={disabled || busy || uploadBusy}
                            onClick={openPicker}
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
                        <ImageIcon size={12} /> Foto bereit — jetzt prüfen
                    </p>
                </div>
            ) : uploadOnly ? (
                <button
                    type="button"
                    disabled={disabled || busy || uploadBusy}
                    onClick={openPicker}
                    onDragOver={(e) => {
                        e.preventDefault();
                        setDragOver(true);
                    }}
                    onDragLeave={() => setDragOver(false)}
                    onDrop={(e) => {
                        e.preventDefault();
                        setDragOver(false);
                        const f = e.dataTransfer.files?.[0];
                        if (f) void onPickFile(f);
                    }}
                    className={`w-full rounded-xl border border-dashed px-4 py-10 text-center text-sm transition-colors ${
                        dragOver
                            ? "border-[#5B9DFF] bg-[#5B9DFF]/10 text-[#9EC5FF]"
                            : "border-[#2A3140] bg-[#0E1117] text-gray-500 hover:border-[#5B9DFF]/40 hover:text-gray-300"
                    }`}
                >
                    {uploadBusy ? (
                        <span className="inline-flex items-center gap-2">
                            <Loader2 size={16} className="animate-spin" /> Bild wird vorbereitet…
                        </span>
                    ) : (
                        <>
                            Tippen zum Hochladen / Fotografieren
                            <br />
                            <span className="text-[11px] text-gray-600">JPG oder PNG — die KI wertet es aus</span>
                        </>
                    )}
                </button>
            ) : (
                <div className="rounded-xl border border-[#2A3140] overflow-hidden touch-none">
                    <canvas
                        ref={canvasRef}
                        className="w-full block cursor-crosshair touch-none"
                        style={{ touchAction: "none" }}
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
