-- The throne-room replacement clears the original event's inspection metadata.
-- Keep the real house destination available to map inspection and the Atlas.
SetMapContextAction(416, {
    kind = "enter_house", source = "script", houseId = 216, targetName = "Throne Room"
})
