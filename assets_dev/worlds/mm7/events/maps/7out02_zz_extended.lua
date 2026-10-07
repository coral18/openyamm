-- ReplaceMapEvent in the MMMerge supplement clears the generated context metadata.
-- Restore the declared destination after that overlay, for gameplay inspection and world tools.
SetMapContextAction(301, {
    kind = "enter_dungeon", source = "script", targetMap = "7d29.blv"
})
