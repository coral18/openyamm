#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace OpenYAMM::Game
{
class MapStats;
class MergedTownPortalSwitchTable;
class Party;

struct WorldGateLanding
{
    std::string mapId;
    std::string mapFileName;
    std::string worldId;
    std::string mapLabel;
    int32_t x = 0, y = 0, z = 0;
    std::optional<int32_t> directionDegrees;
    bool useMapStartPosition = false;
    uint32_t requiredQuestBit = 0;
};

struct WorldGateDestination
{
    std::string id;
    std::string label;
    std::string icon;
    int32_t iconX = 0, iconY = 0;
    uint32_t iconWidth = 0, iconHeight = 0;
    int32_t hitX = 0, hitY = 0;
    uint32_t hitWidth = 0, hitHeight = 0;
    std::vector<WorldGateLanding> landings;
};

struct WorldGateNetwork
{
    std::string id;
    std::string title;
    std::string unlockVariable;
    std::string unlockDescription;
    std::string background;
    std::vector<WorldGateDestination> destinations;
};

struct WorldGateChoice
{
    const WorldGateDestination *pDestination = nullptr;
    const WorldGateLanding *pLanding = nullptr;
};

// Shared authored gate rules. Labels come from the existing portal table, geography from MapStats.
class WorldGateCatalog
{
public:
    bool loadFromText(const std::string &text, const MapStats &maps,
        const MergedTownPortalSwitchTable &portals, std::string &error);
    const WorldGateNetwork *find(const std::string &id) const;
    const WorldGateNetwork *defaultNetwork() const;
    std::optional<WorldGateChoice> match(const std::string &networkId, const std::string &choiceId,
        const std::string &mapId) const;
    std::vector<WorldGateChoice> choices(const WorldGateNetwork &network, const Party *pParty) const;
    bool unlocked(const WorldGateNetwork &network, const std::unordered_map<std::string, int32_t> &variables) const;

private:
    std::string m_defaultNetwork;
    std::vector<WorldGateNetwork> m_networks;
};
}
