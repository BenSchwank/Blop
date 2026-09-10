import os
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, os.path.dirname(os.path.dirname(__file__)))

import payment_manager


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


if __name__ == "__main__":
    unittest.main()
