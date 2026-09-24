-- Seed smart_learning feature for all known tiers (idempotent).
-- Pricing / tier prices are unchanged; this only adds the feature matrix row.

INSERT INTO public.subscription_features (tier_name, feature_key, allowed)
VALUES
    ('free', 'smart_learning', true),
    ('pro', 'smart_learning', true),
    ('premium', 'smart_learning', true)
ON CONFLICT (tier_name, feature_key) DO UPDATE
SET allowed = EXCLUDED.allowed;
