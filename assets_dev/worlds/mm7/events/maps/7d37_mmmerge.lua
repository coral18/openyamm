-- MMMerge map supplement: Haunted Mansion portrait pickup persistence.

local function applyPortraitTakenState()
    evt.SetTexture(15, "t2bs")
    evt.SetMapVar("PortraitTaken", 1)
end

ReplaceMapEvent(376, "Portrait", function()
    if IsQBitSet(QBit(778)) then -- Took Angel Painting
        return
    end

    if not HasItemAnywhere(1423) then -- Angel Statue Painting
        AddValue(InventoryItem(1423), 1423)
    end

    SetQBit(QBit(778)) -- Took Angel Painting
    applyPortraitTakenState()
end, "Portrait")
SetMapItemRewards(376, {1423})

RegisterMapOnLoadEvent(65037, "Restore Haunted Mansion portrait state", function()
    if IsQBitSet(QBit(778)) then -- Took Angel Painting
        applyPortraitTakenState()
    end
end)
