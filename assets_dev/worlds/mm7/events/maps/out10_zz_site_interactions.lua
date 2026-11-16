-- Read-only site classification; load after gameplay overlays.
-- The original event hints classify these existing objects despite the current blank hints.
SetMapContextAction(202, {kind = "well", source = "script"})
SetMapContextAction(452, {kind = "shrine", source = "script"})
SetMapContextAction(453, {kind = "obelisk", source = "script"})
