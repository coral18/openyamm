-- QA-only: use the existing gold-well surface to test actual synchronous event spell receipts.
ReplaceMapEvent(223, "Drink from the Well", function()
    evt.CastSpell(38, 5, 3, 0, 0, 0, 0, 0, 0)
    evt.CastSpell(46, 5, 3, 0, 0, 0, 0, 0, 0)
end)
SetMapContextAction(223, {kind = "well", source = "script"})
