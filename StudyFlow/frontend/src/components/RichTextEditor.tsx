import React, { useEffect, useState } from 'react';
import { useEditor, EditorContent } from '@tiptap/react';
import StarterKit from '@tiptap/starter-kit';
import Heading from '@tiptap/extension-heading';
import Placeholder from '@tiptap/extension-placeholder';
import { Bold, Italic, List, ListOrdered, Heading1, Heading2, Quote, Save, X, Loader2 } from 'lucide-react';

interface RichTextEditorProps {
    initialContent: string;
    title: string;
    onSave: (content: string) => Promise<void>;
    onClose: () => void;
}

const MenuBar = ({ editor, onSave, onClose, isSaving, title }: { editor: any, onSave: () => void, onClose: () => void, isSaving: boolean, title: string }) => {
    if (!editor) {
        return null;
    }

    return (
        <div className="flex items-center justify-between p-4 border-b border-[#333] bg-[#1e1e1e] sticky top-0 z-10 w-full">
            <div className="flex items-center gap-4">
                <button
                    onClick={onClose}
                    className="p-2 text-gray-400 hover:text-white hover:bg-[#333] rounded-xl transition-colors"
                    title="Schließen"
                >
                    <X size={20} />
                </button>
                <div className="h-6 w-px bg-[#333] mx-2"></div>

                <div className="flex bg-[#252526] rounded-lg p-1 border border-[#333]">
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
                </div>
            </div>

            <div className="flex items-center gap-4">
                <div className="hidden md:block text-gray-400 text-sm max-w-[200px] truncate">
                    {title}
                </div>
                <button
                    onClick={onSave}
                    disabled={isSaving}
                    className="flex items-center gap-2 bg-[#5E5CE6] hover:bg-[#7D7AFF] text-white px-5 py-2 rounded-xl text-sm font-semibold transition-colors disabled:opacity-50"
                >
                    {isSaving ? <Loader2 size={16} className="animate-spin" /> : <Save size={16} />}
                    <span>Speichern</span>
                </button>
            </div>
        </div>
    );
};

export default function RichTextEditor({ initialContent, title, onSave, onClose }: RichTextEditorProps) {
    const [isSaving, setIsSaving] = useState(false);

    // Provide default HTML if initialContent is markdown
    // We are trusting Tiptap's StarterKit to parse basic html/markdown 
    // Usually Tiptap consumes HTML. Since our content might be Markdown string,
    // we use a trick: If it's markdown, Tiptap might struggle without a parser.
    // For now, assume we just pass it in. If it's raw markdown, it might just render as text.
    // To support Markdown properly in Tiptap, a markdown extension is needed,
    // but typically users will edit visually and save as HTML.

    // We'll convert newlines to <br> or <p> for basic layout if it lacks HTML tags
    const formattedContent = initialContent.includes('<') ? initialContent : initialContent.split('\n\n').map(p => `<p>${p}</p>`).join('');

    const editor = useEditor({
        extensions: [
            StarterKit,
            Heading.configure({
                levels: [1, 2, 3],
            }),
            Placeholder.configure({
                placeholder: 'Schreibe hier deine Zusammenfassung...',
                emptyEditorClass: 'is-editor-empty',
            }),
        ],
        content: formattedContent,
        editorProps: {
            attributes: {
                class: 'prose-blop focus:outline-none min-h-[500px] max-w-4xl mx-auto py-10 px-6',
            },
        },
    });

    const handleSave = async () => {
        if (!editor) return;
        setIsSaving(true);
        // Save as Markdown or HTML. Tiptap outputs HTML by default.
        // For our `prose-blop` Markdown renderer to still work if needed (or if we just render HTML directly), 
        // we'll get HTML. 
        const htmlContent = editor.getHTML();
        await onSave(htmlContent);
        setIsSaving(false);
    };

    return (
        <div className="fixed inset-0 z-[100] bg-[#1e1e1e] flex flex-col w-screen h-screen overflow-hidden animate-in fade-in slide-in-from-bottom-4 duration-300">
            <MenuBar editor={editor} onSave={handleSave} onClose={onClose} isSaving={isSaving} title={title} />
            <div className="flex-1 overflow-y-auto bg-[#1e1e1e]">
                <EditorContent editor={editor} />
            </div>

            <style jsx global>{`
                .ProseMirror p.is-editor-empty:first-child::before {
                    color: #555;
                    content: attr(data-placeholder);
                    float: left;
                    height: 0;
                    pointer-events: none;
                }
            `}</style>
        </div>
    );
}
