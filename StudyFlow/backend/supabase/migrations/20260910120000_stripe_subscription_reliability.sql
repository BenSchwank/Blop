alter table public.subscription_tiers
  add column if not exists updated_at timestamptz not null default now();

alter table public.subscriptions
  add column if not exists last_provider_sync_at timestamptz,
  add column if not exists last_provider_event_id text,
  add column if not exists activated_at timestamptz;

create unique index if not exists subscriptions_provider_subscription_unique_idx
  on public.subscriptions (provider_subscription_id)
  where provider_subscription_id is not null;

create table if not exists public.subscription_checkouts (
  checkout_session_id text primary key,
  username text not null references public.users(username) on delete cascade,
  tier text not null references public.subscription_tiers(name),
  billing_interval text not null check (billing_interval in ('month', 'year')),
  provider_customer_id text,
  provider_subscription_id text,
  status text not null default 'created' check (status in ('created', 'completed', 'failed', 'expired')),
  created_at timestamptz not null default now(),
  updated_at timestamptz not null default now(),
  completed_at timestamptz
);

create unique index if not exists subscription_checkouts_provider_subscription_unique_idx
  on public.subscription_checkouts (provider_subscription_id)
  where provider_subscription_id is not null;

create index if not exists subscription_checkouts_username_created_idx
  on public.subscription_checkouts (username, created_at desc);

create table if not exists public.stripe_webhook_events (
  event_id text primary key,
  event_type text not null,
  object_id text,
  processing_status text not null default 'processing' check (processing_status in ('processing', 'processed', 'failed')),
  attempt_count integer not null default 1,
  last_error text,
  received_at timestamptz not null default now(),
  processed_at timestamptz,
  updated_at timestamptz not null default now()
);

create index if not exists stripe_webhook_events_status_updated_idx
  on public.stripe_webhook_events (processing_status, updated_at desc);

create table if not exists public.subscription_token_grants (
  provider text not null,
  provider_invoice_id text not null,
  username text not null references public.users(username) on delete cascade,
  tier text not null references public.subscription_tiers(name),
  tokens integer not null,
  granted_at timestamptz not null default now(),
  primary key (provider, provider_invoice_id)
);

create index if not exists subscription_token_grants_username_idx
  on public.subscription_token_grants (username, granted_at desc);

update public.subscriptions
set current_period_start = null,
    current_period_end = null,
    updated_at = now()
where tier = 'free' and provider = 'none';

alter table public.subscription_checkouts disable row level security;
alter table public.stripe_webhook_events disable row level security;
alter table public.subscription_token_grants disable row level security;
