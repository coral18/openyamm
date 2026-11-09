-- Restore the original Bracada condition outcomes in an explicit map-local overlay.
-- The choice graph retains the imported random calls; gameplay and Guide share its leaves.
local price = 100
local choices = {
    [0] = {step = 5, options = {6, 16}},
    [6] = {step = 7, options = {8, 10, 12, 14}},
    [16] = {step = 17, options = {18, 20, 22}},
    [22] = {step = 23, options = {24, 34}},
    [24] = {step = 25, options = {26, 28, 30, 32}},
    [34] = {step = 35, options = {36, 38, 40}},
}
local outcomes = {
    [8] = {condition = Insane, label = "Insanity"},
    [10] = {variable = EarthResistanceBonus, value = 20, label = "Earth resistance bonus"},
    [12] = {damage = 50},
    [14] = {condition = Cursed, label = "Curse"},
    [18] = {variable = Gold, value = 250, label = "Party gold"},
    [20] = {condition = Dead, label = "Death"},
    [26] = {variable = MightBonus, value = 20, label = "Temporary Might"},
    [28] = {variable = IntellectBonus, value = 20, label = "Temporary Intellect"},
    [30] = {variable = PersonalityBonus, value = 20, label = "Temporary Personality"},
    [32] = {variable = EnduranceBonus, value = 20, label = "Temporary Endurance"},
    [36] = {variable = AccuracyBonus, value = 20, label = "Temporary Accuracy"},
    [38] = {variable = SpeedBonus, value = 20, label = "Temporary Speed"},
    [40] = {variable = LuckBonus, value = 20, label = "Temporary Luck"},
}

ReplaceMapEvent(459, "Well", function()
    if not IsAtLeast(Gold, price) then
        if IsAtLeast(Gold, price - 1) then
            SubtractValue(Gold, price - 1)
            evt.StatusText("You make a wish")
        end
        return
    end
    SubtractValue(Gold, price)
    local id = 0
    while choices[id] do
        local choice = choices[id]
        id = PickRandomOption(459, choice.step, choice.options)
    end
    local outcome = assert(outcomes[id], "Unknown Bracada wishing-well outcome")
    if outcome.condition then
        -- Setting a condition with the original zero operand applies it; this is not a cure.
        SetValue(outcome.condition, 0)
    elseif outcome.damage then
        evt.DamagePlayer(Players.Current, const.Damage.Air, outcome.damage)
    else
        AddValue(outcome.variable, outcome.value)
    end
    evt.StatusText("You make a wish")
end, "Well")

local reference = {
    string.format("Party payment: %d gold for a random outcome.", price),
    string.format("With exactly %d gold, the party pays that amount without receiving a random outcome.", price - 1),
    string.format("Below %d gold there is no payment or effect.", price - 1),
    "Character effects concern the selected character; gold concerns the party.",
    "Random outcomes follow the interaction definition; their probabilities are not shown:",
}
local function describe(id)
    local choice = choices[id]
    if choice then
        for _, child in ipairs(choice.options) do
            describe(child)
        end
        return
    end
    local outcome = assert(outcomes[id])
    local text
    if outcome.condition then
        text = "Can apply: " .. outcome.label .. "."
    elseif outcome.damage then
        text = string.format("Damage: %d to the selected character.", outcome.damage)
    else
        text = string.format("%s +%d.", outcome.label, outcome.value)
    end
    reference[#reference + 1] = "- " .. text
end
describe(0)
SetMapContextAction(459, {kind = "well", source = "script"})
SetMapSiteReference(459, table.concat(reference, "\n"))
