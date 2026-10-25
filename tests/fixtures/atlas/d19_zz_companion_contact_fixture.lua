-- QA-only: a remote map can unlock a different roster invitation at an existing resident NPC.
-- No physical site or automatic trigger calls this callback. Atlas indexing must never execute it.
RegisterEvent(65400, "Remote companion contact fixture", function()
    evt.SetNPCTopic(28, 1, 630)
end)
SetMapNpcTopicRefs(65400, {{28, 630}})
