-- Restore OUT06 event 460's original poison selectors without changing global selector mappings.
local poisons = {
    {variable = PoisonedGreen, step = 5, label = "Mild poison"},
    {variable = PoisonedYellow, step = 7, label = "Medium poison"},
    {variable = PoisonedRed, step = 9, label = "Severe poison"},
}
local options = {}
local byStep = {}
local reference = {
    "Effects apply to the selected character.",
    "Requires no existing poison of any severity; otherwise the character decides against drinking.",
    "Random outcomes follow the interaction definition; their probabilities are not shown:",
}
for _, poison in ipairs(poisons) do
    options[#options + 1] = poison.step
    byStep[poison.step] = poison
    reference[#reference + 1] = "- Can apply: " .. poison.label .. "."
end

ReplaceMapEvent(460, "Drink", function()
    for _, poison in ipairs(poisons) do
        if IsAtLeast(poison.variable, 0) then
            evt.StatusText("hmmm, You decide against it.")
            return
        end
    end
    local poison = assert(byStep[PickRandomOption(460, 4, options)])
    SetValue(poison.variable, 0)
    evt.StatusText("Suddenly, You Don't Feel too Well")
end, "Drink")
SetMapContextAction(460, {kind = "drink", source = "script"})
SetMapSiteReference(460, table.concat(reference, "\n"))
