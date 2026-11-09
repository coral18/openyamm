-- Restore the original disease prerequisite and MainCondition action for the eastern well.
-- The generated callback currently decodes those selectors as other conditions and a map variable.
local wellName = "Drink from the Well"
local diseases = {DiseasedGreen, DiseasedYellow, DiseasedRed}

ReplaceMapEvent(205, wellName, function()
    for _, disease in ipairs(diseases) do
        if IsAtLeast(disease, 0) then
            if not IsAutonoteSet(265) then
                SetAutonote(265)
            end
            SetValue(MajorCondition, 0)
            evt.StatusText("Disease Cured")
            return
        end
    end
    evt.StatusText("Refreshing!")
end, wellName)

SetMapContextAction(205, {kind = "well", source = "script"})
SetMapAutonoteRefs(205, {265})
SetMapSiteReference(205,
    "When the selected character has disease of any severity, removes their current conditions.\n"
    .. "Health is unchanged. Other party members are unaffected.")
