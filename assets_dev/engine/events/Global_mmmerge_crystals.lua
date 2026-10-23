-- MMerge crystal decoration harvesting.

local mmmergeCrystalEvents = {
    {event = 65310, item = 2060}, -- Amethyst
    {event = 65311, item = 2056}, -- Diamond
    {event = 65312, item = 2059}, -- Ruby
    {event = 65313, item = 2065}, -- Sapphire
    {event = 65314, item = 2057}, -- Moonstone
    {event = 65315, item = 2062}, -- Purple Topaz
    {event = 65316, item = 2064}, -- Emerald
}

local function harvestMMergeCrystal(itemId)
    if IsAtLeast(PerceptionSkill, 7) then
        GiveItem(itemId)
        evt.ChangeEvent(0)
        evt.FaceAnimation(FaceAnimation.FoundItem)
        return
    end

    evt.FaceAnimation(FaceAnimation.Tired)
end

for _, crystal in ipairs(mmmergeCrystalEvents) do
    RegisterGlobalEvent(crystal.event, "Crystals", function()
        harvestMMergeCrystal(crystal.item)
    end)
    SetGlobalItemRewards(crystal.event, {crystal.item})
end
