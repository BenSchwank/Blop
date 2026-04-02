import React, { useState, useRef, useEffect } from 'react';
import { motion, AnimatePresence } from 'framer-motion';
import { MessageCircle, X, Send, Loader2, Maximize2, Minimize2 } from 'lucide-react';
import ReactMarkdown from 'react-markdown';
import remarkGfm from 'remark-gfm';
import remarkMath from 'remark-math';
import rehypeKatex from 'rehype-katex';
import 'katex/dist/katex.min.css';
import { OVERLAY_FLOATING_CHAT } from '@/constants/overlayLayout';

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

export default function FloatingChat({ folderId, username, modelPreference, activeFile, onUpdateActiveFile, onApplyPatch, onUndoLastPatch, canUndo = false }: FloatingChatProps) {
    const [isOpen, setIsOpen] = useState(false);
    const [isExpanded, setIsExpanded] = useState(false);
    const [messages, setMessages] = useState<Message[]>([
        { role: 'model', content: 'Hallo! Ich bin dein Blop KI-Assistent. Hast du Fragen zu den Dokumenten in diesem Ordner?' }
    ]);
    const [input, setInput] = useState('');
    const [isLoading, setIsLoading] = useState(false);
    const [pendingPatch, setPendingPatch] = useState<DocumentPatch | null>(null);
    const [lastUsageInfo, setLastUsageInfo] = useState<string>('');
    const messagesEndRef = useRef<HTMLDivElement>(null);

    const scrollToBottom = () => {
        messagesEndRef.current?.scrollIntoView({ behavior: 'smooth' });
    };

    useEffect(() => {
        if (isOpen) scrollToBottom();
    }, [messages, isOpen]);

    const handleSend = async () => {
        if (!input.trim() || isLoading) return;

        const userMsg = input.trim();
        setInput('');
        const newMessages: Message[] = [...messages, { role: 'user', content: userMsg }];
        setMessages(newMessages);
        setIsLoading(true);

        try {
            const hasTextDocument = activeFile && typeof activeFile.content === 'string';
            const endpoint = hasTextDocument ? '/api/ai/document-chat' : '/api/ai/chat';
            const res = await fetch(endpoint, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({
                    username,
                    folder_id: folderId,
                    file_id: activeFile?.id,
                    message: userMsg,
                    history: messages.filter(m => m.role !== 'model' || m.content !== 'Hallo! Ich bin dein Blop KI-Assistent. Hast du Fragen zu den Dokumenten in diesem Ordner?'), // Don't send the default greeting
                    model_preference: modelPreference || null,
                    active_file: activeFile || null
                }),
            });

            let errData = null;
            if (!res.ok) {
                try {
                    errData = await res.json();
                } catch(e) {}
                
                if (res.status === 402 || (errData && errData.detail && typeof errData.detail === 'string' && errData.detail.includes("Nicht genügend Tokens"))) {
                    throw new Error(errData?.detail || "Nicht genügend Tokens. Bitte lade dein Abo in den Einstellungen auf!");
                }
                if (errData && errData.detail === "NO_API_KEY_FOUND") {
                    throw new Error("Serverfehler: Der AI Key wurde vom Administrator nicht konfiguriert.");
                }
                throw new Error(typeof errData?.detail === 'string' ? errData.detail : 'Netzwerkfehler');
            }

            const data = await res.json();
            if (typeof data?.tokens_charged === 'number') {
                const modelLabel = data?.used_model || 'auto';
                setLastUsageInfo(`Modell: ${modelLabel} · -${data.tokens_charged} Tokens`);
                if (typeof window !== 'undefined') {
                    window.dispatchEvent(new Event('blop_tokens_updated'));
                }
            }
            if (data?.patch && isPatchResponse(data.patch)) {
                setPendingPatch(data.patch);
                setMessages([
                    ...newMessages,
                    {
                        role: 'model',
                        content: `Vorschlag: **${data.patch.patch_description || 'Dokumentänderung'}**\n\n${data.patch.new_text.substring(0, 1400)}${data.patch.new_text.length > 1400 ? '\n\n…(gekürzt)' : ''}`,
                    },
                ]);
                return;
            }
            const replyText = data.reply;

            // Try to parse JSON to see if it's an action rather than a chat message
            try {
                // The AI might wrap the JSON in markdown code blocks like ```json ... ```
                let jsonStr = replyText.trim();
                if (jsonStr.startsWith('```json')) jsonStr = jsonStr.substring(7);
                if (jsonStr.startsWith('```')) jsonStr = jsonStr.substring(3);
                if (jsonStr.endsWith('```')) jsonStr = jsonStr.substring(0, jsonStr.length - 3);
                jsonStr = jsonStr.trim();

                const jsonObj = JSON.parse(jsonStr);

                if (jsonObj && jsonObj.blop_action === "update_file" && onUpdateActiveFile) {
                    await onUpdateActiveFile(jsonObj.new_content);
                    setMessages([...newMessages, { role: 'model', content: `Ich habe das Dokument "${activeFile?.name}" für dich aktualisiert! ✨` }]);
                    return;
                }
            } catch (jsonError) {
                // Not JSON, or not our specific action - just treat as a normal text reply.
            }

            setMessages([...newMessages, { role: 'model', content: replyText }]);
        } catch (error: any) {
            console.error(error);
            setMessages([...newMessages, { role: 'model', content: error.message || 'Es gab einen Fehler bei der Verbindung zur KI. Bitte versuche es später noch einmal.' }]);
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
        <motion.div
            drag
            dragConstraints={{ left: -1000, right: 0, top: -800, bottom: 0 }}
            dragElastic={0.1}
            dragMomentum={false}
            initial={{ opacity: 0, y: 50 }}
            animate={{ opacity: 1, y: 0 }}
            className={OVERLAY_FLOATING_CHAT}
            style={{ touchAction: 'none' }} // Prevents scrolling while dragging on touch devices
        >
            <AnimatePresence>
                {isOpen && (
                    <motion.div
                        initial={{ opacity: 0, scale: 0.8, y: 20 }}
                        animate={{ opacity: 1, scale: 1, y: 0 }}
                        exit={{ opacity: 0, scale: 0.8, y: 20 }}
                        transition={{ type: 'spring', damping: 25, stiffness: 300 }}
                        className={`bg-[#1e1e1e] border border-[#333] shadow-2xl rounded-2xl mb-4 overflow-hidden flex flex-col ${isExpanded ? 'w-[80vw] h-[80vh] max-w-4xl' : 'w-[350px] sm:w-[400px] h-[500px]'}`}
                        onPointerDownCapture={(e) => e.stopPropagation()} // Prevent drag when interacting with chat
                    >
                        {/* Header */}
                        <div className="bg-[#252526] p-4 border-b border-[#333] flex items-center justify-between shrink-0 cursor-grab active:cursor-grabbing">
                            <div className="flex items-center gap-2">
                                <div className="w-8 h-8 rounded-full bg-blue-500/20 text-blue-400 flex items-center justify-center">
                                    <MessageCircle size={18} />
                                </div>
                                <h3 className="text-white font-medium">Blop KI-Assistent</h3>
                            </div>
                            <div className="flex items-center gap-1">
                                <button
                                    onClick={() => setIsExpanded(!isExpanded)}
                                    className="p-1.5 text-gray-400 hover:text-white hover:bg-[#333] rounded-lg transition-colors"
                                >
                                    {isExpanded ? <Minimize2 size={18} /> : <Maximize2 size={18} />}
                                </button>
                                <button
                                    onClick={() => setIsOpen(false)}
                                    className="p-1.5 text-gray-400 hover:text-white hover:bg-[#333] rounded-lg transition-colors"
                                >
                                    <X size={18} />
                                </button>
                            </div>
                            <div className="flex items-center gap-2">
                                <button
                                    type="button"
                                    disabled={!canUndo || isLoading}
                                    onClick={async () => {
                                        if (!onUndoLastPatch) return;
                                        setIsLoading(true);
                                        try {
                                            await onUndoLastPatch();
                                            setMessages((prev) => [...prev, { role: 'model', content: 'Letzte Änderung rückgängig gemacht. ↩️' }]);
                                        } catch (e: any) {
                                            setMessages((prev) => [...prev, { role: 'model', content: e?.message || 'Rückgängig fehlgeschlagen.' }]);
                                        } finally {
                                            setIsLoading(false);
                                        }
                                    }}
                                    className="px-2 py-1 text-xs rounded-lg border border-[#3d3d3d] text-gray-300 hover:text-white hover:bg-[#333] disabled:opacity-50 disabled:cursor-not-allowed"
                                >
                                    Rückgängig
                                </button>
                            </div>
                        </div>

                        {/* Chat History */}
                        <div className="flex-1 overflow-y-auto p-4 space-y-4 custom-scrollbar bg-[#1a1a1b]">
                            {messages.map((msg, index) => (
                                <div key={index} className={`flex ${msg.role === 'user' ? 'justify-end' : 'justify-start'}`}>
                                    <div
                                        className={`max-w-[85%] rounded-2xl p-4 ${msg.role === 'user'
                                            ? 'bg-blue-600 text-white rounded-tr-sm'
                                            : 'bg-[#2d2d2d] text-gray-200 rounded-tl-sm border border-[#3d3d3d]'
                                            }`}
                                    >
                                        {msg.role === 'model' ? (
                                            <div className="prose prose-invert max-w-none text-sm leading-relaxed">
                                                <ReactMarkdown
                                                remarkPlugins={[remarkGfm, remarkMath]}
                                                rehypePlugins={[[rehypeKatex, { throwOnError: false, strict: false }]]}
                                            >
                                                {msg.content}
                                            </ReactMarkdown>
                                            </div>
                                        ) : (
                                            <p className="text-sm">{msg.content}</p>
                                        )}
                                    </div>
                                </div>
                            ))}
                            {isLoading && (
                                <div className="flex justify-start">
                                    <div className="bg-[#2d2d2d] text-gray-400 rounded-2xl rounded-tl-sm p-4 border border-[#3d3d3d] flex items-center gap-2">
                                        <Loader2 size={16} className="animate-spin" />
                                        <span className="text-sm">Denkt nach...</span>
                                    </div>
                                </div>
                            )}
                            {pendingPatch && (
                                <div className="bg-[#202032] border border-[#3f3f60] rounded-xl p-3">
                                    <p className="text-xs text-gray-300 mb-2">Vorgeschlagene Änderung bereit.</p>
                                    <div className="flex gap-2">
                                        <button
                                            type="button"
                                            onClick={handleApplyPatch}
                                            disabled={isLoading}
                                            className="px-3 py-1.5 text-xs rounded-lg bg-green-600 hover:bg-green-500 text-white disabled:opacity-50"
                                        >
                                            Übernehmen
                                        </button>
                                        <button
                                            type="button"
                                            onClick={handleRejectPatch}
                                            disabled={isLoading}
                                            className="px-3 py-1.5 text-xs rounded-lg bg-[#333] hover:bg-[#444] text-gray-200 disabled:opacity-50"
                                        >
                                            Verwerfen
                                        </button>
                                    </div>
                                </div>
                            )}
                            <div ref={messagesEndRef} />
                        </div>

                        {/* Input Area */}
                        <div className="p-4 bg-[#252526] border-t border-[#333] shrink-0">
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
                                    placeholder="Frage etwas zum Ordner..."
                                    className="flex-1 bg-[#1a1a1b] text-white border border-[#333] rounded-xl px-4 py-3 text-sm focus:outline-none focus:ring-2 focus:ring-blue-500/50 resize-none max-h-32 min-h-[48px] custom-scrollbar"
                                    rows={1}
                                />
                                <button
                                    type="submit"
                                    disabled={!input.trim() || isLoading}
                                    className="bg-blue-600 hover:bg-blue-500 disabled:opacity-50 disabled:cursor-not-allowed text-white p-3 rounded-xl transition-colors shrink-0 flex items-center justify-center h-[48px] w-[48px]"
                                >
                                    <Send size={18} className={input.trim() ? 'translate-x-0.5' : ''} />
                                </button>
                            </form>
                            <div className="text-center mt-2">
                                <span className="text-[10px] text-gray-500">
                                    {lastUsageInfo || 'Du kannst die Chat-Bubble auf dem Bildschirm verschieben.'}
                                </span>
                            </div>
                        </div>
                    </motion.div>
                )}
            </AnimatePresence>

            <motion.button
                whileHover={{ scale: 1.05 }}
                whileTap={{ scale: 0.95 }}
                onClick={() => setIsOpen(!isOpen)}
                className="bg-blue-600 hover:bg-blue-500 text-white rounded-full p-4 shadow-xl border border-blue-400/20 flex items-center justify-center"
            >
                {isOpen ? <X size={24} /> : <MessageCircle size={24} />}
            </motion.button>
        </motion.div>
    );
}
