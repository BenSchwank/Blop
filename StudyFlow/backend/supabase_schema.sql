-- Supabase Schema for Blop Study Backend
-- Execute this script in the Supabase SQL Editor.

-- 1. Create Users Table
CREATE TABLE public.users (
    username TEXT PRIMARY KEY,
    password_hash TEXT NOT NULL,
    tokens INTEGER DEFAULT 500 NOT NULL,
    xp INTEGER DEFAULT 0 NOT NULL,
    streak_days INTEGER DEFAULT 0 NOT NULL,
    is_admin BOOLEAN DEFAULT FALSE NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- 2. Create Folders Table
CREATE TABLE public.folders (
    id TEXT PRIMARY KEY, -- We keep TEXT to match existing front-end generated IDs if needed, e.g. "folder_1234"
    username TEXT REFERENCES public.users(username) ON DELETE CASCADE,
    name TEXT NOT NULL,
    parent_id TEXT REFERENCES public.folders(id) ON DELETE CASCADE,
    plan_data JSONB DEFAULT '[]'::jsonb,
    summary_text TEXT DEFAULT '',
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- 3. Create Files Table (Metadata for PDFs, Summaries, Flashcards, etc)
CREATE TABLE public.files (
    id TEXT PRIMARY KEY, -- "summary_1234", "plan_main", etc.
    username TEXT REFERENCES public.users(username) ON DELETE CASCADE,
    folder_id TEXT REFERENCES public.folders(id) ON DELETE CASCADE,
    name TEXT NOT NULL,
    type TEXT NOT NULL,
    content TEXT, -- For text-based things like flashcards, markdown
    file_url TEXT, -- URL or path referencing the Supabase Storage bucket for binary files
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE
);

-- 4. Create API Keys Table (To store custom keys securely if the user still wants them, though we centralize most)
CREATE TABLE public.api_keys (
    username TEXT PRIMARY KEY REFERENCES public.users(username) ON DELETE CASCADE,
    api_key TEXT NOT NULL
);

-- 5. Storage Buckets (Run this if you prefer doing it in SQL, otherwise use Supabase Dashboard)
-- insert into storage.buckets (id, name, public) values ('blop_documents', 'blop_documents', false);
