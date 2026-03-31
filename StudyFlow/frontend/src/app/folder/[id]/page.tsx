"use client";

import React, { useState, useEffect, useRef, useCallback } from "react";
import { useRouter, useParams } from "next/navigation";
import { ArrowLeft, FileText, MoreVertical, Plus, Loader2, Youtube, Upload, BrainCircuit, X, HelpCircle, Layers, FileOutput, Calendar, Clock, BookOpen, Repeat, Maximize2, Edit, Download, ImageIcon, CheckCircle2, XCircle, ChevronDown, ChevronUp, ListTodo, ExternalLink } from "lucide-react";
import ReactMarkdown from 'react-markdown';
import remarkGfm from 'remark-gfm';
import remarkMath from 'remark-math';
import rehypeKatex from 'rehype-katex';
import 'katex/dist/katex.min.css';
import RichTextEditor from "@/components/RichTextEditor";
import FloatingChat from "@/components/FloatingChat";
import { motion, AnimatePresence } from 'framer-motion';
import { DndContext, DragOverlay, closestCenter, useDraggable, useDroppable, DragStartEvent, DragEndEvent, useSensor, useSensors, MouseSensor } from '@dnd-kit/core';

interface FileData {
    id: string;
    name: string;
    type: string;
    created_at: string;
    content?: unknown;
    file_url?: string;
}

function openYoutubeVideoInNewTab(videoId: string) {
    window.open(
        `https://www.youtube.com/watch?v=${encodeURIComponent(videoId)}`,
        "_blank",
        "noopener,noreferrer"
    );
}

/** YouTube imports are saved with display name `YouTube: {videoId}` (see backend main.py). */
function youtubeVideoIdFromTranscriptFile(file: Pick<FileData, "name" | "type">): string | null {
    if (file.type !== "transcript") return null;
    const name = (file.name || "").trim();
    const colon = /^YouTube:\s*([a-zA-Z0-9_-]{11})\s*$/i.exec(name);
    if (colon) return colon[1];
    const underscored = /^YouTube_([a-zA-Z0-9_-]{11})(?:\.txt)?$/i.exec(name);
    if (underscored) return underscored[1];
    return null;
}

interface QuizQuestion {
    question: string;
    options?: string[];
    answer: string;
}

interface SubfolderRow {
    id: string;
    name: string;
    created_at: string;
}

type PlanTask = string | { description: string; completed?: boolean };

interface PlanDayRow {
    day: number | string;
    topic: string;
    goal: string;
    summary?: string;
    focus?: string;
    tasks?: PlanTask[];
}

interface FlashcardRow {
    front: string;
    back: string;
}

interface DocumentPatch {
    patch_description: string;
    apply_mode: 'append' | 'replace_section';
    section_hint?: string;
    new_text: string;
}

/** Keys used in isGenerating + API paths for quiz/flashcards/plan */
const AI_JOB_LABELS: Record<string, string> = {
    plan: 'Lernplan',
    quiz: 'Quiz',
    flashcards: 'Karteikarten',
    summary: 'Zusammenfassung',
    elaboration: 'Ausarbeitung',
    repetition: 'Wiederholung',
};

type DraggableFileProps = {
    file: FileData;
    icon: React.ReactNode;
    openMenuFileId: string | null;
    setOpenMenuFileId: React.Dispatch<React.SetStateAction<string | null>>;
    setSelectedFile: React.Dispatch<React.SetStateAction<FileData | null>>;
    setFileToRename: React.Dispatch<React.SetStateAction<FileData | null>>;
    setRenameFileValue: React.Dispatch<React.SetStateAction<string>>;
    setIsRenameFileOpen: React.Dispatch<React.SetStateAction<boolean>>;
    handleDelete: (file: FileData) => void;
    onRefineFile: (file: FileData) => void;
};

// Sub-component for interactive Quiz viewing
const QuizViewer = ({ questions }: { questions: QuizQuestion[] }) => {
    const [selectedAnswers, setSelectedAnswers] = useState<Record<number, string>>({});

    const handleSelect = (qIndex: number, option: string) => {
        if (selectedAnswers[qIndex]) return; // prevent changing answer after selection
        setSelectedAnswers(prev => ({ ...prev, [qIndex]: option }));
    };

    return (
        <div className="space-y-6 pb-12">
            {questions.map((q: QuizQuestion, i: number) => {
                const userSelected = selectedAnswers[i];
                return (
                    <div key={i} className="bg-[#151525] p-5 rounded-xl border border-[#2A2A40] shadow-md">
                        <p className="font-medium text-white mb-4 text-lg">{i + 1}. {q.question}</p>
                        <div className="space-y-3">
                            {q.options?.map((opt: string, idx: number) => {
                                const isSelected = userSelected === opt;
                                const isCorrect = opt === q.answer;
                                const showCorrect = userSelected && isCorrect; // Highlight correct answer once user has voted
                                const showWrong = isSelected && !isCorrect;

                                let styles = "border-[#3B3B55] hover:bg-[#1C1C33] cursor-pointer text-gray-300";

                                if (userSelected) {
                                    styles = "border-[#2A2A40] opacity-60 cursor-default"; // General disabled state
                                    if (showCorrect) styles = "border-green-500/50 bg-green-500/10 text-green-400 font-medium opacity-100";
                                    if (showWrong) styles = "border-red-500/50 bg-red-500/10 text-red-400 font-medium opacity-100";
                                }

                                return (
                                    <div
                                        key={idx}
                                        onClick={() => handleSelect(i, opt)}
                                        className={`p-3 rounded-lg border transition-all ${styles}`}
                                    >
                                        <div className="flex items-center justify-between">
                                            <span>{opt}</span>
                                            {showCorrect && <svg className="text-green-500" width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><polyline points="20 6 9 17 4 12"></polyline></svg>}
                                            {showWrong && <svg className="text-red-500" width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><line x1="18" y1="6" x2="6" y2="18"></line><line x1="6" y1="6" x2="18" y2="18"></line></svg>}
                                        </div>
                                    </div>
                                );
                            })}
                        </div>
                    </div>
                );
            })}
        </div>
    );
};

// Subfolder Droppable Wrapper
const DroppableSubfolder = ({ subfolder, onClick }: { subfolder: SubfolderRow, onClick: () => void }) => {
    const { isOver, setNodeRef } = useDroppable({
        id: `drop-${subfolder.id}`,
        data: { type: 'subfolder', subfolder }
    });

    return (
        <motion.button
            ref={setNodeRef}
            layout
            initial={{ opacity: 0, scale: 0.95, y: -5 }}
            animate={{ opacity: 1, scale: 1, y: 0 }}
            exit={{ opacity: 0, scale: 0.95, y: -5 }}
            transition={{ duration: 0.2 }}
            onClick={onClick}
            className={`flex items-center gap-2 border p-3 rounded-xl text-left transition-all group
                ${isOver ? 'bg-[#1C1C33] border-[#5E5CE6] shadow-lg shadow-[#5E5CE6]/20 scale-105' : 'bg-[#151525] hover:bg-[#1C1C33] border-[#2A2A40] hover:border-[#5E5CE6]/40'}
            `}
        >
            <div className="p-1.5 rounded-lg bg-[#5E5CE6]/10 text-[#5E5CE6] shrink-0">
                <svg xmlns="http://www.w3.org/2000/svg" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><path d="M22 19a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h5l2 3h9a2 2 0 0 1 2 2z" /></svg>
            </div>
            <span className="text-sm text-gray-300 group-hover:text-white truncate font-medium">{subfolder.name}</span>
        </motion.button>
    );
};

// Draggable File Wrapper
const DraggableFile = ({ file, icon, openMenuFileId, setOpenMenuFileId, setSelectedFile, setFileToRename, setRenameFileValue, setIsRenameFileOpen, handleDelete, onRefineFile }: DraggableFileProps) => {
    const { attributes, listeners, setNodeRef, isDragging } = useDraggable({
        id: `drag-${file.id}`,
        data: { type: 'file', file }
    });

    return (
        <motion.div
            ref={setNodeRef}
            layout
            initial={{ opacity: 0, scale: 0.95, x: -10 }}
            animate={{ opacity: isDragging ? 0.4 : 1, scale: 1, x: 0 }}
            exit={{ opacity: 0, scale: 0.95, x: -10 }}
            transition={{ duration: 0.2 }}
            {...attributes}
            {...listeners}
            onClick={() => { if (!openMenuFileId) setSelectedFile(file); }}
            onContextMenu={(e) => {
                e.preventDefault();
                setOpenMenuFileId(openMenuFileId === file.id ? null : file.id);
            }}
            className={`bg-[#151525] hover:bg-[#1C1C33] border border-[#2A2A40] p-4 rounded-xl flex items-center justify-between group transition-colors cursor-pointer relative ${isDragging ? 'z-50 shadow-2xl border-[#5E5CE6]' : ''}`}
        >
            <div className="flex items-center gap-4">
                <div className={`p-3 rounded-lg ${file.type === 'plan' ? 'bg-purple-500/10 text-purple-400' :
                    file.type === 'quiz' ? 'bg-orange-500/10 text-orange-400' :
                        file.type === 'flashcards' ? 'bg-green-500/10 text-green-400' :
                            file.type === 'summary' ? 'bg-blue-500/10 text-blue-400' :
                                file.type === 'transcript' ? 'bg-red-500/10 text-red-500' :
                                    'bg-[#1C1C33] text-[#5E5CE6]'
                    }`}>
                    {icon}
                </div>
                <div>
                    <h4 className="text-sm font-medium text-white select-none">{file.name}</h4>
                    <p className="text-xs text-gray-500 capitalize select-none">{file.created_at} • {file.type}</p>
                </div>
            </div>

            {/* 3-dot button + dropdown */}
            <div className="relative" onPointerDown={(e) => e.stopPropagation()} onClick={(e) => e.stopPropagation()}>
                <button
                    onClick={() => setOpenMenuFileId(openMenuFileId === file.id ? null : file.id)}
                    className="text-gray-500 hover:text-white p-2 rounded-lg hover:bg-[#1C1C33] opacity-0 group-hover:opacity-100 transition-all"
                >
                    <MoreVertical size={18} />
                </button>

                {openMenuFileId === file.id && (
                    <div className="absolute right-0 top-full mt-1 w-40 bg-[#0B0B1A] border border-[#2A2A40] rounded-xl shadow-2xl z-50 overflow-hidden animate-in fade-in zoom-in-95 duration-150">
                        <button
                            onClick={() => { setSelectedFile(file); setOpenMenuFileId(null); }}
                            className="w-full flex items-center gap-2 px-4 py-2.5 text-sm text-gray-300 hover:bg-[#1C1C33] hover:text-white transition-colors"
                        >
                            <svg xmlns="http://www.w3.org/2000/svg" width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><path d="M2 12s3-7 10-7 10 7 10 7-3 7-10 7-10-7-10-7Z" /><circle cx="12" cy="12" r="3" /></svg>
                            Öffnen
                        </button>

                        {youtubeVideoIdFromTranscriptFile(file) && (
                            <>
                                <div className="h-px bg-[#1C1C33]" />
                                <button
                                    type="button"
                                    onClick={() => {
                                        const vid = youtubeVideoIdFromTranscriptFile(file);
                                        setOpenMenuFileId(null);
                                        if (vid) openYoutubeVideoInNewTab(vid);
                                    }}
                                    className="w-full flex items-center gap-2 px-4 py-2.5 text-sm text-gray-300 hover:bg-[#1C1C33] hover:text-white transition-colors"
                                >
                                    <ExternalLink size={15} />
                                    Video auf YouTube öffnen
                                </button>
                            </>
                        )}

                        {file.type !== 'pdf' && (
                            <>
                                <div className="h-px bg-[#1C1C33]" />
                                <button
                                    onClick={() => {
                                        setFileToRename(file);
                                        setRenameFileValue(file.name);
                                        setIsRenameFileOpen(true);
                                        setOpenMenuFileId(null);
                                    }}
                                    className="w-full flex items-center gap-2 px-4 py-2.5 text-sm text-gray-300 hover:bg-[#1C1C33] hover:text-white transition-colors"
                                >
                                    <Edit size={15} />
                                    Umbenennen
                                </button>
                            </>
                        )}

                        {(file.type === 'summary' || file.type === 'repetition' || file.type === 'elaboration') && (
                            <>
                                <div className="h-px bg-[#1C1C33]" />
                                <button
                                    onClick={() => {
                                        onRefineFile(file);
                                        setOpenMenuFileId(null);
                                    }}
                                    className="w-full flex items-center gap-2 px-4 py-2.5 text-sm text-gray-300 hover:bg-[#1C1C33] hover:text-white transition-colors"
                                >
                                    <Edit size={15} />
                                    Mit Prompt anpassen
                                </button>
                            </>
                        )}

                        <div className="h-px bg-[#1C1C33]" />
                        <button
                            onClick={async () => {
                                setOpenMenuFileId(null);
                                handleDelete(file);
                            }}
                            className="w-full flex items-center gap-2 px-4 py-2.5 text-sm text-red-400 hover:bg-red-500/10 hover:text-red-300 transition-colors"
                        >
                            <svg xmlns="http://www.w3.org/2000/svg" width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><polyline points="3 6 5 6 21 6" /><path d="M19 6l-1 14a2 2 0 0 1-2 2H8a2 2 0 0 1-2-2L5 6" /><path d="M10 11v6" /><path d="M14 11v6" /><path d="M9 6V4a1 1 0 0 1 1-1h4a1 1 0 0 1 1 1v2" /></svg>
                            Löschen
                        </button>
                    </div>
                )}
            </div>
        </motion.div>
    );
};

export default function FolderPage() {
    const router = useRouter();
    const params = useParams();
    const folderId = params?.id as string;
    const [files, setFiles] = useState<FileData[]>([]);
    const [loading, setLoading] = useState(true);

    // Upload / Action States
    const [isUploadOpen, setIsUploadOpen] = useState(false);
    const [uploadType, setUploadType] = useState<'pdf' | 'youtube' | 'audio' | 'image' | 'document'>('pdf');
    const [filesToUpload, setFilesToUpload] = useState<File[]>([]);
    const [youtubeUrl, setYoutubeUrl] = useState("");
    const [isProcessing, setIsProcessing] = useState(false);

    // Audio Recording State
    const [isRecording, setIsRecording] = useState(false);
    const [mediaRecorder, setMediaRecorder] = useState<MediaRecorder | null>(null);
    const [recordingTime, setRecordingTime] = useState(0);
    const audioTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null);
    const [audioBlob, setAudioBlob] = useState<Blob | null>(null);

    // AI Generation States (parallel: each job type may run once at a time; different types in parallel)
    const [isGenerating, setIsGenerating] = useState<string[]>([]);
    const [completedAiJobs, setCompletedAiJobs] = useState<{ id: string; jobKey: string; label: string; ok: boolean }[]>([]);
    const [aiJobsPanelExpanded, setAiJobsPanelExpanded] = useState(true);
    const [isEditingFile, setIsEditingFile] = useState(false);

    // Toast notification (replaces all showToast() calls)
    const [toast, setToast] = useState<{ msg: string; type: 'error' | 'info' | 'success' } | null>(null);
    const showToast = (msg: string, type: 'error' | 'info' | 'success' = 'error') => {
        setToast({ msg, type });
        setTimeout(() => setToast(null), 6000);
    };

    const registerAiJobFinished = useCallback((jobKey: string, ok: boolean) => {
        const label = AI_JOB_LABELS[jobKey] || jobKey;
        const id = `${jobKey}_${Date.now()}_${Math.random().toString(36).slice(2, 9)}`;
        setCompletedAiJobs((prev) => [{ id, jobKey, label, ok }, ...prev].slice(0, 12));
        window.setTimeout(() => {
            setCompletedAiJobs((prev) => prev.filter((j) => j.id !== id));
        }, 12000);
    }, []);

    const prevGeneratingCountRef = useRef(0);
    useEffect(() => {
        if (isGenerating.length > prevGeneratingCountRef.current) {
            setAiJobsPanelExpanded(true);
        }
        prevGeneratingCountRef.current = isGenerating.length;
    }, [isGenerating.length]);

    const prevProcessingRef = useRef(false);
    useEffect(() => {
        if (isProcessing && !prevProcessingRef.current) {
            setAiJobsPanelExpanded(true);
        }
        prevProcessingRef.current = isProcessing;
    }, [isProcessing]);

    // Plan Config Modal State
    const [isPlanConfigOpen, setIsPlanConfigOpen] = useState(false);
    const [targetGrade, setTargetGrade] = useState(10); // Default to a solid "Gut" (10 Pkt)
    const [planDays, setPlanDays] = useState(7);
    const [planHoursPerDay, setPlanHoursPerDay] = useState(2);
    const [planEndDate, setPlanEndDate] = useState('');
    const [planUseDate, setPlanUseDate] = useState(false);
    const [planActiveDays, setPlanActiveDays] = useState<number[]>([0, 1, 2, 3, 4, 5, 6]); // 0=Mo, 6=Su

    // Handler for changing target grade and auto-calculating realistic study times
    const handleTargetGradeChange = (grade: number) => {
        setTargetGrade(grade);

        // Base realistic calculations:
        // 0-4 Pkt: Minimal effort (~3 days, 1h/day)
        // 5-9 Pkt: Average effort (~5 days, 2h/day)
        // 10-12 Pkt: Good effort (~7-10 days, 3h/day)
        // 13-15 Pkt: Excellent effort (~14-21 days, 4-5h/day)

        let recommendedDays = 7;
        let recommendedHours = 2;

        if (grade <= 4) {
            recommendedDays = 3;
            recommendedHours = 1;
        } else if (grade <= 9) {
            // Scale linearly between 5 and 9
            recommendedDays = 4 + (grade - 5);
            recommendedHours = 2;
        } else if (grade <= 12) {
            recommendedDays = 7 + (grade - 10) * 2; // 7, 9, 11
            recommendedHours = 3;
        } else {
            recommendedDays = 14 + (grade - 13) * 3; // 14, 17, 20
            recommendedHours = 4 + (grade - 13) * 0.5; // 4, 4.5, 5
        }

        setPlanDays(recommendedDays);
        setPlanHoursPerDay(recommendedHours);

        // Ensure "Anzahl Tage" mode is active because we calculate relative days
        setPlanUseDate(false);
    };

    // Summary Config Modal State
    const [isSummaryConfigOpen, setIsSummaryConfigOpen] = useState(false);
    const [summaryDetailLevel, setSummaryDetailLevel] = useState('Normal');

    // Elaboration (Ausarbeitung) Config Modal State
    const [isElaborationConfigOpen, setIsElaborationConfigOpen] = useState(false);
    const [elaborationDetailLevel, setElaborationDetailLevel] = useState('Normal');
    const [elaborationRules, setElaborationRules] = useState('');
    const [isRefineModalOpen, setIsRefineModalOpen] = useState(false);
    const [fileToRefine, setFileToRefine] = useState<FileData | null>(null);
    const [refinePrompt, setRefinePrompt] = useState('');
    const [refineSaveMode, setRefineSaveMode] = useState<'new_file' | 'replace'>('new_file');
    const [isRefining, setIsRefining] = useState(false);

    // Repetition (Wiederholung) Config Modal State
    const [isRepetitionConfigOpen, setIsRepetitionConfigOpen] = useState(false);
    const [repetitionRules, setRepetitionRules] = useState('');

    // Learning Mode (shared across plan/summary/repetition modals)
    const [learningMode, setLearningMode] = useState<'normal' | 'exercise'>('normal');

    // File context menu state
    const [openMenuFileId, setOpenMenuFileId] = useState<string | null>(null);

    // File rename state
    const [isRenameFileOpen, setIsRenameFileOpen] = useState(false);
    const [fileToRename, setFileToRename] = useState<FileData | null>(null);
    const [renameFileValue, setRenameFileValue] = useState("");
    const [isRenamingFile, setIsRenamingFile] = useState(false);

    // Subfolder state
    const [subfolders, setSubfolders] = useState<{ id: string; name: string; created_at: string }[]>([]);
    const [isCreateSubfolderOpen, setIsCreateSubfolderOpen] = useState(false);
    const [newSubfolderName, setNewSubfolderName] = useState('');

    // Native Drag and Drop State (OS to Browser)
    const [isDraggingOverBase, setIsDraggingOverBase] = useState(false);
    const [isUploadingFromDrop, setIsUploadingFromDrop] = useState(false);
    const [uploadDropMessage, setUploadDropMessage] = useState("");

    // Task Help Overlay State
    const [isTaskHelpOpen, setIsTaskHelpOpen] = useState(false);
    const [taskHelpContent, setTaskHelpContent] = useState('');
    const [isTaskHelpLoading, setIsTaskHelpLoading] = useState(false);
    const [activeTaskTitle, setActiveTaskTitle] = useState('');

    // Fullscreen Flashcard State
    const [fullscreenCard, setFullscreenCard] = useState<{ front: string, back: string } | null>(null);
    const [isFullscreenCardFlipped, setIsFullscreenCardFlipped] = useState(false);

    // Global AI Model Override State (for all AI generation overlays)
    const [aiModelPreference, setAiModelPreference] = useState<string>('');

    // Drag & Drop State
    const [activeDragFile, setActiveDragFile] = useState<FileData | null>(null);

    const sensors = useSensors(
        useSensor(MouseSensor, {
            activationConstraint: {
                distance: 25, // Require 25px of movement to start dragging. This allows clicks to fire.
            },
        })
    );

    // Viewer State
    const [selectedFile, setSelectedFile] = useState<FileData | null>(null);
    const [documentUndoStack, setDocumentUndoStack] = useState<Record<string, string[]>>({});
    const [expandedDay, setExpandedDay] = useState<number | null>(0);

    const API_BASE = '/api';

    const fetchFiles = useCallback(async () => {
        try {
            const username = localStorage.getItem("username");
            if (!username) return;

            const [filesRes, subfoldersRes] = await Promise.all([
                fetch(`${API_BASE}/files/${folderId}?username=${username}`),
                fetch(`${API_BASE}/folders/${folderId}/subfolders?username=${username}`)
            ]);
            if (filesRes.ok) setFiles(await filesRes.json());
            if (subfoldersRes.ok) setSubfolders(await subfoldersRes.json());
        } catch (error) {
            console.error("Failed to fetch files:", error);
        } finally {
            setLoading(false);
        }
    }, [folderId]);

    useEffect(() => {
        void fetchFiles();
    }, [fetchFiles]);

    // Setup global drag and drop event listeners
    useEffect(() => {
        const handleDragOver = (e: DragEvent) => {
            e.preventDefault();
            e.stopPropagation();
            // Check if the dragged item is a file (not a dnd-kit element)
            if (e.dataTransfer?.types.includes('Files')) {
                setIsDraggingOverBase(true);
            }
        };

        const handleDragLeave = (e: DragEvent) => {
            e.preventDefault();
            e.stopPropagation();
            setIsDraggingOverBase(false);
        };

        const handleDrop = async (e: DragEvent) => {
            e.preventDefault();
            e.stopPropagation();
            setIsDraggingOverBase(false);

            const filesToUpload = e.dataTransfer?.files;
            if (filesToUpload && filesToUpload.length > 0) {
                // Handle the first dropped file
                const file = filesToUpload[0];
                await processNativeFileUpload(file);
            }
        };

        // We bind to document body so the whole page acts as a drop zone
        document.body.addEventListener('dragover', handleDragOver);
        document.body.addEventListener('dragleave', handleDragLeave);
        document.body.addEventListener('drop', handleDrop);

        return () => {
            document.body.removeEventListener('dragover', handleDragOver);
            document.body.removeEventListener('dragleave', handleDragLeave);
            document.body.removeEventListener('drop', handleDrop);
        };
    // processNativeFileUpload is defined below; folderId keeps drop target in sync
    // eslint-disable-next-line react-hooks/exhaustive-deps
    }, [folderId]);

    // Shared upload processing logic for both Modal and Drag & Drop
    const processNativeFileUpload = async (file: File) => {
        const isAudio = file.type.startsWith('audio/') || file.name.match(/\.(mp3|wav|m4a|webm)$/i);
        const username = localStorage.getItem("username");
        const formData = new FormData();
        formData.append("file", file);

        setIsUploadingFromDrop(true);
        setUploadDropMessage(isAudio ? "Verarbeite Audio..." : "Lade Datei hoch...");

        try {
            const endpoint = isAudio
                ? `${API_BASE}/files/audio?username=${username}&folder_id=${folderId}`
                : `${API_BASE}/files/upload?username=${username}&folder_id=${folderId}`;

            const res = await fetch(endpoint, {
                method: "POST",
                body: formData
            });

            if (res.ok) {
                showToast(isAudio ? "Audio erfolgreich transkribiert!" : "Datei hochgeladen!", "success");
                if (isUploadOpen) setIsUploadOpen(false); // Close modal if it was open
                setFilesToUpload([]); // Reset manual selection form state
                fetchFiles();
            } else {
                const err = await res.json();
                if (res.status === 402 || (err.detail && err.detail.includes("Nicht genügend Tokens"))) {
                    showToast(err.detail || "Nicht genügend Tokens für diese Aktion.");
                } else if (err.detail && err.detail.includes("API_KEY")) {
                    showToast("Serverfehler: Kein Google Gemini API Key auf dem Server konfiguriert.");
                } else {
                    showToast(`Fehler beim Upload: ${err.detail || "Server Error"}`);
                }
            }
        } catch (error) {
            console.error("Upload Error:", error);
            showToast("Netzwerkfehler beim Upload.");
        } finally {
            setIsUploadingFromDrop(false);
            if (isProcessing) setIsProcessing(false); // Clear parent modal loading state if used there
        }
    };

    const handleFileUpload = async (e: React.FormEvent) => {
        e.preventDefault();
        if (filesToUpload.length === 0) return;
        setIsProcessing(true); // Let the processNativeFileUpload handle the actual requests
        
        await Promise.all(filesToUpload.map(file => processNativeFileUpload(file)));
        setIsProcessing(false);
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
                const err = await res.json();
                if (res.status === 402 || (err.detail && err.detail.includes("Nicht genügend Tokens"))) {
                    showToast(err.detail || "Nicht genügend Tokens.");
                } else if (err.detail && err.detail.includes("API_KEY")) {
                    showToast("Serverfehler: Kein Google Gemini API Key auf dem Server konfiguriert.");
                } else {
                    showToast(`Fehler: ${err.detail || "Ungültige URL oder kein Transkript verfügbar."}`);
                }
            }
        } catch (error) {
            console.error(error);
            showToast("Konnte YouTube-Video nicht importieren.");
        } finally {
            setIsProcessing(false);
        }
    };

    const handleImageUpload = async (e: React.FormEvent) => {
        e.preventDefault();
        if (filesToUpload.length === 0) return;
        setIsProcessing(true);
        try {
            const username = localStorage.getItem("username");
            
            await Promise.all(filesToUpload.map(async (file) => {
                const formData = new FormData();
                formData.append("file", file);

                const res = await fetch(`${API_BASE}/files/image?username=${username}&folder_id=${folderId}`, {
                    method: "POST",
                    body: formData
                });

                if (!res.ok) {
                    const err = await res.json();
                    showToast(`Fehler beim Bild-Upload von ${file.name}: ${err.detail || "Unbekannter Fehler"}`);
                }
            }));

            setFilesToUpload([]);
            setIsUploadOpen(false);
            fetchFiles();
            showToast("Bilder erfolgreich hochgeladen!", "success");
        } catch (error) {
            console.error("Image upload error:", error);
            showToast("Konnte Bilder nicht hochladen.");
        } finally {
            setIsProcessing(false);
        }
    };

    const handleDocumentCreate = async (e: React.FormEvent) => {
        e.preventDefault();
        const titleElement = document.getElementById('new-document-title') as HTMLInputElement;
        const title = titleElement?.value;
        if (!title) {
            showToast("Bitte gib einen Titel für das Dokument ein.");
            return;
        }

        setIsProcessing(true);
        try {
            const username = localStorage.getItem("username");
            const res = await fetch(`${API_BASE}/files/document?username=${username}&folder_id=${folderId}`, {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify({ title: title, content: "<h1>" + title + "</h1>\n<p>Start typing...</p>" })
            });

            if (res.ok) {
                setIsUploadOpen(false);
                fetchFiles();
                // Optionally open it right away for editing
                setTimeout(() => {
                    // Try to find the just created file. We fetch it again so it might not be in the state instantly.
                    // This is a simple improvement for later. For now, just refresh the list.
                }, 500);
            } else {
                const err = await res.json();
                showToast(`Fehler beim Erstellen: ${err.detail || 'Unbekannt'}`);
            }
        } catch (error) {
            console.error("Create document error:", error);
            showToast("Netzwerkfehler beim Erstellen.");
        } finally {
            setIsProcessing(false);
        }
    };

    // --- Audio Recording Logic ---
    const startRecording = async () => {
        try {
            const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
            const recorder = new MediaRecorder(stream);
            const chunks: Blob[] = [];

            recorder.ondataavailable = (e) => {
                if (e.data.size > 0) chunks.push(e.data);
            };

            recorder.onstop = () => {
                const blob = new Blob(chunks, { type: 'audio/webm' });
                setAudioBlob(blob);
                stream.getTracks().forEach(track => track.stop());
            };

            recorder.start();
            setMediaRecorder(recorder);
            setIsRecording(true);
            setRecordingTime(0);

            // Safely clear old intervals
            if (audioTimerRef.current) clearInterval(audioTimerRef.current);
            audioTimerRef.current = setInterval(() => {
                setRecordingTime(prev => prev + 1);
            }, 1000);

            // Cleanup interval when recording stops
            recorder.addEventListener('stop', () => {
                if (audioTimerRef.current) {
                    clearInterval(audioTimerRef.current);
                    audioTimerRef.current = null;
                }
            });

        } catch (err) {
            console.error("Error accessing microphone:", err);
            showToast("Fehler beim Zugriff auf das Mikrofon. Bitte Berechtigungen prüfen.");
        }
    };

    const stopRecording = () => {
        if (mediaRecorder && isRecording) {
            mediaRecorder.stop();
            setIsRecording(false);
        }
    };

    const handleAudioUpload = async () => {
        if (!audioBlob) return;
        setIsProcessing(true);
        try {
            const username = localStorage.getItem("username");
            const formData = new FormData();
            formData.append("file", audioBlob, `audio_${Date.now()}.webm`);

            const res = await fetch(`${API_BASE}/files/audio?username=${username}&folder_id=${folderId}`, {
                method: "POST",
                body: formData
            });

            if (res.ok) {
                setAudioBlob(null);
                setRecordingTime(0);
                setIsUploadOpen(false);
                fetchFiles();
            } else {
                const err = await res.json();
                if (res.status === 402 || (err.detail && err.detail.includes("Nicht genügend Tokens"))) {
                    showToast(err.detail || "Nicht genügend Tokens für diese Aktion.");
                } else if (err.detail && err.detail.includes("API_KEY")) {
                    showToast("Serverfehler: Kein Google Gemini API Key konfiguriert.");
                } else {
                    showToast(`Fehler beim Audio-Upload: ${err.detail || "Unbekannter Fehler"}`);
                }
            }
        } catch (error) {
            console.error(error);
            showToast("Konnte Sprachmemo nicht hochladen.");
        } finally {
            setIsProcessing(false);
        }
    };
    // ----------------------------

    // Helper for AI Actions — jobs of different types may run in parallel
    const handleAIAction = async (endpoint: string, stateSetter: React.Dispatch<React.SetStateAction<string[]>>) => {
        stateSetter((prev) => (prev.includes(endpoint) ? prev : [...prev, endpoint]));
        let ok = false;
        try {
            const username = localStorage.getItem("username");
            const res = await fetch(`${API_BASE}/ai/${endpoint}`, {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify({ username, folder_id: folderId, duration_days: 7, model_preference: aiModelPreference || undefined })
            });

            if (res.ok) {
                ok = true;
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
                    
                    if (res.status === 402 || (typeof err.detail === 'string' && err.detail.includes("Nicht genügend Tokens"))) {
                        showToast(err.detail || "Nicht genügend Tokens. Bitte lade dein Abo in den Einstellungen auf!");
                        return;
                    }

                    if (err.detail === "NO_API_KEY_FOUND" || (typeof err.detail === 'string' && err.detail.includes("API Key"))) {
                        showToast("Serverfehler: Der AI Key wurde vom Administrator nicht konfiguriert.");
                        return;
                    }
                    errorMsg = typeof err.detail === 'object' ? JSON.stringify(err.detail) : (err.detail || errorMsg);
                } catch {
                    // Response not JSON — use status code
                }
                showToast(`Fehler: ${errorMsg}`);
            }
        } catch (error) {
            console.error(error);
            showToast("Ein Fehler ist aufgetreten.");
        } finally {
            stateSetter((prev) => prev.filter((t) => t !== endpoint));
            registerAiJobFinished(endpoint, ok);
        }
    };

    const handleGenerate = async (type: 'plan' | 'quiz' | 'flashcards' | 'summary' | 'elaboration' | 'repetition') => {
        if (type === 'plan') {
            setIsPlanConfigOpen(true);
            return;
        }

        if (type === 'summary') {
            setIsSummaryConfigOpen(true);
            return;
        }

        if (type === 'elaboration') {
            setIsElaborationConfigOpen(true);
            return;
        }

        if (type === 'repetition') {
            setIsRepetitionConfigOpen(true);
            return;
        }

        await handleAIAction(type, setIsGenerating);
    };

    const handleSummaryGenerate = async () => {
        setIsSummaryConfigOpen(false);
        setIsGenerating((prev) => (prev.includes('summary') ? prev : [...prev, 'summary']));
        let ok = false;
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
                    detail_level: summaryDetailLevel,
                    model_preference: aiModelPreference || undefined,
                    learning_mode: learningMode
                })
            });

            if (res.ok) {
                ok = true;
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
                } catch { /* not JSON */ }
                showToast(`Zusammenfassungs-Fehler: ${errorMsg}`);
            }
        } catch (error) {
            console.error(error);
            showToast("Ein Fehler ist aufgetreten.");
        } finally {
            setIsGenerating((prev) => prev.filter((t) => t !== 'summary'));
            registerAiJobFinished('summary', ok);
        }
    };

    const handleElaborationGenerate = async () => {
        setIsElaborationConfigOpen(false);
        setIsGenerating((prev) => (prev.includes('elaboration') ? prev : [...prev, 'elaboration']));
        let ok = false;
        try {
            const username = localStorage.getItem("username");
            const res = await fetch(`${API_BASE}/ai/elaboration`, {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify({
                    username,
                    folder_id: folderId,
                    detail_level: elaborationDetailLevel,
                    custom_rules: elaborationRules,
                    model_preference: aiModelPreference || undefined
                })
            });

            if (res.ok) {
                ok = true;
                const data = await res.json();
                setSelectedFile({
                    id: `elaboration_${Date.now()}`,
                    name: 'Ausarbeitung',
                    type: 'summary', // Using 'summary' to inherit Rich Text Editor styling
                    created_at: new Date().toISOString().split('T')[0],
                    content: data.elaboration
                });
                fetchFiles();
            } else {
                let errorMsg = `HTTP ${res.status}`;
                try {
                    const err = await res.json();
                    if (err.detail === "NO_API_KEY_FOUND" || (typeof err.detail === 'string' && err.detail.includes("API Key"))) {
                        const goToSettings = confirm("⚠️ Kein AI Key gefunden!\n\nJetzt zu den Einstellungen?");
                        if (goToSettings) router.push("/settings");
                        return;
                    }
                    errorMsg = err.detail || errorMsg;
                } catch { /* ignore */ }
                showToast(`Ausarbeitungs-Fehler: ${errorMsg}`);
            }
        } catch (error) {
            console.error(error);
            showToast("Ein Fehler ist aufgetreten.");
        } finally {
            setIsGenerating((prev) => prev.filter((t) => t !== 'elaboration'));
            registerAiJobFinished('elaboration', ok);
        }
    };

    const handleRepetitionGenerate = async () => {
        setIsRepetitionConfigOpen(false);
        setIsGenerating((prev) => (prev.includes('repetition') ? prev : [...prev, 'repetition']));
        let ok = false;
        try {
            const username = localStorage.getItem("username");
            const res = await fetch(`${API_BASE}/ai/repetition`, {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify({
                    username,
                    folder_id: folderId,
                    custom_rules: repetitionRules,
                    model_preference: aiModelPreference || undefined,
                    learning_mode: learningMode
                })
            });

            if (res.ok) {
                ok = true;
                const data = await res.json();
                setSelectedFile({
                    id: `repetition_${Date.now()}`,
                    name: 'Wiederholungsbogen',
                    type: 'repetition',
                    created_at: new Date().toISOString().split('T')[0],
                    content: data.repetition
                });
                fetchFiles();
            } else {
                let errorMsg = `HTTP ${res.status}`;
                try {
                    const err = await res.json();
                    if (err.detail === "NO_API_KEY_FOUND" || (typeof err.detail === 'string' && err.detail.includes("API Key"))) {
                        const goToSettings = confirm("⚠️ Kein AI Key gefunden!\n\nJetzt zu den Einstellungen?");
                        if (goToSettings) router.push("/settings");
                        return;
                    }
                    errorMsg = err.detail || errorMsg;
                } catch { /* ignore */ }
                showToast(`Fehler: ${errorMsg}`);
            }
        } catch (error) {
            console.error(error);
            showToast("Ein Fehler ist aufgetreten.");
        } finally {
            setIsGenerating((prev) => prev.filter((t) => t !== 'repetition'));
            registerAiJobFinished('repetition', ok);
        }
    };

    const handlePlanGenerate = async () => {
        setIsPlanConfigOpen(false);
        setIsGenerating((prev) => (prev.includes('plan') ? prev : [...prev, 'plan']));
        let ok = false;
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
                    hours_per_day: planHoursPerDay,
                    active_days: planActiveDays.length < 7 ? planActiveDays : undefined,
                    model_preference: aiModelPreference || undefined,
                    learning_mode: learningMode
                })
            });

            if (res.ok) {
                const data = await res.json();
                if (data.plan && data.plan.length > 0) {
                    ok = true;
                    setSelectedFile({
                        id: 'plan_main',
                        name: 'Aktueller Lernplan',
                        type: 'plan',
                        created_at: new Date().toISOString().split('T')[0],
                        content: data.plan
                    });
                } else {
                    showToast("Lernplan wurde generiert, ist aber leer. Bitte prüfe ob Material im Ordner vorhanden ist.");
                }
                fetchFiles();
            } else {
                let errorMsg = `HTTP ${res.status}`;
                try {
                    const err = await res.json();
                    errorMsg = typeof err.detail === 'object' ? JSON.stringify(err.detail) : (err.detail || errorMsg);
                } catch { /* ignore */ }
                showToast(`Lernplan-Fehler: ${errorMsg}`);
            }
        } catch (error) {
            console.error("Plan error:", error);
            showToast(`Netzwerk-Fehler: ${String(error)}`);
        } finally {
            setIsGenerating((prev) => prev.filter((t) => t !== 'plan'));
            registerAiJobFinished('plan', ok);
        }
    };

    const handleTaskHelp = async (taskDescription: string) => {
        setActiveTaskTitle(taskDescription.substring(0, 60) + (taskDescription.length > 60 ? '...' : ''));
        setTaskHelpContent('');
        setIsTaskHelpOpen(true);
        setIsTaskHelpLoading(true);

        try {
            const username = localStorage.getItem("username");
            const res = await fetch(`${API_BASE}/ai/task-help`, {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify({
                    username,
                    folder_id: folderId,
                    task_description: taskDescription,
                    model_preference: aiModelPreference || undefined
                })
            });

            if (res.ok) {
                const data = await res.json();
                setTaskHelpContent(data.help_text || "Keine Erklärung generiert.");
            } else {
                let errorMsg = `HTTP ${res.status}`;
                try {
                    const err = await res.json();
                    errorMsg = err.detail || errorMsg;
                } catch { /* ignore */ }
                setTaskHelpContent(`Fehler beim Laden der Hilfe: ${errorMsg}`);
            }
        } catch (error) {
            console.error("Task Help Error:", error);
            setTaskHelpContent("Netzwerkfehler beim Laden der Hilfe.");
        } finally {
            setIsTaskHelpLoading(false);
        }
    };
    const handleRenameFile = async () => {
        if (!renameFileValue.trim() || !fileToRename) return;

        setIsRenamingFile(true);
        try {
            const username = localStorage.getItem("username");
            const res = await fetch(`${API_BASE}/files/${folderId}/${fileToRename.id}/rename`, {
                method: "PUT",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify({ name: renameFileValue, username }),
            });

            if (res.ok) {
                setIsRenameFileOpen(false);
                setFileToRename(null);
                setRenameFileValue("");
                fetchFiles();
            } else {
                const err = await res.json();
                showToast(`Fehler beim Umbenennen: ${err.detail || 'Unbekannt'}`);
            }
        } catch (error) {
            console.error("Error renaming file:", error);
            showToast("Netzwerkfehler beim Umbenennen.");
        } finally {
            setIsRenamingFile(false);
        }
    };

    const handleDeleteFile = async (file: FileData) => {
        if (!confirm(`"${file.name}" wirklich löschen?`)) return;
        const username = localStorage.getItem('username');
        const res = await fetch(`${API_BASE}/files/${file.id}?username=${username}&folder_id=${folderId}`, { method: 'DELETE' });
        if (res.ok) fetchFiles();
        else showToast('Fehler beim Löschen.');
    };

    // Drag & Drop Handlers
    const handleDragStart = (event: DragStartEvent) => {
        const { active } = event;
        const file = active.data.current?.file as FileData;
        if (file) {
            setActiveDragFile(file);
        }
    };

    const handleDragEnd = async (event: DragEndEvent) => {
        setActiveDragFile(null);
        const { active, over } = event;

        if (!over) return;

        const sourceFile = active.data.current?.file as FileData;
        const targetSubfolder = over.data.current?.subfolder;

        if (!sourceFile || !targetSubfolder) return;

        // Call backend to move file
        try {
            const username = localStorage.getItem("username");
            const res = await fetch(`${API_BASE}/files/${folderId}/${sourceFile.id}/move`, {
                method: "PUT",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify({ target_folder_id: targetSubfolder.id, username }),
            });

            if (res.ok) {
                fetchFiles(); // Refresh file / subfolder lists
            } else {
                const err = await res.json();
                showToast(`Fehler beim Verschieben: ${err.detail || 'Unbekannt'}`);
            }
        } catch (error) {
            console.error("Error moving file:", error);
            showToast("Netzwerkfehler beim Verschieben der Datei.");
        }
    };

    const persistFileContent = async (fileId: string, newContent: string) => {
        const username = localStorage.getItem("username");
        const res = await fetch(`${API_BASE}/files/update`, {
            method: 'PUT',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                username,
                folder_id: folderId,
                file_id: fileId,
                content: newContent
            })
        });
        if (!res.ok) {
            let detail = 'Unbekannt';
            try {
                const err = await res.json();
                detail = err.detail || detail;
            } catch {
                // ignored
            }
            throw new Error(`Fehler beim Speichern: ${detail}`);
        }
    };

    const applyPatchToContent = (baseContent: string, patch: DocumentPatch): string => {
        if (patch.apply_mode === 'append') {
            return `${baseContent.trimEnd()}\n\n${patch.new_text.trim()}\n`;
        }
        const hint = (patch.section_hint || '').trim();
        if (!hint) {
            return `${baseContent.trimEnd()}\n\n${patch.new_text.trim()}\n`;
        }
        const escapedHint = hint.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
        const sectionRegex = new RegExp(`(${escapedHint}[\\s\\S]*?)(\\n##\\s|\\n#\\s|$)`, 'm');
        const match = baseContent.match(sectionRegex);
        if (!match) {
            return `${baseContent.trimEnd()}\n\n${patch.new_text.trim()}\n`;
        }
        const replacement = `${hint}\n\n${patch.new_text.trim()}\n\n`;
        return baseContent.replace(sectionRegex, `${replacement}$2`);
    };

    const handleApplyChatPatch = async (patch: DocumentPatch) => {
        if (!selectedFile || typeof selectedFile.content !== 'string') {
            throw new Error("Kein bearbeitbares Textdokument geöffnet.");
        }
        const currentContent = selectedFile.content;
        const nextContent = applyPatchToContent(currentContent, patch);
        setDocumentUndoStack((prev) => {
            const stack = prev[selectedFile.id] || [];
            return {
                ...prev,
                [selectedFile.id]: [...stack, currentContent].slice(-20),
            };
        });
        await persistFileContent(selectedFile.id, nextContent);
        const updatedFiles = files.map(f => f.id === selectedFile.id ? { ...f, content: nextContent } : f);
        setFiles(updatedFiles);
        setSelectedFile({ ...selectedFile, content: nextContent });
        showToast("Änderung angewendet.", "success");
    };

    const handleUndoLastPatch = async () => {
        if (!selectedFile || typeof selectedFile.content !== 'string') {
            throw new Error("Kein bearbeitbares Textdokument geöffnet.");
        }
        const stack = documentUndoStack[selectedFile.id] || [];
        if (stack.length === 0) {
            throw new Error("Keine Änderung zum Rückgängig machen vorhanden.");
        }
        const previousContent = stack[stack.length - 1];
        await persistFileContent(selectedFile.id, previousContent);
        setDocumentUndoStack((prev) => ({
            ...prev,
            [selectedFile.id]: (prev[selectedFile.id] || []).slice(0, -1),
        }));
        const updatedFiles = files.map(f => f.id === selectedFile.id ? { ...f, content: previousContent } : f);
        setFiles(updatedFiles);
        setSelectedFile({ ...selectedFile, content: previousContent });
        showToast("Letzte Änderung rückgängig gemacht.", "info");
    };

    const handleRefineFile = (file: FileData) => {
        setFileToRefine(file);
        setRefinePrompt('');
        setRefineSaveMode('new_file');
        setIsRefineModalOpen(true);
    };

    const submitRefineFile = async () => {
        if (!fileToRefine || !refinePrompt.trim()) return;
        setIsRefining(true);
        try {
            const username = localStorage.getItem('username') || '';
            const res = await fetch(`${API_BASE}/ai/elaboration/refine`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    username,
                    folder_id: folderId,
                    file_id: fileToRefine.id,
                    prompt: refinePrompt.trim(),
                    save_mode: refineSaveMode,
                    detail_level: "Sehr detailliert",
                }),
            });
            const data = await res.json();
            if (!res.ok) {
                throw new Error(data.detail || 'Anpassung fehlgeschlagen');
            }
            setIsRefineModalOpen(false);
            setFileToRefine(null);
            setRefinePrompt('');
            await fetchFiles();
            showToast(refineSaveMode === 'replace' ? 'Datei wurde angepasst.' : 'Neue überarbeitete Datei wurde erstellt.', 'success');
        } catch (e: any) {
            showToast(e?.message || 'Fehler bei der Anpassung.');
        } finally {
            setIsRefining(false);
        }
    };

    // --- VIEWER COMPONENTS ---
    const renderViewer = () => {
        if (!selectedFile) return null;

        if (selectedFile.type === 'summary' || selectedFile.type === 'repetition' || selectedFile.type === 'elaboration' || selectedFile.type === 'transcript') {
            const contentStr = typeof selectedFile.content === 'string' ? selectedFile.content : JSON.stringify(selectedFile.content || '');
            const isHtml = /<p>|<h[1-6]>|<ul>|<ol>|<blockquote|<img/i.test(contentStr);
            const ytId = youtubeVideoIdFromTranscriptFile(selectedFile);

            if (isEditingFile || isHtml) {
                return (
                    <RichTextEditor
                        initialContent={contentStr}
                        title={selectedFile.name}
                        youtubeVideoId={selectedFile.type === 'transcript' ? ytId : null}
                        onClose={() => { setSelectedFile(null); setIsEditingFile(false); }}
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
                                    setIsEditingFile(false);
                                } else {
                                    const err = await res.json();
                                    showToast(`Fehler beim Speichern: ${err.detail || 'Unbekannt'}`);
                                }
                            } catch {
                                showToast("Netzwerkfehler beim Speichern.");
                            }
                        }}
                    />
                );
            }

            return (
                <div className="print-friendly-viewer fixed inset-0 z-[100] bg-[#0B0B1A] flex flex-col w-screen h-screen overflow-hidden animate-in fade-in slide-in-from-bottom-4 duration-300">
                    <div className="flex items-center justify-between p-4 border-b border-[#2A2A40] bg-[#0B0B1A] sticky top-0 z-10 w-full">
                        <div className="flex items-center gap-3">
                            <button onClick={() => { setSelectedFile(null); setIsEditingFile(false); }} className="p-2 text-gray-400 hover:text-white hover:bg-[#1C1C33] rounded-xl transition-colors">
                                <X size={20} />
                            </button>
                            <div className="h-6 w-px bg-[#1C1C33] mx-2"></div>
                            <div className={`p-2 rounded-lg bg-blue-500/10 text-blue-400`}>
                                <FileText size={20} />
                            </div>
                            <h3 className="text-lg font-semibold text-white">{selectedFile.name}</h3>
                        </div>
                        <div className="flex items-center gap-2 no-print">
                            <button onClick={() => setIsEditingFile(true)} className="flex items-center gap-2 bg-[#1C1C33] hover:bg-[#2A2A40] border border-[#333] text-white px-3 py-1.5 rounded-lg text-sm font-semibold transition-colors">
                                <Edit size={14} />
                                <span className="hidden sm:inline">Bearbeiten</span>
                            </button>
                            {selectedFile.type === 'transcript' && ytId && (
                                <button
                                    type="button"
                                    onClick={() => openYoutubeVideoInNewTab(ytId)}
                                    className="flex items-center gap-2 bg-[#1C1C33] hover:bg-[#2A2A40] border border-[#333] text-white px-3 py-1.5 rounded-lg text-sm font-semibold transition-colors"
                                    title="Video auf YouTube im Browser öffnen"
                                >
                                    <ExternalLink size={14} />
                                    <span className="hidden sm:inline">YouTube</span>
                                </button>
                            )}
                            {selectedFile.type === 'transcript' && /<!--\s*AUDIO_FILE:/.test(contentStr) && (
                                <button
                                    onClick={async () => {
                                        const username = localStorage.getItem("username") || "";
                                        const match = contentStr.match(/<!-- AUDIO_FILE:(.*?) -->/);
                                        if (match && match[1]) {
                                            const filename = match[1];
                                            const url = `${API_BASE}/files/download_audio?username=${username}&folder_id=${folderId}&filename=${filename}`;
                                            const a = document.createElement('a');
                                            a.href = url;
                                            a.download = filename;
                                            document.body.appendChild(a);
                                            a.click();
                                            a.remove();
                                        } else {
                                            alert("Keine Audio-Datei für diesen Eintrag gefunden.");
                                        }
                                    }}
                                    className="flex items-center gap-2 bg-[#1C1C33] hover:bg-[#2A2A40] border border-[#333] text-white px-3 py-1.5 rounded-lg text-sm font-semibold transition-colors"
                                    title="Original-Audio (MP3) herunterladen"
                                >
                                    <Download size={14} />
                                    <span className="hidden sm:inline">MP3 Download</span>
                                </button>
                            )}
                            <button
                                onClick={() => window.print()}
                                className="flex items-center gap-2 bg-[#1C1C33] hover:bg-[#2A2A40] border border-[#333] text-white px-3 py-1.5 rounded-lg text-sm font-semibold transition-colors"
                                title="Als PDF exportieren"
                            >
                                <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><polyline points="6 9 6 2 18 2 18 9"></polyline><path d="M6 18H4a2 2 0 0 1-2-2v-5a2 2 0 0 1 2-2h16a2 2 0 0 1 2 2v5a2 2 0 0 1-2 2h-2"></path><rect x="6" y="14" width="12" height="8"></rect></svg>
                                <span className="hidden sm:inline">PDF Export</span>
                            </button>
                        </div>
                    </div>
                    <div className="flex-1 overflow-y-auto bg-[#1a1a1a] p-8">
                        <div className="max-w-4xl mx-auto prose prose-invert prose-blop prose-pre:bg-[#151525] prose-pre:border prose-pre:border-[#2A2A40] [&_.katex-display]:max-w-full">
                            {ytId ? (
                                <div className="not-prose mb-8 aspect-video w-full max-h-[min(50vh,420px)] rounded-xl overflow-hidden border border-[#2A2A40] bg-black shadow-lg">
                                    <iframe
                                        title="YouTube"
                                        className="h-full w-full"
                                        src={`https://www.youtube-nocookie.com/embed/${ytId}`}
                                        allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture; web-share"
                                        allowFullScreen
                                    />
                                </div>
                            ) : null}
                            <ReactMarkdown
                                remarkPlugins={[remarkGfm, remarkMath]}
                                rehypePlugins={[[rehypeKatex, { throwOnError: false, strict: false }]]}
                            >
                                {contentStr}
                            </ReactMarkdown>
                        </div>
                    </div>
                </div>
            );
        }

        if (selectedFile.type === 'plan') {
            const plan = (Array.isArray(selectedFile.content) ? selectedFile.content : []) as PlanDayRow[];
            if (!Array.isArray(plan) || plan.length === 0) {
                return (
                    <div className="fixed inset-0 z-[100] bg-[#0B0B1A] flex flex-col items-center justify-center p-4">
                        <div className="text-center">
                            <p className="text-gray-400 text-lg mb-2">⚠️ Lernplan ist leer</p>
                            <button onClick={() => setSelectedFile(null)} className="mt-4 px-4 py-2 bg-[#1C1C33] hover:bg-[#3B3B55] rounded-lg text-white">Zurück</button>
                        </div>
                    </div>
                );
            }

            return (
                <div className="print-friendly-viewer fixed inset-0 z-[100] bg-[#0B0B1A] flex flex-col w-screen h-screen overflow-hidden animate-in fade-in slide-in-from-bottom-4 duration-300">
                    <div className="flex items-center justify-between p-4 border-b border-[#2A2A40] bg-[#0B0B1A] sticky top-0 z-10 w-full">
                        <div className="flex items-center gap-3">
                            <button onClick={() => setSelectedFile(null)} className="p-2 text-gray-400 hover:text-white hover:bg-[#1C1C33] rounded-xl transition-colors">
                                <X size={20} />
                            </button>
                            <div className="h-6 w-px bg-[#1C1C33] mx-2"></div>
                            <div className="p-2 rounded-lg bg-purple-500/10 text-purple-400">
                                <Calendar size={20} />
                            </div>
                            <h3 className="text-lg font-semibold text-white">{selectedFile.name || 'Persönlicher Lernplan'}</h3>
                        </div>
                        <div className="flex items-center gap-2 no-print">
                            <button
                                onClick={() => window.print()}
                                className="flex items-center gap-2 bg-[#1C1C33] hover:bg-[#2A2A40] border border-[#333] text-white px-3 py-1.5 rounded-lg text-sm font-semibold transition-colors"
                                title="Lernplan als PDF drucken"
                            >
                                <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><polyline points="6 9 6 2 18 2 18 9"></polyline><path d="M6 18H4a2 2 0 0 1-2-2v-5a2 2 0 0 1 2-2h16a2 2 0 0 1 2 2v5a2 2 0 0 1-2 2h-2"></path><rect x="6" y="14" width="12" height="8"></rect></svg>
                                <span className="hidden sm:inline">PDF Export</span>
                            </button>
                        </div>
                    </div>

                    <div className="flex-1 overflow-y-auto bg-[#1a1a1a] p-6">
                        <div className="max-w-4xl mx-auto space-y-4">
                            {plan.map((day: PlanDayRow, i: number) => {
                                const isExpanded = expandedDay === i;

                                return (
                                    <div key={i} className={`bg-[#151525] rounded-2xl border transition-all duration-200 overflow-hidden ${isExpanded ? 'border-purple-500/50 shadow-lg shadow-purple-500/5' : 'border-[#2A2A40] hover:border-[#3B3B55]'}`}>
                                        <button
                                            onClick={() => setExpandedDay(isExpanded ? null : i)}
                                            className="w-full text-left p-5 flex items-center justify-between focus:outline-none group"
                                        >
                                            <div className="flex items-center gap-4">
                                                <div className={`w-10 h-10 rounded-xl flex items-center justify-center font-bold text-sm transition-colors ${isExpanded ? 'bg-purple-500 text-white' : 'bg-[#1C1C33] text-gray-400 group-hover:bg-[#3B3B55]'}`}>
                                                    {day.day}
                                                </div>
                                                <div>
                                                    <h3 className={`text-lg transition-colors font-semibold ${isExpanded ? 'text-white' : 'text-gray-300 group-hover:text-white'}`}>{day.topic}</h3>
                                                    {!isExpanded && <p className="text-sm text-gray-500 truncate max-w-md">{day.goal}</p>}
                                                </div>
                                            </div>
                                            <div className={`p-2 rounded-full transition-transform duration-200 ${isExpanded ? 'rotate-180 bg-purple-500/10 text-purple-400' : 'bg-[#0B0B1A] text-gray-500 group-hover:text-white'}`}>
                                                <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><polyline points="6 9 12 15 18 9"></polyline></svg>
                                            </div>
                                        </button>

                                        <div className={`transition-all duration-300 ease-in-out ${isExpanded ? 'max-h-[1000px] opacity-100' : 'max-h-0 opacity-0'}`}>
                                            <div className="p-5 pt-0 ml-14 space-y-4">
                                                <div className="text-sm text-gray-300 bg-[#0B0B1A] p-4 rounded-xl border border-[#2A2A40] shadow-inner">
                                                    <div className="flex items-center gap-2 mb-2">
                                                        <span className="text-purple-400 font-semibold text-xs uppercase tracking-wider">🎯 Tagesziel</span>
                                                    </div>
                                                    <p>{day.goal}</p>
                                                    {day.summary && (
                                                        <div className="mt-3 pt-3 border-t border-[#2A2A40]">
                                                            <span className="text-blue-400 font-semibold text-xs uppercase tracking-wider mb-2 block">📚 Zusammenfassung (Lernstoff)</span>
                                                            <p className="text-gray-300 leading-relaxed">{day.summary}</p>
                                                        </div>
                                                    )}
                                                </div>

                                                <div className="bg-[#0B0B1A] p-4 rounded-xl border border-[#2A2A40]">
                                                    <div className="flex items-center gap-2 mb-3">
                                                        <span className="text-gray-400 font-semibold text-xs uppercase tracking-wider">📝 Aufgaben</span>
                                                    </div>
                                                    <ul className="space-y-3">
                                                        {day.tasks?.map((task: PlanTask, idx: number) => {
                                                            const isCompleted = typeof task === 'object' && task !== null ? Boolean((task as { completed?: boolean }).completed) : false;
                                                            const description = typeof task === 'object' && task !== null ? String((task as { description?: string }).description ?? '') : String(task);
                                                            return (
                                                                <li key={idx} className="flex items-start gap-3 group/task">
                                                                    <button
                                                                        onClick={async (e) => {
                                                                            e.stopPropagation();
                                                                            // Only allow if it's the new object format
                                                                            if (typeof task !== 'object') return;

                                                                            // Deep clone the plan to avoid mutating state directly in a bad way
                                                                            const newPlan = JSON.parse(JSON.stringify(plan));
                                                                            newPlan[i].tasks[idx].completed = !isCompleted;

                                                                            // Optimistically update UI
                                                                            setSelectedFile({ ...selectedFile, content: newPlan });

                                                                            // Update Backend
                                                                            try {
                                                                                const username = localStorage.getItem("username");
                                                                                await fetch(`${API_BASE}/files/update`, {
                                                                                    method: 'PUT',
                                                                                    headers: { 'Content-Type': 'application/json' },
                                                                                    body: JSON.stringify({
                                                                                        username,
                                                                                        folder_id: folderId,
                                                                                        file_id: selectedFile.id,
                                                                                        content: newPlan
                                                                                    })
                                                                                });
                                                                                // Update main files state quietly
                                                                                setFiles(files.map(f => f.id === selectedFile.id ? { ...f, content: newPlan } : f));
                                                                            } catch (e) {
                                                                                console.error("Failed to save checkbox state", e);
                                                                            }
                                                                        }}
                                                                        className={`mt-0.5 w-5 h-5 rounded-md flex items-center justify-center shrink-0 transition-colors focus:outline-none focus:ring-2 focus:ring-purple-500/50 ${isCompleted ? 'bg-purple-500 border-purple-500 text-white' : 'border-2 border-[#3B3B55] group-hover/task:border-purple-500 text-transparent hover:bg-white/5'
                                                                            }`}
                                                                    >
                                                                        <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="3" strokeLinecap="round" strokeLinejoin="round"><polyline points="20 6 9 17 4 12"></polyline></svg>
                                                                    </button>

                                                                    <button
                                                                        onClick={() => handleTaskHelp(description)}
                                                                        className={`flex-1 text-left text-sm transition-colors focus:outline-none focus:text-white rounded px-2 -ml-2 py-0.5 hover:bg-[#1C1C33] ${isCompleted ? 'text-gray-500 line-through' : 'text-gray-300 group-hover/task:text-white'
                                                                            }`}
                                                                        title="Klicke für KI-Erklärung dieser Aufgabe"
                                                                    >
                                                                        {description}
                                                                    </button>
                                                                </li>
                                                            );
                                                        })}
                                                    </ul>
                                                </div>

                                                <div className="grid grid-cols-1 md:grid-cols-2 gap-4 mt-4">
                                                    {day.focus && (
                                                        <div className="text-sm bg-amber-500/10 border border-amber-500/20 p-4 rounded-xl">
                                                            <div className="text-amber-500 font-semibold text-xs uppercase tracking-wider mb-2">⚡ Fokus</div>
                                                            <div className="text-amber-200/90">{day.focus}</div>
                                                        </div>
                                                    )}
                                                </div>
                                            </div>
                                        </div>
                                    </div>
                                );
                            })}
                        </div>
                        {/* Generate Quiz Shortcut */}
                        <div className="max-w-4xl mx-auto mt-8 flex justify-center pb-12">
                            <button
                                disabled={isGenerating.includes('quiz')}
                                onClick={() => {
                                    // Make sure we close the selected file so they can see the generation
                                    setSelectedFile(null);
                                    handleGenerate('quiz');
                                }}
                                className="bg-gradient-to-r from-orange-500 to-amber-500 hover:from-orange-600 hover:to-amber-600 disabled:opacity-50 disabled:pointer-events-none text-white font-medium py-3 px-6 rounded-xl shadow-lg transition-all flex items-center justify-center gap-2 group"
                            >
                                <BrainCircuit className="group-hover:rotate-12 transition-transform duration-300" size={18} />
                                Quiz aus Lernplan generieren
                            </button>
                        </div>
                    </div>
                </div>
            );
        }

        const contentStrGeneric =
            typeof selectedFile.content === "string"
                ? selectedFile.content
                : JSON.stringify(selectedFile.content ?? "");
        const ytIdGeneric = youtubeVideoIdFromTranscriptFile(selectedFile);

        return (
            <div className="print-friendly-viewer fixed inset-0 z-[100] bg-[#0B0B1A] flex flex-col w-screen h-screen overflow-hidden animate-in fade-in slide-in-from-bottom-4 duration-300">
                <div className="flex items-center justify-between p-4 border-b border-[#2A2A40] bg-[#0B0B1A] sticky top-0 z-10 w-full">
                    <div className="flex items-center gap-3">
                        <button onClick={() => setSelectedFile(null)} className="p-2 text-gray-400 hover:text-white hover:bg-[#1C1C33] rounded-xl transition-colors">
                            <X size={20} />
                        </button>
                        <div className="h-6 w-px bg-[#1C1C33] mx-2"></div>
                        <div className={`p-2 rounded-lg ${selectedFile.type === 'quiz' ? 'bg-orange-500/10 text-orange-400' :
                            selectedFile.type === 'flashcards' ? 'bg-green-500/10 text-green-400' :
                                selectedFile.type === 'summary' ? 'bg-blue-500/10 text-blue-400' :
                                    'bg-[#1C1C33] text-gray-400'
                            }`}>
                            {getFileIcon(selectedFile.type)}
                        </div>
                        <h3 className="text-lg font-semibold text-white">{selectedFile.name}</h3>
                    </div>
                    <div className="flex items-center gap-2 no-print">
                        {selectedFile.type === 'transcript' && ytIdGeneric && (
                            <button
                                type="button"
                                onClick={() => openYoutubeVideoInNewTab(ytIdGeneric)}
                                className="flex items-center gap-2 bg-[#1C1C33] hover:bg-[#2A2A40] border border-[#333] text-white px-3 py-1.5 rounded-lg text-sm font-semibold transition-colors h-full"
                                title="Video auf YouTube im Browser öffnen"
                            >
                                <ExternalLink size={14} />
                                <span className="hidden sm:inline">YouTube</span>
                            </button>
                        )}
                        {selectedFile.type === 'transcript' && /<!--\s*AUDIO_FILE:/.test(contentStrGeneric) && (
                            <button
                                onClick={async () => {
                                    const username = localStorage.getItem("username") || "";
                                    const match = contentStrGeneric.match(/<!-- AUDIO_FILE:(.*?) -->/);
                                    if (match && match[1]) {
                                        const filename = match[1];
                                        const url = `${API_BASE}/files/download_audio?username=${username}&folder_id=${folderId}&filename=${filename}`;
                                        const a = document.createElement('a');
                                        a.href = url;
                                        a.download = filename;
                                        document.body.appendChild(a);
                                        a.click();
                                        a.remove();
                                    } else {
                                        alert("Keine Audio-Datei für diesen Eintrag gefunden.");
                                    }
                                }}
                                className="flex items-center gap-2 bg-[#1C1C33] hover:bg-[#2A2A40] border border-[#333] text-white px-3 py-1.5 rounded-lg text-sm font-semibold transition-colors h-full"
                                title="Original-Audio (MP3) herunterladen"
                            >
                                <Download size={14} />
                                <span className="hidden sm:inline">MP3 Download</span>
                            </button>
                        )}
                        <button
                            onClick={() => window.print()}
                            className="flex items-center gap-2 bg-[#1C1C33] hover:bg-[#2A2A40] border border-[#333] text-white px-3 py-1.5 rounded-lg text-sm font-semibold transition-colors"
                            title="Als PDF exportieren"
                        >
                            <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><polyline points="6 9 6 2 18 2 18 9"></polyline><path d="M6 18H4a2 2 0 0 1-2-2v-5a2 2 0 0 1 2-2h16a2 2 0 0 1 2 2v5a2 2 0 0 1-2 2h-2"></path><rect x="6" y="14" width="12" height="8"></rect></svg>
                            <span className="hidden sm:inline">PDF Export</span>
                        </button>
                    </div>
                </div>

                <div className="flex-1 overflow-y-auto bg-[#1a1a1a] p-6 text-gray-300">
                    {renderFileContent(selectedFile)}
                </div>
            </div>
        );
    };

    const renderFileContent = (file: FileData) => {
        if (file.type === 'quiz') {
            const questions = file.content;
            if (!Array.isArray(questions)) return <p>Fehlerhaftes Quiz-Format.</p>;
            return <QuizViewer questions={questions as QuizQuestion[]} />;
        }
        if (file.type === 'flashcards') {
            const cards = file.content;
            if (!Array.isArray(cards)) return <p>Fehlerhaftes Format.</p>;
            const cardRows = cards as FlashcardRow[];
            return (
                <div className="flex flex-col h-full">
                    <div className="flex justify-end mb-4">
                        <button
                            onClick={() => {
                                const csvContent = "data:text/csv;charset=utf-8,"
                                    + cardRows.map(c => `"${c.front.replace(/"/g, '""')}","${c.back.replace(/"/g, '""')}"`).join("\n");
                                const encodedUri = encodeURI(csvContent);
                                const link = document.createElement("a");
                                link.setAttribute("href", encodedUri);
                                link.setAttribute("download", `${file.name || 'Flashcards'}.csv`);
                                document.body.appendChild(link);
                                link.click();
                                document.body.removeChild(link);
                            }}
                            className="bg-[#151525] hover:bg-[#1C1C33] border border-[#3B3B55] text-white px-4 py-2 rounded-xl text-sm font-medium transition-colors flex items-center gap-2"
                        >
                            <svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4" /><polyline points="7 10 12 15 17 10" /><line x1="12" x2="12" y1="15" y2="3" /></svg>
                            Export to Anki (CSV)
                        </button>
                    </div>
                    <div className="grid grid-cols-1 md:grid-cols-2 gap-4 pb-12">
                        {cardRows.map((c: FlashcardRow, i: number) => {
                            // Using a data attribute or simple react state pattern isn't possible directly inside map without a hook.
                            // We will use inline script-like behavior by toggling a class on click for the flip.
                            return (
                                <div
                                    key={i}
                                    className="group relative h-48 w-full [perspective:1000px] cursor-pointer"
                                    onClick={(e) => {
                                        const card = e.currentTarget;
                                        const inner = card.querySelector('.flip-card-inner');
                                        if (inner) {
                                            inner.classList.toggle('[transform:rotateY(180deg)]');
                                        }
                                    }}
                                >
                                    <div className="absolute top-3 right-3 z-10 opacity-0 group-hover:opacity-100 transition-opacity">
                                        <button
                                            onClick={(e) => {
                                                e.stopPropagation();
                                                setFullscreenCard(c);
                                                setIsFullscreenCardFlipped(false);
                                            }}
                                            className="p-1.5 bg-black/40 hover:bg-black/80 rounded-lg text-gray-300 hover:text-white transition-colors backdrop-blur-sm"
                                            title="Vollbild"
                                        >
                                            <Maximize2 size={16} />
                                        </button>
                                    </div>
                                    <div className="flip-card-inner w-full h-full transition-all duration-500 [transform-style:preserve-3d] relative rounded-xl shadow-lg border border-[#2A2A40] bg-[#151525]">
                                        {/* Front Face */}
                                        <div className="absolute inset-0 h-full w-full rounded-xl [backface-visibility:hidden] flex flex-col items-center justify-center p-6 pb-8 text-center text-white font-medium text-[15px] sm:text-base leading-relaxed break-words whitespace-pre-wrap overflow-y-auto custom-scrollbar">
                                            {c.front}
                                            <div className="absolute bottom-3 right-3 text-xs text-gray-500 flex items-center gap-1"><svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2"><path d="m17 2 4 4-4 4" /><path d="M3 11v-1a4 4 0 0 1 4-4h14" /><path d="m7 22-4-4 4-4" /><path d="M21 13v1a4 4 0 0 1-4 4H3" /></svg> Klick zum Drehen</div>
                                        </div>

                                        {/* Back Face */}
                                        <div className="absolute inset-0 h-full w-full rounded-xl [backface-visibility:hidden] [transform:rotateY(180deg)] flex flex-col items-center justify-center p-6 pb-8 text-center text-gray-300 bg-[#0B0B1A] text-[15px] sm:text-base leading-relaxed break-words whitespace-pre-wrap overflow-y-auto custom-scrollbar">
                                            {c.back}
                                        </div>
                                    </div>
                                </div>
                            );
                        })}
                    </div>
                </div>
            );
        }
        if (file.type === 'repetition') {
            // Note: Repetition is now handled by the generic RichTextEditor block at the top of renderViewer
            return null;
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
        <div className="bg-[#0B0B1A] min-h-screen">
            {/* Global Toast */}
            {toast && (
                <div className={`fixed top-4 right-4 z-[9999] p-4 rounded-xl shadow-2xl flex items-center gap-3 animate-in fade-in slide-in-from-top-2 border ${toast.type === 'error' ? 'bg-red-500/10 border-red-500/20 text-red-500' :
                    toast.type === 'success' ? 'bg-green-500/10 border-green-500/20 text-green-400' :
                        'bg-blue-500/10 border-blue-500/20 text-blue-400'
                    }`}>
                    {toast.type === 'error' ? <X size={20} /> : <HelpCircle size={20} />}
                    <span className="font-medium whitespace-pre-wrap max-w-sm">{toast.msg}</span>
                    <button onClick={() => setToast(null)} className="ml-2 hover:opacity-75">
                        <X size={16} />
                    </button>
                </div>
            )}

            <div className="max-w-6xl mx-auto px-6 py-10">

                {/* Header */}
                <div className="flex flex-col md:flex-row md:items-center justify-between gap-6 mb-8">
                    <div className="flex items-center gap-4">
                        <button onClick={() => router.back()} className="p-2 hover:bg-[#1C1C33] rounded-xl text-gray-400 hover:text-white transition-colors"><ArrowLeft size={22} /></button>
                        <div>
                            <h1 className="text-2xl font-bold text-white">Ordner Details</h1>
                            <p className="text-sm text-gray-400">ID: {folderId}</p>
                        </div>
                    </div>

                    <div className="flex flex-wrap gap-2">
                        {/* AI Actions */}
                        <div className="flex gap-2 mr-2 border-r border-[#2A2A40] pr-4">
                            <button onClick={() => handleGenerate('plan')} disabled={isGenerating.includes('plan')} className="p-2.5 bg-purple-500/10 text-purple-400 hover:bg-purple-500/20 rounded-xl transition-all disabled:opacity-50" title="Lernplan erstellen">
                                {isGenerating.includes('plan') ? <Loader2 size={20} className="animate-spin" /> : <BrainCircuit size={20} />}
                            </button>
                            <button onClick={() => handleGenerate('quiz')} disabled={isGenerating.includes('quiz')} className="p-2.5 bg-yellow-500/10 text-yellow-500 hover:bg-yellow-500/20 rounded-xl transition-all disabled:opacity-50" title="Quiz erstellen">
                                {isGenerating.includes('quiz') ? <Loader2 size={20} className="animate-spin" /> : <HelpCircle size={20} />}
                            </button>
                            <button onClick={() => handleGenerate('flashcards')} disabled={isGenerating.includes('flashcards')} className="p-2.5 bg-green-500/10 text-green-400 hover:bg-green-500/20 rounded-xl transition-all disabled:opacity-50" title="Karteikarten erstellen">
                                {isGenerating.includes('flashcards') ? <Loader2 size={20} className="animate-spin" /> : <Layers size={20} />}
                            </button>
                            <button onClick={() => handleGenerate('summary')} disabled={isGenerating.includes('summary')} className="p-2.5 bg-blue-500/10 text-blue-400 hover:bg-blue-500/20 rounded-xl transition-all disabled:opacity-50" title="Zusammenfassung erstellen">
                                {isGenerating.includes('summary') ? <Loader2 size={20} className="animate-spin" /> : <FileOutput size={20} />}
                            </button>
                            <button onClick={() => handleGenerate('elaboration')} disabled={isGenerating.includes('elaboration')} className="p-2.5 bg-orange-500/10 text-orange-400 hover:bg-orange-500/20 rounded-xl transition-all disabled:opacity-50" title="Ausarbeitung erstellen">
                                {isGenerating.includes('elaboration') ? <Loader2 size={20} className="animate-spin" /> : <FileText size={20} />}
                            </button>
                            <button onClick={() => handleGenerate('repetition')} disabled={isGenerating.includes('repetition')} className="p-2.5 bg-teal-500/10 text-teal-400 hover:bg-teal-500/20 rounded-xl transition-all disabled:opacity-50" title="Wiederholung erstellen">
                                {isGenerating.includes('repetition') ? <Loader2 size={20} className="animate-spin" /> : <Repeat size={20} />}
                            </button>
                        </div>

                        <div className="flex gap-2">
                            <button
                                onClick={() => {
                                    setSelectedFile({
                                        id: `draft_${Date.now()}`,
                                        name: "Neues Dokument",
                                        type: "summary",
                                        created_at: new Date().toISOString().split('T')[0],
                                        content: ""
                                    });
                                }}
                                className="flex items-center gap-2 bg-[#252526] hover:bg-[#333] border border-[#2A2A40] text-white px-4 py-2.5 rounded-xl text-sm font-semibold transition-all"
                            >
                                <Edit size={18} />
                                <span className="hidden sm:inline">Neues Dokument</span>
                            </button>
                            <button
                                onClick={() => setIsUploadOpen(true)}
                                className="flex items-center gap-2 bg-[#151525] text-white px-4 py-2.5 rounded-xl text-sm font-semibold border border-[#2A2A40] hover:bg-[#1C1C33] transition-all"
                            >
                                <Plus size={18} />
                                <span>Material</span>
                            </button>
                        </div>
                    </div>
                </div>

                {/* Content */}
                {loading ? (
                    <div className="flex justify-center py-20">
                        <Loader2 className="animate-spin text-[#5E5CE6]" size={32} />
                    </div>
                ) : (
                    <DndContext
                        sensors={sensors}
                        onDragStart={handleDragStart}
                        onDragEnd={handleDragEnd}
                        collisionDetection={closestCenter}
                    >
                        <div className="space-y-6">
                            {/* Subfolders section */}
                            {(subfolders.length > 0 || true) && (
                                <div>
                                    <div className="flex items-center justify-between mb-3">
                                        <h3 className="text-sm font-medium text-gray-400 uppercase tracking-wider flex items-center gap-2">
                                            <svg xmlns="http://www.w3.org/2000/svg" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><path d="M22 19a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h5l2 3h9a2 2 0 0 1 2 2z" /></svg>
                                            Unterordner
                                        </h3>
                                        <button
                                            onClick={() => setIsCreateSubfolderOpen(true)}
                                            className="flex items-center gap-1 text-xs text-gray-400 hover:text-white bg-[#151525] hover:bg-[#1C1C33] px-3 py-1.5 rounded-lg border border-[#2A2A40] transition-colors"
                                        >
                                            <Plus size={13} /> Erstellen
                                        </button>
                                    </div>
                                    {subfolders.length === 0 ? (
                                        <p className="text-xs text-gray-600 italic">Noch keine Unterordner</p>
                                    ) : (
                                        <div className="grid grid-cols-2 sm:grid-cols-3 gap-2">
                                            <AnimatePresence mode="popLayout">
                                                {subfolders.map((sf) => (
                                                    <DroppableSubfolder key={sf.id} subfolder={sf} onClick={() => router.push(`/folder/${sf.id}`)} />
                                                ))}
                                            </AnimatePresence>
                                        </div>
                                    )}
                                </div>
                            )}

                            {/* Separator if both exist */}
                            {subfolders.length > 0 && files.length > 0 && (
                                <div className="h-px bg-[#1C1C33]" />
                            )}

                            {/* Files */}
                            {files.length === 0 ? (
                                <div className="bg-[#151525] border border-[#2A2A40] rounded-2xl p-16 text-center border-dashed">
                                    <div className="flex justify-center mb-6">
                                        <div className="w-16 h-16 bg-[#0B0B1A] rounded-2xl flex items-center justify-center border border-[#2A2A40]">
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
                                    <AnimatePresence mode="popLayout">
                                        {files.map((file) => (
                                            <DraggableFile
                                                key={file.id}
                                                file={file}
                                                icon={getFileIcon(file.type)}
                                                openMenuFileId={openMenuFileId}
                                                setOpenMenuFileId={setOpenMenuFileId}
                                                setSelectedFile={setSelectedFile}
                                                setFileToRename={setFileToRename}
                                                setRenameFileValue={setRenameFileValue}
                                                setIsRenameFileOpen={setIsRenameFileOpen}
                                                handleDelete={handleDeleteFile}
                                                onRefineFile={handleRefineFile}
                                            />
                                        ))}
                                    </AnimatePresence>
                                </div>
                            )}
                        </div>

                        <DragOverlay dropAnimation={{ duration: 250, easing: 'cubic-bezier(0.18, 0.67, 0.6, 1.22)' }}>
                            {activeDragFile ? (
                                <div className="bg-[#1C1C33] border border-[#5E5CE6] p-4 rounded-xl shadow-2xl flex items-center gap-4 opacity-90 scale-105 pointer-events-none">
                                    <div className={`p-3 rounded-lg ${activeDragFile.type === 'plan' ? 'bg-purple-500/10 text-purple-400' :
                                        activeDragFile.type === 'quiz' ? 'bg-orange-500/10 text-orange-400' :
                                            activeDragFile.type === 'flashcards' ? 'bg-green-500/10 text-green-400' :
                                                activeDragFile.type === 'summary' ? 'bg-blue-500/10 text-blue-400' :
                                                    activeDragFile.type === 'transcript' ? 'bg-red-500/10 text-red-500' :
                                                        'bg-[#1C1C33] text-[#5E5CE6]'
                                        }`}>
                                        {getFileIcon(activeDragFile.type)}
                                    </div>
                                    <div>
                                        <h4 className="text-sm font-medium text-white">{activeDragFile.name}</h4>
                                        <p className="text-xs text-gray-500 capitalize">{activeDragFile.type}</p>
                                    </div>
                                </div>
                            ) : null}
                        </DragOverlay>

                    </DndContext>
                )}

                {/* Upload Modal */}
                {isUploadOpen && (
                    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/60 backdrop-blur-sm p-4">
                        <div className="bg-[#0B0B1A] border border-[#2A2A40] rounded-2xl w-full max-w-3xl p-6 shadow-2xl animate-in fade-in zoom-in duration-200">
                            <div className="flex justify-between items-center mb-6">
                                <h3 className="text-lg font-semibold text-white">Material hinzufügen</h3>
                                <button onClick={() => setIsUploadOpen(false)} className="text-gray-400 hover:text-white"><X size={20} /></button>
                            </div>
                            <div className="flex flex-wrap gap-2 p-1 bg-[#151525] rounded-xl mb-6">
                                <button onClick={() => setUploadType('pdf')} className={`flex-1 min-w-[120px] flex items-center justify-center gap-2 py-2 text-sm font-medium rounded-lg transition-all ${uploadType === 'pdf' ? 'bg-[#1C1C33] text-white shadow-sm' : 'text-gray-400 hover:text-gray-200'}`}><Upload size={16} /> PDF Upload</button>
                                <button onClick={() => setUploadType('image')} className={`flex-1 min-w-[100px] flex items-center justify-center gap-2 py-2 text-sm font-medium rounded-lg transition-all ${uploadType === 'image' ? 'bg-[#1C1C33] text-green-400 shadow-sm' : 'text-gray-400 hover:text-gray-200'}`}><ImageIcon size={16} /> Bild</button>
                                <button onClick={() => setUploadType('youtube')} className={`flex-1 min-w-[110px] flex items-center justify-center gap-2 py-2 text-sm font-medium rounded-lg transition-all ${uploadType === 'youtube' ? 'bg-[#1C1C33] text-red-400 shadow-sm' : 'text-gray-400 hover:text-gray-200'}`}><Youtube size={16} /> YouTube</button>
                                <button onClick={() => setUploadType('audio')} className={`flex-1 min-w-[100px] flex items-center justify-center gap-2 py-2 text-sm font-medium rounded-lg transition-all ${uploadType === 'audio' ? 'bg-[#1C1C33] text-blue-400 shadow-sm' : 'text-gray-400 hover:text-gray-200'}`}>
                                    <svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><path d="M12 2a3 3 0 0 0-3 3v7a3 3 0 0 0 6 0V5a3 3 0 0 0-3-3Z" /><path d="M19 10v2a7 7 0 0 1-14 0v-2" /><line x1="12" x2="12" y1="19" y2="22" /></svg> Audio
                                </button>
                                <button onClick={() => setUploadType('document')} className={`flex-1 min-w-[100px] flex items-center justify-center gap-2 py-2 text-sm font-medium rounded-lg transition-all ${uploadType === 'document' ? 'bg-[#1C1C33] text-orange-400 shadow-sm' : 'text-gray-400 hover:text-gray-200'}`} title="Leeres Dokument erstellen">
                                    <FileText size={16} /> Text
                                </button>
                            </div>
                            {uploadType === 'pdf' ? (
                                <form onSubmit={handleFileUpload} className="space-y-4">
                                    <div className="border-2 border-dashed border-[#2A2A40] rounded-xl p-8 text-center hover:border-[#5E5CE6] transition-colors cursor-pointer relative">
                                        <input type="file" accept=".pdf,audio/*" multiple onChange={(e) => setFilesToUpload(Array.from(e.target.files || []))} className="absolute inset-0 w-full h-full opacity-0 cursor-pointer" />
                                        <div className="flex flex-col items-center gap-2 text-gray-400">
                                            <Upload size={24} />
                                            <span className="text-sm">
                                                {filesToUpload.length > 0 
                                                    ? `${filesToUpload.length} Datei(en) ausgewählt` 
                                                    : "Klicken zum Auswählen (mehrere möglich)"}
                                            </span>
                                        </div>
                                    </div>
                                    <button type="submit" disabled={filesToUpload.length === 0 || isProcessing} className="w-full bg-[#5E5CE6] hover:bg-[#4d4ac9] text-white py-2.5 rounded-xl text-sm font-medium transition-all disabled:opacity-50 flex justify-center items-center gap-2">{isProcessing && <Loader2 size={16} className="animate-spin" />} Hochladen</button>
                                </form>
                            ) : uploadType === 'image' ? (
                                <form onSubmit={handleImageUpload} className="space-y-4">
                                    <div className="border-2 border-dashed border-[#2A2A40] rounded-xl p-8 text-center hover:border-green-500 transition-colors cursor-pointer relative">
                                        <input type="file" accept=".jpg,.jpeg,.png,.webp" multiple onChange={(e) => setFilesToUpload(Array.from(e.target.files || []))} className="absolute inset-0 w-full h-full opacity-0 cursor-pointer" />
                                        <div className="flex flex-col items-center gap-2 text-gray-400">
                                            <ImageIcon size={24} />
                                            <span className="text-sm">
                                                {filesToUpload.length > 0 
                                                    ? `${filesToUpload.length} Bild(er) ausgewählt` 
                                                    : "Klicken für Bildauswahl (mehrere möglich)"}
                                            </span>
                                        </div>
                                    </div>
                                    <button type="submit" disabled={filesToUpload.length === 0 || isProcessing} className="w-full bg-green-600 hover:bg-green-700 text-white py-2.5 rounded-xl text-sm font-medium transition-all disabled:opacity-50 flex justify-center items-center gap-2">{isProcessing && <Loader2 size={16} className="animate-spin" />} Bilder analysieren</button>
                                </form>
                            ) : uploadType === 'youtube' ? (
                                <form onSubmit={handleYoutubeImport} className="space-y-4">
                                    <div><label className="block text-xs font-medium text-gray-400 mb-1.5 ml-1">YouTube URL</label><input type="url" placeholder="https://youtube.com/watch?v=..." value={youtubeUrl} onChange={(e) => setYoutubeUrl(e.target.value)} className="w-full bg-[#151525] border border-[#2A2A40] text-white rounded-xl px-4 py-2.5 focus:ring-2 focus:ring-red-500/50 focus:border-red-500 outline-none placeholder:text-gray-600 transition-all font-sans" /></div>
                                    <button type="submit" disabled={!youtubeUrl || isProcessing} className="w-full bg-red-600 hover:bg-red-700 text-white py-2.5 rounded-xl text-sm font-medium transition-all disabled:opacity-50 flex justify-center items-center gap-2">{isProcessing && <Loader2 size={16} className="animate-spin" />} Importieren</button>
                                </form>
                            ) : uploadType === 'document' ? (
                                <form onSubmit={handleDocumentCreate} className="space-y-4">
                                    <div>
                                        <label className="block text-xs font-medium text-gray-400 mb-1.5 ml-1">Dokumenten-Titel</label>
                                        <input id="new-document-title" type="text" placeholder="Mein neues Dokument..." className="w-full bg-[#151525] border border-[#2A2A40] text-white rounded-xl px-4 py-2.5 focus:ring-2 focus:ring-orange-500/50 focus:border-orange-500 outline-none placeholder:text-gray-600 transition-all font-sans" required />
                                    </div>
                                    <button type="submit" disabled={isProcessing} className="w-full bg-orange-600 hover:bg-orange-700 text-white py-2.5 rounded-xl text-sm font-medium transition-all disabled:opacity-50 flex justify-center items-center gap-2">
                                        {isProcessing && <Loader2 size={16} className="animate-spin" />} Dokument erstellen
                                    </button>
                                </form>
                            ) : (
                                <div className="space-y-6">
                                    <div className="flex flex-col items-center justify-center p-8 bg-[#151525] border border-[#2A2A40] rounded-xl relative">
                                        {!audioBlob ? (
                                            <>
                                                {/* Hidden File Input for Audio */}
                                                <input
                                                    type="file"
                                                    accept="audio/webm,audio/mpeg,audio/wav,audio/mp4,audio/m4a"
                                                    onChange={(e) => {
                                                        const file = e.target.files?.[0];
                                                        if (file) setAudioBlob(file);
                                                    }}
                                                    className="absolute inset-0 w-full h-full opacity-0 cursor-pointer z-10"
                                                    title="Klicken, um eine Audio-Datei hochzuladen"
                                                />
                                                <button
                                                    onClick={isRecording ? stopRecording : startRecording}
                                                    className={`w-20 h-20 rounded-full flex items-center justify-center mb-4 transition-all z-20 relative ${isRecording ? 'bg-red-500/20 text-red-500 border-4 border-red-500 animate-pulse' : 'bg-[#1C1C33] text-gray-400 hover:bg-[#3B3B55] hover:text-white'}`}
                                                    title="Aufnahme starten/stoppen"
                                                >
                                                    {isRecording ? <div className="w-6 h-6 bg-red-500 rounded-sm"></div> : <svg xmlns="http://www.w3.org/2000/svg" width="32" height="32" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><path d="M12 2a3 3 0 0 0-3 3v7a3 3 0 0 0 6 0V5a3 3 0 0 0-3-3Z" /><path d="M19 10v2a7 7 0 0 1-14 0v-2" /><line x1="12" x2="12" y1="19" y2="22" /></svg>}
                                                </button>
                                                <div className="text-xl font-mono text-white tracking-widest z-20 relative">
                                                    {Math.floor(recordingTime / 60).toString().padStart(2, '0')}:{(recordingTime % 60).toString().padStart(2, '0')}
                                                </div>
                                                <p className="text-sm text-gray-500 mt-3 text-center z-20 relative pointer-events-none">
                                                    {isRecording ? "Aufnahme läuft..." : "Klicke das Mikrofon zum Aufnehmen\noder ziehe eine Audio-Datei (.mp3, .webm) hierher"}
                                                </p>
                                            </>
                                        ) : (
                                            <div className="z-20 relative w-full flex flex-col items-center">
                                                <div className="w-16 h-16 bg-blue-500/10 text-blue-400 rounded-full flex items-center justify-center mb-4 border border-blue-500/30">
                                                    <svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><path d="M2 10v3" /><path d="M6 6v11" /><path d="M10 3v18" /><path d="M14 8v7" /><path d="M18 5v13" /><path d="M22 10v3" /></svg>
                                                </div>
                                                <h4 className="text-white font-medium mb-1">Audio bereit</h4>
                                                <p className="text-sm text-gray-400 mb-6 truncate max-w-full">
                                                    {audioBlob instanceof File ? audioBlob.name : `Dauer: ${Math.floor(recordingTime / 60)}:${(recordingTime % 60).toString().padStart(2, '0')}`}
                                                </p>
                                                <div className="flex gap-2 w-full mt-2">
                                                    <button onClick={() => { setAudioBlob(null); setRecordingTime(0); }} className="flex-1 px-4 py-2 border border-[#2A2A40] hover:bg-[#1C1C33] text-gray-300 rounded-lg transition-colors text-sm font-medium text-center">Neu starten</button>
                                                    <button onClick={() => {
                                                        if (audioBlob) {
                                                            const url = URL.createObjectURL(audioBlob);
                                                            const a = document.createElement("a");
                                                            a.href = url;
                                                            a.download = `audio_${Date.now()}.webm`;
                                                            a.click();
                                                            URL.revokeObjectURL(url);
                                                        }
                                                    }} className="flex-1 px-4 py-2 border border-[#2A2A40] hover:bg-[#1C1C33] text-blue-400 rounded-lg transition-colors text-sm font-medium flex items-center justify-center gap-2"><Download size={14} /> Speichern</button>
                                                    <button onClick={handleAudioUpload} disabled={isProcessing} className="flex-2 bg-blue-600 hover:bg-blue-700 text-white px-4 py-2 rounded-lg transition-colors text-sm font-medium flex justify-center items-center gap-2 min-w-[40%]">
                                                        {isProcessing ? <><Loader2 size={16} className="animate-spin" /> ...</> : "Als AI Notiz speichern"}
                                                    </button>
                                                </div>
                                            </div>
                                        )}
                                    </div>
                                </div>
                            )}
                        </div>
                    </div>
                )}

                {/* File Viewer Modal */}
                {renderViewer()}

                {/* Plan Config Modal */}
                {isPlanConfigOpen && (
                    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/80 backdrop-blur-sm p-4">
                        <div className="bg-[#0B0B1A] border border-[#2A2A40] rounded-2xl w-full max-w-md shadow-2xl animate-in fade-in zoom-in duration-200">
                            {/* Header */}
                            <div className="flex justify-between items-center p-5 border-b border-[#2A2A40]">
                                <div className="flex items-center gap-3">
                                    <div className="p-2 rounded-xl bg-purple-500/10 text-purple-400">
                                        <BrainCircuit size={20} />
                                    </div>
                                    <div>
                                        <h3 className="text-lg font-semibold text-white">Lernplan erstellen</h3>
                                        <p className="text-xs text-gray-400">Konfiguriere deinen persönlichen Lernplan</p>
                                    </div>
                                </div>
                                <button onClick={() => setIsPlanConfigOpen(false)} className="text-gray-400 hover:text-white p-2 hover:bg-[#1C1C33] rounded-lg transition-colors">
                                    <X size={18} />
                                </button>
                            </div>

                            {/* Body */}
                            <div className="p-5 space-y-5 overflow-y-auto max-h-[70vh]">
                                {/* Learning Mode Toggle */}
                                <div>
                                    <label className="block text-xs font-medium text-gray-400 uppercase tracking-wider mb-2">Modus</label>
                                    <div className="flex gap-2 bg-[#151525] p-1 rounded-xl">
                                        <button
                                            onClick={() => setLearningMode('normal')}
                                            className={`flex-1 py-2 px-3 rounded-lg text-sm font-medium transition-all flex items-center justify-center gap-1.5 ${learningMode === 'normal' ? 'bg-[#5E5CE6] text-white' : 'text-gray-400 hover:text-white'}`}
                                        >
                                            <span>📝</span> Normaler Modus
                                        </button>
                                        <button
                                            onClick={() => setLearningMode('exercise')}
                                            className={`flex-1 py-2 px-3 rounded-lg text-sm font-medium transition-all flex items-center justify-center gap-1.5 ${learningMode === 'exercise' ? 'bg-amber-500 text-white' : 'text-gray-400 hover:text-white'}`}
                                            title="Perfekt wenn du Aufgaben oder Prüfungen hochgeladen hast"
                                        >
                                            <span>🎓</span> Konzepte lernen
                                        </button>
                                    </div>
                                    {learningMode === 'exercise' && (
                                        <p className="text-xs text-amber-400 mt-1.5 flex items-center gap-1"><span>💡</span> AI erklärt die Konzepte dahinter — nicht die Lösungen</p>
                                    )}
                                </div>

                                {/* Duration Mode Toggle */}
                                <div className="flex gap-2 bg-[#151525] p-1 rounded-xl">
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
                                            className="w-full bg-[#151525] border border-[#2A2A40] text-white rounded-xl px-4 py-2.5 focus:ring-2 focus:ring-[#5E5CE6]/50 focus:border-[#5E5CE6] outline-none transition-all"
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

                                {/* Target Grade Slider */}
                                <div className="bg-[#1C1C33]/50 p-4 rounded-xl border border-[#2A2A40]">
                                    <label className="block text-sm font-medium text-gray-200 mb-2 flex justify-between items-center">
                                        <span>Ziel-Note (Punkte)</span>
                                        <div className="flex items-center gap-2">
                                            <span className={`text-lg font-bold ${targetGrade >= 13 ? 'text-green-400' : targetGrade >= 10 ? 'text-blue-400' : targetGrade >= 5 ? 'text-yellow-400' : 'text-red-400'}`}>
                                                {targetGrade}
                                            </span>
                                            <span className="text-xs text-gray-500">Pkt.</span>
                                        </div>
                                    </label>
                                    <input
                                        type="range"
                                        min={1} max={15} step={1}
                                        value={targetGrade}
                                        onChange={e => handleTargetGradeChange(Number(e.target.value))}
                                        className={`w-full ${targetGrade >= 13 ? 'accent-green-500' : targetGrade >= 10 ? 'accent-blue-500' : targetGrade >= 5 ? 'accent-yellow-500' : 'accent-red-500'}`}
                                    />
                                    <div className="flex justify-between text-xs text-gray-500 mt-2">
                                        <span>Ausreichend (5)</span>
                                        <span>Gut (10)</span>
                                        <span>Sehr Gut (15)</span>
                                    </div>
                                    <p className="text-xs text-gray-400 mt-3 flex items-start gap-1.5 leading-relaxed">
                                        <span className="text-[#5E5CE6] mt-0.5"><BrainCircuit size={12} /></span>
                                        Die Lernzeit wird basierend auf deiner Zielnote automatisch empfohlen, kann aber unten noch manuell angepasst werden.
                                    </p>
                                </div>

                                {/* Active Study Days */}
                                <div>
                                    <label className="block text-sm font-medium text-gray-300 mb-2">Aktive Lerntage</label>
                                    <div className="flex justify-between gap-1 sm:gap-2">
                                        {['Mo', 'Di', 'Mi', 'Do', 'Fr', 'Sa', 'So'].map((day, idx) => {
                                            const isActive = planActiveDays.includes(idx);
                                            return (
                                                <button
                                                    key={idx}
                                                    onClick={() => {
                                                        setPlanActiveDays(prev =>
                                                            isActive
                                                                ? prev.filter(d => d !== idx)
                                                                : [...prev, idx].sort()
                                                        );
                                                    }}
                                                    className={`w-10 h-10 rounded-full flex items-center justify-center text-sm font-medium transition-all ${isActive
                                                        ? 'bg-purple-500 text-white shadow-lg shadow-purple-500/20'
                                                        : 'bg-[#151525] border border-[#3B3B55] text-gray-400 hover:border-purple-500/50 hover:text-gray-300'
                                                        }`}
                                                >
                                                    {day}
                                                </button>
                                            );
                                        })}
                                    </div>
                                    {planActiveDays.length === 0 && (
                                        <p className="text-red-400 text-xs mt-2">Bitte mindestens einen Lerntag auswählen</p>
                                    )}
                                </div>

                                {/* Duration Mode Toggle */}
                                <div className="flex gap-2 bg-[#151525] p-1 rounded-xl">
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
                                            className="w-full bg-[#151525] border border-[#2A2A40] text-white rounded-xl px-4 py-2.5 focus:ring-2 focus:ring-[#5E5CE6]/50 focus:border-[#5E5CE6] outline-none transition-all"
                                        />
                                        {planEndDate && (
                                            <p className="text-xs text-gray-400 mt-1.5">
                                                ≈ {Math.max(1, Math.ceil((new Date(planEndDate).getTime() - new Date().setHours(0, 0, 0, 0)) / 86400000))} Tage bis zur Prüfung
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
                                <div className="bg-[#151525] rounded-xl p-4 border border-[#2A2A40]">
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
                                {/* Model Preference */}
                                <div className="pt-4 border-t border-[#2A2A40]">
                                    <label className="block text-sm font-medium text-gray-300 mb-2">Bevorzugtes AI-Modell (Optional)</label>
                                    <select
                                        className="w-full bg-[#151525] border border-[#2A2A40] text-gray-300 rounded-xl px-4 py-2.5 focus:ring-2 focus:ring-[#5E5CE6]/50 focus:border-[#5E5CE6] outline-none transition-all appearance-none"
                                        value={aiModelPreference}
                                        onChange={(e) => setAiModelPreference(e.target.value)}
                                    >
                                        <option value="">🚀 Automatisch (Empfohlen)</option>
                                        <option value="gemini-1.5-flash">⚡ Gemini 1.5 Flash (Schnell & Günstig)</option>
                                        <option value="gemini-1.5-pro">🧠 Gemini 1.5 Pro (Sehr Stark, Höheres Quota)</option>
                                        <option value="gemini-2.0-flash">🔥 Gemini 2.0 Flash</option>
                                        <option value="gemini-pro">🤖 Gemini Pro (Legacy)</option>
                                    </select>
                                    <p className="text-xs text-gray-500 mt-2">Wenn du Fehler wegen &apos;Quota Exceeded&apos; bekommst, wähle hier ein kleineres Modell (z.B. Flash).</p>
                                </div>
                            </div>

                            {/* Footer */}
                            <div className="flex gap-3 p-5 border-t border-[#2A2A40]">
                                <button onClick={() => setIsPlanConfigOpen(false)} className="flex-1 py-2.5 bg-[#151525] hover:bg-[#1C1C33] text-gray-300 rounded-xl text-sm font-medium transition-colors">
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
                        <div className="bg-[#0B0B1A] border border-[#2A2A40] rounded-2xl w-full max-w-md shadow-2xl animate-in fade-in zoom-in duration-200">
                            {/* Header */}
                            <div className="flex justify-between items-center p-5 border-b border-[#2A2A40]">
                                <div className="flex items-center gap-3">
                                    <div className="p-2 rounded-xl bg-blue-500/10 text-blue-400">
                                        <FileOutput size={20} />
                                    </div>
                                    <div>
                                        <h3 className="text-lg font-semibold text-white">Zusammenfassung erstellen</h3>
                                        <p className="text-xs text-gray-400">Wie detailliert soll der Text sein?</p>
                                    </div>
                                </div>
                                <button onClick={() => setIsSummaryConfigOpen(false)} className="text-gray-400 hover:text-white p-2 hover:bg-[#1C1C33] rounded-lg transition-colors">
                                    <X size={18} />
                                </button>
                            </div>

                            {/* Body */}
                            <div className="p-5 space-y-5">
                                {/* Learning Mode Toggle */}
                                <div>
                                    <label className="block text-xs font-medium text-gray-400 uppercase tracking-wider mb-2">Modus</label>
                                    <div className="flex gap-2 bg-[#151525] p-1 rounded-xl">
                                        <button
                                            onClick={() => setLearningMode('normal')}
                                            className={`flex-1 py-2 px-3 rounded-lg text-sm font-medium transition-all flex items-center justify-center gap-1.5 ${learningMode === 'normal' ? 'bg-[#5E5CE6] text-white' : 'text-gray-400 hover:text-white'}`}
                                        >
                                            <span>📝</span> Normaler Modus
                                        </button>
                                        <button
                                            onClick={() => setLearningMode('exercise')}
                                            className={`flex-1 py-2 px-3 rounded-lg text-sm font-medium transition-all flex items-center justify-center gap-1.5 ${learningMode === 'exercise' ? 'bg-amber-500 text-white' : 'text-gray-400 hover:text-white'}`}
                                            title="Perfekt wenn du Aufgaben oder Prüfungen hochgeladen hast"
                                        >
                                            <span>🎓</span> Konzepte lernen
                                        </button>
                                    </div>
                                    {learningMode === 'exercise' && (
                                        <p className="text-xs text-amber-400 mt-1.5"><span>💡</span> AI erklärt die Konzepte dahinter — nicht die Lösungen</p>
                                    )}
                                </div>

                                <div>
                                    <label className="block text-sm font-medium text-gray-300 mb-3">Detailgrad wählen</label>
                                    <div className="grid grid-cols-1 gap-3">
                                        {['Kurz', 'Normal', 'Ausführlich'].map((level) => (
                                            <button
                                                key={level}
                                                onClick={() => setSummaryDetailLevel(level)}
                                                className={`flex items-center justify-between p-4 rounded-xl border transition-all ${summaryDetailLevel === level
                                                    ? 'bg-blue-500/10 border-blue-500/50 text-blue-400'
                                                    : 'bg-[#151525] border-[#2A2A40] text-gray-400 hover:border-[#3B3B55] hover:text-gray-300'}`}
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
                                {/* Model Preference */}
                                <div className="pt-4 border-t border-[#2A2A40]">
                                    <label className="block text-sm font-medium text-gray-300 mb-2">Bevorzugtes AI-Modell (Optional)</label>
                                    <select
                                        className="w-full bg-[#151525] border border-[#2A2A40] text-gray-300 rounded-xl px-4 py-2.5 focus:ring-2 focus:ring-[#5E5CE6]/50 focus:border-[#5E5CE6] outline-none transition-all appearance-none"
                                        value={aiModelPreference}
                                        onChange={(e) => setAiModelPreference(e.target.value)}
                                    >
                                        <option value="">🚀 Automatisch (Empfohlen)</option>
                                        <option value="gemini-1.5-flash">⚡ Gemini 1.5 Flash (Schnell & Günstig)</option>
                                        <option value="gemini-1.5-pro">🧠 Gemini 1.5 Pro (Sehr Stark, Höheres Quota)</option>
                                        <option value="gemini-2.0-flash">🔥 Gemini 2.0 Flash</option>
                                        <option value="gemini-pro">🤖 Gemini Pro (Legacy)</option>
                                    </select>
                                </div>
                            </div>

                            {/* Footer */}
                            <div className="flex gap-3 p-5 border-t border-[#2A2A40]">
                                <button onClick={() => setIsSummaryConfigOpen(false)} className="flex-1 py-2.5 bg-[#151525] hover:bg-[#1C1C33] text-gray-300 rounded-xl text-sm font-medium transition-colors">
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

                {/* Elaboration (Ausarbeitung) Config Modal */}
                {isElaborationConfigOpen && (
                    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/80 backdrop-blur-sm p-4">
                        <div className="bg-[#0B0B1A] border border-[#2A2A40] rounded-2xl w-full max-w-md shadow-2xl animate-in fade-in zoom-in duration-200">
                            {/* Header */}
                            <div className="flex justify-between items-center p-5 border-b border-[#2A2A40]">
                                <div className="flex items-center gap-3">
                                    <div className="p-2 rounded-xl bg-orange-500/10 text-orange-400">
                                        <FileText size={20} />
                                    </div>
                                    <div>
                                        <h3 className="text-lg font-semibold text-white">Ausarbeitung erstellen</h3>
                                        <p className="text-xs text-gray-400">Konfiguriere deinen Text</p>
                                    </div>
                                </div>
                                <button onClick={() => setIsElaborationConfigOpen(false)} className="text-gray-400 hover:text-white p-2 hover:bg-[#1C1C33] rounded-lg transition-colors">
                                    <X size={18} />
                                </button>
                            </div>

                            {/* Body */}
                            <div className="p-5 space-y-5">
                                <div>
                                    <label className="block text-sm font-medium text-gray-300 mb-3">Länge / Detailgrad</label>
                                    <div className="grid grid-cols-1 gap-3">
                                        {['Kurz', 'Normal', 'Ausführlich'].map((level) => (
                                            <button
                                                key={level}
                                                onClick={() => setElaborationDetailLevel(level)}
                                                className={`flex items-center justify-between p-4 rounded-xl border transition-all ${elaborationDetailLevel === level
                                                    ? 'bg-orange-500/10 border-orange-500/50 text-orange-400'
                                                    : 'bg-[#151525] border-[#2A2A40] text-gray-400 hover:border-[#3B3B55] hover:text-gray-300'}`}
                                            >
                                                <span className="font-medium">{level}</span>
                                                {elaborationDetailLevel === level && <div className="w-2 h-2 rounded-full bg-orange-500"></div>}
                                            </button>
                                        ))}
                                    </div>
                                </div>

                                <div>
                                    <label className="block text-sm font-medium text-gray-300 mb-2">Spezifische Anweisungen (Optional)</label>
                                    <textarea
                                        value={elaborationRules}
                                        onChange={(e) => setElaborationRules(e.target.value)}
                                        placeholder="z.B. Fokussiere dich besonders auf Kapitel 3, schreibe im Stil eines Zeitungsartikels, formuliere Thesen..."
                                        className="w-full h-24 bg-[#151525] border border-[#2A2A40] text-white rounded-xl p-3 text-sm focus:ring-2 focus:ring-orange-500/50 focus:border-orange-500 outline-none resize-none placeholder:text-gray-600"
                                    />
                                </div>
                                {/* Model Preference */}
                                <div className="pt-4 border-t border-[#2A2A40]">
                                    <label className="block text-sm font-medium text-gray-300 mb-2">Bevorzugtes AI-Modell (Optional)</label>
                                    <select
                                        className="w-full bg-[#151525] border border-[#2A2A40] text-gray-300 rounded-xl px-4 py-2.5 focus:ring-2 focus:ring-[#5E5CE6]/50 focus:border-[#5E5CE6] outline-none transition-all appearance-none"
                                        value={aiModelPreference}
                                        onChange={(e) => setAiModelPreference(e.target.value)}
                                    >
                                        <option value="">🚀 Automatisch (Empfohlen)</option>
                                        <option value="gemini-1.5-flash">⚡ Gemini 1.5 Flash (Schnell & Günstig)</option>
                                        <option value="gemini-1.5-pro">🧠 Gemini 1.5 Pro (Sehr Stark, Höheres Quota)</option>
                                        <option value="gemini-2.0-flash">🔥 Gemini 2.0 Flash</option>
                                        <option value="gemini-pro">🤖 Gemini Pro (Legacy)</option>
                                    </select>
                                </div>
                            </div>

                            {/* Footer */}
                            <div className="flex gap-3 p-5 border-t border-[#2A2A40]">
                                <button onClick={() => setIsElaborationConfigOpen(false)} className="flex-1 py-2.5 bg-[#151525] hover:bg-[#1C1C33] text-gray-300 rounded-xl text-sm font-medium transition-colors">
                                    Abbrechen
                                </button>
                                <button
                                    onClick={handleElaborationGenerate}
                                    className="flex-1 py-2.5 bg-orange-500 hover:bg-orange-600 text-white rounded-xl text-sm font-semibold transition-colors flex items-center justify-center gap-2"
                                >
                                    <FileText size={16} />
                                    Generieren
                                </button>
                            </div>
                        </div>
                    </div>
                )}

                {/* Repetition (Wiederholung) Config Modal */}
                {isRepetitionConfigOpen && (
                    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/80 backdrop-blur-sm p-4">
                        <div className="bg-[#0B0B1A] border border-[#2A2A40] rounded-2xl w-full max-w-md shadow-2xl animate-in fade-in zoom-in duration-200">
                            {/* Header */}
                            <div className="flex justify-between items-center p-5 border-b border-[#2A2A40]">
                                <div className="flex items-center gap-3">
                                    <div className="p-2 rounded-xl bg-teal-500/10 text-teal-400">
                                        <svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><path d="m17 2 4 4-4 4" /><path d="M3 11v-1a4 4 0 0 1 4-4h14" /><path d="m7 22-4-4 4-4" /><path d="M21 13v1a4 4 0 0 1-4 4H3" /></svg>
                                    </div>
                                    <div>
                                        <h3 className="text-lg font-semibold text-white">Wiederholung erstellen</h3>
                                        <p className="text-xs text-gray-400">Ausführliche Prüfungs- oder Themenwiederholung</p>
                                    </div>
                                </div>
                                <button onClick={() => setIsRepetitionConfigOpen(false)} className="text-gray-400 hover:text-white p-2 hover:bg-[#1C1C33] rounded-lg transition-colors">
                                    <X size={18} />
                                </button>
                            </div>

                            {/* Body */}
                            <div className="p-5 space-y-4">
                                {/* Learning Mode Toggle */}
                                <div>
                                    <label className="block text-xs font-medium text-gray-400 uppercase tracking-wider mb-2">Modus</label>
                                    <div className="flex gap-2 bg-[#151525] p-1 rounded-xl">
                                        <button
                                            onClick={() => setLearningMode('normal')}
                                            className={`flex-1 py-2 px-3 rounded-lg text-sm font-medium transition-all flex items-center justify-center gap-1.5 ${learningMode === 'normal' ? 'bg-[#5E5CE6] text-white' : 'text-gray-400 hover:text-white'}`}
                                        >
                                            <span>📝</span> Normaler Modus
                                        </button>
                                        <button
                                            onClick={() => setLearningMode('exercise')}
                                            className={`flex-1 py-2 px-3 rounded-lg text-sm font-medium transition-all flex items-center justify-center gap-1.5 ${learningMode === 'exercise' ? 'bg-amber-500 text-white' : 'text-gray-400 hover:text-white'}`}
                                            title="Perfekt wenn du Aufgaben oder Prüfungen hochgeladen hast"
                                        >
                                            <span>🎓</span> Konzepte lernen
                                        </button>
                                    </div>
                                    {learningMode === 'exercise' && (
                                        <p className="text-xs text-amber-400 mt-1.5"><span>💡</span> Fokus auf Konzepte und Lösungsmethoden — trotzdem alle Aufgabenbereiche aus dem Material abdecken</p>
                                    )}
                                    {learningMode === 'normal' && (
                                        <p className="text-xs text-gray-500 mt-1.5"><span>💡</span> Ohne Schwerpunkte: die KI geht die gesamte Prüfung bzw. alle Aufgaben der Reihe nach durch (Musterlösungsniveau).</p>
                                    )}
                                </div>

                                <label className="block text-sm font-medium text-gray-300 mb-2">Schwerpunkte setzen (Optional)</label>
                                <textarea
                                    value={repetitionRules}
                                    onChange={(e) => setRepetitionRules(e.target.value)}
                                    placeholder="z. B. „nur Vektorrechnung“ oder „Aufgabe 3 überspringen“. Leer lassen = gesamtes Material, jede Teilaufgabe ausführlich."
                                    className="w-full h-28 bg-[#151525] border border-[#2A2A40] text-white rounded-xl p-3 text-sm focus:ring-2 focus:ring-teal-500/50 focus:border-teal-500 outline-none resize-none placeholder:text-gray-600"
                                />

                                <p className="text-xs text-gray-500 mt-3 flex items-center gap-1.5 mb-4">
                                    <HelpCircle size={14} /> Tipp: Nutze Active Recall, um dir Wissen nachhaltig einzuprägen.
                                </p>

                                {/* Model Preference */}
                                <div className="pt-4 border-t border-[#2A2A40]">
                                    <label className="block text-sm font-medium text-gray-300 mb-2">Bevorzugtes AI-Modell (Optional)</label>
                                    <select
                                        className="w-full bg-[#151525] border border-[#2A2A40] text-gray-300 rounded-xl px-4 py-2.5 focus:ring-2 focus:ring-[#5E5CE6]/50 focus:border-[#5E5CE6] outline-none transition-all appearance-none"
                                        value={aiModelPreference}
                                        onChange={(e) => setAiModelPreference(e.target.value)}
                                    >
                                        <option value="">🚀 Automatisch (Empfohlen)</option>
                                        <option value="gemini-1.5-flash">⚡ Gemini 1.5 Flash (Schnell & Günstig)</option>
                                        <option value="gemini-1.5-pro">🧠 Gemini 1.5 Pro (Sehr Stark, Höheres Quota)</option>
                                        <option value="gemini-2.0-flash">🔥 Gemini 2.0 Flash</option>
                                        <option value="gemini-pro">🤖 Gemini Pro (Legacy)</option>
                                    </select>
                                </div>
                            </div>

                            {/* Footer */}
                            <div className="flex gap-3 p-5 border-t border-[#2A2A40]">
                                <button onClick={() => setIsRepetitionConfigOpen(false)} className="flex-1 py-2.5 bg-[#151525] hover:bg-[#1C1C33] text-gray-300 rounded-xl text-sm font-medium transition-colors">
                                    Abbrechen
                                </button>
                                <button
                                    onClick={handleRepetitionGenerate}
                                    className="flex-1 py-2.5 bg-teal-500 hover:bg-teal-600 text-white rounded-xl text-sm font-semibold transition-colors flex items-center justify-center gap-2"
                                >
                                    <svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><path d="m17 2 4 4-4 4" /><path d="M3 11v-1a4 4 0 0 1 4-4h14" /><path d="m7 22-4-4 4-4" /><path d="M21 13v1a4 4 0 0 1-4 4H3" /></svg>
                                    Los geht&apos;s
                                </button>
                            </div>
                        </div>
                    </div >
                )
                }

                {/* Task Help Overlay */}
                {isTaskHelpOpen && (
                    <div className="fixed inset-0 z-[110] flex items-center justify-center bg-black/80 backdrop-blur-sm p-4 animate-in fade-in duration-200">
                        <div className="bg-[#0B0B1A] border border-[#2A2A40] rounded-2xl w-full max-w-lg shadow-2xl flex flex-col max-h-[85vh]">
                            {/* Header */}
                            <div className="flex justify-between items-center p-5 border-b border-[#2A2A40] shrink-0">
                                <div className="flex items-center gap-3">
                                    <div className="p-2 rounded-xl bg-purple-500/10 text-purple-400">
                                        <BrainCircuit size={20} />
                                    </div>
                                    <div className="max-w-[300px] overflow-hidden">
                                        <h3 className="text-lg font-semibold text-white truncate" title={activeTaskTitle}>{activeTaskTitle}</h3>
                                        <p className="text-xs text-purple-400 font-medium">KI-Lernassistenz</p>
                                    </div>
                                </div>
                                <button onClick={() => setIsTaskHelpOpen(false)} className="text-gray-400 hover:text-white p-2 hover:bg-[#1C1C33] rounded-lg transition-colors">
                                    <X size={18} />
                                </button>
                            </div>

                            {/* Body */}
                            <div className="p-6 overflow-y-auto">
                                {isTaskHelpLoading ? (
                                    <div className="flex flex-col items-center justify-center py-12 text-center">
                                        <div className="w-12 h-12 border-4 border-[#2A2A40] border-t-purple-500 rounded-full animate-spin mb-4"></div>
                                        <p className="text-gray-300 font-medium">Analysiere Ordner-Inhalte...</p>
                                        <p className="text-sm text-gray-500 mt-2">Ich überlege, was du genau tun sollst.</p>
                                    </div>
                                ) : (
                                    <div className="prose prose-invert prose-purple max-w-none prose-sm sm:prose-base">
                                        <ReactMarkdown remarkPlugins={[remarkGfm]}>
                                            {taskHelpContent}
                                        </ReactMarkdown>
                                    </div>
                                )}
                            </div>

                            {/* Footer */}
                            {!isTaskHelpLoading && (
                                <div className="p-5 border-t border-[#2A2A40] shrink-0 bg-[#151525] rounded-b-2xl">
                                    <button
                                        onClick={() => setIsTaskHelpOpen(false)}
                                        className="w-full py-2.5 bg-[#1C1C33] hover:bg-[#3B3B55] text-white rounded-xl font-medium transition-colors"
                                    >
                                        Verstanden!
                                    </button>
                                </div>
                            )}
                        </div>
                    </div>
                )}

                {/* Fullscreen Flashcard Modal */}
                {fullscreenCard && (
                    <div className="fixed inset-0 z-[120] flex items-center justify-center bg-black/90 backdrop-blur-md p-4 animate-in fade-in duration-200">
                        <button
                            onClick={() => setFullscreenCard(null)}
                            className="absolute top-6 right-6 text-gray-400 hover:text-white p-3 hover:bg-[#1C1C33] rounded-full transition-colors z-10"
                        >
                            <X size={24} />
                        </button>

                        <div className="w-full max-w-4xl h-[70vh] [perspective:1500px] cursor-pointer" onClick={() => setIsFullscreenCardFlipped(!isFullscreenCardFlipped)}>
                            <div className={`w-full h-full transition-all duration-700 [transform-style:preserve-3d] relative rounded-3xl shadow-2xl border border-[#3B3B55] ${isFullscreenCardFlipped ? '[transform:rotateY(180deg)]' : ''}`}>
                                {/* Front Face */}
                                <div className="absolute inset-0 h-full w-full rounded-3xl [backface-visibility:hidden] bg-[#151525] flex flex-col items-center justify-center p-12 text-center text-white font-medium text-2xl md:text-4xl leading-relaxed break-words whitespace-pre-wrap overflow-y-auto custom-scrollbar">
                                    {fullscreenCard.front}
                                    <div className="absolute bottom-6 text-sm text-gray-500 flex items-center gap-2"><svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2"><path d="m17 2 4 4-4 4" /><path d="M3 11v-1a4 4 0 0 1 4-4h14" /><path d="m7 22-4-4 4-4" /><path d="M21 13v1a4 4 0 0 1-4 4H3" /></svg> Klick zum Drehen</div>
                                </div>

                                {/* Back Face */}
                                <div className="absolute inset-0 h-full w-full rounded-3xl [backface-visibility:hidden] [transform:rotateY(180deg)] bg-[#0B0B1A] flex flex-col items-center justify-center p-12 text-center text-gray-300 text-2xl md:text-4xl leading-relaxed break-words whitespace-pre-wrap overflow-y-auto custom-scrollbar">
                                    {fullscreenCard.back}
                                </div>
                            </div>
                        </div>
                    </div>
                )}

                {/* Rename File Modal */}
                {isRenameFileOpen && fileToRename && (
                    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/80 backdrop-blur-sm p-4">
                        <div className="bg-[#0B0B1A] border border-[#2A2A40] rounded-2xl w-full max-w-sm shadow-2xl animate-in fade-in zoom-in duration-200">
                            <div className="flex justify-between items-center p-5 border-b border-[#2A2A40]">
                                <div className="flex items-center gap-3">
                                    <div className="p-2 rounded-xl bg-[#5E5CE6]/10 text-[#5E5CE6]">
                                        <Edit size={18} />
                                    </div>
                                    <h3 className="text-base font-semibold text-white">Datei umbenennen</h3>
                                </div>
                                <button onClick={() => { setIsRenameFileOpen(false); setFileToRename(null); }} className="text-gray-400 hover:text-white p-2 hover:bg-[#1C1C33] rounded-lg">
                                    <X size={18} />
                                </button>
                            </div>
                            <div className="p-5">
                                <input
                                    type="text"
                                    placeholder="Neuer Name..."
                                    value={renameFileValue}
                                    onChange={(e) => setRenameFileValue(e.target.value)}
                                    onKeyDown={(e) => { if (e.key === 'Enter') document.getElementById('rename-file-btn')?.click(); }}
                                    className="w-full bg-[#151525] border border-[#2A2A40] text-white rounded-xl px-4 py-2.5 outline-none focus:ring-2 focus:ring-[#5E5CE6]/50 focus:border-[#5E5CE6] transition-all text-sm placeholder:text-gray-600"
                                    autoFocus
                                />
                            </div>
                            <div className="flex gap-3 p-5 border-t border-[#2A2A40]">
                                <button onClick={() => { setIsRenameFileOpen(false); setFileToRename(null); }} className="flex-1 py-2.5 bg-[#151525] hover:bg-[#1C1C33] text-gray-300 rounded-xl text-sm font-medium transition-colors">
                                    Abbrechen
                                </button>
                                <button
                                    id="rename-file-btn"
                                    disabled={!renameFileValue.trim() || isRenamingFile || renameFileValue === fileToRename.name}
                                    onClick={() => void handleRenameFile()}
                                    className="flex-1 py-2.5 bg-[#5E5CE6] hover:bg-[#4d4ac9] text-white rounded-xl text-sm font-semibold transition-colors disabled:opacity-50"
                                >
                                    {isRenamingFile ? <Loader2 size={16} className="animate-spin mx-auto" /> : "Speichern"}
                                </button>
                            </div>
                        </div>
                    </div>
                )}

                {/* Create Subfolder Modal */}
                {isCreateSubfolderOpen && (
                    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/80 backdrop-blur-sm p-4">
                        <div className="bg-[#0B0B1A] border border-[#2A2A40] rounded-2xl w-full max-w-sm shadow-2xl animate-in fade-in zoom-in duration-200">
                            <div className="flex justify-between items-center p-5 border-b border-[#2A2A40]">
                                <div className="flex items-center gap-3">
                                    <div className="p-2 rounded-xl bg-[#5E5CE6]/10 text-[#5E5CE6]">
                                        <svg xmlns="http://www.w3.org/2000/svg" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><path d="M22 19a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h5l2 3h9a2 2 0 0 1 2 2z" /></svg>
                                    </div>
                                    <h3 className="text-base font-semibold text-white">Unterordner erstellen</h3>
                                </div>
                                <button onClick={() => setIsCreateSubfolderOpen(false)} className="text-gray-400 hover:text-white p-2 hover:bg-[#1C1C33] rounded-lg">
                                    <X size={18} />
                                </button>
                            </div>
                            <div className="p-5">
                                <input
                                    type="text"
                                    placeholder="Name des Unterordners..."
                                    value={newSubfolderName}
                                    onChange={(e) => setNewSubfolderName(e.target.value)}
                                    onKeyDown={(e) => { if (e.key === 'Enter') document.getElementById('create-subfolder-btn')?.click(); }}
                                    className="w-full bg-[#151525] border border-[#2A2A40] text-white rounded-xl px-4 py-2.5 outline-none focus:ring-2 focus:ring-[#5E5CE6]/50 focus:border-[#5E5CE6] transition-all text-sm placeholder:text-gray-600"
                                    autoFocus
                                />
                            </div>
                            <div className="flex gap-3 p-5 border-t border-[#2A2A40]">
                                <button onClick={() => setIsCreateSubfolderOpen(false)} className="flex-1 py-2.5 bg-[#151525] hover:bg-[#1C1C33] text-gray-300 rounded-xl text-sm font-medium transition-colors">
                                    Abbrechen
                                </button>
                                <button
                                    id="create-subfolder-btn"
                                    disabled={!newSubfolderName.trim()}
                                    onClick={async () => {
                                        if (!newSubfolderName.trim()) return;
                                        const username = localStorage.getItem('username');
                                        const res = await fetch(`${API_BASE}/folders/${folderId}/subfolders`, {
                                            method: 'POST',
                                            headers: { 'Content-Type': 'application/json' },
                                            body: JSON.stringify({ name: newSubfolderName.trim(), username })
                                        });
                                        if (res.ok) {
                                            setNewSubfolderName('');
                                            setIsCreateSubfolderOpen(false);
                                            fetchFiles(); // refreshes subfolders too
                                        } else {
                                            showToast('Fehler beim Erstellen des Unterordners.');
                                        }
                                    }}
                                    className="flex-1 py-2.5 bg-[#5E5CE6] hover:bg-[#4d4ac9] text-white rounded-xl text-sm font-semibold transition-colors disabled:opacity-50"
                                >
                                    Erstellen
                                </button>
                            </div>
                        </div>
                    </div>
                )}

                {isRefineModalOpen && fileToRefine && (
                    <div className="fixed inset-0 z-[120] bg-black/70 backdrop-blur-sm flex items-center justify-center p-4">
                        <div className="w-full max-w-2xl bg-[#0B0B1A] border border-[#2A2A40] rounded-2xl shadow-2xl overflow-hidden">
                            <div className="p-5 border-b border-[#2A2A40]">
                                <h3 className="text-lg font-semibold text-white">Dokument mit Prompt anpassen</h3>
                                <p className="text-xs text-gray-400 mt-1">{fileToRefine.name}</p>
                            </div>
                            <div className="p-5 space-y-4">
                                <textarea
                                    value={refinePrompt}
                                    onChange={(e) => setRefinePrompt(e.target.value)}
                                    placeholder="z.B. Abschnitt 3 ausführlicher erklären, mehr Beispiele ergänzen, Sprache klarer machen ..."
                                    className="w-full min-h-[140px] bg-[#151525] border border-[#2A2A40] text-white rounded-xl px-4 py-3 text-sm outline-none focus:ring-2 focus:ring-[#5E5CE6]/50"
                                />
                                <div className="flex flex-wrap gap-2">
                                    <button
                                        type="button"
                                        onClick={() => setRefineSaveMode('new_file')}
                                        className={`px-3 py-1.5 rounded-lg text-xs border ${refineSaveMode === 'new_file' ? 'bg-[#5E5CE6] text-white border-[#5E5CE6]' : 'border-[#2A2A40] text-gray-300 hover:bg-[#1C1C33]'}`}
                                    >
                                        Als neue Datei speichern
                                    </button>
                                    <button
                                        type="button"
                                        onClick={() => setRefineSaveMode('replace')}
                                        className={`px-3 py-1.5 rounded-lg text-xs border ${refineSaveMode === 'replace' ? 'bg-amber-600 text-white border-amber-500' : 'border-[#2A2A40] text-gray-300 hover:bg-[#1C1C33]'}`}
                                    >
                                        Bestehende Datei überschreiben
                                    </button>
                                </div>
                            </div>
                            <div className="p-5 border-t border-[#2A2A40] flex gap-3">
                                <button
                                    onClick={() => {
                                        setIsRefineModalOpen(false);
                                        setFileToRefine(null);
                                        setRefinePrompt('');
                                    }}
                                    className="flex-1 py-2.5 bg-[#151525] hover:bg-[#1C1C33] text-gray-300 rounded-xl text-sm font-medium"
                                >
                                    Abbrechen
                                </button>
                                <button
                                    onClick={submitRefineFile}
                                    disabled={!refinePrompt.trim() || isRefining}
                                    className="flex-1 py-2.5 bg-[#5E5CE6] hover:bg-[#4d4ac9] text-white rounded-xl text-sm font-semibold disabled:opacity-50 flex items-center justify-center gap-2"
                                >
                                    {isRefining ? <Loader2 size={15} className="animate-spin" /> : null}
                                    Anpassung starten
                                </button>
                            </div>
                        </div>
                    </div>
                )}

                {/* Global Chat Bubble */}
                {/* KI-Aufgaben: parallel laufende Generierungen + zuletzt fertig */}
                <AnimatePresence>
                    {(isGenerating.length > 0 || completedAiJobs.length > 0 || isProcessing) && (
                        <motion.div
                            initial={{ opacity: 0, y: 16 }}
                            animate={{ opacity: 1, y: 0 }}
                            exit={{ opacity: 0, y: 16 }}
                            className="no-print fixed bottom-20 right-3 left-3 sm:left-auto md:bottom-6 z-[85] w-auto sm:w-[min(100vw-1.5rem,20rem)] max-h-[min(55vh,22rem)] flex flex-col rounded-2xl border border-[#2A2A40] bg-[#0B0B1A]/95 shadow-2xl backdrop-blur-md overflow-hidden"
                        >
                            <button
                                type="button"
                                onClick={() => setAiJobsPanelExpanded((e) => !e)}
                                className="flex items-center justify-between gap-2 px-3 py-2.5 bg-[#151525] border-b border-[#2A2A40] text-left w-full hover:bg-[#1C1C33] transition-colors"
                            >
                                <span className="flex items-center gap-2 text-sm font-semibold text-white">
                                    <ListTodo size={16} className="text-[#5E5CE6] shrink-0" />
                                    KI &amp; Verarbeitung
                                    <span className="text-xs font-normal text-gray-400">
                                        ({isGenerating.length + (isProcessing ? 1 : 0)} aktiv
                                        {completedAiJobs.length > 0 ? ` · ${completedAiJobs.length} fertig` : ''})
                                    </span>
                                </span>
                                {aiJobsPanelExpanded ? <ChevronUp size={18} className="text-gray-400 shrink-0" /> : <ChevronDown size={18} className="text-gray-400 shrink-0" />}
                            </button>
                            {aiJobsPanelExpanded && (
                                <div className="p-3 space-y-3 overflow-y-auto text-sm custom-scrollbar">
                                    {(isGenerating.length > 0 || isProcessing) && (
                                        <div>
                                            <div className="text-[10px] font-semibold uppercase tracking-wider text-gray-500 mb-1.5">Läuft gerade</div>
                                            <ul className="space-y-1.5">
                                                {isProcessing && (
                                                    <li className="flex items-center gap-2 text-gray-300 bg-[#151525] rounded-lg px-2.5 py-2 border border-[#2A2A40]">
                                                        <Loader2 size={14} className="animate-spin text-[#5E5CE6] shrink-0" />
                                                        <span>Material wird hochgeladen / verarbeitet</span>
                                                    </li>
                                                )}
                                                {isGenerating.map((key) => (
                                                    <li key={key} className="flex items-center gap-2 text-gray-200 bg-[#151525] rounded-lg px-2.5 py-2 border border-[#2A2A40]">
                                                        <Loader2 size={14} className="animate-spin text-amber-400 shrink-0" />
                                                        <span>{AI_JOB_LABELS[key] || key}</span>
                                                    </li>
                                                ))}
                                            </ul>
                                            <p className="text-[11px] text-gray-500 mt-2 leading-snug">
                                                Mehrere Aktionen gleichzeitig sind möglich — jede läuft unabhängig, solange sie nicht schon für dieselbe Funktion aktiv ist.
                                            </p>
                                        </div>
                                    )}
                                    {completedAiJobs.length > 0 && (
                                        <div>
                                            <div className="text-[10px] font-semibold uppercase tracking-wider text-gray-500 mb-1.5">Zuletzt abgeschlossen</div>
                                            <ul className="space-y-1.5">
                                                {completedAiJobs.map((j) => (
                                                    <li
                                                        key={j.id}
                                                        className={`flex items-center gap-2 rounded-lg px-2.5 py-2 border ${j.ok ? 'bg-emerald-500/10 border-emerald-500/25 text-emerald-200' : 'bg-red-500/10 border-red-500/25 text-red-200'}`}
                                                    >
                                                        {j.ok ? <CheckCircle2 size={14} className="shrink-0" /> : <XCircle size={14} className="shrink-0" />}
                                                        <span>{j.label}</span>
                                                        <span className="text-[10px] opacity-80 ml-auto">{j.ok ? 'Fertig' : 'Fehler'}</span>
                                                    </li>
                                                ))}
                                            </ul>
                                        </div>
                                    )}
                                </div>
                            )}
                        </motion.div>
                    )}
                </AnimatePresence>

                <FloatingChat
                    folderId={folderId}
                    username={typeof window !== 'undefined' ? localStorage.getItem('username') || 'Gast' : 'Gast'}
                    modelPreference={aiModelPreference}
                    activeFile={selectedFile}
                    onUpdateActiveFile={async (newContent) => {
                        if (!selectedFile) return;

                        try {
                            await persistFileContent(selectedFile.id, String(newContent ?? ""));
                            const updatedFiles = files.map(f => f.id === selectedFile.id ? { ...f, content: String(newContent ?? "") } : f);
                            setFiles(updatedFiles);
                            setSelectedFile({ ...selectedFile, content: String(newContent ?? "") });
                            showToast("Dokument wurde durch die KI aktualisiert und gespeichert! ✨", "success");
                        } catch {
                            showToast("Fehler beim Speichern der KI-Änderung.");
                        }
                    }}
                    onApplyPatch={handleApplyChatPatch}
                    onUndoLastPatch={handleUndoLastPatch}
                    canUndo={Boolean(selectedFile && (documentUndoStack[selectedFile.id] || []).length > 0)}
                />

                {/* Global OS file drop overlay */}
                {isDraggingOverBase && (
                    <div className="fixed inset-0 z-[200] bg-[#5E5CE6]/90 backdrop-blur-sm flex flex-col items-center justify-center pointer-events-none animate-in fade-in duration-200">
                        <div className="bg-[#0B0B1A] border-2 border-dashed border-[#5E5CE6] rounded-3xl p-16 shadow-2xl flex flex-col items-center">
                            <Upload size={64} className="text-[#5E5CE6] mb-6 animate-bounce" />
                            <h2 className="text-3xl font-bold text-white mb-2">Hier ablegen</h2>
                            <p className="text-gray-300 text-lg">PDFs oder Audio (mp3/wav) direkt hochladen</p>
                        </div>
                    </div>
                )}

                {/* Upload processing overlay */}
                {isUploadingFromDrop && (
                    <div className="fixed inset-0 z-[200] bg-black/80 backdrop-blur-sm flex flex-col items-center justify-center animate-in fade-in duration-200">
                        <div className="bg-[#0B0B1A] border border-[#2A2A40] rounded-3xl p-12 shadow-2xl flex flex-col items-center">
                            <Loader2 size={48} className="text-[#5E5CE6] animate-spin mb-6" />
                            <h2 className="text-xl font-bold text-white mb-2">{uploadDropMessage}</h2>
                            <p className="text-gray-400 text-sm">Das kann einen Moment dauern...</p>
                        </div>
                    </div>
                )}
            </div>
        </div>
    );
}

