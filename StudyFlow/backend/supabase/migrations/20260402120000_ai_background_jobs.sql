-- Hintergrund-Jobs (Lernvideo, Podcast): Status über Instanz-Neustarts und Load-Balancer hinweg.
-- Einmal im Supabase SQL Editor ausführen (oder als Migration).

create table if not exists public.ai_background_jobs (
  job_id text primary key,
  job_type text not null,
  username text not null,
  status text not null,
  detail text,
  result jsonb,
  updated_at timestamptz not null default now()
);

create index if not exists ai_background_jobs_username_job_id_idx
  on public.ai_background_jobs (username, job_id);

comment on table public.ai_background_jobs is 'Blop Study: async AI jobs (learning_video, podcast); backend upserts status.';

-- Nur serverseitiger Zugriff (SUPABASE_KEY). Bei Schreibfehlern: Service-Role-Key nutzen oder RLS-Policies ergänzen.
alter table public.ai_background_jobs disable row level security;
