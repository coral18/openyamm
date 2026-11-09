-- Restore the original OUT12 condition selectors without changing payment or random-choice structure.
-- Gameplay and the read-only reference share these authored outcomes.
local price = 5000
local groups = {
    {id = 5, step = 6, choices = {7, 9, 11, 12}},
    {id = 14, step = 15, choices = {16, 18, 20}},
    {id = 22, step = 23, choices = {24, 26, 28}},
}
local outcomes = {
    [7] = {operation = "condition", variable = Eradicated, label = "Eradication"},
    [9] = {operation = "age_experience", value = 5000},
    [11] = {operation = "none"},
    [12] = {operation = "condition", variable = Dead, label = "Death"},
    [16] = {operation = "add", variable = AirResistanceBonus, value = 50, label = "Air resistance bonus"},
    [18] = {operation = "add", variable = FireResistanceBonus, value = 50, label = "Fire resistance bonus"},
    [20] = {operation = "condition", variable = Stoned, label = "Petrification"},
    [24] = {operation = "add", variable = Gold, value = 10000, label = "Party gold"},
    [26] = {operation = "add", variable = SkillPoints, value = 10, label = "Skill points"},
    [28] = {operation = "subtract", variable = ArmorClassBonus, value = 50, label = "Armor class bonus"},
}
local roots = {}
local groupById = {}
for _, group in ipairs(groups) do
    roots[#roots + 1] = group.id
    groupById[group.id] = group
end

local function applyOutcome(outcome)
    if outcome.operation == "condition" then
        -- A condition Set applies that condition, even with the original zero operand.
        SetValue(outcome.variable, 0)
    elseif outcome.operation == "age_experience" then
        SetValue(Age, 0)
        AddValue(Experience, outcome.value)
    elseif outcome.operation == "add" then
        AddValue(outcome.variable, outcome.value)
    elseif outcome.operation == "subtract" then
        SubtractValue(outcome.variable, outcome.value)
    else
        assert(outcome.operation == "none", "Unknown wishing-well outcome")
    end
end

ReplaceMapEvent(205, "Well", function()
    if not IsAtLeast(Gold, price) then
        if IsAtLeast(Gold, price - 1) then
            SubtractValue(Gold, price - 1)
            evt.StatusText("You make a wish")
        end
        return
    end
    SubtractValue(Gold, price)
    local group = assert(groupById[PickRandomOption(205, 4, roots)])
    local outcome = assert(outcomes[PickRandomOption(205, group.step, group.choices)])
    applyOutcome(outcome)
    evt.StatusText("You make a wish")
end, "Well")

local reference = {
    string.format("Party payment: %d gold for a random outcome.", price),
    string.format("With exactly %d gold, the party pays that amount without receiving a random outcome.", price - 1),
    string.format("Below %d gold there is no payment or effect.", price - 1),
    "Character effects concern the selected character; gold concerns the party.",
    "Declared random outcomes (probabilities are not shown):",
}
for _, group in ipairs(groups) do
    for _, id in ipairs(group.choices) do
        local outcome = outcomes[id]
        local text
        if outcome.operation == "condition" then
            text = "Can apply: " .. outcome.label .. "."
        elseif outcome.operation == "age_experience" then
            text = string.format("Clears magical age adjustment; Experience +%d.", outcome.value)
        elseif outcome.operation == "none" then
            text = "No additional effect."
        else
            text = string.format("%s %s%d.", outcome.label,
                outcome.operation == "subtract" and "-" or "+", outcome.value)
        end
        reference[#reference + 1] = "- " .. text
    end
end
SetMapContextAction(205, {kind = "well", source = "script"})
SetMapSiteReference(205, table.concat(reference, "\n"))
