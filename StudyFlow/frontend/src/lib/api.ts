// API Configuration - Uses environment variable in production, localhost in development
const API_BASE = '/api';

console.log('🔗 API Base URL:', API_BASE); // Debug log

function sessionHeaders(extra: Record<string, string> = {}): Record<string, string> {
    const sid =
        typeof window !== 'undefined'
            ? (localStorage.getItem('session_id') || '').trim()
            : '';
    const headers: Record<string, string> = { ...extra };
    if (sid) headers['X-Session-Id'] = sid;
    return headers;
}

function withSessionQuery(url: string, username: string): string {
    const sid =
        typeof window !== 'undefined'
            ? (localStorage.getItem('session_id') || '').trim()
            : '';
    const u = new URL(url, typeof window !== 'undefined' ? window.location.origin : 'http://localhost');
    if (username) u.searchParams.set('username', username);
    if (sid) u.searchParams.set('session_id', sid);
    return u.pathname + u.search;
}

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
        const res = await fetch(withSessionQuery(`${API_BASE}/folders`, username), {
            headers: sessionHeaders(),
        });
        if (!res.ok) throw new Error('Failed to fetch folders');
        return res.json();
    },

    createFolder: async (name: string, username: string): Promise<Folder> => {
        const sid =
            typeof window !== 'undefined'
                ? (localStorage.getItem('session_id') || '').trim()
                : '';
        const res = await fetch(
            withSessionQuery(`${API_BASE}/folders`, username),
            {
                method: 'POST',
                headers: sessionHeaders({ 'Content-Type': 'application/json' }),
                body: JSON.stringify({ name, username, session_id: sid }),
            }
        );
        if (!res.ok) throw new Error('Failed to create folder');
        return res.json();
    },

    // Files
    getFiles: async (folderId: string, username: string): Promise<FileItem[]> => {
        const res = await fetch(
            withSessionQuery(`${API_BASE}/files/${folderId}`, username),
            { headers: sessionHeaders() }
        );
        if (!res.ok) throw new Error('Failed to fetch files');
        return res.json();
    },
};
