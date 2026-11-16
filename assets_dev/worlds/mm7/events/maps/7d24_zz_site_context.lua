-- Site context metadata; load after gameplay overlays
-- The throne-room callback replacement clears its original inspection metadata.
SetMapContextAction(416, {kind = "enter_house", source = "script", houseId = 216})
