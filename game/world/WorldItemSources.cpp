#include "game/world/WorldItemSources.h"
#include "game/world/WorldDatabase.h"
#include "game/events/EventRuntime.h"
#include "game/gameplay/GameplayRuntimeInterfaces.h"
#include "game/maps/MapDeltaData.h"
#include "game/tables/ItemTable.h"
#include "game/tables/ObjectTable.h"
#include "game/SpriteObjectDefs.h"
#include "game/StringUtils.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <set>

namespace OpenYAMM::Game
{
namespace
{
const std::string ObservationPrefix = "openyamm.discovery.item/";

bool wholeNumber(const std::string &text, uint32_t &value)
{
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    return parsed.ec == std::errc() && parsed.ptr == text.data() + text.size() && text == std::to_string(value);
}

bool validMinute(float minute)
{
    return std::isfinite(minute) && minute >= 0 && double(minute) <= INT32_MAX;
}

std::string sourceKey(const WorldItemSource &source)
{
    return std::string(source.kind == WorldItemSourceKind::Chest ? "chest/"
        : source.kind == WorldItemSourceKind::Carried ? "carried/" : "placed/")
        + std::to_string(source.sourceIndex) + "/" + std::to_string(source.itemId);
}

void record(EventRuntimeState &state, const WorldItemSource &source)
{
    const std::string key = ObservationPrefix + sourceKey(source) + "/";
    if (source.position)
        for (size_t i = 0; i < 3; ++i) state.namedMapVars[key + std::string(1, "xyz"[i])] = (*source.position)[i];
    state.namedMapVars[key + "time"] = source.gameMinutes;
}
}

std::vector<WorldItemSource> buildWorldItemSources(const MapDeltaData &delta,
    const ItemTable &items, const ObjectTable &objects)
{
    std::vector<WorldItemSource> result;
    const auto add = [&](uint32_t id, uint32_t index, WorldItemSourceKind kind,
        std::optional<std::array<int32_t, 3>> position)
    {
        const ItemDefinition *pItem = items.get(id);
        if (pItem == nullptr || pItem->name.empty() || toLowerCopy(pItem->equipStat) == "gold") return;
        WorldItemSource source;
        source.itemId = id; source.sourceIndex = index; source.kind = kind; source.position = position;
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
            std::array<int32_t, 3>{actor.x, actor.y, actor.z});
    }
    return result;
}

std::string worldItemSourceLabel(const WorldItemSource &source)
{
    if (source.observed) return source.kind == WorldItemSourceKind::Chest ? "Observed in chest" : "Last picked up";
    switch (source.kind)
    {
    case WorldItemSourceKind::Placed: return "Authored placement";
    case WorldItemSourceKind::Chest: return "Fixed chest entry";
    case WorldItemSourceKind::Carried: return "Authored actor-carried item";
    }
    return "Item source";
}

void recordWorldChestItems(EventRuntimeState &state, const GameplayChestViewState &view, float gameMinutes)
{
    if (!validMinute(gameMinutes)) return;
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
        if (kind != "chest" && (kind != "placed" || source.sourceIndex != 0)) continue;
        source.kind = kind == "chest" ? WorldItemSourceKind::Chest : WorldItemSourceKind::Placed;
        source.id = "observed/" + id; source.observed = true; source.gameMinutes = minute;
        if (source.kind == WorldItemSourceKind::Placed)
        {
            const std::string prefix = ObservationPrefix + id + "/";
            if (!state.namedMapVars.contains(prefix + "x") || !state.namedMapVars.contains(prefix + "y")
                || !state.namedMapVars.contains(prefix + "z")) continue;
            source.position = {{state.namedMapVars.at(prefix + "x"), state.namedMapVars.at(prefix + "y"),
                state.namedMapVars.at(prefix + "z")}};
        }
        result.push_back(std::move(source));
    }
    std::sort(result.begin(), result.end(), [](const WorldItemSource &a, const WorldItemSource &b)
    { return a.id < b.id; });
    return result;
}

void WorldDatabase::appendItemSource(const std::string &mapId, WorldItemSource source, const ItemTable &items)
{
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
    std::erase_if(m_itemSources, [&](const WorldItemSource &source) { return source.mapId == id && !source.observed; });
    std::erase_if(m_features, [&](const WorldMapFeature &feature)
    { return feature.mapId == id && feature.kind == WorldMapFeatureKind::Item && !feature.observedItem; });
    for (WorldItemSource &source : sources)
    {
        source.observed = false;
        appendItemSource(id, std::move(source), items);
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
