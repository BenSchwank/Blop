-- Blop Study: Subscription system (Stripe + PayPal), flexible tier/feature matrix.
-- Run once in Supabase SQL Editor (or as migration).

-- Tiers: free, pro, premium, custom (admin-only).
create table if not exists public.subscription_tiers (
  name text primary key,
  display_name text not null,
  price_monthly_eur numeric(10,2) not null default 0,
  price_yearly_eur numeric(10,2) not null default 0,
  tokens_monthly integer not null default 0,
  is_admin_only boolean not null default false,
  is_default boolean not null default false,
  created_at timestamptz not null default now()
);

-- Feature matrix per tier. Admin can edit this to change what each tier unlocks.
create table if not exists public.subscription_features (
  tier_name text not null references public.subscription_tiers(name) on delete cascade,
  feature_key text not null,
  allowed boolean not null default true,
  created_at timestamptz not null default now(),
  primary key (tier_name, feature_key)
);

-- Current subscription per user.
create table if not exists public.subscriptions (
  username text primary key references public.users(username) on delete cascade,
  tier text not null references public.subscription_tiers(name),
  status text not null default 'active',
  provider text not null default 'none',
  provider_customer_id text,
  provider_subscription_id text,
  current_period_start timestamptz,
  current_period_end timestamptz,
  cancel_at_period_end boolean not null default false,
  created_at timestamptz not null default now(),
  updated_at timestamptz not null default now()
);

-- Indexes
create index if not exists subscription_features_tier_idx
  on public.subscription_features (tier_name);

create index if not exists subscriptions_status_period_idx
  on public.subscriptions (status, current_period_end);

-- Comments
comment on table public.subscription_tiers is 'Blop Study: subscription tiers (free, pro, premium, custom). Admin-editable.';
comment on table public.subscription_features is 'Blop Study: which features are allowed per tier. Admin-editable.';
comment on table public.subscriptions is 'Blop Study: current subscription state per user. Server writes via SUPABASE_KEY.';

-- Server-side writes only (backend uses SUPABASE_KEY / service-role). RLS would block the backend client.
alter table public.subscription_tiers disable row level security;
alter table public.subscription_features disable row level security;
alter table public.subscriptions disable row level security;

-- Seed default tiers and feature matrix.
-- Feature keys must match the keys used in backend (plan, quiz, flashcards, summary,
-- audio_transcribe, image_to_text, podcast, learning_video, elaboration, elaboration_refine,
-- repetition, task_help, chat).
insert into public.subscription_tiers (name, display_name, price_monthly_eur, price_yearly_eur, tokens_monthly, is_admin_only, is_default)
values
  ('free', 'Free', 0.00, 0.00, 50, false, true),
  ('pro', 'Pro', 3.00, 24.00, 300, false, false),
  ('premium', 'Premium', 5.00, 48.00, 1000, false, false),
  ('custom', 'Custom', 0.00, 0.00, 0, true, false)
on conflict (name) do nothing;

insert into public.subscription_features (tier_name, feature_key, allowed)
values
  -- Free: basic AI features only
  ('free', 'plan', true),
  ('free', 'quiz', true),
  ('free', 'flashcards', true),
  ('free', 'summary', true),
  ('free', 'audio_transcribe', false),
  ('free', 'image_to_text', false),
  ('free', 'podcast', false),
  ('free', 'learning_video', false),
  ('free', 'elaboration', false),
  ('free', 'elaboration_refine', false),
  ('free', 'repetition', false),
  ('free', 'task_help', false),
  ('free', 'chat', false),
  -- Pro: + audio & image
  ('pro', 'plan', true),
  ('pro', 'quiz', true),
  ('pro', 'flashcards', true),
  ('pro', 'summary', true),
  ('pro', 'audio_transcribe', true),
  ('pro', 'image_to_text', true),
  ('pro', 'podcast', true),
  ('pro', 'learning_video', false),
  ('pro', 'elaboration', true),
  ('pro', 'elaboration_refine', true),
  ('pro', 'repetition', true),
  ('pro', 'task_help', true),
  ('pro', 'chat', true),
  -- Premium: everything
  ('premium', 'plan', true),
  ('premium', 'quiz', true),
  ('premium', 'flashcards', true),
  ('premium', 'summary', true),
  ('premium', 'audio_transcribe', true),
  ('premium', 'image_to_text', true),
  ('premium', 'podcast', true),
  ('premium', 'learning_video', true),
  ('premium', 'elaboration', true),
  ('premium', 'elaboration_refine', true),
  ('premium', 'repetition', true),
  ('premium', 'task_help', true),
  ('premium', 'chat', true)
on conflict (tier_name, feature_key) do nothing;

-- Ensure every existing user gets a subscription row (defaults to free).
insert into public.subscriptions (username, tier, status, provider)
select username, 'free', 'active', 'none'
from public.users
where not exists (
  select 1 from public.subscriptions s where s.username = users.username
);
