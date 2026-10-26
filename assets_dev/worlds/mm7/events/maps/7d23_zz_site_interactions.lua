-- Site interaction metadata; load after gameplay overlays
-- The active callback checks possession on each member, including inventory, not equipped suits only.
SetMapInteractionReference(501, {kind = "leave_dungeon", itemId = 1406, itemScope = "each_member", targetMap = "7out15.odm"})
