-- Restore the original Murmurwoods poison severity and retain the merged journal entry.
local poison = {variable = PoisonedYellow, label = "Medium poison"}
ReplaceMapEvent(101, "Drink from the well", function()
    evt.StatusText("That was not so refreshing")
    SetValue(poison.variable, 0)
    if not IsAutonoteSet(224) then
        SetAutonote(224)
    end
end, "Drink from the well")
SetMapContextAction(101, {kind = "well", source = "script"})
SetMapAutonoteRefs(101, {224})
SetMapSiteReference(101, "Effects apply to the selected character.\nCan apply: " .. poison.label .. ".")
