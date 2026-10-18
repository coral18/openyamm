#include "game/world/WorldTransportKnowledge.h"
#include "game/events/EventRuntime.h"
#include "game/gameplay/TravelRuntime.h"
#include "game/maps/MapIdentity.h"
#include "game/tables/MapStats.h"
#include "game/StringUtils.h"

#include <algorithm>
#include <bit>
#include <charconv>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace OpenYAMM::Game
{
namespace
{
std::string mapKey(const MapStatsEntry &map)
{
    return map.canonicalId.empty() ? buildCanonicalMapId(map.worldId, map.fileName) : map.canonicalId;
}

int32_t weekdayMask(const HouseEntry::TransportRoute &route)
{
    int32_t mask = 0;
    for (size_t day = 0; day < route.daysAvailable.size(); ++day)
        if (route.daysAvailable[day]) mask |= 1 << day;
    return mask;
}

bool readId(std::string_view text, uint32_t &value)
{
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    return parsed.ec == std::errc() && parsed.ptr == text.data() + text.size()
        && text == std::to_string(value);
}
}

std::string worldTransportKey(uint32_t houseId, uint32_t routeIndex)
{
    return "openyamm.discovery.transport/" + std::to_string(houseId) + "/" + std::to_string(routeIndex) + "/";
}

void recordWorldTransportObservation(EventRuntimeState &state, uint32_t houseId,
    const TransportOffer &offer, const std::vector<MapStatsEntry> &maps)
{
    const auto destination = std::find_if(maps.begin(), maps.end(), [&](const MapStatsEntry &map)
    { return toLowerCopy(map.fileName) == toLowerCopy(offer.route.mapFileName); });
    if (houseId == 0 || destination == maps.end() || offer.quote.travelDays < 1 || offer.quote.gold < 0
        || offer.route.travelDays > uint32_t(std::numeric_limits<int32_t>::max()) || weekdayMask(offer.route) == 0)
        throw std::invalid_argument("Invalid transport observation");
    const std::string prefix = worldTransportKey(houseId, offer.route.routeIndex);
    // Explicit zeros retire previous destinations through the existing global-state merge.
    for (auto &[key, value] : state.namedGlobalVars)
        if (key.starts_with(prefix)) value = 0;
    state.namedGlobalVars[prefix + "seen"] = 1;
    state.namedGlobalVars[prefix + "map/" + mapKey(*destination)] = 1;
    // A quoted destination is learned geography, not visited or explored terrain.
    state.namedGlobalVars["openyamm.discovery." + mapKey(*destination)] = 1;
    state.namedGlobalVars[prefix + "days"] = int32_t(offer.route.travelDays);
    state.namedGlobalVars[prefix + "week"] = weekdayMask(offer.route);
    state.namedGlobalVars[prefix + "required"] = std::bit_cast<int32_t>(offer.route.requiredQBit);
    state.namedGlobalVars[prefix + "quoted_days"] = offer.quote.travelDays;
    state.namedGlobalVars[prefix + "gold"] = offer.quote.gold;
}

WorldTransportState worldObservedTransport(const HouseTable &houses, const MapStats &maps,
    const std::unordered_map<std::string, int32_t> &variables)
{
    WorldTransportState result;
    result.observedOnly = true;
    const std::string root = "openyamm.discovery.transport/";
    for (const auto &[key, value] : variables)
    {
        if (value != 1 || !key.starts_with(root) || !key.ends_with("/seen")) continue;
        const std::string_view ids(key.data() + root.size(), key.size() - root.size() - 5);
        const size_t slash = ids.find('/');
        uint32_t houseId = 0, routeIndex = 0;
        if (slash == std::string_view::npos || !readId(ids.substr(0, slash), houseId)
            || !readId(ids.substr(slash + 1), routeIndex) || houses.get(houseId) == nullptr)
        {
            result.issues.push_back("Unknown transport observation: " + key);
            continue;
        }
        const std::string prefix = worldTransportKey(houseId, routeIndex);
        const MapStatsEntry *pDestination = nullptr;
        size_t destinations = 0;
        for (const MapStatsEntry &map : maps.getEntries())
        {
            const auto known = variables.find(prefix + "map/" + mapKey(map));
            if (known != variables.end() && known->second == 1)
            {
                pDestination = &map;
                ++destinations;
            }
        }
        const auto field = [&](const char *pName) -> std::optional<int32_t>
        {
            const auto found = variables.find(prefix + pName);
            return found == variables.end() ? std::nullopt : std::optional(found->second);
        };
        const auto days = field("days"), week = field("week"), required = field("required");
        const auto quoted = field("quoted_days"), gold = field("gold");
        if (destinations != 1 || !days || *days < 0 || !week || *week < 1 || *week > 127
            || !required || !quoted || *quoted < 1 || !gold || *gold < 0)
        {
            result.issues.push_back("Invalid transport observation: " + prefix);
            continue;
        }
        WorldTransportEntry entry;
        entry.observed = true;
        entry.route.routeIndex = routeIndex;
        entry.route.mapFileName = pDestination->fileName;
        entry.route.destinationName = pDestination->name;
        entry.route.travelDays = uint32_t(*days);
        entry.route.requiredQBit = std::bit_cast<uint32_t>(*required);
        for (size_t day = 0; day < entry.route.daysAvailable.size(); ++day)
            entry.route.daysAvailable[day] = (*week & (1 << day)) != 0;
        entry.quote = TransportQuote{*quoted, *gold};
        result.houses[houseId].push_back(std::move(entry));
    }
    return result;
}

WorldTransportState worldCurrentTransport(const HouseTable &houses, uint32_t mapId, const EventRuntimeState &state)
{
    WorldTransportState result;
    for (const auto &[id, house] : houses.entries())
    {
        if (house.mapId != mapId || house.transportRoutes.empty()) continue;
        std::vector<WorldTransportEntry> &routes = result.houses[id];
        for (const HouseEntry::TransportRoute &route : house.transportRoutes)
            routes.push_back({effectiveTransportRoute(house, route, &state), {}, false, true});
    }
    return result;
}

bool worldTransportObservationMatches(uint32_t houseId, const WorldTransportEntry &entry,
    const std::string &canonicalDestination,
    const std::unordered_map<std::string, int32_t> &variables)
{
    if (!entry.observed || !entry.quote || entry.route.travelDays > uint32_t(std::numeric_limits<int32_t>::max()))
        return false;
    const std::string prefix = worldTransportKey(houseId, entry.route.routeIndex);
    const auto matches = [&](const std::string &field, int32_t expected)
    {
        const auto found = variables.find(prefix + field);
        return found != variables.end() && found->second == expected;
    };
    return matches("seen", 1) && matches("map/" + canonicalDestination, 1)
        && matches("days", int32_t(entry.route.travelDays)) && matches("week", weekdayMask(entry.route))
        && matches("required", std::bit_cast<int32_t>(entry.route.requiredQBit))
        && matches("quoted_days", entry.quote->travelDays) && matches("gold", entry.quote->gold);
}
}
