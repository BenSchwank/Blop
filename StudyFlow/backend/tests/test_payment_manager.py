import os
import sys
import unittest
from unittest.mock import MagicMock, patch

sys.path.insert(0, os.path.dirname(os.path.dirname(__file__)))

import payment_manager


class StripeV15Object:
    """Minimal stand-in for stripe-python v15 StripeObject (no dict inheritance)."""

    def __init__(self, data):
        self._data = data

    def to_dict(self, recursive=True):
        def convert(value):
            if isinstance(value, StripeV15Object):
                return value.to_dict()
            if isinstance(value, list):
                return [convert(item) for item in value]
            return value

        return {key: convert(value) for key, value in self._data.items()}

    def __getitem__(self, key):
        return self._data[key]

    def get(self, key, default=None):
        return self._data.get(key, default)


class StripeV15ListObject(StripeV15Object):
    @property
    def data(self):
        return self._data.get("data", [])

    def __iter__(self):
        return iter(self.data)

    def __getitem__(self, key):
        if isinstance(key, int):
            raise KeyError(key)
        return super().__getitem__(key)


class StripePayloadCompatibilityTests(unittest.TestCase):
    def test_extracts_legacy_invoice_subscription(self):
        invoice = {"id": "in_1", "subscription": "sub_legacy"}
        self.assertEqual(payment_manager._invoice_subscription_id(invoice), "sub_legacy")

    def test_extracts_basil_invoice_subscription(self):
        invoice = {
            "id": "in_2",
            "parent": {
                "type": "subscription_details",
                "subscription_details": {"subscription": "sub_basil"},
            },
        }
        self.assertEqual(payment_manager._invoice_subscription_id(invoice), "sub_basil")

    def test_maps_legacy_price_to_tier(self):
        subscription = {
            "metadata": {},
            "items": {"data": [{"price": {"id": "price_pro"}}]},
        }
        prices = {("pro", "month"): "price_pro"}
        with patch.object(payment_manager, "STRIPE_PRICE_IDS", prices):
            self.assertEqual(payment_manager._tier_from_stripe_subscription(subscription), "pro")

    def test_maps_dahlia_price_to_tier(self):
        subscription = {
            "metadata": {},
            "items": {
                "data": [{"pricing": {"price_details": {"price": "price_premium"}}}]
            },
        }
        prices = {("premium", "month"): "price_premium"}
        with patch.object(payment_manager, "STRIPE_PRICE_IDS", prices):
            self.assertEqual(payment_manager._tier_from_stripe_subscription(subscription), "premium")

    def test_reads_item_level_billing_interval(self):
        subscription = {
            "metadata": {},
            "items": {
                "data": [{"price": {"recurring": {"interval": "year"}}}]
            },
        }
        self.assertEqual(payment_manager._interval_from_stripe_subscription(subscription), "year")

    def test_metadata_tier_supports_zero_value_invoice_flow(self):
        subscription = {"metadata": {"tier": "pro"}, "items": {"data": []}}
        self.assertEqual(payment_manager._tier_from_stripe_subscription(subscription), "pro")


class StripeV15ConversionTests(unittest.TestCase):
    def test_stripe_dict_uses_to_dict_not_to_dict_recursive(self):
        obj = StripeV15Object({"id": "cus_1", "email": "ben@example.com"})
        self.assertFalse(hasattr(obj, "to_dict_recursive"))
        self.assertEqual(
            payment_manager._stripe_dict(obj),
            {"id": "cus_1", "email": "ben@example.com"},
        )

    def test_stripe_list_data_from_v15_list_object(self):
        listed = StripeV15ListObject({
            "object": "list",
            "data": [StripeV15Object({"id": "cus_1"}), StripeV15Object({"id": "cus_2"})],
            "has_more": False,
        })
        rows = payment_manager._stripe_list_data(listed)
        self.assertEqual(len(rows), 2)
        self.assertEqual(payment_manager._stripe_dict(rows[0])["id"], "cus_1")

    def test_indexing_list_object_wrapper_raises_keyerror_zero(self):
        listed = StripeV15ListObject({"object": "list", "data": [{"id": "si_1"}]})
        with self.assertRaises(KeyError) as ctx:
            listed[0]
        self.assertEqual(ctx.exception.args, (0,))

    def test_first_item_reads_basil_period_fields_without_keyerror(self):
        items = StripeV15ListObject({
            "object": "list",
            "data": [
                StripeV15Object({
                    "id": "si_1",
                    "current_period_start": 1700000000,
                    "current_period_end": 1702678400,
                    "price": {"id": "price_pro", "recurring": {"interval": "month"}},
                })
            ],
        })
        subscription = {
            "id": "sub_1",
            "metadata": {"tier": "pro", "username": "ben", "interval": "month"},
            "status": "active",
            "customer": "cus_1",
            "items": items,
        }
        first = payment_manager._first_stripe_list_item(subscription.get("items"))
        self.assertEqual(first["current_period_start"], 1700000000)
        self.assertEqual(first["current_period_end"], 1702678400)


class SyncStripeSubscriptionTests(unittest.TestCase):
    def test_sync_maps_active_pro_subscription_for_email(self):
        customer = StripeV15Object({
            "id": "cus_ben",
            "email": "benmartischwank@gmail.com",
            "metadata": {"username": "ben"},
        })
        subscription = StripeV15Object({
            "id": "sub_ben",
            "status": "active",
            "created": 1700000000,
            "customer": "cus_ben",
            "cancel_at_period_end": False,
            "metadata": {},
            "items": StripeV15ListObject({
                "object": "list",
                "data": [
                    StripeV15Object({
                        "id": "si_1",
                        "current_period_start": 1700000000,
                        "current_period_end": 1702678400,
                        "price": {"id": "price_pro_live", "recurring": {"interval": "month"}, "product": "prod_pro"},
                    })
                ],
            }),
        })
        customers = StripeV15ListObject({"object": "list", "data": [customer], "has_more": False})
        subscriptions = StripeV15ListObject({"object": "list", "data": [subscription], "has_more": False})

        handled = {}

        def fake_handle(sub, grant_period_tokens=False, operation_key=None, event_id=None):
            handled["subscription"] = payment_manager._stripe_dict(sub)
            handled["grant"] = grant_period_tokens
            handled["operation_key"] = operation_key

        with patch.object(payment_manager, "_stripe_enabled", return_value=True), \
             patch.object(payment_manager, "STRIPE_PRICE_IDS", {("pro", "month"): "price_pro_live"}), \
             patch.object(payment_manager.stripe.Customer, "list", return_value=customers), \
             patch.object(payment_manager.stripe.Customer, "search", side_effect=Exception("search disabled")), \
             patch.object(payment_manager.stripe.Subscription, "list", return_value=subscriptions), \
             patch.object(payment_manager.stripe.Subscription, "modify", return_value=subscription), \
             patch.object(payment_manager, "_handle_subscription_created_or_updated", side_effect=fake_handle):
            result = payment_manager.sync_stripe_subscription("ben", "benmartischwank@gmail.com")

        self.assertEqual(result["status"], "success")
        self.assertTrue(result["found"])
        self.assertEqual(result["tier"], "pro")
        self.assertEqual(handled["subscription"]["id"], "sub_ben")
        self.assertEqual(handled["subscription"]["metadata"]["username"], "ben")
        self.assertTrue(handled["grant"])

    def test_credit_tokens_fallback_is_additive_and_idempotent(self):
        rows = {
            "users": [{"username": "ben", "tokens": 6150}],
            "subscription_token_grants": [],
        }

        class Result:
            def __init__(self, data):
                self.data = data

        class Query:
            def __init__(self, table):
                self.table_name = table
                self.filters = {}
                self.payload = None
                self.op = None

            def select(self, *args, **kwargs):
                self.op = "select"
                return self

            def insert(self, payload):
                self.op = "insert"
                self.payload = payload
                return self

            def update(self, payload):
                self.op = "update"
                self.payload = payload
                return self

            def eq(self, key, value):
                self.filters[key] = value
                return self

            def execute(self):
                table_rows = rows.setdefault(self.table_name, [])
                if self.op == "select":
                    return Result([
                        row for row in table_rows
                        if all(row.get(key) == value for key, value in self.filters.items())
                    ])
                if self.op == "insert":
                    if any(
                        row.get("provider") == self.payload.get("provider")
                        and row.get("provider_invoice_id") == self.payload.get("provider_invoice_id")
                        for row in table_rows
                    ):
                        raise Exception("duplicate")
                    table_rows.append(dict(self.payload))
                    return Result([self.payload])
                if self.op == "update":
                    for row in table_rows:
                        if all(row.get(key) == value for key, value in self.filters.items()):
                            row.update(self.payload)
                    return Result(table_rows)
                raise AssertionError(self.op)

        class FakeDb:
            def table(self, name):
                return Query(name)

            def rpc(self, *args, **kwargs):
                raise Exception("rpc unavailable")

        with patch.object(payment_manager, "_subscription_db", return_value=FakeDb()), \
             patch.object(payment_manager, "_subscription_credits_for_interval", return_value=300):
            first = payment_manager._credit_tokens("ben", "pro", "month", "subscription_period:sub_1:1700000000")
            second = payment_manager._credit_tokens("ben", "pro", "month", "subscription_period:sub_1:1700000000")

        self.assertTrue(first)
        self.assertFalse(second)
        self.assertEqual(rows["users"][0]["tokens"], 6450)
        self.assertEqual(len(rows["subscription_token_grants"]), 1)


if __name__ == "__main__":
    unittest.main()
