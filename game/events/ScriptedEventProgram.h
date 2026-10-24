#pragma once

#include "game/events/ScriptedEventTimer.h"
#include "game/events/SiteInteractionReference.h"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct lua_State;

namespace OpenYAMM::Game
{
class ScriptedEventProgram
{
public:
    using TimerTrigger = ScriptedEventTimerDefinition;

    // Authored relocation possibilities, not a prediction that an event/branch is currently available.
    struct NpcLocation
    {
        uint32_t npcId = 0;
        uint32_t houseId = 0;
        bool operator==(const NpcLocation &) const = default;
    };

    struct ContextActionMetadata
    {
        std::string kind;
        std::string source;
        std::optional<uint32_t> houseId;
        std::optional<std::string> targetMap;
        std::optional<std::string> targetName;
        std::vector<uint32_t> chestIds;
        bool hidden = false;
        std::optional<SiteInteractionReference> interactionReference;
    };

    static std::optional<ScriptedEventProgram> loadFromLuaText(
        const std::string &luaSourceText,
        const std::string &luaSourceName,
        ScriptedEventScope scope,
        std::string &error);

    const std::optional<std::string> &luaSourceText() const;
    const std::optional<std::string> &luaSourceName() const;
    uint64_t cacheId() const;
    ScriptedEventScope scope() const;
    const std::vector<uint16_t> &eventIds() const;
    const std::vector<uint16_t> &canShowTopicEventIds() const;
    const std::vector<uint16_t> &onLoadEventIds() const;
    const std::vector<uint16_t> &onLeaveEventIds() const;
    const std::vector<uint16_t> &monsterKilledHookEventIds() const;
    const std::vector<uint16_t> &monsterDamageHookEventIds() const;
    const std::vector<std::string> &textureNames() const;
    const std::vector<std::string> &spriteNames() const;
    const std::vector<uint32_t> &castSpellIds() const;
    const std::unordered_map<uint16_t, std::vector<uint32_t>> &itemRewards() const;
    const std::unordered_map<uint16_t, std::vector<uint32_t>> &autonoteReferences() const;
    const std::unordered_map<uint16_t, std::string> &siteReferences() const;
    const std::unordered_map<uint16_t, SiteInteractionReference> &interactionReferences() const;
    const std::unordered_map<uint16_t, std::vector<std::vector<std::string>>> &siteGroups() const;
    const std::unordered_map<uint16_t, std::vector<NpcLocation>> &npcLocations() const;
    const std::vector<TimerTrigger> &timerTriggers() const;
    bool hasEvent(uint16_t eventId) const;
    bool isLevitateSensitivePressurePlate(uint16_t eventId, uint32_t faceAttributes) const;
    bool isHintOnlyEvent(uint16_t eventId) const;
    std::optional<std::string> getHint(uint16_t eventId) const;
    std::optional<std::string> summarizeEvent(uint16_t eventId) const;
    std::vector<uint32_t> getOpenedChestIds(uint16_t eventId) const;
    std::optional<ContextActionMetadata> getContextActionMetadata(uint16_t eventId) const;

private:
    static bool populateMetadataFromLua(
        lua_State *pLuaState,
        ScriptedEventScope scope,
        ScriptedEventProgram &program,
        std::string &error);

    std::optional<std::string> m_luaSourceText;
    std::optional<std::string> m_luaSourceName;
    uint64_t m_cacheId = 0;
    ScriptedEventScope m_scope = ScriptedEventScope::Map;
    std::vector<uint16_t> m_eventIds;
    std::vector<uint16_t> m_canShowTopicEventIds;
    std::vector<uint16_t> m_onLoadEventIds;
    std::vector<uint16_t> m_onLeaveEventIds;
    std::vector<uint16_t> m_monsterKilledHookEventIds;
    std::vector<uint16_t> m_monsterDamageHookEventIds;
    std::unordered_map<uint16_t, std::string> m_hints;
    std::unordered_map<uint16_t, std::string> m_summaries;
    std::unordered_map<uint16_t, std::vector<uint32_t>> m_openedChestIdsByEventId;
    std::unordered_map<uint16_t, ContextActionMetadata> m_contextActionsByEventId;
    std::vector<std::string> m_textureNames;
    std::vector<std::string> m_spriteNames;
    std::vector<uint32_t> m_castSpellIds;
    std::unordered_map<uint16_t, std::vector<uint32_t>> m_itemRewards;
    std::unordered_map<uint16_t, std::vector<uint32_t>> m_autonoteReferences;
    std::unordered_map<uint16_t, std::string> m_siteReferences;
    std::unordered_map<uint16_t, std::string> m_siteReferenceKinds;
    std::unordered_map<uint16_t, SiteInteractionReference> m_interactionReferences;
    std::unordered_map<uint16_t, std::vector<std::vector<std::string>>> m_siteGroups;
    std::unordered_map<uint16_t, std::vector<NpcLocation>> m_npcLocations;
    uint32_t m_levitateTrapFaceMask = 0;
    std::vector<uint16_t> m_levitateTrapEvents;
    std::vector<TimerTrigger> m_timerTriggers;
};

std::vector<ScriptedEventTimerDefinition> scriptedEventTimerDefinitionsFromPrograms(
    const std::optional<ScriptedEventProgram> &localProgram,
    const std::optional<ScriptedEventProgram> &globalProgram);
}
