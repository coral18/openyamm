-- Correct the authored Pure Accuracy quest without changing the generated legacy export.
-- Legacy 244 used completion bit 133 for acceptance; legacy 246 wrote Blazen's unrelated bit 134.
local migrationKey = "openyamm.migration.mm8.pure-accuracy-v1"

local function migrateAccuracyQuest()
    if evt.GetGlobalVar(migrationKey, 0) ~= 0 then return end

    evt.ForPlayer(Players.All)
    local active = IsQBitSet(QBit(132))
    local legacyActive = IsQBitSet(QBit(133))
    local blazen = IsQBitSet(QBit(134))
    local request = evt.GetNPCTopic(77, 0)
    local reward = evt.GetNPCTopic(77, 2)
    local originalFlags = (active and 1 or 0) + (legacyActive and 2 or 0) + (blazen and 4 or 0)
    evt.SetGlobalVar(migrationKey .. ".original-flags", originalFlags)
    evt.SetGlobalVar(migrationKey .. ".request-topic", request)
    evt.SetGlobalVar(migrationKey .. ".reward-topic", reward)

    local migrationCase = 3 -- Unclassified/modded state: preserve it, rather than guess progress.
    if not active and request == 0 and reward == 246 and legacyActive then
        SetQBit(QBit(132))
        ClearQBit(QBit(133))
        migrationCase = 1
    elseif not active and request == 0 and reward == 0 and not legacyActive and blazen then
        SetQBit(QBit(133))
        evt.RememberQuest(132)
        migrationCase = 2
    elseif not active and request == 244 and reward == 246 and not legacyActive then
        migrationCase = 0
    end

    -- 134 can have been set by either legacy reward. Never erase ambiguous Blazen progress.
    evt.SetGlobalVar(migrationKey .. ".case", migrationCase)
    evt.SetGlobalVar(migrationKey, 1)
end

RegisterGlobalOnLoadEvent(65024, "MM8 Pure Accuracy quest state migration", migrateAccuracyQuest)

ReplaceGlobalEvent(244, "Potion of Pure Accuracy", function()
    evt.ForPlayer(Players.All)
    if IsQBitSet(QBit(132)) or IsQBitSet(QBit(133)) then return end
    evt.SetMessage(Game.NPCText[689])
    SetQBit(QBit(132))
    evt.SetNPCTopic(77, 0, 0)
end)

RegisterCanShowTopic(244, function()
    return not IsQBitSet(QBit(132)) and not IsQBitSet(QBit(133))
end)

ReplaceGlobalEvent(246, "Do you have the Ingredients?", function()
    evt.ForPlayer(Players.All)
    if not IsQBitSet(QBit(132)) or IsQBitSet(QBit(133)) then return end
    local reagents = {{200, 204, 2}, {205, 209, 1}, {210, 214, 4}}
    for _, group in ipairs(reagents) do
        if not evt.CheckItemsCount(group[1], group[2], group[3]) then
            evt.SetMessage(Game.NPCText[684])
            return
        end
    end
    for _, group in ipairs(reagents) do
        evt.RemoveItems(group[1], group[2], group[3])
    end
    evt.SetMessage(Game.NPCText[691])
    ClearQBit(QBit(132))
    SetQBit(QBit(133))
    evt.ForPlayer(Players.Current)
    AddValue(InventoryItem(269), 269)
    evt.ForPlayer(Players.All)
    AddValue(Experience, 5000)
    evt.SetNPCTopic(77, 2, 0)
end)

RegisterCanShowTopic(246, function()
    return IsQBitSet(QBit(132)) and not IsQBitSet(QBit(133))
end)
SetGlobalItemRewards(246, {269})
