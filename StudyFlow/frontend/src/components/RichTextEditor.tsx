import React, { useEffect, useState, useRef, useCallback } from 'react';
import { createPortal } from 'react-dom';
import ReactMarkdown from 'react-markdown';
import remarkGfm from 'remark-gfm';
import remarkMath from 'remark-math';
import rehypeKatex from 'rehype-katex';
import 'katex/dist/katex.min.css';
import { useEditor, EditorContent } from '@tiptap/react';
import StarterKit from '@tiptap/starter-kit';
import Heading from '@tiptap/extension-heading';
import Placeholder from '@tiptap/extension-placeholder';
import Image from '@tiptap/extension-image';
import { Bold, Italic, List, ListOrdered, Heading1, Heading2, Heading3, Quote, Code, ImageIcon, Save, X, Loader2, Check } from 'lucide-react';
import { marked } from 'marked';
import { htmlToMarkdownForPdfExport } from '@/lib/htmlToMarkdownExport';

interface RichTextEditorProps {
    initialContent: string;
    title: string;
    onSave: (content: string) => Promise<void>;
    onClose: () => void;
    /** When set (e.g. YouTube import), show embedded player above the editor. */
    youtubeVideoId?: string | null;
}

const MenuBar = ({ editor, onSave, onClose, onExportPdf, isSaving, title, saveStatus }: { editor: any, onSave: () => void, onClose: () => void, onExportPdf: () => void, isSaving: boolean, title: string, saveStatus: 'idle' | 'saving' | 'saved' }) => {
    if (!editor) {
        return null;
    }

    const addImage = () => {
        const url = window.prompt('URL des Bildes eingeben (z.B. von Imgur oder einem anderen Host):');
        if (url) {
            editor.chain().focus().setImage({ src: url }).run();
        }
    };

    return (
        <div className="flex items-center justify-between p-4 border-b border-[#333] bg-[#1e1e1e] sticky top-0 z-50 w-full overflow-x-auto custom-scrollbar">
            <div className="flex items-center gap-4 min-w-max">
                <button
                    onClick={onClose}
                    className="p-2 text-gray-400 hover:text-white hover:bg-[#333] rounded-xl transition-colors shrink-0"
                    title="Schließen"
                >
                    <X size={20} />
                </button>
                <div className="h-6 w-px bg-[#333] mx-2 shrink-0"></div>

                <div className="flex bg-[#252526] rounded-lg p-1 border border-[#333] shrink-0">
                    <button
                        onClick={() => editor.chain().focus().toggleHeading({ level: 1 }).run()}
                        className={`p-2 rounded-md transition-colors ${editor.isActive('heading', { level: 1 }) ? 'bg-[#5E5CE6] text-white' : 'text-gray-400 hover:text-white hover:bg-[#333]'}`}
                        title="Überschrift 1"
                    >
                        <Heading1 size={18} />
                    </button>
                    <button
                        onClick={() => editor.chain().focus().toggleHeading({ level: 2 }).run()}
                        className={`p-2 rounded-md transition-colors ${editor.isActive('heading', { level: 2 }) ? 'bg-[#5E5CE6] text-white' : 'text-gray-400 hover:text-white hover:bg-[#333]'}`}
                        title="Überschrift 2"
                    >
                        <Heading2 size={18} />
                    </button>
                    <button
                        onClick={() => editor.chain().focus().toggleHeading({ level: 3 }).run()}
                        className={`p-2 rounded-md transition-colors ${editor.isActive('heading', { level: 3 }) ? 'bg-[#5E5CE6] text-white' : 'text-gray-400 hover:text-white hover:bg-[#333]'}`}
                        title="Überschrift 3"
                    >
                        <Heading3 size={18} />
                    </button>
                    <div className="w-px bg-[#333] mx-1 my-1"></div>
                    <button
                        onClick={() => editor.chain().focus().toggleBold().run()}
                        className={`p-2 rounded-md transition-colors ${editor.isActive('bold') ? 'bg-[#5E5CE6] text-white' : 'text-gray-400 hover:text-white hover:bg-[#333]'}`}
                        title="Fett"
                    >
                        <Bold size={18} />
                    </button>
                    <button
                        onClick={() => editor.chain().focus().toggleItalic().run()}
                        className={`p-2 rounded-md transition-colors ${editor.isActive('italic') ? 'bg-[#5E5CE6] text-white' : 'text-gray-400 hover:text-white hover:bg-[#333]'}`}
                        title="Kursiv"
                    >
                        <Italic size={18} />
                    </button>
                    <button
                        onClick={() => editor.chain().focus().toggleCodeBlock().run()}
                        className={`p-2 rounded-md transition-colors ${editor.isActive('codeBlock') ? 'bg-[#5E5CE6] text-white' : 'text-gray-400 hover:text-white hover:bg-[#333]'}`}
                        title="Code Block"
                    >
                        <Code size={18} />
                    </button>
                    <div className="w-px bg-[#333] mx-1 my-1"></div>
                    <button
                        onClick={() => editor.chain().focus().toggleBulletList().run()}
                        className={`p-2 rounded-md transition-colors ${editor.isActive('bulletList') ? 'bg-[#5E5CE6] text-white' : 'text-gray-400 hover:text-white hover:bg-[#333]'}`}
                        title="Aufzählung"
                    >
                        <List size={18} />
                    </button>
                    <button
                        onClick={() => editor.chain().focus().toggleOrderedList().run()}
                        className={`p-2 rounded-md transition-colors ${editor.isActive('orderedList') ? 'bg-[#5E5CE6] text-white' : 'text-gray-400 hover:text-white hover:bg-[#333]'}`}
                        title="Nummerierte Liste"
                    >
                        <ListOrdered size={18} />
                    </button>
                    <button
                        onClick={() => editor.chain().focus().toggleBlockquote().run()}
                        className={`p-2 rounded-md transition-colors ${editor.isActive('blockquote') ? 'bg-[#5E5CE6] text-white' : 'text-gray-400 hover:text-white hover:bg-[#333]'}`}
                        title="Zitat"
                    >
                        <Quote size={18} />
                    </button>
                    <div className="w-px bg-[#333] mx-1 my-1"></div>
                    <button
                        onClick={addImage}
                        className="p-2 rounded-md transition-colors text-gray-400 hover:text-white hover:bg-[#333]"
                        title="Bild einfügen"
                    >
                        <ImageIcon size={18} />
                    </button>
                </div>
            </div>

            <div className="flex items-center gap-4 shrink-0 pl-4 no-print">
                <div className="hidden md:block text-gray-400 text-sm max-w-[200px] truncate" title={title}>
                    {title}
                </div>

                <button
                    type="button"
                    onClick={onExportPdf}
                    className="flex items-center gap-2 bg-[#252526] hover:bg-[#333] border border-[#444] text-white px-4 py-2 rounded-xl text-sm font-semibold transition-colors"
                    title="Als PDF exportieren (Druckdialog – „Als PDF speichern“)"
                >
                    <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><polyline points="6 9 6 2 18 2 18 9"></polyline><path d="M6 18H4a2 2 0 0 1-2-2v-5a2 2 0 0 1 2-2h16a2 2 0 0 1 2 2v5a2 2 0 0 1-2 2h-2"></path><rect x="6" y="14" width="12" height="8"></rect></svg>
                    <span className="hidden sm:inline">PDF Export</span>
                </button>

                <button
                    onClick={onSave}
                    disabled={isSaving}
                    className="flex items-center gap-2 bg-[#5E5CE6] hover:bg-[#7D7AFF] text-white px-5 py-2 rounded-xl text-sm font-semibold transition-colors disabled:opacity-50"
                >
                    {isSaving || saveStatus === 'saving' ? (
                        <>
                            <Loader2 size={16} className="animate-spin" />
                            <span className="hidden sm:inline">Speichert...</span>
                        </>
                    ) : saveStatus === 'saved' ? (
                        <>
                            <Check size={16} />
                            <span className="hidden sm:inline">Gespeichert</span>
                        </>
                    ) : (
                        <>
                            <Save size={16} />
                            <span className="hidden sm:inline">Speichern</span>
                        </>
                    )}
                </button>
            </div>
        </div>
    );
};

export default function RichTextEditor({ initialContent, title, onSave, onClose, youtubeVideoId }: RichTextEditorProps) {
    const [isSaving, setIsSaving] = useState(false);
    const [saveStatus, setSaveStatus] = useState<'idle' | 'saving' | 'saved'>('idle');
    const [printMarkdown, setPrintMarkdown] = useState<string | null>(null);
    const saveTimeoutRef = useRef<NodeJS.Timeout | null>(null);

    // If initialContent is raw markdown (e.g. from the AI), Tiptap requires HTML to style it properly.
    // If it already looks like HTML (from a previous edit), we can just use it directly. 
    // Tiptap always wraps text in basic block elements like <p>, <h1>, <ul> etc.
    const isHtml = /<p>|<h[1-6]>|<ul>|<ol>|<blockquote>|<img/i.test(initialContent);
    const parsedHtml = isHtml ? initialContent : marked.parse(initialContent, { async: false }) as string;

    // Fallback to ensuring simple newlines aren't completely lost if marked isn't doing what we expect
    const finalHtml = parsedHtml || '';

    const editor = useEditor({
        extensions: [
            StarterKit,
            Image.configure({
                HTMLAttributes: {
                    class: 'rounded-xl shadow-lg max-w-full my-6', // Added nice styling classes for images in the editor
                },
            }),
            Heading.configure({
                levels: [1, 2, 3],
            }),
            Placeholder.configure({
                placeholder: 'Schreibe hier...',
                emptyEditorClass: 'is-editor-empty',
            }),
        ],
        content: finalHtml,
        editorProps: {
            attributes: {
                class: 'prose-blop focus:outline-none min-h-[500px] max-w-4xl mx-auto py-10 px-6 prose-img:rounded-xl prose-img:shadow-lg prose-pre:bg-[#151525] prose-pre:border prose-pre:border-[#2A2A40]',
            },
        },
        onUpdate: ({ editor }) => {
            // Auto-save logic
            const currentHtml = editor.getHTML();
            if (saveTimeoutRef.current) {
                clearTimeout(saveTimeoutRef.current);
            }
            
            setSaveStatus('idle'); // Indicate that there are unsaved changes
            
            saveTimeoutRef.current = setTimeout(async () => {
                setSaveStatus('saving');
                try {
                    await onSave(currentHtml);
                    setSaveStatus('saved');
                    // Reset back to idle after a while so the button looks normal
                    setTimeout(() => { if (setSaveStatus) setSaveStatus('idle'); }, 2000);
                } catch (e) {
                    setSaveStatus('idle');
                }
            }, 2000); // 2 second debounce
        }
    });

    const handleSave = async () => {
        if (!editor) return;
        setIsSaving(true);
        setSaveStatus('saving');
        
        if (saveTimeoutRef.current) {
            clearTimeout(saveTimeoutRef.current);
        }
        
        const htmlContent = editor.getHTML();
        await onSave(htmlContent);
        
        setIsSaving(false);
        setSaveStatus('saved');
        setTimeout(() => setSaveStatus('idle'), 2000);
    };

    const runPdfExport = useCallback(() => {
        if (!editor) return;
        const md = htmlToMarkdownForPdfExport(editor.getHTML());
        setPrintMarkdown(md);
    }, [editor]);

    /* Same pipeline as read-only viewer: Markdown → KaTeX → system print / „Als PDF speichern“. */
    useEffect(() => {
        if (printMarkdown === null) return;

        document.body.classList.add('blop-printing-markdown');

        let fallbackTimer: ReturnType<typeof setTimeout> | undefined;
        let printDelay: ReturnType<typeof setTimeout> | undefined;
        let raf1 = 0;
        let raf2 = 0;

        const finish = () => {
            if (fallbackTimer !== undefined) clearTimeout(fallbackTimer);
            fallbackTimer = undefined;
            window.removeEventListener('afterprint', finish);
            document.body.classList.remove('blop-printing-markdown');
            setPrintMarkdown(null);
        };

        raf1 = requestAnimationFrame(() => {
            raf2 = requestAnimationFrame(() => {
                printDelay = setTimeout(() => {
                    window.addEventListener('afterprint', finish, { once: true });
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
            window.removeEventListener('afterprint', finish);
            document.body.classList.remove('blop-printing-markdown');
            setPrintMarkdown(null);
        };
    }, [printMarkdown]);

    // Cleanup timeout on unmount
    useEffect(() => {
        return () => {
            if (saveTimeoutRef.current) {
                clearTimeout(saveTimeoutRef.current);
            }
            document.body.classList.remove('blop-printing-markdown');
        };
    }, []);

    const printPortal =
        printMarkdown !== null &&
        typeof document !== 'undefined' &&
        createPortal(
            <div
                id="blop-markdown-print-mount"
                className="fixed inset-0 z-[200000] overflow-auto bg-white text-gray-900 print:static print:overflow-visible"
            >
                <div className="no-print sticky top-0 z-10 border-b border-gray-200 bg-white/95 px-4 py-3 text-sm text-gray-600 backdrop-blur">
                    Druckdialog öffnet sich gleich. Wähle <strong>Als PDF speichern</strong> als Ziel – gleiche Darstellung wie in der Leseansicht (inkl. Formeln).
                    <span className="mt-2 block text-xs text-gray-500">
                        In Chrome: unter <strong>Weitere Einstellungen</strong> die Option <strong>Kopf- und Fußzeilen</strong> deaktivieren, damit Datum, Titel und URL nicht im PDF erscheinen.
                    </span>
                </div>
                <div className="blop-markdown-export-prose mx-auto max-w-3xl px-6 py-8 prose prose-slate max-w-none [&_.katex-display]:max-w-full">
                    <h1 className="not-prose text-2xl font-bold text-gray-900 mb-6 print:mb-4">{title}</h1>
                    <ReactMarkdown
                        remarkPlugins={[remarkGfm, remarkMath]}
                        rehypePlugins={[[rehypeKatex, { throwOnError: false, strict: false }]]}
                    >
                        {printMarkdown}
                    </ReactMarkdown>
                </div>
            </div>,
            document.body
        );

    return (
        <div className="print-friendly-viewer fixed inset-0 z-[100] bg-[#1e1e1e] flex flex-col w-screen h-screen overflow-hidden animate-in fade-in slide-in-from-bottom-4 duration-300">
            <MenuBar editor={editor} onSave={handleSave} onClose={onClose} onExportPdf={runPdfExport} isSaving={isSaving} title={title} saveStatus={saveStatus} />
            {youtubeVideoId ? (
                <div className="shrink-0 border-b border-[#333] bg-[#14141f] px-4 py-4">
                    <div className="max-w-4xl mx-auto aspect-video max-h-[min(40vh,360px)] rounded-xl overflow-hidden border border-[#2A2A40] bg-black shadow-lg">
                        <iframe
                            title="YouTube"
                            className="w-full h-full"
                            src={`https://www.youtube-nocookie.com/embed/${youtubeVideoId}`}
                            allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture; web-share"
                            allowFullScreen
                        />
                    </div>
                </div>
            ) : null}
            <div className="flex-1 overflow-y-auto bg-[#1e1e1e] min-h-0">
                <EditorContent editor={editor} />
            </div>
            {printPortal}

            <style jsx global>{`
                .ProseMirror p.is-editor-empty:first-child::before {
                    color: #555;
                    content: attr(data-placeholder);
                    float: left;
                    height: 0;
                    pointer-events: none;
                }
                .ProseMirror pre {
                    background: #1C1C33;
                    color: #fff;
                    font-family: 'JetBrains Mono', 'Fira Code', monospace;
                    padding: 1rem;
                    border-radius: 0.75rem;
                    margin: 1.5rem 0;
                    overflow-x: auto;
                    border: 1px solid #3B3B55;
                }
                .ProseMirror pre code {
                    color: inherit;
                    padding: 0;
                    background: none;
                    font-size: 0.875rem;
                }
                .ProseMirror img.ProseMirror-selectednode {
                    outline: 2px solid #5E5CE6;
                    outline-offset: 2px;
                }
            `}</style>
        </div>
    );
}
