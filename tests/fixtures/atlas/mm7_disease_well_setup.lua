-- Test preparation only: mount in an isolated Atlas fixture, never in the normal runtime.
RegisterMapOnLoadEvent(65050, "Atlas disease-well fixture", function()
    if IsAtLeast(MapVar(74), 1) then
        return
    end
    SetValue(MapVar(74), 1)
    evt.ForPlayer(0)
    SetValue(DiseasedGreen, 1)
    SetValue(Cursed, 1)
    SetValue(CurrentHealth, 20)
    evt.ForPlayer(1)
    SetValue(DiseasedRed, 1)
    SetValue(Cursed, 1)
    SetValue(CurrentHealth, 20)
    evt.ForPlayer(Players.Current)
end)
