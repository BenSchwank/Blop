import React, { useState, useRef, useEffect } from 'react';
import { motion, AnimatePresence } from 'framer-motion';
import { MessageCircle, X, Send, Loader2, Maximize2, Minimize2 } from 'lucide-react';
import ReactMarkdown from 'react-markdown';
import remarkGfm from 'remark-gfm';
import remarkMath from 'remark-math';
import rehypeKatex from 'rehype-katex';
import 'katex/dist/katex.min.css';
import { OVERLAY_FLOATING_CHAT, OVERLAY_FLOATING_CHAT_DOCK } from '@/constants/overlayLayout';

interface Message {
    role: 'user' | 'model';
    content: string;
}

interface FloatingChatProps {
    folderId: string;
    username: string;
    modelPreference?: string;
    activeFile?: any | null;
    onUpdateActiveFile?: (newContent: any) => Promise<void>;
    onApplyPatch?: (patch: DocumentPatch) => Promise<void>;
    onUndoLastPatch?: () => Promise<void>;
    canUndo?: boolean;
    onOpenChange?: (open: boolean, expanded: boolean) => void;
}

interface DocumentPatch {
    patch_description: string;
    apply_mode: 'append' | 'replace_section';
    section_hint?: string;
    new_text: string;
    confidence?: number;
}

function isPatchResponse(obj: any): obj is DocumentPatch {
    return obj && typeof obj === 'object' && typeof obj.new_text === 'string' && typeof obj.apply_mode === 'string';
}

function wantsDocumentEdit(text: string): boolean {
    return /\b(änder|aender|umschreib|ergänz|erganz|korrigier|füge|fuege|lösch|loesch|ersetze|kürze|kuerze|formulier)/i.test(text);
}

/** Repair classic UTF-8-as-Latin1 mojibake (PrÃ¤prozessor, â). */
function fixMojibake(text: string): string {
    if (!text || !/[ÃÂâ]/.test(text)) return text;
    try {
        const bytes = Uint8Array.from(Array.from(text, (ch) => ch.charCodeAt(0) & 0xff));
        const decoded = new TextDecoder("utf-8").decode(bytes);
        if (decoded.includes("�")) return text;
        return decoded;
    } catch {
        return text;
    }
}

export default function FloatingChat({ folderId, username, modelPreference, activeFile, onUpdateActiveFile, onApplyPatch, onUndoLastPatch, canUndo = false, onOpenChange }: FloatingChatProps) {
    const [isOpen, setIsOpen] = useState(false);
    const [isExpanded, setIsExpanded] = useState(false);
    const [editDocument, setEditDocument] = useState(false);
    const [messages, setMessages] = useState<Message[]>([
        { role: 'model', content: 'Hallo! Ich bin dein Blop KI-Assistent. Hast du Fragen zu den Dokumenten in diesem Ordner?' }
    ]);
    const [input, setInput] = useState('');
    const [isLoading, setIsLoading] = useState(false);
    const [pendingPatch, setPendingPatch] = useState<DocumentPatch | null>(null);
    const [lastUsageInfo, setLastUsageInfo] = useState<string>('');
    const [vvOffset, setVvOffset] = useState({ top: 0, height: 0 });
    const messagesEndRef = useRef<HTMLDivElement>(null);
    const dockRef = useRef<HTMLDivElement>(null);

    const scrollToBottom = () => {
        messagesEndRef.current?.scrollIntoView({ behavior: 'smooth' });
    };

    useEffect(() => {
        if (isOpen) scrollToBottom();
    }, [messages, isOpen]);

    const onOpenChangeRef = useRef(onOpenChange);
    onOpenChangeRef.current = onOpenChange;
    useEffect(() => {
        onOpenChangeRef.current?.(isOpen, isExpanded);
    }, [isOpen, isExpanded]);

    // Keep the mobile chat sheet inside the visual viewport (keyboard-safe).
    useEffect(() => {
        if (!isOpen || typeof window === 'undefined') return;
        const vv = window.visualViewport;
        if (!vv) return;
        const sync = () => {
            setVvOffset({ top: vv.offsetTop, height: vv.height });
        };
        sync();
        vv.addEventListener('resize', sync);
        vv.addEventListener('scroll', sync);
        return () => {
            vv.removeEventListener('resize', sync);
            vv.removeEventListener('scroll', sync);
        };
    }, [isOpen]);

    const handleSend = async () => {
        if (!input.trim() || isLoading) return;

        const userMsg = input.trim();
        setInput('');
        const newMessages: Message[] = [...messages, { role: 'user', content: userMsg }];
        setMessages(newMessages);
        setIsLoading(true);

        const requestBody = {
            username,
            folder_id: folderId,
            file_id: activeFile?.id,
            message: userMsg,
            history: messages.filter(m => m.role !== 'model' || m.content !== 'Hallo! Ich bin dein Blop KI-Assistent. Hast du Fragen zu den Dokumenten in diesem Ordner?'),
            model_preference: modelPreference || null,
            active_file: activeFile || null,
        };

        try {
            const hasTextDocument = activeFile && typeof activeFile.content === 'string';
            const usePatch = Boolean(hasTextDocument && (editDocument || wantsDocumentEdit(userMsg)));

            if (usePatch) {
                const res = await fetch('/api/ai/document-chat', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(requestBody),
                });
                let errData = null;
                if (!res.ok) {
                    try { errData = await res.json(); } catch (_) {}
                    if (res.status === 402 || errData?.detail?.includes?.("Nicht genügend Tokens")) {
                        throw new Error(errData?.detail || "Nicht genügend Tokens. Bitte lade dein Abo in den Einstellungen auf!");
                    }
                    if (errData?.detail === "NO_API_KEY_FOUND") throw new Error("Serverfehler: Der AI Key wurde vom Administrator nicht konfiguriert.");
                    throw new Error(typeof errData?.detail === 'string' ? errData.detail : 'Netzwerkfehler');
                }
                const data = await res.json();
                if (typeof data?.tokens_charged === 'number') {
                    setLastUsageInfo(`Modell: ${data.used_model || 'auto'} · -${data.tokens_charged} Tokens`);
                    window.dispatchEvent(new Event('blop_tokens_updated'));
                }
                if (data?.patch && isPatchResponse(data.patch)) {
                    setPendingPatch(data.patch);
                    setMessages([...newMessages, {
                        role: 'model',
                        content: `Vorschlag: **${data.patch.patch_description || 'Dokumentänderung'}**\n\n${data.patch.new_text.substring(0, 1400)}${data.patch.new_text.length > 1400 ? '\n\n…(gekürzt)' : ''}`,
                    }]);
                } else {
                    const reply = typeof data?.reply === 'string' && data.reply.trim()
                        ? data.reply
                        : 'Ich konnte keine Dokumentänderung vorschlagen. Formuliere die Änderung genauer, oder schalte „Dokument ändern“ aus und stell die Frage normal.';
                    setMessages([...newMessages, { role: 'model', content: reply }]);
                }
                return;
            }

            // General chat: use streaming endpoint
            const res = await fetch('/api/ai/chat/stream', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(requestBody),
            });

            if (!res.ok) {
                let errData = null;
                try { errData = await res.json(); } catch (_) {}
                if (res.status === 402 || errData?.detail?.includes?.("Nicht genügend Tokens")) {
                    throw new Error(errData?.detail || "Nicht genügend Tokens. Bitte lade dein Abo in den Einstellungen auf!");
                }
                if (errData?.detail === "NO_API_KEY_FOUND") throw new Error("Serverfehler: Der AI Key wurde vom Administrator nicht konfiguriert.");
                throw new Error(typeof errData?.detail === 'string' ? errData.detail : 'Netzwerkfehler');
            }

            // Add a placeholder message and fill it as chunks arrive
            const aiMsgIndex = newMessages.length;
            setMessages([...newMessages, { role: 'model', content: '' }]);

            const reader = res.body!.getReader();
            const decoder = new TextDecoder("utf-8");
            let streamedContent = '';
            let buffer = '';

            while (true) {
                const { done, value } = await reader.read();
                if (done) break;
                buffer += decoder.decode(value, { stream: true });
                const lines = buffer.split('\n');
                buffer = lines.pop() ?? '';

                for (const line of lines) {
                    if (!line.startsWith('data: ')) continue;
                    const payload = line.slice(6).trim();
                    if (!payload) continue;
                    let data: any;
                    try { data = JSON.parse(payload); } catch { continue; }

                    if (data.error) throw new Error(fixMojibake(String(data.error)));

                    if (data.text) {
                        streamedContent = fixMojibake(streamedContent + data.text);
                        setMessages(prev => {
                            const updated = [...prev];
                            updated[aiMsgIndex] = { role: 'model', content: streamedContent };
                            return updated;
                        });
                    }

                    if (data.done) {
                        if (typeof data.tokens_charged === 'number') {
                            setLastUsageInfo(`Modell: ${data.used_model || 'auto'} · -${data.tokens_charged} Tokens`);
                            window.dispatchEvent(new Event('blop_tokens_updated'));
                        }
                    }
                }
            }

            // Check if the full streamed content is a JSON file-update action
            try {
                let jsonStr = streamedContent.trim();
                if (jsonStr.startsWith('```json')) jsonStr = jsonStr.slice(7);
                if (jsonStr.startsWith('```')) jsonStr = jsonStr.slice(3);
                if (jsonStr.endsWith('```')) jsonStr = jsonStr.slice(0, -3);
                const jsonObj = JSON.parse(jsonStr.trim());
                if (jsonObj?.blop_action === "update_file" && onUpdateActiveFile) {
                    await onUpdateActiveFile(jsonObj.new_content);
                    setMessages([...newMessages, { role: 'model', content: `Ich habe das Dokument "${activeFile?.name}" für dich aktualisiert! ✨` }]);
                }
            } catch { /* normal text reply */ }

        } catch (error: any) {
            console.error(error);
            setMessages(prev => {
                // Replace empty placeholder if it exists, otherwise append
                const last = prev[prev.length - 1];
                if (last?.role === 'model' && last.content === '') {
                    return [...prev.slice(0, -1), { role: 'model', content: error.message || 'Verbindungsfehler. Bitte versuche es erneut.' }];
                }
                return [...prev, { role: 'model', content: error.message || 'Verbindungsfehler. Bitte versuche es erneut.' }];
            });
        } finally {
            setIsLoading(false);
        }
    };

    const handleApplyPatch = async () => {
        if (!pendingPatch || !onApplyPatch) return;
        setIsLoading(true);
        try {
            await onApplyPatch(pendingPatch);
            setMessages((prev) => [...prev, { role: 'model', content: 'Änderung übernommen und gespeichert. ✅' }]);
            setPendingPatch(null);
        } catch (error: any) {
            setMessages((prev) => [...prev, { role: 'model', content: error?.message || 'Patch konnte nicht angewendet werden.' }]);
        } finally {
            setIsLoading(false);
        }
    };

    const handleRejectPatch = () => {
        if (!pendingPatch) return;
        setPendingPatch(null);
        setMessages((prev) => [...prev, { role: 'model', content: 'Änderung verworfen.' }]);
    };

    return (
        <>
            <AnimatePresence>
                {isOpen && (
                    <motion.div
                        key="chat-dock"
                        ref={dockRef}
                        initial={{ opacity: 0, x: 40 }}
                        animate={{ opacity: 1, x: 0 }}
                        exit={{ opacity: 0, x: 40 }}
                        transition={{ type: 'spring', damping: 28, stiffness: 320 }}
                        className={`${OVERLAY_FLOATING_CHAT_DOCK} max-md:w-auto ${isExpanded ? 'md:w-[min(100vw-1.25rem,40rem)]' : 'md:w-[min(100vw-1.25rem,26rem)]'}`}
                        style={
                            vvOffset.height > 0
                                ? {
                                      // Mobile: pin to visualViewport so the composer stays above the keyboard
                                      top: typeof window !== 'undefined' && window.innerWidth < 768
                                          ? Math.max(8, vvOffset.top + 8)
                                          : undefined,
                                      height:
                                          typeof window !== 'undefined' && window.innerWidth < 768
                                              ? Math.max(240, vvOffset.height - 16)
                                              : undefined,
                                      bottom:
                                          typeof window !== 'undefined' && window.innerWidth < 768
                                              ? 'auto'
                                              : undefined,
                                  }
                                : undefined
                        }
                        role="dialog"
                        aria-label="Blop KI-Assistent"
                    >
                        <div className="bg-[#1A1F2A] p-2.5 md:p-3.5 border-b border-[#2A3140] flex items-center justify-between shrink-0 gap-2">
                            <div className="flex items-center gap-2 md:gap-2.5 min-w-0">
                                <div className="w-8 h-8 md:w-9 md:h-9 rounded-full bg-[#5B9DFF]/15 text-[#5B9DFF] flex items-center justify-center ring-1 ring-[#5B9DFF]/25">
                                    <MessageCircle size={18} />
                                </div>
                                <div className="min-w-0">
                                    <h3 className="text-white font-semibold text-sm truncate">Blop KI-Assistent</h3>
                                    <p className="text-[10px] text-gray-500 truncate hidden md:block">Rechts am Rand · zum Ordner fragen</p>
                                </div>
                            </div>
                            <div className="flex items-center gap-1 shrink-0">
                                {canUndo ? (
                                <button
                                    type="button"
                                    disabled={isLoading}
                                    onClick={async () => {
                                        if (!onUndoLastPatch) return;
                                        setIsLoading(true);
                                        try {
                                            await onUndoLastPatch();
                                            setMessages((prev) => [...prev, { role: 'model', content: 'Letzte Änderung rückgängig gemacht.' }]);
                                        } catch (e: any) {
                                            setMessages((prev) => [...prev, { role: 'model', content: e?.message || 'Rückgängig fehlgeschlagen.' }]);
                                        } finally {
                                            setIsLoading(false);
                                        }
                                    }}
                                    className="px-2 py-1 text-[11px] rounded-lg border border-[#3A4558] text-gray-300 hover:text-white hover:bg-[#243044] disabled:opacity-40"
                                >
                                    Rückgängig
                                </button>
                                ) : null}
                                <button
                                    type="button"
                                    onClick={() => setIsExpanded(!isExpanded)}
                                    className="p-1.5 text-gray-400 hover:text-white hover:bg-[#243044] rounded-lg max-md:hidden"
                                    aria-label={isExpanded ? 'Schmaler' : 'Breiter'}
                                >
                                    {isExpanded ? <Minimize2 size={16} /> : <Maximize2 size={16} />}
                                </button>
                                <button
                                    type="button"
                                    onClick={() => setIsOpen(false)}
                                    className="p-1.5 text-gray-400 hover:text-white hover:bg-[#243044] rounded-lg"
                                    aria-label="Schließen"
                                >
                                    <X size={16} />
                                </button>
                            </div>
                        </div>

                        <div className="flex-1 overflow-y-auto p-4 space-y-3 custom-scrollbar bg-[#0E1117] min-h-0">
                            {messages.map((msg, index) => (
                                <div key={index} className={`flex ${msg.role === 'user' ? 'justify-end' : 'justify-start'}`}>
                                    <div
                                        className={`max-w-[88%] rounded-2xl px-3.5 py-3 ${msg.role === 'user'
                                            ? 'bg-[#5B9DFF] text-white rounded-tr-md'
                                            : 'bg-[#1A1F2A] text-gray-100 rounded-tl-md border border-[#2A3140]'
                                            }`}
                                    >
                                        {msg.role === 'model' ? (
                                            <div className="prose prose-invert prose-blop max-w-none text-sm leading-relaxed prose-p:my-2 prose-ul:my-2 prose-ol:my-2 prose-li:my-0.5 prose-pre:bg-[#12141A] prose-pre:border prose-pre:border-[#2A3140] prose-code:text-[#9EC5FF] [&_.katex-display]:my-3 [&_.katex-display]:overflow-x-auto">
                                                <ReactMarkdown
                                                    remarkPlugins={[remarkGfm, remarkMath]}
                                                    rehypePlugins={[[rehypeKatex, { throwOnError: false, strict: false }]]}
                                                >
                                                    {msg.content}
                                                </ReactMarkdown>
                                            </div>
                                        ) : (
                                            <p className="text-sm whitespace-pre-wrap">{msg.content}</p>
                                        )}
                                    </div>
                                </div>
                            ))}
                            {isLoading && (
                                <div className="flex justify-start">
                                    <div className="bg-[#1A1F2A] text-gray-400 rounded-2xl rounded-tl-md px-3.5 py-3 border border-[#2A3140] flex items-center gap-2">
                                        <Loader2 size={16} className="animate-spin text-[#5B9DFF]" />
                                        <span className="text-sm">Denkt nach…</span>
                                    </div>
                                </div>
                            )}
                            {pendingPatch && (
                                <div className="bg-[#152033] border border-[#5B9DFF]/35 rounded-xl p-3">
                                    <p className="text-xs text-gray-300 mb-2">Vorgeschlagene Dokumentänderung bereit.</p>
                                    <div className="flex gap-2">
                                        <button
                                            type="button"
                                            onClick={handleApplyPatch}
                                            disabled={isLoading}
                                            className="px-3 py-1.5 text-xs rounded-lg bg-[#5B9DFF] hover:bg-[#4A8AE6] text-white disabled:opacity-50"
                                        >
                                            Übernehmen
                                        </button>
                                        <button
                                            type="button"
                                            onClick={handleRejectPatch}
                                            disabled={isLoading}
                                            className="px-3 py-1.5 text-xs rounded-lg bg-[#243044] hover:bg-[#2E3A4F] text-gray-200 disabled:opacity-50"
                                        >
                                            Verwerfen
                                        </button>
                                    </div>
                                </div>
                            )}
                            <div ref={messagesEndRef} />
                        </div>

                        <div className="p-3.5 bg-[#1A1F2A] border-t border-[#2A3140] shrink-0 space-y-2">
                            {activeFile && typeof activeFile.content === 'string' ? (
                                <button
                                    type="button"
                                    onClick={() => setEditDocument((v) => !v)}
                                    className={`inline-flex items-center gap-2 text-[11px] px-2.5 py-1 rounded-full border transition-colors ${
                                        editDocument
                                            ? 'bg-[#5B9DFF]/15 text-[#9EC5FF] border-[#5B9DFF]/40'
                                            : 'bg-transparent text-gray-400 border-[#3A4558] hover:border-[#5B9DFF]/30'
                                    }`}
                                >
                                    <span className={`w-1.5 h-1.5 rounded-full ${editDocument ? 'bg-[#5B9DFF]' : 'bg-gray-500'}`} />
                                    Dokument ändern
                                </button>
                            ) : null}
                            <form
                                onSubmit={(e) => { e.preventDefault(); handleSend(); }}
                                className="flex items-end gap-2"
                            >
                                <textarea
                                    value={input}
                                    onChange={(e) => setInput(e.target.value)}
                                    onKeyDown={(e) => {
                                        if (e.key === 'Enter' && !e.shiftKey) {
                                            e.preventDefault();
                                            handleSend();
                                        }
                                    }}
                                    placeholder="Frage etwas zum Ordner…"
                                    className="flex-1 bg-[#0E1117] text-white border border-[#2A3140] rounded-xl px-3.5 py-3 text-sm focus:outline-none focus:ring-2 focus:ring-[#5B9DFF]/40 focus:border-[#5B9DFF]/50 resize-none max-h-32 min-h-[48px] custom-scrollbar"
                                    rows={1}
                                />
                                <button
                                    type="submit"
                                    disabled={!input.trim() || isLoading}
                                    className="bg-[#5B9DFF] hover:bg-[#4A8AE6] disabled:opacity-50 disabled:cursor-not-allowed text-white p-3 rounded-xl transition-colors shrink-0 flex items-center justify-center h-[48px] w-[48px]"
                                >
                                    <Send size={18} className={input.trim() ? 'translate-x-0.5' : ''} />
                                </button>
                            </form>
                            {lastUsageInfo ? (
                                <p className="text-center text-[10px] text-gray-500">{lastUsageInfo}</p>
                            ) : null}
                        </div>
                    </motion.div>
                )}
            </AnimatePresence>

            {!isOpen ? (
                <motion.div
                    initial={{ opacity: 0, y: 50 }}
                    animate={{ opacity: 1, y: 0 }}
                    className={OVERLAY_FLOATING_CHAT}
                >
                    <motion.button
                        whileHover={{ scale: 1.05 }}
                        whileTap={{ scale: 0.95 }}
                        onClick={() => setIsOpen(true)}
                        className="bg-[#5B9DFF] hover:bg-[#4A8AE6] text-white rounded-full p-4 shadow-xl border border-[#5B9DFF]/30 flex items-center justify-center"
                        aria-label="Chat öffnen"
                    >
                        <MessageCircle size={24} />
                    </motion.button>
                </motion.div>
            ) : null}
        </>
    );
}
