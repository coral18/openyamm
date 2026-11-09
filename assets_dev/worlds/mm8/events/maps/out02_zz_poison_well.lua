-- Restore the original Ravenshore well's poison severity.
local poison = {variable = PoisonedYellow, label = "Medium poison"}
ReplaceMapEvent(102, "Drink from the well", function()
    evt.StatusText("That was not so refreshing")
    SetValue(poison.variable, 0)
end, "Drink from the well")
SetMapContextAction(102, {kind = "well", source = "script"})
SetMapSiteReference(102, "Effects apply to the selected character.\nCan apply: " .. poison.label .. ".")
