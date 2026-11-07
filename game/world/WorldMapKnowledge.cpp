#include "game/world/WorldMapKnowledge.h"
#include "game/world/WorldGateCatalog.h"
#include "game/world/WorldDatabase.h"
#include "game/events/EventRuntime.h"
#include "game/StringUtils.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <tuple>

namespace OpenYAMM::Game
{
std::string worldHouseAddressVersionKey(uint32_t houseId)
{
    if (houseId == 0) throw std::invalid_argument("Invalid observed house ID");
    return "openyamm.discovery.house-address.v1/" + std::to_string(houseId) + "/seen";
}

std::string worldHouseAddressKey(uint32_t houseId, const std::string &mapId)
{
    if (houseId == 0 || !mapId.starts_with("world."))
        throw std::invalid_argument("Invalid observed house address");
    return "openyamm.discovery.house-address.v1/" + std::to_string(houseId) + "/map/" + mapId;
}

bool worldHouseAddressesRecorded(const std::unordered_map<std::string, int32_t> &variables, uint32_t houseId)
{
    const auto found = variables.find(worldHouseAddressVersionKey(houseId));
    return found != variables.end() && found->second == 1;
}

bool worldHouseAddressKnown(const std::unordered_map<std::string, int32_t> &variables,
    uint32_t houseId, const std::string &mapId)
{
    const auto found = variables.find(worldHouseAddressKey(houseId, mapId));
    return found != variables.end() && found->second == 1;
}

void recordWorldHouseVisit(EventRuntimeState &state, uint32_t houseId, const MapStatsEntry &location,
    const MapStatsEntry *pLegacyPrimary)
{
    const std::string globalKey = WorldDatabase::houseDiscoveryKey(houseId);
    const std::string versionKey = worldHouseAddressVersionKey(houseId);
    const std::string addressKey = worldHouseAddressKey(houseId, WorldDatabase::mapId(location));
    const auto old = state.namedGlobalVars.find(globalKey);
    if (!worldHouseAddressesRecorded(state.namedGlobalVars, houseId)
        && old != state.namedGlobalVars.end() && old->second > 0 && pLegacyPrimary != nullptr)
    {
        // Earlier saves resolved this global flag to the table's primary map. Preserve that interpretation
        // explicitly before adding richer address history; browsing Guide never performs this migration.
        state.namedGlobalVars[worldHouseAddressKey(houseId, WorldDatabase::mapId(*pLegacyPrimary))] = 1;
    }
    state.namedGlobalVars[addressKey] = 1;
    state.namedGlobalVars[versionKey] = 1;
    state.namedGlobalVars[globalKey] = 1;
}

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
    refilled.observedGeneratedNpcs = previous.observedGeneratedNpcs;
    for (const auto &[key, value] : previous.namedMapVars)
        if (key.starts_with("openyamm.discovery.") || key.starts_with("openyamm.map-notes.user/"))
            refilled.namedMapVars[key] = value;
}
}
