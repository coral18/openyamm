#include "game/world/WorldMapKnowledge.h"
#include "game/world/WorldGateCatalog.h"
#include "game/events/EventRuntime.h"
#include "game/StringUtils.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <optional>
#include <tuple>

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

void recordWorldGateOffers(EventRuntimeState &state, uint16_t eventId, const std::string &networkId,
    const std::vector<WorldGateChoice> &choices, float gameMinutes)
{
    if (state.mapFileName.empty() || eventId == 0 || networkId.empty() || !std::isfinite(gameMinutes)
        || gameMinutes < 0 || double(gameMinutes) > INT32_MAX) return;
    const std::string root = "openyamm.discovery.gate/" + std::to_string(eventId) + "/" + networkId + "/";
    std::erase_if(state.namedMapVars, [&](const auto &entry) { return entry.first.starts_with(root); });
    for (const WorldGateChoice &choice : choices)
    {
        if (choice.pDestination == nullptr || choice.pLanding == nullptr) continue;
        const std::string key = root + choice.pDestination->id + "/" + choice.pLanding->mapId + "/";
        state.namedMapVars[key + "seen"] = 1;
        state.namedMapVars[key + "time"] = int32_t(std::floor(gameMinutes));
    }
}

std::vector<WorldGateOffer> worldGateOffers(const EventRuntimeState &state)
{
    std::vector<WorldGateOffer> result;
    const std::string root = "openyamm.discovery.gate/";
    for (const auto &[key, value] : state.namedMapVars)
    {
        if (value != 1 || !key.starts_with(root) || !key.ends_with("/seen")) continue;
        std::string_view rest(key.data() + root.size(), key.size() - root.size() - 5);
        std::vector<std::string> parts;
        while (true)
        {
            const size_t slash = rest.find('/');
            parts.emplace_back(rest.substr(0, slash));
            if (slash == std::string_view::npos) break;
            rest.remove_prefix(slash + 1);
        }
        if (parts.size() != 4) continue;
        uint32_t event = 0;
        const auto parsed = std::from_chars(parts[0].data(), parts[0].data() + parts[0].size(), event);
        if (parsed.ec != std::errc{} || parsed.ptr != parts[0].data() + parts[0].size()
            || event == 0 || event > UINT16_MAX || parts[0] != std::to_string(event)) continue;
        if (parts[1].empty() || parts[2].empty() || !parts[3].starts_with("world.")) continue;
        const auto time = state.namedMapVars.find(key.substr(0, key.size() - 4) + "time");
        if (time == state.namedMapVars.end() || time->second < 0) continue;
        result.push_back({uint16_t(event), parts[1], parts[2], parts[3], time->second});
    }
    std::sort(result.begin(), result.end(), [](const WorldGateOffer &a, const WorldGateOffer &b)
    { return std::tie(a.eventId, a.networkId, a.choiceId) < std::tie(b.eventId, b.networkId, b.choiceId); });
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
