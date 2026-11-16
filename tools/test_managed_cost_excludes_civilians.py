"""GetManagedCost must ignore civilians and vehicle crews.

Those pools are excluded from the military allocator (EnableBudget /
BudgetController), but GetManagedCost used to add their physical counts.
That filled HasRoomForInboundInfantry / ClampInboundInfantryRequest while
players fought in a town, so capture QRF, reinforce waves, and defend
filler could starve forever.

Run: python -m unittest discover -s tools -p "test_managed_cost_excludes_civilians.py"
"""
import unittest

from test_dynamic_base_flow import method, source


class ManagedCostExcludesCiviliansTests(unittest.TestCase):
    def test_get_managed_cost_skips_civilian_and_vehicle_caches(self):
        body = method(source("IA_DynamicAISpawning.c"), "GetManagedCost")
        self.assertIn("IsCivilianCache()", body)
        self.assertIn("IsVehicleCache()", body)
        skip = body.index("IsCivilianCache()")
        self.assertLess(skip, body.index("IsBudgetActive()"))
        self.assertLess(skip, body.index("GetDynamicAIPhysicalAliveCount()"))
        self.assertIn("continue", body[skip : body.index("IsBudgetActive()")])

    def test_inbound_gates_use_get_managed_cost(self):
        spawning = source("IA_DynamicAISpawning.c")
        has_room = method(spawning, "HasRoomForInboundInfantry")
        self.assertIn("GetManagedCost()", has_room)
        clamp = method(spawning, "ClampInboundInfantryRequest")
        self.assertIn("GetManagedCost()", clamp)

    def test_budget_enable_still_excludes_same_pools(self):
        # Allocator and inbound gate must agree on who is military.
        enable = method(source("IA_DynamicAIBudgetCache.c"), "EnableBudget")
        self.assertIn("IsCivilianCache()", enable)
        self.assertIn("IsVehicleCache()", enable)


if __name__ == "__main__":
    unittest.main()
