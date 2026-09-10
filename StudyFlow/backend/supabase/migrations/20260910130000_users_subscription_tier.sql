-- Add legacy subscription_tier column to public.users for backwards compatibility.
-- This column is kept in sync by set_user_subscription and used by admin/user endpoints.

ALTER TABLE public.users ADD COLUMN IF NOT EXISTS subscription_tier TEXT DEFAULT 'free';
