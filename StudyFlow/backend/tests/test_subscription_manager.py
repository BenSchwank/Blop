import os
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, os.path.dirname(os.path.dirname(__file__)))

from subscription_manager import SubscriptionManager


class Result:
    def __init__(self, data):
        self.data = data


class Query:
    def __init__(self, database, table):
        self.database = database
        self.table = table
        self.operation = None
        self.payload = None
        self.filters = {}

    def select(self, *args, **kwargs):
        self.operation = "select"
        return self

    def insert(self, payload):
        self.operation = "insert"
        self.payload = payload
        return self

    def eq(self, key, value):
        self.filters[key] = value
        return self

    def execute(self):
        rows = self.database.rows.setdefault(self.table, [])
        if self.operation == "select":
            return Result([
                row for row in rows
                if all(row.get(key) == value for key, value in self.filters.items())
            ])
        if self.operation == "insert":
            rows.append(dict(self.payload))
            return Result([self.payload])
        raise AssertionError(f"Unsupported operation: {self.operation}")


class FakeDatabase:
    def __init__(self, rows):
        self.rows = rows

    def table(self, name):
        return Query(self, name)


class SubscriptionPersistenceTests(unittest.TestCase):
    def test_ensure_does_not_overwrite_existing_pro_subscription(self):
        existing = {
            "username": "ben",
            "tier": "pro",
            "status": "active",
            "provider": "stripe",
            "provider_subscription_id": "sub_123",
        }
        database = FakeDatabase({"subscriptions": [existing.copy()]})
        with patch.object(SubscriptionManager, "_get_db", return_value=database):
            SubscriptionManager._ensure_subscription_row("ben", "free")
        self.assertEqual(database.rows["subscriptions"], [existing])

    def test_ensure_inserts_free_only_when_missing(self):
        database = FakeDatabase({"subscriptions": []})
        with patch.object(SubscriptionManager, "_get_db", return_value=database):
            SubscriptionManager._ensure_subscription_row("new-user", "free")
        self.assertEqual(len(database.rows["subscriptions"]), 1)
        self.assertEqual(database.rows["subscriptions"][0]["tier"], "free")
        self.assertIsNone(database.rows["subscriptions"][0]["current_period_end"])

    def test_reset_tokens_never_reduces_existing_balance(self):
        database = FakeDatabase({
            "subscriptions": [],
            "users": [{"username": "ben", "tokens": 6150, "subscription_tier": "free"}],
            "subscription_tiers": [{
                "name": "pro",
                "tokens_monthly": 300,
                "is_admin_only": False,
            }],
        })

        class UpsertQuery(Query):
            def upsert(self, payload):
                self.operation = "upsert"
                self.payload = payload
                return self

            def update(self, payload):
                self.operation = "update"
                self.payload = payload
                return self

            def execute(self):
                rows = self.database.rows.setdefault(self.table, [])
                if self.operation == "select":
                    return Result([
                        row for row in rows
                        if all(row.get(key) == value for key, value in self.filters.items())
                    ])
                if self.operation == "upsert":
                    for idx, row in enumerate(rows):
                        if row.get("username") == self.payload.get("username"):
                            rows[idx] = {**row, **self.payload}
                            return Result([rows[idx]])
                    rows.append(dict(self.payload))
                    return Result([self.payload])
                if self.operation == "update":
                    for row in rows:
                        if all(row.get(key) == value for key, value in self.filters.items()):
                            row.update(self.payload)
                    return Result(rows)
                if self.operation == "insert":
                    rows.append(dict(self.payload))
                    return Result([self.payload])
                raise AssertionError(self.operation)

        class Db(FakeDatabase):
            def table(self, name):
                return UpsertQuery(self, name)

        db = Db(database.rows)
        with patch.object(SubscriptionManager, "_get_db", return_value=db), \
             patch.object(SubscriptionManager, "get_tier", return_value={
                 "name": "pro", "tokens_monthly": 300, "is_admin_only": False
             }):
            SubscriptionManager.set_user_subscription(
                username="ben",
                tier="pro",
                status="active",
                provider="stripe",
                reset_tokens=True,
            )
        self.assertEqual(db.rows["users"][0]["tokens"], 6150)


if __name__ == "__main__":
    unittest.main()
