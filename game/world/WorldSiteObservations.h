#pragma once

#include <cstdint>
#include <cstddef>
#include <optional>
#include <map>
#include <string>
#include <unordered_set>
#include <vector>

namespace OpenYAMM::Game
{
class IGameplayWorldRuntime;
struct GameplayWorldHit;
class Party;
class SpellTable;
class ItemTable;
struct EventRuntimeState;

struct WorldSiteValue
{
    std::string attribute;
    std::optional<size_t> member;
    int32_t value = 0;
};

struct WorldSiteValueSnapshot
{
    struct Buff
    {
        std::optional<size_t> member;
        uint8_t slot = 0;
        uint32_t spellId = 0;
        float remainingSeconds = 0;
        int32_t power = 0;
        uint32_t skillLevel = 0;
        uint8_t mastery = 0;
    };
    std::vector<std::string> members;
    std::vector<WorldSiteValue> values;
    std::vector<Buff> buffs;
    float gameMinutes = 0;
    std::map<uint32_t, uint64_t> items;
};

struct WorldSiteItemObservation
{
    uint32_t itemId = 0;
    int64_t minDelta = 0, maxDelta = 0;
};

struct WorldSiteSpellObservation
{
    enum Action : uint8_t { Applied = 1, Updated = 2, Removed = 4 };
    uint32_t spellId = 0;
    bool party = true;
    uint8_t actions = 0;
};

struct WorldSiteEffectRange
{
    std::string attribute;
    int32_t minBefore = 0, minAfter = 0;
    int32_t maxBefore = 0, maxAfter = 0;
    int64_t minDelta() const
    {
        return int64_t(minAfter) - minBefore;
    }
    int64_t maxDelta() const
    {
        return int64_t(maxAfter) - maxBefore;
    }
};

struct WorldSiteObservation
{
    std::string siteId;
    uint16_t eventId = 0;
    int32_t gameMinutes = 0;
    bool lastChanged = false;
    std::vector<WorldSiteEffectRange> effects;
    std::string error;
    std::optional<bool> objectRemoved;
    std::vector<WorldSiteSpellObservation> spells;
    std::vector<WorldSiteItemObservation> items;
};

// One actual event-target activation, shared by ordinary input and world-targeted spells.
class WorldSiteInteractionObservation
{
public:
    WorldSiteInteractionObservation(IGameplayWorldRuntime &world, const GameplayWorldHit &hit);
    void finish(IGameplayWorldRuntime &world, bool activated) const;

private:
    std::optional<WorldSiteValueSnapshot> m_before;
    std::string m_map, m_site;
    uint16_t m_event = 0;
    uint32_t m_sprite = 0;
    bool m_wasHidden = false;
};

WorldSiteValueSnapshot captureWorldSiteValues(const EventRuntimeState &state, const Party &party,
    float gameMinutes = 0);
// Synchronous before/after values only. Does not inspect possible branches or infer unobserved conditions.
// Numeric named map state uses stable attribute symbols, survives refill and requires no save-format change.
bool recordWorldSiteObservation(EventRuntimeState &state, const std::string &siteId, uint16_t eventId,
    const WorldSiteValueSnapshot &before, const WorldSiteValueSnapshot &after, float gameMinutes);
std::vector<WorldSiteObservation> worldSiteObservations(const EventRuntimeState &state);
std::string describeWorldSiteObservation(const WorldSiteObservation &observation, const SpellTable *pSpells = nullptr,
    const ItemTable *pItems = nullptr, const std::unordered_set<uint32_t> &identifiedItems = {});
} // namespace OpenYAMM::Game
