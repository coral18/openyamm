#pragma once

#include "game/tables/HouseTable.h"
#include "game/world/WorldFloorPlan.h"
#include "game/world/WorldMapExploration.h"
#include "game/tables/MapStats.h"
#include "game/tables/MergedBaseTables.h"
#include "game/tables/NpcDialogTable.h"

#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace OpenYAMM::Game
{
enum class WorldRecordKind { Region, Dungeon, House, Npc, Trainer, Travel };

// Presentation metadata only. Names, services, requirements and schedules belong to the game tables.
struct WorldMapPresentation
{
    std::string fileName;
    std::string parentFileName;
    int areaId = 0;
    std::optional<std::pair<float, float>> position;
};

struct WorldRecord
{
    std::string id;
    std::string parentId;
    WorldRecordKind kind = WorldRecordKind::Region;
    const MapStatsEntry *map = nullptr;
    const HouseEntry *house = nullptr;
    const NpcEntry *npc = nullptr;
    const MergedTeacherTopicEntry *teacher = nullptr;
    const HouseEntry::TransportRoute *route = nullptr;
    const MapEdgeTransition *walkingRoute = nullptr;
    std::optional<MapBoundaryEdge> walkingEdge;
    const MapStatsEntry *destination = nullptr;
    std::optional<std::pair<float, float>> position;

    std::string name() const;
    std::string searchText() const;
};

struct WorldKnowledge
{
    std::unordered_map<std::string, int32_t> variables;
    // Existing map snapshots provide visited locations for campaigns saved before discovery tracking.
    std::unordered_set<std::string> visitedMaps;
    std::unordered_map<std::string, WorldFloorPlanKnowledge> floorPlans;
    std::unordered_map<std::string, WorldRegionMapKnowledge> regionMaps;
    std::optional<WorldPartyMapPosition> partyPosition;
    bool knows(const WorldRecord &record) const;
};

// A read-only index over the authoritative tables. The tables must outlive this index.
// Intended for Atlas, future editors and other consumers of world information.
class WorldDatabase
{
public:
    WorldDatabase(const MapStats &maps, const HouseTable &houses, const NpcDialogTable &npcs,
                  const MergedTeacherTopicTable &teachers, const std::string &worldId,
                  const std::vector<WorldMapPresentation> &presentation = {});

    const std::vector<WorldRecord> &records() const;
    const WorldRecord *find(const std::string &id) const;
    std::vector<const WorldRecord *> query(const WorldKnowledge &knowledge, bool guide,
        const std::string &search = {}, std::optional<WorldRecordKind> kind = {},
        const std::string &withinMap = {}, bool includeTravel = true) const;
    bool visible(const WorldRecord &record, const WorldKnowledge &knowledge, bool guide) const;

    static std::string mapId(const MapStatsEntry &map);
    static std::string mapDiscoveryKey(const MapStatsEntry &map);
    static std::string houseDiscoveryKey(uint32_t id);
    static std::string npcDiscoveryKey(uint32_t id);
    static const char *kindName(WorldRecordKind kind);

private:
    std::vector<WorldRecord> m_records;
    std::unordered_map<std::string, size_t> m_byId;
};
} // namespace OpenYAMM::Game
