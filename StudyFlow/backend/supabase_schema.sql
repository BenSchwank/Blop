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

-- 6. Share Requests (username-to-username)
CREATE TABLE IF NOT EXISTS public.share_requests (
    id TEXT PRIMARY KEY,
    sender_username TEXT NOT NULL REFERENCES public.users(username) ON DELETE CASCADE,
    target_username TEXT NOT NULL REFERENCES public.users(username) ON DELETE CASCADE,
    file_id TEXT NOT NULL REFERENCES public.files(id) ON DELETE CASCADE,
    status TEXT NOT NULL DEFAULT 'pending',
    message TEXT,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW() NOT NULL,
    accepted_at TIMESTAMP WITH TIME ZONE
);

CREATE INDEX IF NOT EXISTS share_requests_target_status_idx
    ON public.share_requests (target_username, status, created_at DESC);

-- 7. Share Links (token-based import)
CREATE TABLE IF NOT EXISTS public.share_links (
    id TEXT PRIMARY KEY,
    token_hash TEXT NOT NULL UNIQUE,
    sender_username TEXT NOT NULL REFERENCES public.users(username) ON DELETE CASCADE,
    file_id TEXT NOT NULL REFERENCES public.files(id) ON DELETE CASCADE,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW() NOT NULL,
    expires_at TIMESTAMP WITH TIME ZONE,
    max_uses INTEGER NOT NULL DEFAULT 1,
    use_count INTEGER NOT NULL DEFAULT 0,
    is_active BOOLEAN NOT NULL DEFAULT TRUE,
    last_used_at TIMESTAMP WITH TIME ZONE
);

CREATE INDEX IF NOT EXISTS share_links_active_expiry_idx
    ON public.share_links (is_active, expires_at);

-- 8. Sessions (server-side, persistent across Render restarts)
CREATE TABLE IF NOT EXISTS public.sessions (
    id TEXT PRIMARY KEY,
    username TEXT NOT NULL REFERENCES public.users(username) ON DELETE CASCADE,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    last_active TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS sessions_username_idx
    ON public.sessions (username);

CREATE INDEX IF NOT EXISTS sessions_last_active_idx
    ON public.sessions (last_active);

COMMENT ON TABLE public.sessions IS 'Blop Study: server-side session tokens written by the backend.';

ALTER TABLE public.sessions DISABLE ROW LEVEL SECURITY;

-- 9. Subscription tiers (admin-editable feature matrix)
CREATE TABLE IF NOT EXISTS public.subscription_tiers (
    name TEXT PRIMARY KEY,
    display_name TEXT NOT NULL,
    price_monthly_eur NUMERIC(10,2) NOT NULL DEFAULT 0,
    price_yearly_eur NUMERIC(10,2) NOT NULL DEFAULT 0,
    tokens_monthly INTEGER NOT NULL DEFAULT 0,
    is_admin_only BOOLEAN NOT NULL DEFAULT FALSE,
    is_default BOOLEAN NOT NULL DEFAULT FALSE,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS public.subscription_features (
    tier_name TEXT NOT NULL REFERENCES public.subscription_tiers(name) ON DELETE CASCADE,
    feature_key TEXT NOT NULL,
    allowed BOOLEAN NOT NULL DEFAULT TRUE,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    PRIMARY KEY (tier_name, feature_key)
);

CREATE INDEX IF NOT EXISTS subscription_features_tier_idx
    ON public.subscription_features (tier_name);

-- 10. Current subscription per user
CREATE TABLE IF NOT EXISTS public.subscriptions (
    username TEXT PRIMARY KEY REFERENCES public.users(username) ON DELETE CASCADE,
    tier TEXT NOT NULL REFERENCES public.subscription_tiers(name),
    status TEXT NOT NULL DEFAULT 'active',
    provider TEXT NOT NULL DEFAULT 'none',
    provider_customer_id TEXT,
    provider_subscription_id TEXT,
    current_period_start TIMESTAMP WITH TIME ZONE,
    current_period_end TIMESTAMP WITH TIME ZONE,
    cancel_at_period_end BOOLEAN NOT NULL DEFAULT FALSE,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    last_provider_sync_at TIMESTAMP WITH TIME ZONE,
    last_provider_event_id TEXT,
    activated_at TIMESTAMP WITH TIME ZONE
);

CREATE INDEX IF NOT EXISTS subscriptions_status_period_idx
    ON public.subscriptions (status, current_period_end);

CREATE UNIQUE INDEX IF NOT EXISTS subscriptions_provider_subscription_unique_idx
    ON public.subscriptions (provider_subscription_id)
    WHERE provider_subscription_id IS NOT NULL;

CREATE TABLE IF NOT EXISTS public.subscription_checkouts (
    checkout_session_id TEXT PRIMARY KEY,
    username TEXT NOT NULL REFERENCES public.users(username) ON DELETE CASCADE,
    tier TEXT NOT NULL REFERENCES public.subscription_tiers(name),
    billing_interval TEXT NOT NULL CHECK (billing_interval IN ('month', 'year')),
    provider_customer_id TEXT,
    provider_subscription_id TEXT,
    status TEXT NOT NULL DEFAULT 'created' CHECK (status IN ('created', 'completed', 'failed', 'expired')),
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    completed_at TIMESTAMP WITH TIME ZONE
);

CREATE UNIQUE INDEX IF NOT EXISTS subscription_checkouts_provider_subscription_unique_idx
    ON public.subscription_checkouts (provider_subscription_id)
    WHERE provider_subscription_id IS NOT NULL;

CREATE INDEX IF NOT EXISTS subscription_checkouts_username_created_idx
    ON public.subscription_checkouts (username, created_at DESC);

CREATE TABLE IF NOT EXISTS public.stripe_webhook_events (
    event_id TEXT PRIMARY KEY,
    event_type TEXT NOT NULL,
    object_id TEXT,
    processing_status TEXT NOT NULL DEFAULT 'processing' CHECK (processing_status IN ('processing', 'processed', 'failed')),
    attempt_count INTEGER NOT NULL DEFAULT 1,
    last_error TEXT,
    received_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    processed_at TIMESTAMP WITH TIME ZONE,
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS public.subscription_token_grants (
    provider TEXT NOT NULL,
    provider_invoice_id TEXT NOT NULL,
    username TEXT NOT NULL REFERENCES public.users(username) ON DELETE CASCADE,
    tier TEXT NOT NULL REFERENCES public.subscription_tiers(name),
    tokens INTEGER NOT NULL,
    granted_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    PRIMARY KEY (provider, provider_invoice_id)
);

ALTER TABLE public.subscription_tiers DISABLE ROW LEVEL SECURITY;
ALTER TABLE public.subscription_features DISABLE ROW LEVEL SECURITY;
ALTER TABLE public.subscriptions DISABLE ROW LEVEL SECURITY;
ALTER TABLE public.subscription_checkouts DISABLE ROW LEVEL SECURITY;
ALTER TABLE public.stripe_webhook_events DISABLE ROW LEVEL SECURITY;
ALTER TABLE public.subscription_token_grants DISABLE ROW LEVEL SECURITY;
