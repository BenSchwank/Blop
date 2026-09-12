-- Atomic, idempotent subscription token grants.
-- Inserts the grant row first (PK = provider + grant_key); only then adds tokens.
-- Retries and concurrent webhook/sync deliveries therefore cannot double-credit.

create or replace function public.grant_subscription_tokens(
  p_username text,
  p_provider text,
  p_grant_key text,
  p_tier text,
  p_tokens integer
)
returns jsonb
language plpgsql
security definer
set search_path = public
as $$
declare
  inserted_count integer := 0;
  new_balance integer;
begin
  if p_tokens is null or p_tokens <= 0 then
    return jsonb_build_object('granted', false, 'reason', 'non_positive_tokens');
  end if;

  insert into public.subscription_token_grants (
    provider,
    provider_invoice_id,
    username,
    tier,
    tokens
  ) values (
    p_provider,
    p_grant_key,
    p_username,
    p_tier,
    p_tokens
  )
  on conflict (provider, provider_invoice_id) do nothing;

  get diagnostics inserted_count = row_count;
  if inserted_count = 0 then
    return jsonb_build_object('granted', false, 'reason', 'already_granted');
  end if;

  update public.users
  set tokens = coalesce(tokens, 0) + p_tokens
  where username = p_username
  returning tokens into new_balance;

  if new_balance is null then
    delete from public.subscription_token_grants
    where provider = p_provider
      and provider_invoice_id = p_grant_key;
    raise exception 'user % not found for token grant', p_username;
  end if;

  return jsonb_build_object(
    'granted', true,
    'tokens', p_tokens,
    'new_balance', new_balance
  );
end;
$$;

revoke all on function public.grant_subscription_tokens(text, text, text, text, integer) from public;
grant execute on function public.grant_subscription_tokens(text, text, text, text, integer) to service_role;
