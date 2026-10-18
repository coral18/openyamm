#pragma once

#include "game/tables/HouseTable.h"
#include "game/world/WorldFloorPlan.h"
#include "game/world/WorldMapExploration.h"
#include "game/world/WorldMapFeatures.h"
#include "game/world/WorldMonsterKnowledge.h"
#include "game/world/WorldMapNotes.h"
#include "game/world/WorldNpcKnowledge.h"
#include "game/world/WorldTransportKnowledge.h"
#include "game/tables/MapStats.h"
#include "game/tables/MergedBaseTables.h"
#include "game/tables/NpcDialogTable.h"

#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace OpenYAMM::Game
{
struct OutdoorMapData;
class ScriptedEventProgram;
enum class WorldRecordKind { Region, Dungeon, House, Npc, Trainer, Travel };

// A physical interaction site in map geometry, referring to an existing house or dungeon record.
// Several faces of the same door share one site; distinct doors remain separate.
struct WorldMapLocation
{
    std::string id;
    std::string mapId;
    std::string recordId;
    uint16_t eventId = 0;
    float x = 0, y = 0;
};

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
    std::optional<WorldTransportEntry> transport;
    const MapEdgeTransition *walkingRoute = nullptr;
    std::optional<MapBoundaryEdge> walkingEdge;
    const MapStatsEntry *destination = nullptr;
    std::optional<std::pair<float, float>> position;

    std::optional<std::string> npcName;
    bool lastObservedNpc = false;
    std::string name() const;
    std::string searchText() const;
};

struct WorldWalkingLink
{
    std::string recordId, sourceId, destinationId;
    std::pair<float, float> source, destination;
    bool locked = false;
};

struct WorldKnowledge
{
    std::unordered_map<std::string, int32_t> variables;
    // Existing map snapshots provide visited locations for campaigns saved before discovery tracking.
    std::unordered_set<std::string> visitedMaps;
    std::unordered_set<std::string> informedRecords;
    std::unordered_map<std::string, WorldFloorPlanKnowledge> floorPlans;
    std::unordered_map<std::string, WorldRegionMapKnowledge> regionMaps;
    std::optional<WorldPartyMapPosition> partyPosition;
    std::unordered_set<uint32_t> autonoteIds;
    std::unordered_map<std::string, std::unordered_set<std::string>> interactedSites;
    std::unordered_map<std::string, std::vector<WorldMonsterObservation>> monsterObservations;
    std::unordered_map<std::string, std::vector<WorldMapNote>> notes;
    WorldNpcState currentNpcs;
    WorldTransportState currentTransport;
    std::unordered_set<uint32_t> questBits;
    bool knows(const WorldRecord &record) const;
};

// A read-only index over the authoritative tables. The tables must outlive this index.
// Intended for Atlas, future editors and other consumers of world information.
class WorldDatabase
{
public:
    WorldDatabase(const MapStats &maps, const HouseTable &houses, const NpcDialogTable &npcs,
                  const MergedTeacherTopicTable &teachers, const std::string &worldId,
                  const std::vector<WorldMapPresentation> &presentation = {}, const WorldNpcState &npcsState = {},
                  const WorldTransportState &transportState = {});

    const std::vector<WorldRecord> &records() const;
    const std::vector<std::string> &transportIssues() const;
    const WorldRecord *find(const std::string &id) const;
    std::vector<const WorldRecord *> query(const WorldKnowledge &knowledge, bool guide,
        const std::string &search = {}, std::optional<WorldRecordKind> kind = {},
        const std::string &withinMap = {}, bool includeTravel = true) const;
    bool visible(const WorldRecord &record, const WorldKnowledge &knowledge, bool guide) const;
    bool walkingRouteUnlocked(const WorldRecord &record, const WorldKnowledge &knowledge) const;
    std::vector<WorldWalkingLink> walkingLinks(const WorldKnowledge &knowledge, bool guide) const;
    void indexMapLocations(const OutdoorMapData &map, const ScriptedEventProgram &events);
    void indexMapLocations(const MapStatsEntry &map, const IndoorMapData &geometry,
                           const ScriptedEventProgram &events);
    void indexMapMonsters(const MapStatsEntry &map, const OutdoorMapData &geometry, const MonsterTable &monsters,
                          const std::vector<WorldMonsterObservation> &observations = {});
    void indexMapMonsters(const MapStatsEntry &map, const IndoorMapData &geometry, const MonsterTable &monsters,
                          const std::vector<WorldMonsterObservation> &observations = {});
    void indexMapNotes(const WorldKnowledge &knowledge);
    const std::vector<WorldMapLocation> &locations() const;
    const std::vector<WorldMapFeature> &features() const;
    bool featureVisible(const WorldMapFeature &feature, const WorldKnowledge &knowledge, bool guide) const;
    std::vector<const WorldMapFeature *> queryFeatures(const WorldKnowledge &knowledge, bool guide,
        const std::string &search = {}, const std::string &withinMap = {}) const;
    bool locationVisible(const WorldMapLocation &location, const WorldKnowledge &knowledge, bool guide) const;
    std::vector<const WorldRecord *> recordsAtLocation(const WorldMapLocation &location,
        const WorldKnowledge &knowledge, bool guide) const;

    static std::string mapId(const MapStatsEntry &map);
    static std::string mapDiscoveryKey(const MapStatsEntry &map);
    static std::string houseDiscoveryKey(uint32_t id);
    static std::string npcDiscoveryKey(uint32_t id);
    static const char *kindName(WorldRecordKind kind);

private:
    void indexMapSites(const WorldRecord &source, const std::vector<WorldInteractionSite> &sites,
        const ScriptedEventProgram &events);
    void replaceMapMonsterFeatures(const MapStatsEntry &map, std::vector<WorldMapFeature> features,
        const MonsterTable &monsters, const std::vector<WorldMonsterObservation> &observations);
    std::vector<WorldRecord> m_records;
    std::vector<std::string> m_transportIssues;
    std::unordered_map<std::string, size_t> m_byId;
    std::vector<WorldMapLocation> m_locations;
    std::vector<WorldMapFeature> m_features;
};
} // namespace OpenYAMM::Game
