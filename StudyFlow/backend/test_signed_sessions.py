"""Unit tests for HMAC-signed sessions (no Supabase / filesystem required)."""
import os
import time
import unittest

# Ensure a stable secret for the test process before importing AuthManager helpers.
os.environ["SESSION_SECRET"] = "unit-test-session-secret"

from auth_manager import AuthManager, SIGNED_SESSION_PREFIX  # noqa: E402


class SignedSessionTests(unittest.TestCase):
    def test_create_and_validate(self):
        sid = AuthManager.create_session("ben")
        self.assertTrue(sid.startswith(SIGNED_SESSION_PREFIX))
        self.assertEqual(AuthManager.validate_session(sid), "ben")

    def test_tampered_token_rejected(self):
        sid = AuthManager.create_session("ben")
        # Flip one character in the signature.
        bad = sid[:-1] + ("A" if sid[-1] != "A" else "B")
        self.assertIsNone(AuthManager.validate_session(bad))

    def test_logout_revokes_jti(self):
        sid = AuthManager.create_session("ben")
        AuthManager.logout_session(sid)
        self.assertIsNone(AuthManager.validate_session(sid))

    def test_expired_token_rejected(self):
        sid = AuthManager.create_session("ben")
        body, sig = sid[len(SIGNED_SESSION_PREFIX) :].split(".")
        import json
        from auth_manager import AuthManager as AM

        payload = json.loads(AM._b64url_decode(body).decode("utf-8"))
        payload["exp"] = int(time.time()) - 10
        new_body = AM._b64url_encode(
            json.dumps(payload, separators=(",", ":"), sort_keys=True).encode("utf-8")
        )
        import hashlib
        import hmac

        new_sig = AM._b64url_encode(
            hmac.new(AM._session_secret(), new_body.encode("ascii"), hashlib.sha256).digest()
        )
        expired = f"{SIGNED_SESSION_PREFIX}{new_body}.{new_sig}"
        self.assertIsNone(AuthManager.validate_session(expired))

    def test_legacy_uuid_unknown_rejected(self):
        self.assertIsNone(AuthManager.validate_session("00000000-0000-0000-0000-000000000000"))


if __name__ == "__main__":
    unittest.main()
