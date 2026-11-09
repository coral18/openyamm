-- Restore OUT13 event 214's original disease prerequisites and outcomes.
local diseases = {
    {variable = DiseasedGreen, step = 5, label = "Mild disease"},
    {variable = DiseasedYellow, step = 7, label = "Medium disease"},
    {variable = DiseasedRed, step = 9, label = "Severe disease"},
}
local options = {}
local byStep = {}
local reference = {
    "Effects apply to the selected character.",
    "Requires no existing disease of any severity; otherwise the character decides against drinking.",
    "Random outcomes follow the interaction definition; their probabilities are not shown:",
}
for _, disease in ipairs(diseases) do
    options[#options + 1] = disease.step
    byStep[disease.step] = disease
    reference[#reference + 1] = "- Can apply: " .. disease.label .. "."
end
ReplaceMapEvent(214, "Drink", function()
    for _, disease in ipairs(diseases) do
        if IsAtLeast(disease.variable, 0) then
            evt.StatusText("You decide it would be a bad idea to try that again.")
            return
        end
    end
    local disease = assert(byStep[PickRandomOption(214, 4, options)])
    SetValue(disease.variable, 0)
    evt.StatusText("You do not feel well.")
end, "Drink")
SetMapContextAction(214, {kind = "drink", source = "script"})
SetMapSiteReference(214, table.concat(reference, "\n"))
