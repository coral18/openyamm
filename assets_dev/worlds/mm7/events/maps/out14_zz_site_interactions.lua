-- Read-only shrine reference; load after gameplay overlays.
SetMapInteractionReference(401, {
    kind = "quest_objective", requiredQBit = 561, completionQBit = 565,
    objectiveGroupQBit = 562, otherObjectiveQBits = {563, 564}
})
SetMapSiteGroups(401, {{
    "bmodel/84/surface/0", "bmodel/84/surface/8", "bmodel/84/surface/16", "bmodel/84/surface/17",
    "bmodel/85/surface/0", "bmodel/85/surface/5"
}})
SetMapInteractionReference(452, {
    kind = "offering", itemId = 1419, requiredQBit = 712, completionQBit = 713,
    spriteId = 25, consumesItem = true
})
