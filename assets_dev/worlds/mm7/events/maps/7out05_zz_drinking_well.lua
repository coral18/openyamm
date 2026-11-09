-- Restore the original Deyja well's Drunk prerequisite and effect.
local condition = {variable = Drunk, label = "Drunkenness"}
ReplaceMapEvent(206, "Drink from the Well", function()
    if not IsAtLeast(condition.variable, 0) then
        SetValue(condition.variable, 0)
        evt.StatusText("You Feel Great!!")
        return
    end
    local step = PickRandomOption(206, 5, {5, 7})
    if step == 5 then
        evt.StatusText("Haven't you had enough ?")
    elseif step == 7 then
        evt.StatusText("Do you think that's such a good idea ?")
    end
end, "Drink from the Well")
SetMapContextAction(206, {kind = "well", source = "script"})
SetMapSiteReference(206,
    "Effects apply to the selected character.\nCan apply: " .. condition.label .. ".\n"
    .. "If already drunk, gives one of two messages without changing conditions.")
