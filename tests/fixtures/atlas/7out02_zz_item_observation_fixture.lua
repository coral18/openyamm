-- QA-only: two queued unidentified items exercise delivery to pack/cursor at an existing well.
ReplaceMapEvent(223, "Drink from the Well", function()
    if IsAtLeast(MapVar(21), 1) then return end
    SetValue(MapVar(21), 1)
    evt.SetHeldItem(1, {identified = false})
    evt.SetHeldItem(1, {identified = false})
end)
SetMapContextAction(223, {kind = "well", source = "script"})
