// API Configuration - Uses environment variable in production, localhost in development
const API_BASE = process.env.NEXT_PUBLIC_API_URL || 'http://localhost:8000/api';

console.log('🔗 API Base URL:', API_BASE); // Debug log

export interface Folder {
    id: string;
    name: string;
    files?: FileItem[];
}

export interface FileItem {
    id: string;
    name: string;
    type: 'pdf' | 'video' | 'plan' | 'summary' | 'flashcards' | 'transcript';
    created_at: string;
}

export const api = {
    // Folders
    getFolders: async (username: string): Promise<Folder[]> => {
        const res = await fetch(`${API_BASE}/folders?username=${username}`);
        if (!res.ok) throw new Error('Failed to fetch folders');
        return res.json();
    },

    createFolder: async (name: string, username: string): Promise<Folder> => {
        const res = await fetch(`${API_BASE}/folders`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ name, username }),
        });
        if (!res.ok) throw new Error('Failed to create folder');
        return res.json();
    },

    // Files
    getFiles: async (folderId: string, username: string): Promise<FileItem[]> => {
        const res = await fetch(`${API_BASE}/files/${folderId}?username=${username}`);
        if (!res.ok) throw new Error('Failed to fetch files');
        return res.json();
    },
};
