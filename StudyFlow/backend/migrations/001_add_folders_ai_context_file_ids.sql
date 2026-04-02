-- Run in Supabase SQL editor (or via migration tool).
-- Adds per-folder list of file IDs to include in AI context. NULL = use all materials.

alter table public.folders
  add column if not exists ai_context_file_ids jsonb default null;

comment on column public.folders.ai_context_file_ids is 'JSON array of file UUIDs to use for AI; null or empty means all files in folder.';
