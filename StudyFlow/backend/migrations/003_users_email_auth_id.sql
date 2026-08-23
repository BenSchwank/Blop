-- Registration writes an e-mail and the Supabase Auth id into public.users.
-- Deployments created from the original schema lack both columns, which makes
-- every new sign-up fail with "Konto konnte nicht vollständig angelegt werden".

ALTER TABLE public.users ADD COLUMN IF NOT EXISTS email TEXT;
ALTER TABLE public.users ADD COLUMN IF NOT EXISTS auth_id UUID;
ALTER TABLE public.users ADD COLUMN IF NOT EXISTS preferred_model TEXT DEFAULT '';

CREATE UNIQUE INDEX IF NOT EXISTS users_email_unique_idx
    ON public.users (lower(email))
    WHERE email IS NOT NULL;
