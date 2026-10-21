#include "game/world/WorldItemSources.h"
#include "game/world/WorldDatabase.h"
#include "game/data/ActorNameResolver.h"
#include "game/events/EventRuntime.h"
#include "game/events/ScriptedEventProgram.h"
#include "game/gameplay/GameplayRuntimeInterfaces.h"
#include "game/maps/MapDeltaData.h"
#include "game/tables/ItemTable.h"
#include "game/tables/ObjectTable.h"
#include "game/SpriteObjectDefs.h"
#include "game/StringUtils.h"

#include <algorithm>
#include <bit>
#include <charconv>
#include <cmath>
#include <set>

namespace OpenYAMM::Game
{
namespace
{
const std::string ObservationPrefix = "openyamm.discovery.item/";
const std::string ChestObservationPrefix = "openyamm.discovery.chest/";
const std::string PendingRewardPrefix = "openyamm.item-grant/";

uint32_t queuedItemId(const EventRuntimeState &state, bool detailed, size_t index)
{
    if (detailed)
        return index < state.grantedItems.size() ? state.grantedItems[index].objectDescriptionId : 0;
    return index < state.grantedItemIds.size() ? state.grantedItemIds[index] : 0;
}

bool wholeNumber(const std::string &text, uint32_t &value)
{
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    return parsed.ec == std::errc() && parsed.ptr == text.data() + text.size() && text == std::to_string(value);
}

bool validMinute(float minute)
{
    return std::isfinite(minute) && minute >= 0 && double(minute) <= INT32_MAX;
}

std::optional<std::array<int32_t, 3>> sourcePosition(float x, float y, float z)
{
    std::array<int32_t, 3> result;
    const std::array<float, 3> values = {x, y, z};
    for (size_t i = 0; i < values.size(); ++i)
    {
        const double rounded = std::round(double(values[i]));
        if (!std::isfinite(rounded) || rounded < INT32_MIN || rounded > INT32_MAX) return std::nullopt;
        result[i] = int32_t(rounded);
    }
    return result;
}

std::string sourceKey(const WorldItemSource &source)
{
    return std::string(source.kind == WorldItemSourceKind::Chest ? "chest/"
        : source.kind == WorldItemSourceKind::Carried ? "carried/"
        : source.kind == WorldItemSourceKind::MapReward ? "map-reward/"
        : source.kind == WorldItemSourceKind::SharedReward ? "shared-reward/" : "placed/")
        + std::to_string(source.sourceIndex) + "/" + std::to_string(source.itemId);
}

void record(EventRuntimeState &state, const WorldItemSource &source)
{
    const std::string key = ObservationPrefix + sourceKey(source) + "/";
    if (source.position)
        for (size_t i = 0; i < 3; ++i) state.namedMapVars[key + std::string(1, "xyz"[i])] = (*source.position)[i];
    state.namedMapVars[key + "time"] = source.gameMinutes;
    if (source.kind == WorldItemSourceKind::Carried) state.namedMapVars[key + "monster"] = source.monsterId;
}
}

std::vector<WorldItemSource> buildWorldItemSources(const MapDeltaData &delta,
    const ItemTable &items, const ObjectTable &objects)
{
    std::vector<WorldItemSource> result;
    const auto add = [&](uint32_t id, uint32_t index, WorldItemSourceKind kind,
        std::optional<std::array<int32_t, 3>> position, int16_t monsterId = 0)
    {
        if (id == 0) return;
        const ItemDefinition *pItem = items.get(id);
        if (pItem == nullptr || pItem->name.empty() || toLowerCopy(pItem->equipStat) == "gold") return;
        WorldItemSource source;
        source.itemId = id; source.sourceIndex = index; source.kind = kind; source.position = position;
        source.monsterId = std::max(int16_t(0), monsterId);
        source.id = "authored/" + sourceKey(source);
        result.push_back(std::move(source));
    };
    const auto rawId = [](const std::vector<uint8_t> &bytes, size_t offset) -> uint32_t
    {
        if (offset > bytes.size() || bytes.size() - offset < 4) return 0;
        return uint32_t(bytes[offset]) | (uint32_t(bytes[offset + 1]) << 8)
            | (uint32_t(bytes[offset + 2]) << 16) | (uint32_t(bytes[offset + 3]) << 24);
    };
    for (size_t i = 0; i < delta.spriteObjects.size(); ++i)
    {
        const MapDeltaSpriteObject &object = delta.spriteObjects[i];
        const ObjectEntry *pDefinition = objects.get(object.objectDescriptionId);
        if (object.rawContainingItem.size() < 36 || object.semanticPolicyHidden || object.semanticLootContainer
            || pDefinition == nullptr || (pDefinition->flags & ObjectDescUnpickable) != 0) continue;
        const uint32_t id = rawId(object.rawContainingItem, 0);
        if (id <= INT32_MAX)
            add(id, uint32_t(i), WorldItemSourceKind::Placed, std::array<int32_t, 3>{object.x, object.y, object.z});
    }
    for (size_t i = 0; i < delta.chests.size(); ++i)
    {
        std::set<uint32_t> fixed;
        for (size_t offset = 0; offset + 36 <= delta.chests[i].rawItems.size(); offset += 36)
        {
            const uint32_t id = rawId(delta.chests[i].rawItems, offset);
            // Negative legacy IDs are treasure-roll placeholders, never fixed item definitions.
            if (id > 0 && id <= INT32_MAX) fixed.insert(id);
        }
        for (uint32_t id : fixed) add(id, uint32_t(i), WorldItemSourceKind::Chest, std::nullopt);
    }
    for (size_t i = 0; i < delta.actors.size(); ++i)
    {
        const MapDeltaActor &actor = delta.actors[i];
        add(actor.carriedItemId, uint32_t(i), WorldItemSourceKind::Carried,
            std::array<int32_t, 3>{actor.x, actor.y, actor.z}, resolveMapDeltaActorMonsterId(actor));
    }
    return result;
}

std::string worldItemSourceLabel(const WorldItemSource &source)
{
    if (source.observed)
    {
        if (source.kind == WorldItemSourceKind::Chest) return "Observed in chest";
        if (source.kind == WorldItemSourceKind::Carried) return "Taken from creature";
        if (source.kind == WorldItemSourceKind::MapReward || source.kind == WorldItemSourceKind::SharedReward)
            return "Received reward";
        return "Last picked up";
    }
    switch (source.kind)
    {
    case WorldItemSourceKind::Placed: return "Authored placement";
    case WorldItemSourceKind::Chest: return "Fixed chest entry";
    case WorldItemSourceKind::Carried: return "Authored actor-carried item";
    case WorldItemSourceKind::MapReward: return "Possible event reward";
    case WorldItemSourceKind::SharedReward: return "Possible NPC reward";
    }
    return "Item source";
}

void recordWorldChestView(EventRuntimeState &state, const GameplayChestViewState &view, float gameMinutes)
{
    if (!validMinute(gameMinutes) || view.items.size() > INT32_MAX) return;
    const std::string key = ChestObservationPrefix + std::to_string(view.chestId) + "/";
    state.namedMapVars[key + "time"] = int32_t(gameMinutes);
    state.namedMapVars[key + "visible"] = int32_t(view.items.size());
    state.namedMapVars[key + "empty"] = view.items.empty() && view.hiddenItems.empty() ? 1 : 0;
    // Only visible slots have actually been presented. Hidden overflow contents stay unknown.
    for (const GameplayChestItemState &item : view.items)
    {
        const uint32_t id = item.item.objectDescriptionId != 0 ? item.item.objectDescriptionId : item.itemId;
        if (item.isGold || id == 0) continue;
        WorldItemSource source;
        source.itemId = id; source.sourceIndex = view.chestId; source.kind = WorldItemSourceKind::Chest;
        source.gameMinutes = int32_t(gameMinutes);
        record(state, source);
    }
}

std::vector<WorldChestObservation> worldChestObservations(const EventRuntimeState &state)
{
    std::vector<WorldChestObservation> result;
    for (const auto &[key, minute] : state.namedMapVars)
    {
        if (!key.starts_with(ChestObservationPrefix) || !key.ends_with("/time") || minute < 0) continue;
        const std::string id = key.substr(ChestObservationPrefix.size(),
            key.size() - ChestObservationPrefix.size() - 5);
        WorldChestObservation observation;
        if (!wholeNumber(id, observation.chestId)) continue;
        const std::string prefix = ChestObservationPrefix + id + "/";
        const auto visible = state.namedMapVars.find(prefix + "visible");
        const auto empty = state.namedMapVars.find(prefix + "empty");
        if (visible == state.namedMapVars.end() || empty == state.namedMapVars.end() || visible->second < 0
            || (empty->second != 0 && empty->second != 1) || (empty->second == 1 && visible->second != 0))
            continue;
        observation.visibleSlots = uint32_t(visible->second);
        observation.empty = empty->second == 1;
        observation.gameMinutes = minute;
        result.push_back(observation);
    }
    std::sort(result.begin(), result.end(), [](const WorldChestObservation &a, const WorldChestObservation &b)
    { return a.chestId < b.chestId; });
    return result;
}

void recordWorldItemPickup(EventRuntimeState &state, const GameplayWorldItemInspectState &item, float gameMinutes)
{
    if (item.isGold || item.item.objectDescriptionId == 0 || !item.position || !validMinute(gameMinutes)) return;
    WorldItemSource source;
    source.itemId = item.item.objectDescriptionId; source.gameMinutes = int32_t(gameMinutes);
    source.position.emplace();
    for (size_t i = 0; i < 3; ++i)
    {
        const double value = std::round(double((*item.position)[i]));
        if (!std::isfinite(value) || value < INT32_MIN || value > INT32_MAX) return;
        (*source.position)[i] = int32_t(value);
    }
    record(state, source);
}

void recordWorldActorItem(IGameplayWorldRuntime &runtime, const GameplayCorpseViewState &view,
    const GameplayChestItemState &item)
{
    EventRuntimeState *pState = runtime.eventRuntimeState();
    const uint32_t itemId = item.item.objectDescriptionId != 0 ? item.item.objectDescriptionId : item.itemId;
    const float minute = runtime.gameMinutes();
    GameplayRuntimeActorState actor;
    if (pState == nullptr || item.isGold || itemId == 0 || !validMinute(minute) || view.fromSummonedMonster
        || !runtime.actorRuntimeState(view.sourceIndex, actor)) return;
    WorldItemSource source;
    source.itemId = itemId;
    source.sourceIndex = view.sourceIndex;
    source.kind = WorldItemSourceKind::Carried;
    source.monsterId = std::max(int16_t(0), actor.monsterId);
    source.position = sourcePosition(actor.preciseX, actor.preciseY, actor.preciseZ);
    if (!source.position) return;
    source.gameMinutes = int32_t(minute);
    record(*pState, source);
}

std::vector<WorldItemSource> worldObservedItemSources(const EventRuntimeState &state)
{
    std::vector<WorldItemSource> result;
    for (const auto &[key, minute] : state.namedMapVars)
    {
        if (!key.starts_with(ObservationPrefix) || !key.ends_with("/time") || minute < 0) continue;
        const std::string id = key.substr(ObservationPrefix.size(), key.size() - ObservationPrefix.size() - 5);
        const size_t first = id.find('/'), second = id.find('/', first == std::string::npos ? 0 : first + 1);
        if (first == std::string::npos || second == std::string::npos) continue;
        WorldItemSource source;
        if (!wholeNumber(id.substr(first + 1, second - first - 1), source.sourceIndex)
            || !wholeNumber(id.substr(second + 1), source.itemId) || source.itemId == 0) continue;
        const std::string kind = id.substr(0, first);
        if (kind == "chest") source.kind = WorldItemSourceKind::Chest;
        else if (kind == "placed" && source.sourceIndex == 0) source.kind = WorldItemSourceKind::Placed;
        else if (kind == "carried") source.kind = WorldItemSourceKind::Carried;
        else if ((kind == "map-reward" || kind == "shared-reward")
            && source.sourceIndex > 0 && source.sourceIndex <= UINT16_MAX)
            source.kind = kind == "map-reward" ? WorldItemSourceKind::MapReward : WorldItemSourceKind::SharedReward;
        else continue;
        source.id = "observed/" + id; source.observed = true; source.gameMinutes = minute;
        if (source.kind == WorldItemSourceKind::Placed || source.kind == WorldItemSourceKind::Carried)
        {
            const std::string prefix = ObservationPrefix + id + "/";
            if (!state.namedMapVars.contains(prefix + "x") || !state.namedMapVars.contains(prefix + "y")
                || !state.namedMapVars.contains(prefix + "z")) continue;
            source.position = {{state.namedMapVars.at(prefix + "x"), state.namedMapVars.at(prefix + "y"),
                state.namedMapVars.at(prefix + "z")}};
        }
        if (source.kind == WorldItemSourceKind::Carried)
        {
            const auto monster = state.namedMapVars.find(ObservationPrefix + id + "/monster");
            if (monster != state.namedMapVars.end())
            {
                if (monster->second < 0 || monster->second > INT16_MAX) continue;
                source.monsterId = int16_t(monster->second);
            }
        }
        result.push_back(std::move(source));
    }
    std::sort(result.begin(), result.end(), [](const WorldItemSource &a, const WorldItemSource &b)
    { return a.id < b.id; });
    return result;
}

void recordWorldItemReward(EventRuntimeState &state, uint32_t itemId,
    uint16_t eventId, bool global, float gameMinutes)
{
    if (itemId == 0 || eventId == 0 || !validMinute(gameMinutes)) return;
    WorldItemSource source;
    source.itemId = itemId;
    source.sourceIndex = eventId;
    source.kind = global ? WorldItemSourceKind::SharedReward : WorldItemSourceKind::MapReward;
    source.gameMinutes = int32_t(gameMinutes);
    record(state, source);
}

void stageWorldItemReward(EventRuntimeState &state, bool detailed, size_t index,
    uint16_t eventId, bool global, float gameMinutes)
{
    const std::string prefix = PendingRewardPrefix + (detailed ? "items/" : "ids/") + std::to_string(index) + "/";
    std::erase_if(state.namedMapVars, [&](const auto &entry) { return entry.first.starts_with(prefix); });
    const uint32_t itemId = queuedItemId(state, detailed, index);
    if (itemId == 0 || eventId == 0 || !validMinute(gameMinutes)) return;
    // Named variables are int32; preserve the full opaque uint32 identifier, including its high bit.
    state.namedMapVars[prefix + "item"] = std::bit_cast<int32_t>(itemId);
    state.namedMapVars[prefix + "event"] = eventId;
    state.namedMapVars[prefix + "global"] = global ? 1 : 0;
    state.namedMapVars[prefix + "time"] = int32_t(gameMinutes);
}

void confirmWorldItemReward(EventRuntimeState &state, bool detailed, size_t index)
{
    const std::string prefix = PendingRewardPrefix + (detailed ? "items/" : "ids/") + std::to_string(index) + "/";
    const auto item = state.namedMapVars.find(prefix + "item"), event = state.namedMapVars.find(prefix + "event"),
        global = state.namedMapVars.find(prefix + "global"), minute = state.namedMapVars.find(prefix + "time");
    if (item != state.namedMapVars.end() && event != state.namedMapVars.end()
        && global != state.namedMapVars.end() && minute != state.namedMapVars.end()
        && item->second != 0 && std::bit_cast<uint32_t>(item->second) == queuedItemId(state, detailed, index)
        && event->second > 0 && event->second <= UINT16_MAX && (global->second == 0 || global->second == 1)
        && minute->second >= 0)
    {
        // Keep the exact saved integer minute; a float round-trip can lose precision in long campaigns.
        WorldItemSource source;
        source.itemId = std::bit_cast<uint32_t>(item->second);
        source.sourceIndex = uint32_t(event->second);
        source.kind = global->second != 0 ? WorldItemSourceKind::SharedReward : WorldItemSourceKind::MapReward;
        source.gameMinutes = minute->second;
        record(state, source);
    }
    std::erase_if(state.namedMapVars, [&](const auto &entry) { return entry.first.starts_with(prefix); });
}

void clearWorldItemRewardQueue(EventRuntimeState &state)
{
    std::erase_if(state.namedMapVars, [](const auto &entry) { return entry.first.starts_with(PendingRewardPrefix); });
}

void WorldDatabase::appendItemSource(const std::string &mapId, WorldItemSource source, const ItemTable &items)
{
    if (source.itemId == 0) return;
    const ItemDefinition *pItem = items.get(source.itemId);
    if (pItem == nullptr || pItem->name.empty()) return;
    source.mapId = mapId;
    source.id = mapId + "/item-source/" + source.id;
    if (source.position)
    {
        WorldMapFeature feature;
        feature.id = source.id; feature.mapId = mapId; feature.kind = WorldMapFeatureKind::Item;
        feature.site = {source.id, 0, float((*source.position)[0]), float((*source.position)[1]),
            float((*source.position)[2])};
        feature.title = pItem->name + " - " + worldItemSourceLabel(source);
        feature.itemId = source.itemId; feature.observedItem = source.observed;
        m_features.push_back(std::move(feature));
    }
    m_itemSources.push_back(std::move(source));
}

void WorldDatabase::indexMapItemSources(const MapStatsEntry &map, std::vector<WorldItemSource> sources,
    const ItemTable &items)
{
    const std::string id = mapId(map);
    if (find(id) == nullptr) return;
    std::unordered_set<std::string> replaced;
    std::erase_if(m_itemSources, [&](const WorldItemSource &source)
    {
        const bool remove = source.mapId == id && !source.observed && source.kind != WorldItemSourceKind::MapReward
            && source.kind != WorldItemSourceKind::SharedReward;
        if (remove) replaced.insert(source.id);
        return remove;
    });
    std::erase_if(m_features, [&](const WorldMapFeature &feature) { return replaced.contains(feature.id); });
    for (WorldItemSource &source : sources)
    {
        source.observed = false;
        appendItemSource(id, std::move(source), items);
    }
}

void WorldDatabase::indexMapItemRewards(const MapStatsEntry &map, const ScriptedEventProgram &program,
    const std::vector<WorldInteractionSite> &sites, const ItemTable &items)
{
    const std::string id = mapId(map);
    if (find(id) == nullptr) return;
    std::unordered_set<std::string> replaced;
    std::erase_if(m_itemSources, [&](const WorldItemSource &source)
    {
        const bool remove = source.mapId == id && !source.observed && source.kind == WorldItemSourceKind::MapReward;
        if (remove) replaced.insert(source.id);
        return remove;
    });
    std::erase_if(m_features, [&](const WorldMapFeature &feature) { return replaced.contains(feature.id); });
    for (const auto &[eventId, rewards] : program.itemRewards())
        for (uint32_t itemId : rewards)
        {
            WorldItemSource source;
            source.itemId = itemId; source.sourceIndex = eventId; source.kind = WorldItemSourceKind::MapReward;
            source.id = "authored/map-reward/" + std::to_string(eventId) + "/" + std::to_string(itemId);
            bool placed = false;
            for (const WorldInteractionSite &site : sites)
                if (site.eventId == eventId)
                {
                    WorldItemSource located = source;
                    located.id += "/" + site.id;
                    located.position = sourcePosition(site.x, site.y, site.z);
                    if (!located.position) continue;
                    appendItemSource(id, std::move(located), items);
                    placed = true;
                }
            if (!placed) appendItemSource(id, std::move(source), items);
        }
}

void WorldDatabase::indexSharedItemRewards(const ScriptedEventProgram &program, const ItemTable &items)
{
    std::erase_if(m_itemSources, [](const WorldItemSource &source)
    { return !source.observed && source.kind == WorldItemSourceKind::SharedReward; });
    for (const WorldRecord &record : m_records)
    {
        if (record.kind != WorldRecordKind::Npc || !record.npc || !record.map) continue;
        std::unordered_set<uint32_t> indexedTopics;
        for (uint32_t topic : record.npcTopicIds)
        {
            if (!indexedTopics.insert(topic).second) continue;
            if (topic <= UINT16_MAX)
            {
                const auto rewards = program.itemRewards().find(uint16_t(topic));
                if (rewards != program.itemRewards().end())
                {
                    for (uint32_t itemId : rewards->second)
                    {
                        WorldItemSource source;
                        source.itemId = itemId;
                        source.sourceIndex = topic;
                        source.kind = WorldItemSourceKind::SharedReward;
                        source.id = "authored/npc-reward/" + std::to_string(record.npc->id) + "/"
                            + std::to_string(topic) + "/" + std::to_string(itemId);
                        source.providerId = record.id;
                        appendItemSource(WorldDatabase::mapId(*record.map), std::move(source), items);
                    }
                }
            }
        }
    }
}

void WorldDatabase::indexObservedItemSources(const WorldKnowledge &knowledge, const ItemTable &items)
{
    std::erase_if(m_itemSources, [](const WorldItemSource &source) { return source.observed; });
    std::erase_if(m_features, [](const WorldMapFeature &feature)
    { return feature.kind == WorldMapFeatureKind::Item && feature.observedItem; });
    for (const WorldRecord &map : m_records)
    {
        if (map.kind != WorldRecordKind::Region && map.kind != WorldRecordKind::Dungeon) continue;
        const auto known = knowledge.itemObservations.find(toLowerCopy(map.map->fileName));
        if (known == knowledge.itemObservations.end()) continue;
        for (WorldItemSource source : known->second)
        {
            source.observed = true;
            appendItemSource(map.id, std::move(source), items);
        }
    }
}

std::vector<const WorldItemSource *> WorldDatabase::itemSources(const WorldKnowledge &knowledge, bool guide) const
{
    std::vector<const WorldItemSource *> result;
    for (const WorldItemSource &source : m_itemSources)
        if (const WorldRecord *pMap = find(source.mapId); pMap != nullptr && visible(*pMap, knowledge, guide)
            && (guide || source.observed)) result.push_back(&source);
    std::sort(result.begin(), result.end(), [](const auto *left, const auto *right) { return left->id < right->id; });
    return result;
}
}
