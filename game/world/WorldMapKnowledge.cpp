#include "game/world/WorldMapKnowledge.h"
#include "game/events/EventRuntime.h"
#include "game/StringUtils.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <optional>

namespace OpenYAMM::Game
{
bool recordWorldMapArrival(EventRuntimeState &state, const std::string &sourceMapId, uint32_t eventId,
    float x, float y, float z, float gameMinutes)
{
    const auto coordinate = [](float value) -> std::optional<int32_t>
    {
        const double rounded = std::round(double(value));
        if (!std::isfinite(rounded) || rounded < INT32_MIN || rounded > INT32_MAX) return std::nullopt;
        return int32_t(rounded);
    };
    const auto px = coordinate(x), py = coordinate(y), pz = coordinate(z), time = coordinate(std::floor(gameMinutes));
    if (sourceMapId.empty() || state.mapFileName.empty() || eventId == 0 || eventId > UINT16_MAX
        || !px || !py || !pz || !time || gameMinutes < 0)
        return false;
    const std::string key = "openyamm.discovery.arrival/" + sourceMapId + "/event/" + std::to_string(eventId) + "/";
    state.namedMapVars[key + "seen"] = 1;
    state.namedMapVars[key + "x"] = *px;
    state.namedMapVars[key + "y"] = *py;
    state.namedMapVars[key + "z"] = *pz;
    state.namedMapVars[key + "time"] = *time;
    return true;
}

std::vector<WorldMapArrival> worldMapArrivals(const EventRuntimeState &state, const std::string &mapFileName)
{
    std::vector<WorldMapArrival> result;
    const std::string root = "openyamm.discovery.arrival/";
    if (mapFileName.empty()) return result;
    for (const auto &[key, value] : state.namedMapVars)
    {
        if (value != 1 || !key.starts_with(root) || !key.ends_with("/seen")) continue;
        const size_t separator = key.rfind("/event/");
        if (separator == std::string::npos || separator <= root.size()) continue;
        const std::string number = key.substr(separator + 7, key.size() - separator - 12);
        uint32_t eventId = 0;
        const auto parsed = std::from_chars(number.data(), number.data() + number.size(), eventId);
        if (parsed.ec != std::errc() || parsed.ptr != number.data() + number.size()
            || eventId == 0 || eventId > UINT16_MAX || number != std::to_string(eventId)) continue;
        const std::string prefix = key.substr(0, key.size() - 4);
        const auto field = [&](const char *pName) -> std::optional<int32_t>
        {
            const auto found = state.namedMapVars.find(prefix + pName);
            return found == state.namedMapVars.end() ? std::nullopt : std::optional<int32_t>(found->second);
        };
        const auto x = field("x"), y = field("y"), z = field("z"), time = field("time");
        if (!x || !y || !z || !time || *time < 0) continue;
        result.push_back({key.substr(root.size(), separator - root.size()), uint16_t(eventId),
            toLowerCopy(mapFileName), *x, *y, *z, *time});
    }
    std::sort(result.begin(), result.end(), [](const WorldMapArrival &a, const WorldMapArrival &b)
    { return std::pair{a.sourceMapId, a.eventId} < std::pair{b.sourceMapId, b.eventId}; });
    return result;
}

void restoreWorldMapKnowledgeAfterRefill(const EventRuntimeState &previous, EventRuntimeState &refilled)
{
    refilled.runtimeMapNotes = previous.runtimeMapNotes;
    for (const auto &[key, value] : previous.namedMapVars)
        if (key.starts_with("openyamm.discovery.") || key.starts_with("openyamm.map-notes.user/"))
            refilled.namedMapVars[key] = value;
}
}
