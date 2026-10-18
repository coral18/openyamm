-- QA-only global overlay: the normal Expert Bow offer must remain undiscovered when its topic is hidden.
RegisterCanShowTopic(315, function()
    return false
end)
