-- QA-only: queued gifts arrive after the inherited callback's immediate-effects snapshot.
local function deliver()
    evt.ChangeEvent(0)
    evt.SetHeldItem(1, {identified = false})
    evt.SetHeldItem(1, {identified = false})
end
ReplaceGlobalEvent(551, "Beacon fire", deliver)
ReplaceGlobalEvent(269, "Red Barrel", deliver)
