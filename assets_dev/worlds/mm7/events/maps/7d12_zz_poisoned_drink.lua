-- Restore the original Clanker's Laboratory drink and its two independent Endurance checks.
local threshold = 35
local poison = {variable = PoisonedRed, label = "Severe poison"}
ReplaceMapEvent(451, "Take a Drink", function()
    if not IsAtLeast(EnduranceBonus, threshold) and not IsAtLeast(BaseEndurance, threshold) then
        SetValue(poison.variable, 0)
        evt.StatusText("Not Very Refreshing")
        return
    end
    evt.StatusText("Refreshing")
end, "Take a Drink")
SetMapContextAction(451, {kind = "drink", source = "script"})
SetMapSiteReference(451, string.format(
    "Effects apply to the selected character.\nCan apply: %s.\n"
    .. "Requires base Endurance below %d and temporary Endurance below %d.\n"
    .. "If either value is at least %d, there is no condition change.",
    poison.label, threshold, threshold, threshold))
