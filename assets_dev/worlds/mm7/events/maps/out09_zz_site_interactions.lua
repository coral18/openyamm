-- Read-only shrine reference; load after gameplay overlays.
-- The original event hints classify these existing objects despite the current blank hints.
SetMapContextAction(452, {kind = "shrine", source = "script"})
SetMapContextAction(453, {kind = "obelisk", source = "script"})
SetMapInteractionReference(6, {kind = "dimension_door", network = "merge.dimension_doors"})
SetMapInteractionReference(401, {
    kind = "quest_objective", requiredQBit = 561, completionQBit = 563,
    objectiveGroupQBit = 562, otherObjectiveQBits = {564, 565}
})
SetMapSiteGroups(401, {{
    "bmodel/58/surface/0", "bmodel/58/surface/8", "bmodel/58/surface/16", "bmodel/58/surface/17",
    "bmodel/59/surface/0", "bmodel/59/surface/5"
}})
