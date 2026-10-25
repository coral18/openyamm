-- QA-only: use the existing Dagger Wound well to test actual synchronous event spell receipts.
ReplaceMapEvent(101, "Drink from the well", function()
    evt.CastSpell(38, 5, 3, 0, 0, 0, 0, 0, 0)
    evt.CastSpell(46, 5, 3, 0, 0, 0, 0, 0, 0)
end)
SetMapContextAction(101, {kind = "well", source = "script"})
