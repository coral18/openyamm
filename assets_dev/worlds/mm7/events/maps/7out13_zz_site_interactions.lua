-- Read-only shrine reference; load after gameplay overlays.
SetMapInteractionReference(401, {
    kind = "quest_objective", requiredQBit = 561, completionQBit = 564,
    objectiveGroupQBit = 562, otherObjectiveQBits = {563, 565}
})
SetMapInteractionReference(452, {
    kind = "offering", itemId = 1420, requiredQBit = 712, completionQBit = 714,
    spriteId = 25, consumesItem = true
})
