#pragma once

#include <cstdint>
#include <functional>
#include <vector>
#include <string>

namespace OpenYAMM::Game
{
class Party;
class MapStats;
class NpcDialogTable;
class HouseTable;
class ItemTable;

struct JournalQuestLocation
{
    enum class Role { Giver, Objective, Return };
    enum class Kind { Map, Npc, House, Item };
    Role role = Role::Objective;
    Kind kind = Kind::Map;
    std::string target;
    // Journal-disclosed facts are distinct from secret sources available only in Guide Mode.
    bool disclosed = false;
};

struct JournalQuestEntry
{
    uint32_t qbitId = 0;
    std::string text;
    std::string notes;
    std::string owner;
    std::vector<uint32_t> requiredQBitIds;
    std::vector<uint32_t> forbiddenQBitIds;
    bool usesExplicitConditions = false;
    std::string worldId;
    std::vector<JournalQuestLocation> locations;
};

class JournalQuestTable
{
public:
    bool loadFromRows(const std::vector<std::vector<std::string>> &rows);
    bool loadLocationRows(const std::vector<std::vector<std::string>> &rows);
    bool validateLocations(const MapStats &maps, const NpcDialogTable &npcs,
        const HouseTable &houses, const ItemTable &items) const;
    const std::vector<JournalQuestEntry> &entries() const;
    using QBitReader = std::function<bool(uint32_t)>;
    bool isVisible(const JournalQuestEntry &entry, const QBitReader &hasQBit) const;
    std::vector<uint32_t> questsKnownAroundChange(uint32_t qbitId, bool wasSet, bool isSet,
        const QBitReader &hasQBit) const;
    static std::string discoveryKey(uint32_t qbitId);
    bool hasQuestText(uint32_t qbitId) const;
    bool isVisible(const JournalQuestEntry &entry, const Party &party) const;
    bool qbitChangeRevealsQuest(uint32_t qbitId, bool wasSet, bool isSet, const Party &party) const;

private:
    std::vector<JournalQuestEntry> m_entries;
};
}
