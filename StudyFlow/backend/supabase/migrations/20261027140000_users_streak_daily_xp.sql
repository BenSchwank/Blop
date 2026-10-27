-- Gamification: last study day + daily XP for streaks / daily goal.
ALTER TABLE public.users
    ADD COLUMN IF NOT EXISTS last_active_date DATE,
    ADD COLUMN IF NOT EXISTS daily_xp INTEGER DEFAULT 0 NOT NULL;
