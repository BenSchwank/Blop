-- Persistente Sessions: Render Free Tier verliert user_data/sessions.json bei Container-Neustarts.
-- Diese Tabelle ersetzt die lokale Datei als Single Source of Truth.
-- Einmal im Supabase SQL Editor ausführen (oder als Migration).

create table if not exists public.sessions (
  id text primary key,
  username text not null references public.users(username) on delete cascade,
  created_at timestamptz not null default now(),
  last_active timestamptz not null default now()
);

create index if not exists sessions_username_idx
  on public.sessions (username);

create index if not exists sessions_last_active_idx
  on public.sessions (last_active);

comment on table public.sessions is 'Blop Study: server-side session tokens; backend reads/writes with SUPABASE_KEY.';

-- Nur serverseitiger Zugriff (SUPABASE_KEY / Service-Role). RLS würde den Backend-Client blockieren.
alter table public.sessions disable row level security;
