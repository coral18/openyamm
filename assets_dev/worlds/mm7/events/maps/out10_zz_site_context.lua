-- Site context metadata; load after gameplay overlays
-- The base callbacks have no hints. These two physical wells restore SP/HP and record notes 280/281.
SetMapContextAction(207, {kind = "well", source = "script"})
SetMapContextAction(208, {kind = "well", source = "script"})
