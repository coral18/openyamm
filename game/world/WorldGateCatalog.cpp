#include "game/world/WorldGateCatalog.h"
#include "game/world/WorldDatabase.h"
#include "game/party/Party.h"
#include "game/tables/MapStats.h"
#include "game/tables/MergedBaseTables.h"

#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>
#include <string_view>
#include <yaml-cpp/yaml.h>

namespace OpenYAMM::Game
{
namespace
{
void fields(const YAML::Node &node, std::initializer_list<std::string_view> allowed)
{
    if (!node.IsMap()) throw std::invalid_argument("Expected a gate data mapping");
    std::set<std::string> used;
    for (const auto &entry : node)
    {
        const std::string key = entry.first.as<std::string>();
        if (!used.insert(key).second || std::find(allowed.begin(), allowed.end(), key) == allowed.end())
            throw std::invalid_argument("Unknown or duplicate gate field: " + key);
    }
}

std::string string(const YAML::Node &node)
{
    if (!node.IsScalar()) throw std::invalid_argument("Expected a gate identifier");
    const std::string value = node.as<std::string>();
    if (value.empty() || value.size() > 160 || value.find("..") != std::string::npos
        || value.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._/:-")
            != std::string::npos) throw std::invalid_argument("Invalid gate identifier: " + value);
    return value;
}

int32_t integer(const YAML::Node &node)
{
    if (!node.IsScalar()) throw std::invalid_argument("Expected a gate integer");
    const int64_t value = node.as<int64_t>();
    if (value < INT32_MIN || value > INT32_MAX) throw std::invalid_argument("Gate integer out of range");
    return int32_t(value);
}

void sequence(const YAML::Node &node, size_t minimum, size_t maximum)
{
    if (!node.IsSequence() || node.size() < minimum || node.size() > maximum)
        throw std::invalid_argument("Invalid gate data array size");
}
}

bool WorldGateCatalog::loadFromText(const std::string &text, const MapStats &maps,
    const MergedTownPortalSwitchTable &portals, std::string &error)
{
    m_networks.clear();
    m_defaultNetwork.clear();
    error.clear();
    try
    {
        const YAML::Node root = YAML::Load(text);
        fields(root, {"version", "default_network", "networks"});
        if (integer(root["version"]) != 1) throw std::invalid_argument("Unsupported gate catalog version");
        const std::string defaultId = string(root["default_network"]);
        sequence(root["networks"], 1, 64);
        std::vector<WorldGateNetwork> networks;
        std::set<std::string> ids;
        for (const YAML::Node &entry : root["networks"])
        {
            fields(entry, {"id", "title", "portal_group", "unlock_variable", "unlock_description",
                "background", "destinations"});
            WorldGateNetwork network;
            network.id = string(entry["id"]);
            if (!ids.insert(network.id).second) throw std::invalid_argument("Duplicate gate network: " + network.id);
            if (network.id.size() > 128
                || network.id.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789._-") != std::string::npos
                || network.id.front() == '.' || network.id.back() == '.')
                throw std::invalid_argument("Invalid namespaced gate network ID");
            network.title = entry["title"].as<std::string>();
            if (network.title.empty() || network.title.size() > 128 || network.title.find('\0') != std::string::npos)
                throw std::invalid_argument("Invalid gate title");
            network.unlockVariable = string(entry["unlock_variable"]);
            network.unlockDescription = entry["unlock_description"].as<std::string>();
            if (network.unlockDescription.empty() || network.unlockDescription.size() > 1024
                || network.unlockDescription.find('\0') != std::string::npos)
                throw std::invalid_argument("Invalid gate unlock description");
            network.background = string(entry["background"]);
            const std::string groupName = string(entry["portal_group"]);
            if (groupName.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-")
                != std::string::npos) throw std::invalid_argument("Invalid gate portal group ID");
            const auto group = std::find_if(portals.groups().begin(), portals.groups().end(),
                [&](const MergedTownPortalSwitchGroup &value) { return value.name == groupName; });
            if (group == portals.groups().end()) throw std::invalid_argument("Unknown gate portal group: " + groupName);
            sequence(entry["destinations"], 1, 64);
            std::set<uint32_t> destinations;
            for (const YAML::Node &item : entry["destinations"])
            {
                fields(item, {"source_id", "icon", "hit_rect", "landings"});
                const int32_t sourceId = integer(item["source_id"]);
                if (sourceId < 0 || !destinations.insert(uint32_t(sourceId)).second)
                    throw std::invalid_argument("Invalid or duplicate gate destination");
                const auto source = std::find_if(group->destinations.begin(), group->destinations.end(),
                    [&](const MergedTownPortalDestination &value) { return value.id == uint32_t(sourceId); });
                if (source == group->destinations.end() || source->description.empty())
                    throw std::invalid_argument("Unknown gate destination source");
                WorldGateDestination destination;
                destination.id = groupName + ":" + std::to_string(sourceId);
                destination.label = source->description;
                destination.icon = string(item["icon"]);
                destination.iconX = source->iconX;
                destination.iconY = source->iconY;
                destination.iconWidth = source->iconWidth.value_or(0);
                destination.iconHeight = source->iconHeight.value_or(0);
                sequence(item["hit_rect"], 4, 4);
                destination.hitX = integer(item["hit_rect"][0]);
                destination.hitY = integer(item["hit_rect"][1]);
                const int32_t width = integer(item["hit_rect"][2]), height = integer(item["hit_rect"][3]);
                if (destination.hitX < 0 || destination.hitY < 0 || width <= 0 || height <= 0
                    || int64_t(destination.hitX) + width > 8192 || int64_t(destination.hitY) + height > 8192)
                    throw std::invalid_argument("Invalid gate destination hit rectangle");
                destination.hitWidth = uint32_t(width);
                destination.hitHeight = uint32_t(height);
                sequence(item["landings"], 1, 32);
                std::set<uint32_t> bits;
                for (const YAML::Node &landingNode : item["landings"])
                {
                    fields(landingNode, {"map", "position", "direction", "map_start", "requires_qbit"});
                    WorldGateLanding landing;
                    landing.mapId = string(landingNode["map"]);
                    const auto map = std::find_if(maps.getEntries().begin(), maps.getEntries().end(),
                        [&](const MapStatsEntry &value)
                        { return WorldDatabase::mapId(value) == landing.mapId; });
                    if (map == maps.getEntries().end())
                        throw std::invalid_argument("Unknown gate map: " + landing.mapId);
                    landing.mapFileName = map->fileName;
                    landing.worldId = map->worldId;
                    landing.mapLabel = map->name;
                    if (landingNode["requires_qbit"])
                    {
                        const int32_t bit = integer(landingNode["requires_qbit"]);
                        if (bit <= 0) throw std::invalid_argument("Invalid gate quest bit");
                        landing.requiredQuestBit = uint32_t(bit);
                    }
                    if (!bits.insert(landing.requiredQuestBit).second
                        || (landing.requiredQuestBit == 0
                            && destination.landings.size() + 1 != item["landings"].size()))
                        throw std::invalid_argument("Gate landing branches require distinct bits and a final default");
                    if (landingNode["map_start"]) landing.useMapStartPosition = landingNode["map_start"].as<bool>();
                    if (landingNode["position"])
                    {
                        if (landing.useMapStartPosition) throw std::invalid_argument("Ambiguous gate landing position");
                        sequence(landingNode["position"], 3, 3);
                        landing.x = integer(landingNode["position"][0]);
                        landing.y = integer(landingNode["position"][1]);
                        landing.z = integer(landingNode["position"][2]);
                    }
                    else if (!landing.useMapStartPosition) throw std::invalid_argument("Missing gate landing position");
                    if (landingNode["direction"]) landing.directionDegrees = integer(landingNode["direction"]);
                    destination.landings.push_back(std::move(landing));
                }
                if (!bits.contains(0)) throw std::invalid_argument("Gate destination has no default landing");
                network.destinations.push_back(std::move(destination));
            }
            networks.push_back(std::move(network));
        }
        if (!ids.contains(defaultId)) throw std::invalid_argument("Unknown default gate network");
        m_networks = std::move(networks);
        m_defaultNetwork = defaultId;
        return true;
    }
    catch (const std::exception &exception)
    {
        error = exception.what();
        return false;
    }
}

const WorldGateNetwork *WorldGateCatalog::find(const std::string &id) const
{
    const auto found = std::find_if(m_networks.begin(), m_networks.end(),
        [&](const WorldGateNetwork &network) { return network.id == id; });
    return found == m_networks.end() ? nullptr : &*found;
}

const WorldGateNetwork *WorldGateCatalog::defaultNetwork() const { return find(m_defaultNetwork); }

std::optional<WorldGateChoice> WorldGateCatalog::match(const std::string &networkId, const std::string &choiceId,
    const std::string &mapId) const
{
    const WorldGateNetwork *pNetwork = find(networkId);
    if (pNetwork == nullptr) return std::nullopt;
    for (const WorldGateDestination &destination : pNetwork->destinations)
        if (destination.id == choiceId)
            for (const WorldGateLanding &landing : destination.landings)
                if (landing.mapId == mapId) return WorldGateChoice{&destination, &landing};
    return std::nullopt;
}

std::vector<WorldGateChoice> WorldGateCatalog::choices(const WorldGateNetwork &network, const Party *pParty) const
{
    std::vector<WorldGateChoice> result;
    for (const WorldGateDestination &destination : network.destinations)
        for (const WorldGateLanding &landing : destination.landings)
            if (landing.requiredQuestBit == 0 || (pParty != nullptr && pParty->hasQuestBit(landing.requiredQuestBit)))
            {
                result.push_back({&destination, &landing});
                break;
            }
    return result;
}

bool WorldGateCatalog::unlocked(const WorldGateNetwork &network,
    const std::unordered_map<std::string, int32_t> &variables) const
{
    const auto found = variables.find(network.unlockVariable);
    return found != variables.end() && found->second != 0;
}
}
