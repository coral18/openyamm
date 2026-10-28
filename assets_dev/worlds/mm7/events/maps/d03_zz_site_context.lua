-- Site context metadata; load after gameplay overlays
-- The callback selects the current council room; these are authored alternatives.
SetMapContextAction(5, {kind = "enter_house", source = "script", houseId = 219, houseIds = {219, 221}})
