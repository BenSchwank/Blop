-- Blop Study sharing: requests + share links
-- Run once in Supabase SQL editor (or via migration runner).

create table if not exists public.share_requests (
  id text primary key,
  sender_username text not null references public.users(username) on delete cascade,
  target_username text not null references public.users(username) on delete cascade,
  file_id text not null references public.files(id) on delete cascade,
  status text not null default 'pending',
  message text,
  created_at timestamptz not null default now(),
  accepted_at timestamptz
);

create index if not exists share_requests_target_status_idx
  on public.share_requests (target_username, status, created_at desc);

create index if not exists share_requests_sender_idx
  on public.share_requests (sender_username, created_at desc);

create table if not exists public.share_links (
  id text primary key,
  token_hash text not null unique,
  sender_username text not null references public.users(username) on delete cascade,
  file_id text not null references public.files(id) on delete cascade,
  created_at timestamptz not null default now(),
  expires_at timestamptz,
  max_uses integer not null default 1,
  use_count integer not null default 0,
  is_active boolean not null default true,
  last_used_at timestamptz
);

create index if not exists share_links_sender_idx
  on public.share_links (sender_username, created_at desc);

create index if not exists share_links_active_expiry_idx
  on public.share_links (is_active, expires_at);

comment on table public.share_requests is 'Blop Study: user-to-user file share requests.';
comment on table public.share_links is 'Blop Study: token-based share links for file import.';

-- Backend uses server-side key; keep table policies simple for now.
alter table public.share_requests disable row level security;
alter table public.share_links disable row level security;
