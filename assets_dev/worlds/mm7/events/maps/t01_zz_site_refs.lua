-- Read-only reference for the authored mechanism puzzle; load after gameplay overlays.
SetMapSiteReference(12,
    "Requires prior activation of all three linked door mechanisms in this location.\n"
    .. "Their current door positions do not matter; using them again retains that progress.\n"
    .. "Once ready, each use of this altar toggles a local door.", nil, {4, 6, 11})
