-- MMMerge supplement: Escaton's Crystal can be reused after the Conflux Key opened it once.

ReplaceMapEvent(104, "Dimensional travel", function()
    SetQBit(QBit(302)) -- Discover Ravenshore for Town Portal before opening travel.
    MM8.OpenDimensionDoor()
end, "Dimensional travel")
evt.meta.map.contextActions[104] = {kind = "teleport", targetName = "Dimensional travel"}

ReplaceMapEvent(504, "Enter Escaton's Crystal", function()
    evt.ForPlayer(Players.All)

    if HasItem(610) or MM8.GetMapFlag("CrystalOpened") then
        MM8.SetMapFlag("CrystalOpened", true)
        evt.ForPlayer(Players.Current)
        evt.MoveToMap(-1024, -1626, 0, 520, 0, 0, 355, 1, "d10.blv")
        return
    end

    evt.ForPlayer(Players.Current)
    evt.FaceAnimation(FaceAnimation.DoorLocked)
end, "Enter Escaton's Crystal")
