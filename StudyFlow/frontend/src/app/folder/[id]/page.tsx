"use client";

import React, { useState, useEffect } from "react";
import { useRouter, useParams } from "next/navigation";
import { ArrowLeft, FileText, MoreVertical, Plus, Loader2, Youtube, Upload, BrainCircuit, X, HelpCircle, Layers, FileOutput, Calendar, Clock, BookOpen } from "lucide-react";
import ReactMarkdown from 'react-markdown';
import remarkGfm from 'remark-gfm';
import RichTextEditor from "@/components/RichTextEditor";

interface FileData {
    id: string;
    name: string;
    type: string;
    created_at: string;
    content?: any;
}

export default function FolderPage() {
    const router = useRouter();
    const params = useParams();
    const folderId = params?.id as string;
    const [files, setFiles] = useState<FileData[]>([]);
    const [loading, setLoading] = useState(true);

    // Upload / Action States
    const [isUploadOpen, setIsUploadOpen] = useState(false);
    const [uploadType, setUploadType] = useState<'pdf' | 'youtube'>('pdf');
    const [fileToUpload, setFileToUpload] = useState<File | null>(null);
    const [youtubeUrl, setYoutubeUrl] = useState("");
    const [isProcessing, setIsProcessing] = useState(false);

    // AI Generation States
    const [isGenerating, setIsGenerating] = useState<string | null>(null); // 'plan', 'quiz', 'cards', 'summary'

    // Plan Config Modal State
    const [isPlanConfigOpen, setIsPlanConfigOpen] = useState(false);
    const [planDays, setPlanDays] = useState(7);
    const [planHoursPerDay, setPlanHoursPerDay] = useState(2);
    const [planEndDate, setPlanEndDate] = useState('');
    const [planUseDate, setPlanUseDate] = useState(false);

    // Summary Config Modal State
    const [isSummaryConfigOpen, setIsSummaryConfigOpen] = useState(false);
    const [summaryDetailLevel, setSummaryDetailLevel] = useState('Normal');

    // Viewer State
    const [selectedFile, setSelectedFile] = useState<FileData | null>(null);
    const [expandedDay, setExpandedDay] = useState<number | null>(0);

    const API_BASE = '/api';

    const fetchFiles = async () => {
        try {
            const username = localStorage.getItem("username");
            if (!username) return;

            const res = await fetch(`${API_BASE}/files/${folderId}?username=${username}`);
            if (res.ok) {
                const data = await res.json();
                setFiles(data);
            }
        } catch (error) {
            console.error("Failed to fetch files:", error);
        } finally {
            setLoading(false);
        }
    };

    useEffect(() => {
        fetchFiles();
    }, [folderId]);

    const handleFileUpload = async (e: React.FormEvent) => {
        e.preventDefault();
        if (!fileToUpload) return;

        setIsProcessing(true);
        try {
            const username = localStorage.getItem("username");
            const formData = new FormData();
            formData.append("file", fileToUpload);

            const res = await fetch(`${API_BASE}/files/upload?username=${username}&folder_id=${folderId}`, {
                method: "POST",
                body: formData
            });

            if (res.ok) {
                setFileToUpload(null);
                setIsUploadOpen(false);
                fetchFiles();
            } else {
                alert("Fehler beim Upload (Server Error)");
            }
        } catch (error) {
            console.error(error);
            alert("Fehler beim Upload.");
        } finally {
            setIsProcessing(false);
        }
    };

    const handleYoutubeImport = async (e: React.FormEvent) => {
        e.preventDefault();
        if (!youtubeUrl) return;

        setIsProcessing(true);
        try {
            const username = localStorage.getItem("username");
            const res = await fetch(`${API_BASE}/files/youtube?username=${username}&folder_id=${folderId}`, {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify({ url: youtubeUrl })
            });

            if (res.ok) {
                setYoutubeUrl("");
                setIsUploadOpen(false);
                fetchFiles();
            } else {
                alert("Fehler: Ungültige URL oder kein Transkript verfügbar.");
            }
        } catch (error) {
            console.error(error);
        } finally {
            setIsProcessing(false);
        }
    };

    const handleAIAction = async (endpoint: string, stateSetter: React.Dispatch<React.SetStateAction<string | null>>, successMessage: string) => {
        try {
            const username = localStorage.getItem("username");
            const res = await fetch(`${API_BASE}/ai/${endpoint}`, {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify({ username, folder_id: folderId, duration_days: 7 })
            });

            if (res.ok) {
                const data = await res.json();

                // Open the result immediately from the response (no race condition)
                if (endpoint === 'plan' && data.plan) {
                    setSelectedFile({ id: 'plan_main', name: 'Aktueller Lernplan', type: 'plan', created_at: new Date().toISOString().split('T')[0], content: data.plan });
                } else if (endpoint === 'quiz' && data.quiz) {
                    setSelectedFile({ id: 'quiz_main', name: 'Quiz', type: 'quiz', created_at: new Date().toISOString().split('T')[0], content: data.quiz });
                } else if (endpoint === 'flashcards' && data.flashcards) {
                    setSelectedFile({ id: 'cards_main', name: 'Karteikarten', type: 'flashcards', created_at: new Date().toISOString().split('T')[0], content: data.flashcards });
                } else if (endpoint === 'summary' && data.summary) {
                    setSelectedFile({ id: 'summary_main', name: 'Zusammenfassung', type: 'summary', created_at: new Date().toISOString().split('T')[0], content: data.summary });
                }

                // Also refresh the file list in background
                fetchFiles();
            } else {
                let errorMsg = `HTTP ${res.status}`;
                try {
                    const err = await res.json();
                    if (err.detail === "NO_API_KEY_FOUND" || (typeof err.detail === 'string' && err.detail.includes("API Key"))) {
                        const goToSettings = confirm("⚠️ Kein AI Key gefunden!\n\nDu musst erst deinen Google Gemini API Key in den Einstellungen hinterlegen, um AI-Features zu nutzen.\n\nJetzt zu den Einstellungen?");
                        if (goToSettings) {
                            router.push("/settings");
                        }
                        return;
                    }
                    errorMsg = typeof err.detail === 'object' ? JSON.stringify(err.detail) : (err.detail || errorMsg);
                } catch (_) {
                    // Response not JSON — use status code
                }
                alert(`Fehler: ${errorMsg}`);
            }
        } catch (error) {
            console.error(error);
            alert("Ein Fehler ist aufgetreten.");
        } finally {
            stateSetter(null); // Reset the specific AI generation state
        }
    };

    const handleGenerate = async (type: 'plan' | 'quiz' | 'flashcards' | 'summary') => {
        if (type === 'plan') {
            // Open plan config modal instead of directly generating
            setIsPlanConfigOpen(true);
            return;
        }

        if (type === 'summary') {
            // Open summary config modal
            setIsSummaryConfigOpen(true);
            return;
        }

        setIsGenerating(type);
        let msg = "Erfolgreich generiert!";
        if (type === 'quiz') msg = "Quiz erstellt!";
        else if (type === 'flashcards') msg = "Karteikarten erstellt!";

        await handleAIAction(type, setIsGenerating, msg);
    };

    const handleSummaryGenerate = async () => {
        setIsSummaryConfigOpen(false);
        setIsGenerating('summary');

        // Pass detailLevel to backend (Need to update ai/summary endpoint to accept this optionally, 
        // or just append it to a prompt. If backend doesn't accept it yet, we just generate standard for now.
        // I will add `detail_level` to the POST body.)
        try {
            const username = localStorage.getItem("username");
            const res = await fetch(`${API_BASE}/ai/summary`, {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify({
                    username,
                    folder_id: folderId,
                    detail_level: summaryDetailLevel
                })
            });

            if (res.ok) {
                const data = await res.json();
                setSelectedFile({
                    id: `summary_${Date.now()}`, // Temporary ID for immediate viewing
                    name: 'Automatische Zusammenfassung',
                    type: 'summary',
                    created_at: new Date().toISOString().split('T')[0],
                    content: data.summary
                });
                fetchFiles();
            } else {
                let errorMsg = `HTTP ${res.status}`;
                try {
                    const err = await res.json();
                    if (err.detail === "NO_API_KEY_FOUND" || (typeof err.detail === 'string' && err.detail.includes("API Key"))) {
                        const goToSettings = confirm("⚠️ Kein AI Key gefunden!\n\nDu musst erst deinen Google Gemini API Key in den Einstellungen hinterlegen, um AI-Features zu nutzen.\n\nJetzt zu den Einstellungen?");
                        if (goToSettings) router.push("/settings");
                        return;
                    }
                    errorMsg = typeof err.detail === 'object' ? JSON.stringify(err.detail) : (err.detail || errorMsg);
                } catch (_) { }
                alert(`Zusammenfassungs-Fehler: ${errorMsg}`);
            }
        } catch (error) {
            console.error(error);
            alert("Ein Fehler ist aufgetreten.");
        } finally {
            setIsGenerating(null);
        }
    };

    const handlePlanGenerate = async () => {
        setIsPlanConfigOpen(false);
        setIsGenerating('plan');

        // Calculate duration from end date if selected
        let days = planDays;
        if (planUseDate && planEndDate) {
            const end = new Date(planEndDate);
            const today = new Date();
            today.setHours(0, 0, 0, 0);
            const diff = Math.ceil((end.getTime() - today.getTime()) / (1000 * 60 * 60 * 24));
            days = Math.max(1, diff);
        }

        try {
            const username = localStorage.getItem("username");
            const res = await fetch(`${API_BASE}/ai/plan`, {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify({
                    username,
                    folder_id: folderId,
                    duration_days: days,
                    hours_per_day: planHoursPerDay
                })
            });

            if (res.ok) {
                const data = await res.json();
                if (data.plan && data.plan.length > 0) {
                    setSelectedFile({
                        id: 'plan_main',
                        name: 'Aktueller Lernplan',
                        type: 'plan',
                        created_at: new Date().toISOString().split('T')[0],
                        content: data.plan
                    });
                } else {
                    alert("Lernplan wurde generiert, ist aber leer. Bitte prüfe ob Material im Ordner vorhanden ist.");
                }
                fetchFiles();
            } else {
                let errorMsg = `HTTP ${res.status}`;
                try {
                    const err = await res.json();
                    errorMsg = typeof err.detail === 'object' ? JSON.stringify(err.detail) : (err.detail || errorMsg);
                } catch (_) { }
                alert(`Lernplan-Fehler: ${errorMsg}`);
            }
        } catch (error) {
            console.error("Plan error:", error);
            alert(`Netzwerk-Fehler: ${String(error)}`);
        } finally {
            setIsGenerating(null);
        }
    };

    // --- VIEWER COMPONENTS ---
    const renderViewer = () => {
        if (!selectedFile) return null;

        if (selectedFile.type === 'summary') {
            return (
                <RichTextEditor
                    initialContent={typeof selectedFile.content === 'string' ? selectedFile.content : JSON.stringify(selectedFile.content || '')}
                    title={selectedFile.name}
                    onClose={() => setSelectedFile(null)}
                    onSave={async (newContent) => {
                        try {
                            const username = localStorage.getItem("username");
                            const res = await fetch(`${API_BASE}/files/update`, {
                                method: 'PUT',
                                headers: { 'Content-Type': 'application/json' },
                                body: JSON.stringify({
                                    username,
                                    folder_id: folderId,
                                    file_id: selectedFile.id,
                                    content: newContent
                                })
                            });

                            if (res.ok) {
                                // Update local state so it doesn't revert on close
                                const updatedFiles = files.map(f => f.id === selectedFile.id ? { ...f, content: newContent } : f);
                                setFiles(updatedFiles);
                                setSelectedFile({ ...selectedFile, content: newContent });
                                // Optional: You could show a small toast here instead of alert for better UX
                            } else {
                                const err = await res.json();
                                alert(`Fehler beim Speichern: ${err.detail || 'Unbekannt'}`);
                            }
                        } catch (e) {
                            alert("Netzwerkfehler beim Speichern.");
                        }
                    }}
                />
            );
        }

        if (selectedFile.type === 'plan') {
            const plan = selectedFile.content || [];
            if (!Array.isArray(plan) || plan.length === 0) {
                return (
                    <div className="fixed inset-0 z-[100] bg-[#1e1e1e] flex flex-col items-center justify-center p-4">
                        <div className="text-center">
                            <p className="text-gray-400 text-lg mb-2">⚠️ Lernplan ist leer</p>
                            <button onClick={() => setSelectedFile(null)} className="mt-4 px-4 py-2 bg-[#333] hover:bg-[#444] rounded-lg text-white">Zurück</button>
                        </div>
                    </div>
                );
            }

            return (
                <div className="fixed inset-0 z-[100] bg-[#1e1e1e] flex flex-col w-screen h-screen overflow-hidden animate-in fade-in slide-in-from-bottom-4 duration-300">
                    <div className="flex items-center justify-between p-4 border-b border-[#333] bg-[#1e1e1e] sticky top-0 z-10 w-full">
                        <div className="flex items-center gap-3">
                            <button onClick={() => setSelectedFile(null)} className="p-2 text-gray-400 hover:text-white hover:bg-[#333] rounded-xl transition-colors">
                                <X size={20} />
                            </button>
                            <div className="h-6 w-px bg-[#333] mx-2"></div>
                            <div className="p-2 rounded-lg bg-purple-500/10 text-purple-400">
                                <Calendar size={20} />
                            </div>
                            <h3 className="text-lg font-semibold text-white">{selectedFile.name || 'Persönlicher Lernplan'}</h3>
                        </div>
                    </div>

                    <div className="flex-1 overflow-y-auto bg-[#1a1a1a] p-6">
                        <div className="max-w-4xl mx-auto space-y-4">
                            {plan.map((day: any, i: number) => {
                                const isExpanded = expandedDay === i;

                                return (
                                    <div key={i} className={`bg-[#252526] rounded-2xl border transition-all duration-200 overflow-hidden ${isExpanded ? 'border-purple-500/50 shadow-lg shadow-purple-500/5' : 'border-[#333] hover:border-[#444]'}`}>
                                        <button
                                            onClick={() => setExpandedDay(isExpanded ? null : i)}
                                            className="w-full text-left p-5 flex items-center justify-between focus:outline-none group"
                                        >
                                            <div className="flex items-center gap-4">
                                                <div className={`w-10 h-10 rounded-xl flex items-center justify-center font-bold text-sm transition-colors ${isExpanded ? 'bg-purple-500 text-white' : 'bg-[#333] text-gray-400 group-hover:bg-[#444]'}`}>
                                                    {day.day}
                                                </div>
                                                <div>
                                                    <h3 className={`text-lg transition-colors font-semibold ${isExpanded ? 'text-white' : 'text-gray-300 group-hover:text-white'}`}>{day.topic}</h3>
                                                    {!isExpanded && <p className="text-sm text-gray-500 truncate max-w-md">{day.goal}</p>}
                                                </div>
                                            </div>
                                            <div className={`p-2 rounded-full transition-transform duration-200 ${isExpanded ? 'rotate-180 bg-purple-500/10 text-purple-400' : 'bg-[#1e1e1e] text-gray-500 group-hover:text-white'}`}>
                                                <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><polyline points="6 9 12 15 18 9"></polyline></svg>
                                            </div>
                                        </button>

                                        <div className={`transition-all duration-300 ease-in-out ${isExpanded ? 'max-h-[1000px] opacity-100' : 'max-h-0 opacity-0'}`}>
                                            <div className="p-5 pt-0 ml-14 space-y-4">
                                                <div className="text-sm text-gray-300 bg-[#1e1e1e] p-4 rounded-xl border border-[#333] shadow-inner">
                                                    <div className="flex items-center gap-2 mb-2">
                                                        <span className="text-purple-400 font-semibold text-xs uppercase tracking-wider">🎯 Tagesziel</span>
                                                    </div>
                                                    <p>{day.goal}</p>
                                                </div>

                                                <div className="bg-[#1e1e1e] p-4 rounded-xl border border-[#333]">
                                                    <div className="flex items-center gap-2 mb-3">
                                                        <span className="text-gray-400 font-semibold text-xs uppercase tracking-wider">📝 Aufgaben</span>
                                                    </div>
                                                    <ul className="space-y-3">
                                                        {day.tasks?.map((task: string, idx: number) => (
                                                            <li key={idx} className="flex items-start gap-3 group/task">
                                                                <div className="mt-0.5 w-5 h-5 rounded-md border-2 border-[#444] group-hover/task:border-purple-500 flex items-center justify-center shrink-0 transition-colors"></div>
                                                                <span className="text-gray-300 text-sm group-hover/task:text-white transition-colors">{task}</span>
                                                            </li>
                                                        ))}
                                                    </ul>
                                                </div>

                                                <div className="grid grid-cols-1 md:grid-cols-2 gap-4 mt-4">
                                                    {day.focus && (
                                                        <div className="text-sm bg-amber-500/10 border border-amber-500/20 p-4 rounded-xl">
                                                            <div className="text-amber-500 font-semibold text-xs uppercase tracking-wider mb-2">⚡ Fokus</div>
                                                            <div className="text-amber-200/90">{day.focus}</div>
                                                        </div>
                                                    )}
                                                    {day.tip && (
                                                        <div className="text-sm bg-green-500/10 border border-green-500/20 p-4 rounded-xl">
                                                            <div className="text-green-500 font-semibold text-xs uppercase tracking-wider mb-2">💡 Tipp</div>
                                                            <div className="text-green-200/90">{day.tip}</div>
                                                        </div>
                                                    )}
                                                </div>
                                            </div>
                                        </div>
                                    </div>
                                );
                            })}
                        </div>
                    </div>
                </div>
            );
        }

        return (
            <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/80 backdrop-blur-sm p-4">
                <div className="bg-[#1e1e1e] border border-[#333] rounded-2xl w-full max-w-2xl h-[80vh] flex flex-col shadow-2xl animate-in fade-in zoom-in duration-200">
                    {/* Header */}
                    <div className="flex justify-between items-center p-4 border-b border-[#333]">
                        <div className="flex items-center gap-3">
                            <div className={`p-2 rounded-lg ${selectedFile.type === 'quiz' ? 'bg-orange-500/10 text-orange-400' :
                                selectedFile.type === 'flashcards' ? 'bg-green-500/10 text-green-400' :
                                    selectedFile.type === 'summary' ? 'bg-blue-500/10 text-blue-400' :
                                        'bg-[#333] text-gray-400'
                                }`}>
                                {getFileIcon(selectedFile.type)}
                            </div>
                            <h3 className="text-lg font-semibold text-white">{selectedFile.name}</h3>
                        </div>
                        <button onClick={() => setSelectedFile(null)} className="text-gray-400 hover:text-white p-2 hover:bg-[#333] rounded-lg">
                            <X size={20} />
                        </button>
                    </div>

                    {/* Content */}
                    <div className="flex-1 overflow-y-auto p-6 text-gray-300">
                        {renderFileContent(selectedFile)}
                    </div>
                </div>
            </div>
        );
    };

    const renderFileContent = (file: FileData) => {
        if (file.type === 'quiz') {
            const questions = file.content || [];
            if (!Array.isArray(questions)) return <p>Fehlerhaftes Quiz-Format.</p>;
            return (
                <div className="space-y-6">
                    {questions.map((q: any, i: number) => (
                        <div key={i} className="bg-[#252526] p-4 rounded-xl border border-[#333]">
                            <p className="font-medium text-white mb-3">{i + 1}. {q.question}</p>
                            <div className="space-y-2">
                                {q.options?.map((opt: string, idx: number) => (
                                    <div key={idx} className={`p-3 rounded-lg border ${opt === q.answer ? 'border-green-500/50 bg-green-500/10' : 'border-[#333] hover:bg-[#333]'}`}>
                                        {opt}
                                    </div>
                                ))}
                            </div>
                        </div>
                    ))}
                </div>
            );
        }
        if (file.type === 'flashcards') {
            const cards = file.content || [];
            if (!Array.isArray(cards)) return <p>Fehlerhaftes Format.</p>;
            return (
                <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
                    {cards.map((c: any, i: number) => (
                        <div key={i} className="bg-[#252526] p-4 rounded-xl border border-[#333] flex flex-col gap-2">
                            <div className="font-medium text-white border-b border-[#333] pb-2">{c.front}</div>
                            <div className="text-gray-400 mt-2">{c.back}</div>
                        </div>
                    ))}
                </div>
            );
        }
        return <p className="text-center text-gray-500 mt-10">Vorschau für diesen Datentyp nicht verfügbar.</p>;
    };

    const getFileIcon = (type: string) => {
        switch (type) {
            case 'plan': return <BrainCircuit size={20} />;
            case 'transcript': return <Youtube size={20} />;
            case 'quiz': return <HelpCircle size={20} />;
            case 'flashcards': return <Layers size={20} />;
            case 'summary': return <FileOutput size={20} />;
            default: return <FileText size={20} />;
        }
    };

    return (
        <div className="bg-[#1e1e1e] min-h-screen">
            <div className="max-w-6xl mx-auto px-6 py-10">

                {/* Header */}
                <div className="flex flex-col md:flex-row md:items-center justify-between gap-6 mb-8">
                    <div className="flex items-center gap-4">
                        <button onClick={() => router.back()} className="p-2 hover:bg-[#333] rounded-xl text-gray-400 hover:text-white transition-colors"><ArrowLeft size={22} /></button>
                        <div>
                            <h1 className="text-2xl font-bold text-white">Ordner Details</h1>
                            <p className="text-sm text-gray-400">ID: {folderId}</p>
                        </div>
                    </div>

                    <div className="flex flex-wrap gap-2">
                        {/* AI Actions */}
                        <div className="flex gap-2 mr-2 border-r border-[#333] pr-4">
                            <button onClick={() => handleGenerate('plan')} disabled={!!isGenerating} className="p-2.5 bg-purple-500/10 text-purple-400 hover:bg-purple-500/20 rounded-xl transition-all disabled:opacity-50" title="Lernplan erstellen">
                                {isGenerating === 'plan' ? <Loader2 size={20} className="animate-spin" /> : <BrainCircuit size={20} />}
                            </button>
                            <button onClick={() => handleGenerate('quiz')} disabled={!!isGenerating} className="p-2.5 bg-orange-500/10 text-orange-400 hover:bg-orange-500/20 rounded-xl transition-all disabled:opacity-50" title="Quiz erstellen">
                                {isGenerating === 'quiz' ? <Loader2 size={20} className="animate-spin" /> : <HelpCircle size={20} />}
                            </button>
                            <button onClick={() => handleGenerate('flashcards')} disabled={!!isGenerating} className="p-2.5 bg-green-500/10 text-green-400 hover:bg-green-500/20 rounded-xl transition-all disabled:opacity-50" title="Karteikarten erstellen">
                                {isGenerating === 'flashcards' ? <Loader2 size={20} className="animate-spin" /> : <Layers size={20} />}
                            </button>
                            <button onClick={() => handleGenerate('summary')} disabled={!!isGenerating} className="p-2.5 bg-blue-500/10 text-blue-400 hover:bg-blue-500/20 rounded-xl transition-all disabled:opacity-50" title="Zusammenfassung erstellen">
                                {isGenerating === 'summary' ? <Loader2 size={20} className="animate-spin" /> : <FileOutput size={20} />}
                            </button>
                        </div>

                        <button
                            onClick={() => setIsUploadOpen(true)}
                            className="flex items-center gap-2 bg-[#252526] text-white px-4 py-2.5 rounded-xl text-sm font-semibold border border-[#333] hover:bg-[#333] transition-all"
                        >
                            <Plus size={18} />
                            <span>Material</span>
                        </button>
                    </div>
                </div>

                {/* Content */}
                {loading ? (
                    <div className="flex justify-center py-20">
                        <Loader2 className="animate-spin text-[#5E5CE6]" size={32} />
                    </div>
                ) : files.length === 0 ? (
                    <div className="bg-[#252526] border border-[#333] rounded-2xl p-16 text-center border-dashed">
                        {/* Empty State */}
                        <div className="flex justify-center mb-6">
                            <div className="w-16 h-16 bg-[#1e1e1e] rounded-2xl flex items-center justify-center border border-[#333]">
                                <FileText size={24} className="text-gray-500" />
                            </div>
                        </div>
                        <h3 className="text-xl font-medium text-white mb-2">Dieser Ordner ist noch leer</h3>
                        <p className="text-base text-gray-400 mb-8">
                            Lade Skripte (PDF) hoch oder füge YouTube-Videos hinzu,<br />damit die AI einen Lernplan erstellen kann.
                        </p>
                        <button onClick={() => setIsUploadOpen(true)} className="inline-flex items-center gap-2 bg-[#5E5CE6] hover:bg-[#4d4ac9] text-white px-5 py-2.5 rounded-xl text-sm font-medium transition-all"><Plus size={18} /><span>Material hinzufügen</span></button>
                    </div>
                ) : (
                    <div className="grid grid-cols-1 gap-3">
                        {files.map((file) => (
                            <div
                                key={file.id}
                                onClick={() => setSelectedFile(file)}
                                className="bg-[#252526] hover:bg-[#2d2d2d] border border-[#333] p-4 rounded-xl flex items-center justify-between group transition-colors cursor-pointer"
                            >
                                <div className="flex items-center gap-4">
                                    <div className={`p-3 rounded-lg ${file.type === 'plan' ? 'bg-purple-500/10 text-purple-400' :
                                        file.type === 'quiz' ? 'bg-orange-500/10 text-orange-400' :
                                            file.type === 'flashcards' ? 'bg-green-500/10 text-green-400' :
                                                file.type === 'summary' ? 'bg-blue-500/10 text-blue-400' :
                                                    file.type === 'transcript' ? 'bg-red-500/10 text-red-500' :
                                                        'bg-[#333] text-[#5E5CE6]'
                                        }`}>
                                        {getFileIcon(file.type)}
                                    </div>
                                    <div>
                                        <h4 className="text-sm font-medium text-white">{file.name}</h4>
                                        <p className="text-xs text-gray-500 capitalize">{file.created_at} • {file.type}</p>
                                    </div>
                                </div>
                                <button className="text-gray-500 hover:text-white p-2 rounded-lg hover:bg-[#333] opacity-0 group-hover:opacity-100 transition-all">
                                    <MoreVertical size={18} />
                                </button>
                            </div>
                        ))}
                    </div>
                )}
            </div>

            {/* Upload Modal */}
            {isUploadOpen && (
                <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/60 backdrop-blur-sm p-4">
                    <div className="bg-[#1e1e1e] border border-[#333] rounded-2xl w-full max-w-md p-6 shadow-2xl animate-in fade-in zoom-in duration-200">
                        <div className="flex justify-between items-center mb-6">
                            <h3 className="text-lg font-semibold text-white">Material hinzufügen</h3>
                            <button onClick={() => setIsUploadOpen(false)} className="text-gray-400 hover:text-white"><X size={20} /></button>
                        </div>
                        <div className="flex gap-2 p-1 bg-[#252526] rounded-xl mb-6">
                            <button onClick={() => setUploadType('pdf')} className={`flex-1 flex items-center justify-center gap-2 py-2 text-sm font-medium rounded-lg transition-all ${uploadType === 'pdf' ? 'bg-[#333] text-white shadow-sm' : 'text-gray-400 hover:text-gray-200'}`}><Upload size={16} /> PDF Upload</button>
                            <button onClick={() => setUploadType('youtube')} className={`flex-1 flex items-center justify-center gap-2 py-2 text-sm font-medium rounded-lg transition-all ${uploadType === 'youtube' ? 'bg-[#333] text-red-400 shadow-sm' : 'text-gray-400 hover:text-gray-200'}`}><Youtube size={16} /> YouTube</button>
                        </div>
                        {uploadType === 'pdf' ? (
                            <form onSubmit={handleFileUpload} className="space-y-4">
                                <div className="border-2 border-dashed border-[#333] rounded-xl p-8 text-center hover:border-[#5E5CE6] transition-colors cursor-pointer relative">
                                    <input type="file" accept=".pdf" onChange={(e) => setFileToUpload(e.target.files?.[0] || null)} className="absolute inset-0 w-full h-full opacity-0 cursor-pointer" />
                                    <div className="flex flex-col items-center gap-2 text-gray-400"><Upload size={24} /><span className="text-sm">{fileToUpload ? fileToUpload.name : "Klicken zum Auswählen"}</span></div>
                                </div>
                                <button type="submit" disabled={!fileToUpload || isProcessing} className="w-full bg-[#5E5CE6] hover:bg-[#4d4ac9] text-white py-2.5 rounded-xl text-sm font-medium transition-all disabled:opacity-50 flex justify-center items-center gap-2">{isProcessing && <Loader2 size={16} className="animate-spin" />} Hochladen</button>
                            </form>
                        ) : (
                            <form onSubmit={handleYoutubeImport} className="space-y-4">
                                <div><label className="block text-xs font-medium text-gray-400 mb-1.5 ml-1">YouTube URL</label><input type="url" placeholder="https://youtube.com/watch?v=..." value={youtubeUrl} onChange={(e) => setYoutubeUrl(e.target.value)} className="w-full bg-[#252526] border border-[#333] text-white rounded-xl px-4 py-2.5 focus:ring-2 focus:ring-red-500/50 focus:border-red-500 outline-none placeholder:text-gray-600 transition-all font-sans" /></div>
                                <button type="submit" disabled={!youtubeUrl || isProcessing} className="w-full bg-red-600 hover:bg-red-700 text-white py-2.5 rounded-xl text-sm font-medium transition-all disabled:opacity-50 flex justify-center items-center gap-2">{isProcessing && <Loader2 size={16} className="animate-spin" />} Importieren</button>
                            </form>
                        )}
                    </div>
                </div>
            )}

            {/* File Viewer Modal */}
            {renderViewer()}

            {/* Plan Config Modal */}
            {isPlanConfigOpen && (
                <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/80 backdrop-blur-sm p-4">
                    <div className="bg-[#1e1e1e] border border-[#333] rounded-2xl w-full max-w-md shadow-2xl animate-in fade-in zoom-in duration-200">
                        {/* Header */}
                        <div className="flex justify-between items-center p-5 border-b border-[#333]">
                            <div className="flex items-center gap-3">
                                <div className="p-2 rounded-xl bg-purple-500/10 text-purple-400">
                                    <BrainCircuit size={20} />
                                </div>
                                <div>
                                    <h3 className="text-lg font-semibold text-white">Lernplan erstellen</h3>
                                    <p className="text-xs text-gray-400">Konfiguriere deinen persönlichen Lernplan</p>
                                </div>
                            </div>
                            <button onClick={() => setIsPlanConfigOpen(false)} className="text-gray-400 hover:text-white p-2 hover:bg-[#333] rounded-lg transition-colors">
                                <X size={18} />
                            </button>
                        </div>

                        {/* Body */}
                        <div className="p-5 space-y-5">
                            {/* Duration Mode Toggle */}
                            <div className="flex gap-2 bg-[#252526] p-1 rounded-xl">
                                <button
                                    onClick={() => setPlanUseDate(false)}
                                    className={`flex-1 py-2 px-3 rounded-lg text-sm font-medium transition-all flex items-center justify-center gap-2 ${!planUseDate ? 'bg-[#5E5CE6] text-white' : 'text-gray-400 hover:text-white'}`}
                                >
                                    <Clock size={14} /> Anzahl Tage
                                </button>
                                <button
                                    onClick={() => setPlanUseDate(true)}
                                    className={`flex-1 py-2 px-3 rounded-lg text-sm font-medium transition-all flex items-center justify-center gap-2 ${planUseDate ? 'bg-[#5E5CE6] text-white' : 'text-gray-400 hover:text-white'}`}
                                >
                                    <Calendar size={14} /> Enddatum
                                </button>
                            </div>

                            {/* Days OR End Date */}
                            {!planUseDate ? (
                                <div>
                                    <label className="block text-sm font-medium text-gray-300 mb-2">
                                        Anzahl Lerntage
                                        <span className="ml-2 text-[#5E5CE6] font-bold">{planDays} Tage</span>
                                    </label>
                                    <input
                                        type="range"
                                        min={1} max={60} step={1}
                                        value={planDays}
                                        onChange={e => setPlanDays(Number(e.target.value))}
                                        className="w-full accent-[#5E5CE6]"
                                    />
                                    <div className="flex justify-between text-xs text-gray-500 mt-1">
                                        <span>1 Tag</span><span>30 Tage</span><span>60 Tage</span>
                                    </div>
                                </div>
                            ) : (
                                <div>
                                    <label className="block text-sm font-medium text-gray-300 mb-2">Prüfungsdatum</label>
                                    <input
                                        type="date"
                                        value={planEndDate}
                                        min={new Date().toISOString().split('T')[0]}
                                        onChange={e => setPlanEndDate(e.target.value)}
                                        className="w-full bg-[#252526] border border-[#333] text-white rounded-xl px-4 py-2.5 focus:ring-2 focus:ring-[#5E5CE6]/50 focus:border-[#5E5CE6] outline-none transition-all"
                                    />
                                    {planEndDate && (
                                        <p className="text-xs text-gray-400 mt-1.5">
                                            ≈ {Math.max(1, Math.ceil((new Date(planEndDate).getTime() - new Date().setHours(0, 0, 0, 0)) / 86400000))} Lerntage bis zur Prüfung
                                        </p>
                                    )}
                                </div>
                            )}

                            {/* Hours per Day */}
                            <div>
                                <label className="block text-sm font-medium text-gray-300 mb-2">
                                    Lernzeit pro Tag
                                    <span className="ml-2 text-[#5E5CE6] font-bold">{planHoursPerDay} Std.</span>
                                </label>
                                <input
                                    type="range"
                                    min={0.5} max={8} step={0.5}
                                    value={planHoursPerDay}
                                    onChange={e => setPlanHoursPerDay(Number(e.target.value))}
                                    className="w-full accent-[#5E5CE6]"
                                />
                                <div className="flex justify-between text-xs text-gray-500 mt-1">
                                    <span>30 Min</span><span>4 Std</span><span>8 Std</span>
                                </div>
                            </div>

                            {/* Summary */}
                            <div className="bg-[#252526] rounded-xl p-4 border border-[#333]">
                                <div className="flex items-center gap-2 text-sm text-gray-300">
                                    <BookOpen size={14} className="text-purple-400" />
                                    <span>
                                        {planUseDate && planEndDate
                                            ? `${Math.max(1, Math.ceil((new Date(planEndDate).getTime() - new Date().setHours(0, 0, 0, 0)) / 86400000))} Tage`
                                            : `${planDays} Tage`
                                        } × {planHoursPerDay} Std.
                                        = <strong className="text-white">
                                            {((planUseDate && planEndDate
                                                ? Math.max(1, Math.ceil((new Date(planEndDate).getTime() - new Date().setHours(0, 0, 0, 0)) / 86400000))
                                                : planDays) * planHoursPerDay).toFixed(1)} Std. gesamt
                                        </strong>
                                    </span>
                                </div>
                            </div>
                        </div>

                        {/* Footer */}
                        <div className="flex gap-3 p-5 border-t border-[#333]">
                            <button onClick={() => setIsPlanConfigOpen(false)} className="flex-1 py-2.5 bg-[#252526] hover:bg-[#333] text-gray-300 rounded-xl text-sm font-medium transition-colors">
                                Abbrechen
                            </button>
                            <button
                                onClick={handlePlanGenerate}
                                disabled={planUseDate && !planEndDate}
                                className="flex-1 py-2.5 bg-purple-500 hover:bg-purple-600 text-white rounded-xl text-sm font-semibold transition-colors flex items-center justify-center gap-2 disabled:opacity-50"
                            >
                                <BrainCircuit size={16} />
                                Lernplan generieren
                            </button>
                        </div>
                    </div>
                </div>
            )}

            {/* Summary Config Modal */}
            {isSummaryConfigOpen && (
                <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/80 backdrop-blur-sm p-4">
                    <div className="bg-[#1e1e1e] border border-[#333] rounded-2xl w-full max-w-md shadow-2xl animate-in fade-in zoom-in duration-200">
                        {/* Header */}
                        <div className="flex justify-between items-center p-5 border-b border-[#333]">
                            <div className="flex items-center gap-3">
                                <div className="p-2 rounded-xl bg-blue-500/10 text-blue-400">
                                    <FileOutput size={20} />
                                </div>
                                <div>
                                    <h3 className="text-lg font-semibold text-white">Zusammenfassung erstellen</h3>
                                    <p className="text-xs text-gray-400">Wie detailliert soll der Text sein?</p>
                                </div>
                            </div>
                            <button onClick={() => setIsSummaryConfigOpen(false)} className="text-gray-400 hover:text-white p-2 hover:bg-[#333] rounded-lg transition-colors">
                                <X size={18} />
                            </button>
                        </div>

                        {/* Body */}
                        <div className="p-5 space-y-5">
                            <div>
                                <label className="block text-sm font-medium text-gray-300 mb-3">Detailgrad wählen</label>
                                <div className="grid grid-cols-1 gap-3">
                                    {['Kurz', 'Normal', 'Ausführlich'].map((level) => (
                                        <button
                                            key={level}
                                            onClick={() => setSummaryDetailLevel(level)}
                                            className={`flex items-center justify-between p-4 rounded-xl border transition-all ${summaryDetailLevel === level
                                                ? 'bg-blue-500/10 border-blue-500/50 text-blue-400'
                                                : 'bg-[#252526] border-[#333] text-gray-400 hover:border-[#444] hover:text-gray-300'}`}
                                        >
                                            <span className="font-medium">{level}</span>
                                            {summaryDetailLevel === level && <div className="w-2 h-2 rounded-full bg-blue-500"></div>}
                                        </button>
                                    ))}
                                </div>
                                <p className="text-xs text-gray-500 mt-4 text-center">
                                    {summaryDetailLevel === 'Kurz' && 'Perfekt für einen schnellen Überblick vor der Prüfung.'}
                                    {summaryDetailLevel === 'Normal' && 'Ausgewogene Erklärung aller wesentlichen Konzepte.'}
                                    {summaryDetailLevel === 'Ausführlich' && 'Sehr tiefe Erklärung fast aller Details im Material.'}
                                </p>
                            </div>
                        </div>

                        {/* Footer */}
                        <div className="flex gap-3 p-5 border-t border-[#333]">
                            <button onClick={() => setIsSummaryConfigOpen(false)} className="flex-1 py-2.5 bg-[#252526] hover:bg-[#333] text-gray-300 rounded-xl text-sm font-medium transition-colors">
                                Abbrechen
                            </button>
                            <button
                                onClick={handleSummaryGenerate}
                                className="flex-1 py-2.5 bg-blue-500 hover:bg-blue-600 text-white rounded-xl text-sm font-semibold transition-colors flex items-center justify-center gap-2"
                            >
                                <FileOutput size={16} />
                                Generieren
                            </button>
                        </div>
                    </div>
                </div>
            )}
        </div>
    );
}
