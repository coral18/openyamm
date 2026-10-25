#include "game/world/WorldGuide.h"
#include "game/world/WorldDatabase.h"
#include "game/tables/ItemTable.h"
#include "game/tables/SpellTable.h"
#include "game/tables/ClassMultiplierTable.h"
#include "game/tables/ClassSkillTable.h"
#include "game/tables/MergedBaseTables.h"
#include "game/tables/PotionMixingTable.h"
#include "game/tables/PotionNoteTable.h"
#include "game/tables/RosterTable.h"
#include "game/tables/HouseTable.h"
#include "game/tables/MapStats.h"
#include "game/tables/MonsterTable.h"
#include "game/tables/JournalQuestTable.h"
#include "game/tables/NpcDialogTable.h"
#include "game/maps/MapIdentity.h"
#include "game/StringUtils.h"
#include "game/items/ItemInspectionText.h"
#include "game/items/ItemRuntime.h"
#include "game/gameplay/GuildSpellbookRules.h"
#include "game/party/SpellSchool.h"
#include "game/party/Party.h"
#include "game/events/ScriptedEventProgram.h"

#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <stdexcept>
#include <tuple>

namespace OpenYAMM::Game
{
namespace
{
void appendCompanionContacts(WorldGuideEntry &entry, const WorldGuideSource &source, uint32_t rosterId, bool guide)
{
    if (source.pNpcs == nullptr || source.pWorld == nullptr || source.pWorldKnowledge == nullptr) return;
    std::map<uint32_t, bool> contacts;
    const RosterEntry *pDefinition = source.pRoster != nullptr ? source.pRoster->get(rosterId) : nullptr;
    const auto include = [&](uint32_t npcId, uint32_t topic)
    {
        const auto offer = source.pNpcs->getRosterJoinOfferForTopic(topic);
        if (source.pNpcs->getNpc(npcId) == nullptr) return;
        if (offer && offer->rosterId == rosterId) contacts[npcId] = false;
        else if (pDefinition != nullptr && pDefinition->unlockQuestBitId != 0 && source.pGlobalEvents != nullptr)
        {
            const auto grants = source.pGlobalEvents->qbitGrants().find(uint16_t(topic));
            if (topic <= UINT16_MAX && grants != source.pGlobalEvents->qbitGrants().end()
                && std::find(grants->second.begin(), grants->second.end(), pDefinition->unlockQuestBitId)
                    != grants->second.end()) contacts.try_emplace(npcId, true);
        }
    };
    for (const NpcEntry &npc : source.pNpcs->entries())
    {
        if (worldRosterOfferKnown(source.pWorldKnowledge->variables, npc.id, rosterId)) contacts[npc.id] = false;
        if (guide)
            for (uint32_t topic : npc.topicIds) include(npc.id, topic);
    }
    if (guide)
    {
        for (const WorldRecord &record : source.pWorld->records())
            if (record.kind == WorldRecordKind::Npc && !record.possibleNpcLocation)
                for (uint32_t topic : record.npcTopicIds) include(record.npc->id, topic);
        for (const ScriptedEventProgram *pProgram : {source.pGlobalEvents, source.pLocalEvents})
            if (pProgram != nullptr)
                for (const auto &[event, references] : pProgram->npcTopicReferences())
                    for (const ScriptedEventProgram::NpcTopicReference &reference : references)
                        include(reference.npcId, reference.topicId);
    }
    std::set<std::string> linked;
    std::string text;
    for (const auto &[npcId, unlockContact] : contacts)
    {
        bool located = false;
        const NpcEntry &npc = *source.pNpcs->getNpc(npcId);
        for (const WorldRecord &record : source.pWorld->records())
        {
            if (record.kind != WorldRecordKind::Npc || record.npc->id != npcId
                || !source.pWorld->visible(record, *source.pWorldKnowledge, guide)) continue;
            const std::string label = std::string(unlockContact ? "Possible unlock contact: "
                : record.possibleNpcLocation ? "Possible scripted contact: "
                : record.lastObservedNpc ? "Last observed recruitment contact: " : "Configured recruitment contact: ")
                + record.name() + " | " + record.house->name + " - " + record.map->name;
            text += "\n" + label;
            if (linked.insert(record.id).second)
                entry.links.push_back({WorldGuideLink::Kind::Location, label, record.id});
            located = true;
        }
        if (guide)
            for (const WorldMapFeature &feature : source.pWorld->features())
            {
                if (!feature.interactionReference || feature.interactionReference->npcId != npcId
                    || !source.pWorld->featureVisible(feature, *source.pWorldKnowledge, true)) continue;
                const WorldRecord *pMap = source.pWorld->find(feature.mapId);
                if (pMap == nullptr) continue;
                const std::string label = std::string(unlockContact
                    ? "Possible unlock encounter: " : "Possible encounter: ")
                    + npc.name + " | " + pMap->name();
                text += "\n" + label;
                if (linked.insert(feature.id).second)
                    entry.links.push_back({WorldGuideLink::Kind::Feature, label, feature.id});
                located = true;
            }
        if (!located && guide && npc.houseId != 0)
        {
            const std::string houseId = source.worldId + ":house/" + std::to_string(npc.houseId);
            if (const WorldRecord *pHouse = source.pWorld->find(houseId))
            {
                const std::string label = "Authored contact: " + npc.name + " | " + pHouse->name()
                    + " - " + pHouse->map->name;
                text += "\n" + label;
                if (linked.insert(houseId).second)
                    entry.links.push_back({WorldGuideLink::Kind::Location, label, houseId});
            }
        }
        else if (!located && !guide)
            text += "\nInvitation seen from " + npc.name + "; no mapped contact location is known in this world.";
    }
    entry.details += "\n\nRecruitment contacts in this world:";
    entry.details += text.empty() ? (guide ? "\nNo mapped recruitment contact in this catalog."
        : "\nNo recruitment contact recorded yet.") : text;
    entry.details += guide
        ? "\nContact links describe possible dialogue sources; quest and topic conditions still apply."
        : "\nRecorded invitations and locations are history, not a current availability check.";
}

std::string itemDefinitionText(const ItemDefinition &item)
{
    std::string text = item.name + "\n" + item.equipStat + " | " + item.skillGroup
        + "\nValue: " + std::to_string(item.value) + " gold\n";
    const std::string statistics = itemInspectDetailText(nullptr, item);
    if (!statistics.empty()) text += statistics + "\n";
    switch (item.rarity)
    {
    case ItemRarity::Common: text += "Rarity: Common\n"; break;
    case ItemRarity::Artifact: text += "Rarity: Artifact\n"; break;
    case ItemRarity::Relic: text += "Rarity: Relic\n"; break;
    case ItemRarity::Special: text += "Rarity: Special\n"; break;
    }
    if (item.requiredMight > 0) text += "Required Might: " + std::to_string(item.requiredMight) + "\n";
    if (item.requiredAccuracy > 0) text += "Required Accuracy: " + std::to_string(item.requiredAccuracy) + "\n";
    if (!item.allowedClassRoots.empty())
    {
        text += "Class families: ";
        for (size_t i = 0; i < item.allowedClassRoots.size(); ++i)
            text += (i == 0 ? "" : ", ") + displayClassName(item.allowedClassRoots[i]);
        text += "\n";
    }
    else if (const auto restriction = ItemRuntime::classRestriction(item))
        text += "Class family: " + displayClassName(*restriction) + "\n";
    if (const auto restriction = ItemRuntime::raceRestriction(item))
        text += "Restriction: " + (*restriction == "DarkElfOrElf" ? "Dark Elf or Elf"
            : *restriction == "DarkElf" ? "Dark Elf" : *restriction) + "\n";
    if (!item.notes.empty()) text += "\n" + item.notes;
    return text;
}

std::string classStartingSkills(const WorldGuideSource &source, const std::string &className,
    const std::vector<std::pair<uint32_t, std::string>> &races)
{
    if (source.pCharacterSelection == nullptr) return {};
    struct Group
    {
        std::vector<std::string> races, granted, optional;
    };
    std::vector<Group> groups;
    for (const auto &[raceId, raceName] : races)
    {
        const std::vector<std::string> *pAllowed = source.pCharacterSelection->allowedClassesForRaceId(raceId);
        if (pAllowed == nullptr || std::none_of(pAllowed->begin(), pAllowed->end(), [&](const std::string &name)
            { return canonicalClassName(name) == className; })) continue;
        Group profile;
        profile.races.push_back(raceName);
        for (const std::string &skill : allCanonicalSkillNames())
        {
            const StartingSkillAvailability availability =
                source.pSkills->getEffectiveStartingSkillAvailability(className, raceId, skill);
            if (availability == StartingSkillAvailability::HasByDefault)
                profile.granted.push_back(displaySkillName(skill));
            else if (availability == StartingSkillAvailability::CanLearn)
                profile.optional.push_back(displaySkillName(skill));
        }
        const auto existing = std::find_if(groups.begin(), groups.end(), [&](const Group &group)
        {
            return group.granted == profile.granted && group.optional == profile.optional;
        });
        if (existing == groups.end())
            groups.push_back(std::move(profile));
        else
            existing->races.push_back(raceName);
    }
    const auto names = [](const std::vector<std::string> &values)
    {
        std::string result;
        for (const std::string &value : values)
        {
            if (!result.empty()) result += ", ";
            result += value;
        }
        return result.empty() ? "None" : result;
    };
    std::string result;
    if (!groups.empty()) result = "\n\nStarting skills by race:\n";
    for (const Group &group : groups)
        result += names(group.races) + "\nGranted: " + names(group.granted)
            + "\nOptional choices: " + names(group.optional) + "\n";
    return result;
}

struct CreatureLootReference
{
    int16_t monsterId = 0;
    uint32_t itemId = 0;
    std::string provenance;
};

std::vector<CreatureLootReference> creatureLootReferences(const WorldGuideSource &source,
    const WorldGuideKnowledge &knowledge, bool guide)
{
    std::vector<CreatureLootReference> result;
    if (source.pMonsters == nullptr || source.pItems == nullptr) return result;
    std::set<std::tuple<int16_t, uint32_t, std::string>> included;
    const auto add = [&](int16_t monsterId, uint32_t itemId, const std::string &provenance)
    {
        const auto *pMonster = source.pMonsters->findStatsById(monsterId);
        const ItemDefinition *pItem = source.pItems->get(itemId);
        if (monsterId <= 0 || pMonster == nullptr || pMonster->name.empty() || pItem == nullptr || pItem->name.empty()
            || (!guide && (!knowledge.monsters.contains(monsterId)
                || (!knowledge.items.contains(itemId) && !knowledge.identifiedItems.contains(itemId))))) return;
        if (included.emplace(monsterId, itemId, provenance).second)
            result.push_back({monsterId, itemId, provenance});
    };
    if (guide)
        for (const auto &[id, monster] : source.pMonsters->statsEntries())
            if (id > 0 && id <= INT16_MAX)
                for (const auto &drop : source.pMonsters->deathDropsForMonsterId(int16_t(id)))
                    if (drop.chancePercent > 0 && drop.chancePercent <= 100)
                        add(int16_t(id), drop.itemId, "Death drop (" + std::to_string(drop.chancePercent) + "%)");
    if (source.pWorld != nullptr && source.pWorldKnowledge != nullptr)
        for (const WorldItemSource *pItem : source.pWorld->itemSources(*source.pWorldKnowledge, guide))
            if ((pItem->kind == WorldItemSourceKind::Carried || pItem->kind == WorldItemSourceKind::DeathDrop)
                && (guide || pItem->observed))
                add(pItem->monsterId, pItem->itemId, pItem->observed ? "Recovered loot" : "Authored carried item");
    std::sort(result.begin(), result.end(), [](const CreatureLootReference &a, const CreatureLootReference &b)
    { return std::tie(a.monsterId, a.itemId, a.provenance) < std::tie(b.monsterId, b.itemId, b.provenance); });
    return result;
}
}

bool worldItemPropertiesKnown(const ItemDefinition &item, const WorldGuideKnowledge &knowledge, bool guide)
{
    return guide || knowledge.identifiedItems.contains(item.itemId)
        || (knowledge.items.contains(item.itemId) && !ItemRuntime::requiresIdentification(item));
}

std::optional<WorldItemComparison> buildWorldItemComparison(const WorldGuideSource &source,
    const WorldGuideKnowledge &knowledge, bool guide, uint32_t pinnedItem, uint32_t selectedItem)
{
    if (source.pItems == nullptr || pinnedItem == 0 || selectedItem == 0 || pinnedItem == selectedItem)
        return std::nullopt;
    WorldItemComparison result;
    const std::array<uint32_t, 2> ids{pinnedItem, selectedItem};
    for (size_t i = 0; i < ids.size(); ++i)
    {
        const ItemDefinition *pItem = source.pItems->get(ids[i]);
        if (pItem == nullptr || pItem->name.empty() || !worldItemPropertiesKnown(*pItem, knowledge, guide))
            return std::nullopt;
        result.items[i] = pItem;
        result.details[i] = itemDefinitionText(*pItem);
    }
    return result;
}

std::optional<WorldClassComparison> buildWorldClassComparison(const WorldGuideSource &source,
    const WorldGuideKnowledge &knowledge, bool guide, const std::string &anchorClass,
    std::optional<uint32_t> raceId)
{
    const std::string anchor = canonicalClassName(anchorClass);
    if (source.pClasses == nullptr || source.pSkills == nullptr || source.pClasses->get(anchor) == nullptr
        || (!guide && !knowledge.classes.contains(anchor)))
        return std::nullopt;
    WorldClassComparison result;
    result.raceId = raceId;
    if (source.pCharacterSelection != nullptr)
        for (const auto &[id, name] : source.pCharacterSelection->raceNames())
            if (!name.empty() && (guide || knowledge.races.contains(id))) result.races.emplace_back(id, name);
    std::sort(result.races.begin(), result.races.end(), [](const auto &left, const auto &right)
    { return left.second == right.second ? left.first < right.first : left.second < right.second; });
    if (raceId && std::none_of(result.races.begin(), result.races.end(),
        [&](const auto &race) { return race.first == *raceId; }))
        return std::nullopt;

    std::unordered_map<std::string, std::vector<std::string>> family;
    for (const auto &[id, definition] : source.pClasses->entries())
        for (const std::string &target : source.pSkills->promotionClassNamesFromMetadata(id))
            if (source.pClasses->get(target) != nullptr)
            {
                family[id].push_back(target);
                family[target].push_back(id);
            }
    std::unordered_set<std::string> visited{anchor};
    std::vector<std::string> pending{anchor};
    for (size_t i = 0; i < pending.size(); ++i)
        for (const std::string &neighbor : family[pending[i]])
            if (visited.insert(neighbor).second) pending.push_back(neighbor);
    // Traverse before filtering: a known later class may have an undiscovered intermediate tier.
    for (const std::string &id : pending)
        if (guide || knowledge.classes.contains(id)) result.classes.push_back({id, source.pClasses->get(id)});
    std::sort(result.classes.begin() + 1, result.classes.end(), [](const auto &left, const auto &right)
    {
        if (left.pDefinition->promotionStep != right.pDefinition->promotionStep)
            return left.pDefinition->promotionStep < right.pDefinition->promotionStep;
        if (left.pDefinition->displayName != right.pDefinition->displayName)
            return left.pDefinition->displayName < right.pDefinition->displayName;
        return left.id < right.id;
    });
    for (const std::string &skill : allCanonicalSkillNames())
    {
        WorldClassComparisonRow row{skill};
        for (const WorldClassComparisonColumn &column : result.classes)
            row.caps.push_back(raceId ? source.pSkills->getEffectiveCap(column.id, *raceId, skill)
                : source.pSkills->getClassCap(column.id, skill));
        if (std::any_of(row.caps.begin(), row.caps.end(), [](SkillMastery cap) { return cap != SkillMastery::None; }))
            result.skills.push_back(std::move(row));
    }
    return result;
}

std::vector<WorldAlchemyRecipe> planWorldAlchemy(uint32_t target, const std::vector<WorldAlchemyRecipe> &recipes,
    const std::unordered_set<uint32_t> &ingredients)
{
    std::map<uint32_t, uint64_t> costs;
    std::map<uint32_t, WorldAlchemyRecipe> best;
    for (uint32_t ingredient : ingredients)
        costs[ingredient] = 1;
    for (size_t pass = 0; pass < recipes.size(); ++pass)
    {
        bool changed = false;
        for (const WorldAlchemyRecipe &recipe : recipes)
        {
            if (recipe.result == 0 || recipe.result == recipe.first || recipe.result == recipe.second
                || !costs.contains(recipe.first) || (recipe.second != 0 && !costs.contains(recipe.second)))
                continue;
            const uint64_t cost = costs.at(recipe.first) + (recipe.second == 0 ? 0 : costs.at(recipe.second));
            // Bound malformed/modded graphs without integer wrap or exponentially large plans.
            if (cost > 1024 || (costs.contains(recipe.result) && costs.at(recipe.result) <= cost))
                continue;
            costs[recipe.result] = cost;
            best[recipe.result] = recipe;
            changed = true;
        }
        if (!changed)
            break;
    }
    std::vector<WorldAlchemyRecipe> result;
    std::unordered_set<uint32_t> included;
    const std::function<void(uint32_t)> visit = [&](uint32_t id)
    {
        if (!best.contains(id) || !included.insert(id).second)
            return;
        const WorldAlchemyRecipe &recipe = best.at(id);
        visit(recipe.first);
        if (recipe.second != 0)
            visit(recipe.second);
        result.push_back(recipe);
    };
    visit(target);
    return result;
}

std::vector<WorldGuideEntry> buildWorldGuide(const WorldGuideSource &source, WorldGuideSection section,
    const WorldGuideKnowledge &knowledge, bool guide)
{
    std::vector<WorldGuideEntry> result;
    const auto itemName = [&](uint32_t id)
    {
        const ItemDefinition *pItem = source.pItems != nullptr ? source.pItems->get(id) : nullptr;
        return pItem != nullptr ? pItem->name : "Item " + std::to_string(id);
    };
    if (section == WorldGuideSection::Quests && source.pQuests != nullptr)
    {
        for (const JournalQuestEntry &quest : source.pQuests->entries())
        {
            const bool active = knowledge.activeQuests.contains(quest.qbitId);
            if (quest.worldId != source.worldId || (!guide && !active && !knowledge.quests.contains(quest.qbitId)))
            {
                continue;
            }
            WorldGuideEntry entry{std::to_string(quest.qbitId), quest.text};
            entry.details = std::string(active ? "Active quest" : knowledge.quests.contains(quest.qbitId)
                ? "Recorded earlier (no longer in the active journal)" : "Guide reference") + "\n\n" + quest.text;
            for (const JournalQuestLocation &location : quest.locations)
            {
                const bool conditional = !location.requiredQBitIds.empty() || !location.forbiddenQBitIds.empty();
                const bool available = source.pQuests->locationAvailable(location,
                    [&](uint32_t bit) { return knowledge.questBits.contains(bit); });
                const bool remembered = knowledge.questLocations.contains(
                    JournalQuestTable::locationDiscoveryKey(quest.qbitId, location));
                const bool learned = location.disclosed && (remembered || !conditional || (active && available));
                if (!guide && !learned) continue;
                const std::string role = location.role == JournalQuestLocation::Role::Giver ? "Giver"
                    : location.role == JournalQuestLocation::Role::Return ? "Return to" : "Objective";
                std::string name, target;
                std::vector<WorldGuideLink> possibleContacts;
                WorldGuideLink::Kind kind = WorldGuideLink::Kind::Location;
                if (location.kind == JournalQuestLocation::Kind::Map && source.pMaps != nullptr)
                {
                    for (const MapStatsEntry &map : source.pMaps->getEntries())
                    {
                        const std::string id = map.canonicalId.empty()
                            ? buildCanonicalMapId(map.worldId, map.fileName) : map.canonicalId;
                        if (id == location.target && map.worldId == source.worldId)
                        {
                            name = map.name;
                            target = id;
                            break;
                        }
                    }
                }
                else if (location.kind == JournalQuestLocation::Kind::Npc && source.pNpcs != nullptr)
                {
                    const NpcEntry *pNpc = source.pNpcs->getNpc(uint32_t(std::stoul(location.target)));
                    if (pNpc != nullptr) name = pNpc->name;
                    // The selected world index distinguishes current and last-observed NPC locations.
                    if (source.pWorld != nullptr && source.pWorldKnowledge != nullptr)
                    {
                        const WorldRecord *pContact = source.pWorld->find(source.worldId + ":npc/" + location.target);
                        if (pContact != nullptr && source.pWorld->visible(*pContact, *source.pWorldKnowledge, guide))
                        {
                            name = pContact->name();
                            target = pContact->id;
                        }
                        if (guide && pNpc != nullptr)
                            for (const WorldRecord &candidate : source.pWorld->records())
                                if (candidate.possibleNpcLocation && candidate.npc->id == pNpc->id)
                                {
                                    const std::string label = "Possible: " + pNpc->name + " - "
                                        + candidate.map->name + " - " + candidate.house->name;
                                    possibleContacts.push_back({WorldGuideLink::Kind::Location,
                                        label, candidate.id});
                                }
                    }
                }
                else if (location.kind == JournalQuestLocation::Kind::House && source.pHouses != nullptr)
                {
                    const HouseEntry *pHouse = source.pHouses->get(uint32_t(std::stoul(location.target)));
                    if (pHouse != nullptr)
                    {
                        name = pHouse->name.empty() ? pHouse->buildingName : pHouse->name;
                        target = pHouse->canonicalId.empty()
                            ? source.worldId + ":house/" + location.target : pHouse->canonicalId;
                    }
                }
                else if (location.kind == JournalQuestLocation::Kind::Item && source.pItems != nullptr)
                {
                    const ItemDefinition *pItem = source.pItems->get(uint32_t(std::stoul(location.target)));
                    if (pItem != nullptr)
                    {
                        name = pItem->name;
                        target = location.target;
                        kind = WorldGuideLink::Kind::Item;
                    }
                }
                if (!name.empty())
                {
                    entry.details += "\n\n" + role + ": " + name;
                    if (conditional && !learned) entry.details += " (conditional clue)";
                    else if (conditional && learned && !available) entry.details += " (recorded earlier)";
                    // Merely hearing an item's name does not disclose its full mechanics or hidden description.
                    if (!target.empty() && (kind != WorldGuideLink::Kind::Item || guide
                        || worldItemPropertiesKnown(*source.pItems->get(uint32_t(std::stoul(location.target))),
                            knowledge, false)))
                    {
                        entry.links.push_back({kind, role + ": " + name, target, 0,
                            learned && location.kind != JournalQuestLocation::Kind::Npc});
                    }
                    for (const WorldGuideLink &possible : possibleContacts)
                        if (std::none_of(entry.links.begin(), entry.links.end(), [&](const WorldGuideLink &existing)
                            { return existing.target == possible.target; }))
                        {
                            entry.details += "\n\n" + possible.label + " (quest conditions may apply)";
                            entry.links.push_back(possible);
                        }
                }
            }
            result.push_back(std::move(entry));
        }
    }
    else if (section == WorldGuideSection::Monsters && source.pMonsters != nullptr)
    {
        std::map<int16_t, std::vector<CreatureLootReference>> loot;
        for (const CreatureLootReference &reference : creatureLootReferences(source, knowledge, guide))
            loot[reference.monsterId].push_back(reference);
        std::map<int16_t, std::vector<const WorldMapFeature *>> sources;
        if (source.pWorld != nullptr && source.pWorldKnowledge != nullptr)
            for (const WorldMapFeature *pFeature : source.pWorld->queryFeatures(*source.pWorldKnowledge, guide))
                if (pFeature->monster)
                    for (int16_t id : pFeature->monster->definitions) sources[id].push_back(pFeature);
        for (const auto &[id, monster] : source.pMonsters->statsEntries())
        {
            if (id <= 0 || id > INT16_MAX || monster.name.empty()
                || (!guide && !knowledge.monsters.contains(int16_t(id))))
                continue;
            const auto damage = [](const MonsterTable::MonsterStatsEntry::DamageProfile &profile)
            { return std::to_string(profile.diceRolls) + "d" + std::to_string(profile.diceSides)
                + " + " + std::to_string(profile.bonus); };
            WorldGuideEntry entry{std::to_string(id), monster.name};
            entry.details = monster.name + "\nLevel: " + std::to_string(monster.level)
                + " | HP: " + std::to_string(monster.hitPoints) + " | AC: " + std::to_string(monster.armorClass)
                + "\nExperience: " + std::to_string(monster.experience)
                + "\n\nAttack: " + monster.attack1Type + " " + damage(monster.attack1Damage);
            if (monster.attack2Chance > 0)
                entry.details += "\nSecond attack (" + std::to_string(monster.attack2Chance) + "%): "
                    + monster.attack2Type + " " + damage(monster.attack2Damage);
            if (monster.hasSpell1)
                entry.details += "\nSpell: " + monster.spell1Name;
            if (monster.hasSpell2)
                entry.details += "\nSpell: " + monster.spell2Name;
            if (monster.specialAttackKind != MonsterSpecialAttackKind::None)
                entry.details += "\nSpecial attack: " + monster.specialAttackDescriptor;
            entry.details += "\n\nResistances\nFire " + std::to_string(monster.fireResistance)
                + " | Air " + std::to_string(monster.airResistance) + " | Water "
                + std::to_string(monster.waterResistance) + " | Earth " + std::to_string(monster.earthResistance)
                + "\nMind " + std::to_string(monster.mindResistance) + " | Spirit "
                + std::to_string(monster.spiritResistance) + " | Body " + std::to_string(monster.bodyResistance)
                + "\nLight " + std::to_string(monster.lightResistance) + " | Dark "
                + std::to_string(monster.darkResistance) + " | Physical " + std::to_string(monster.physicalResistance)
                + "\n\nLoot\nGold: " + std::to_string(monster.loot.goldDiceRolls) + "d"
                + std::to_string(monster.loot.goldDiceSides) + "\nItem chance: "
                + std::to_string(monster.loot.itemChance) + "% | Item level: " + std::to_string(monster.loot.itemLevel)
                + "\n\nShared base definition. Runtime scaling and effects may change these values.";
            for (const CreatureLootReference &reference : loot[int16_t(id)])
            {
                const ItemDefinition &item = *source.pItems->get(reference.itemId);
                InventoryItem display;
                display.identified = worldItemPropertiesKnown(item, knowledge, guide);
                entry.links.push_back({WorldGuideLink::Kind::Item,
                    reference.provenance + ": " + ItemRuntime::displayName(display, item),
                    std::to_string(reference.itemId)});
            }
            if (!loot[int16_t(id)].empty())
                entry.details += "\n\nCreature loot links distinguish drop chances, initial carried items "
                    "and recovered loot. They do not guarantee a drop from every creature.";
            const std::string picture = toLowerCopy(monster.pictureName);
            const auto *pCanonical = source.pMonsters->findStatsByPictureName(picture);
            // Use the same explicit picture tiers as encounter resolution, never numeric ID adjacency.
            if (pCanonical != nullptr && pCanonical->id == id && picture.size() > 2
                && picture[picture.size() - 2] == ' ' && picture.back() >= 'a' && picture.back() <= 'c')
                for (char tier : {'A', 'B', 'C'})
                {
                    const auto *pVariant = source.pMonsters->findStatsByPictureName(
                        picture.substr(0, picture.size() - 1) + tier);
                    if (pVariant != nullptr && pVariant->id > 0 && pVariant->id <= INT16_MAX
                        && pVariant->id != id && !pVariant->name.empty()
                        && (guide || knowledge.monsters.contains(int16_t(pVariant->id))))
                        entry.links.push_back({WorldGuideLink::Kind::Monster,
                            "Variant " + std::string(1, tier) + ": " + pVariant->name, std::to_string(pVariant->id)});
                }
            for (const WorldMapFeature *pFeature : sources[int16_t(id)])
            {
                const WorldRecord *pMap = source.pWorld->find(pFeature->mapId);
                entry.links.push_back({WorldGuideLink::Kind::Feature,
                    pFeature->title + " - " + pMap->name() + " ("
                        + std::to_string(int64_t(pFeature->site.x)) + ", "
                        + std::to_string(int64_t(pFeature->site.y)) + ")",
                    pFeature->id});
            }
            if (!sources[int16_t(id)].empty())
                entry.details += "\n\nMap links distinguish initial placements, possible encounter areas and last "
                    "observations. They do not show the current population.";
            result.push_back(std::move(entry));
        }
    }
    else if (section == WorldGuideSection::Items && source.pItems != nullptr)
    {
        std::map<uint32_t, std::vector<CreatureLootReference>> loot;
        for (const CreatureLootReference &reference : creatureLootReferences(source, knowledge, guide))
            loot[reference.itemId].push_back(reference);
        std::map<uint32_t, std::vector<const WorldItemSource *>> sources;
        if (source.pWorld != nullptr && source.pWorldKnowledge != nullptr)
            for (const WorldItemSource *pItemSource : source.pWorld->itemSources(*source.pWorldKnowledge, guide))
                sources[pItemSource->itemId].push_back(pItemSource);
        for (const ItemDefinition &item : source.pItems->entries())
        {
            if (item.itemId == 0 || item.name.empty() || (!guide && !knowledge.items.contains(item.itemId)
                && !knowledge.identifiedItems.contains(item.itemId)))
                continue;
            const bool identified = worldItemPropertiesKnown(item, knowledge, guide);
            InventoryItem unknown;
            unknown.identified = false;
            WorldGuideEntry entry{std::to_string(item.itemId), identified ? item.name
                : ItemRuntime::displayName(unknown, item)};
            entry.details = identified ? itemDefinitionText(item)
                : entry.title + "\nNot identified by this party.";
            std::set<std::pair<std::string, std::string>> linkedSources;
            for (const WorldItemSource *pItemSource : sources[item.itemId])
            {
                const WorldRecord *pMap = source.pWorld->find(pItemSource->mapId);
                std::string target = pMap->id;
                std::string label = worldItemSourceLabel(*pItemSource) + ": " + pMap->name();
                WorldGuideLink::Kind kind = WorldGuideLink::Kind::Location;
                if (const WorldRecord *pProvider = source.pWorld->find(pItemSource->providerId);
                    pProvider != nullptr && source.pWorld->visible(*pProvider, *source.pWorldKnowledge, guide))
                {
                    target = pProvider->id;
                    label = worldItemSourceLabel(*pItemSource) + ": " + pProvider->name() + " - " + pMap->name();
                }
                for (const WorldMapFeature &feature : source.pWorld->features())
                    if ((feature.id == pItemSource->id || (pItemSource->kind == WorldItemSourceKind::Chest
                        && feature.mapId == pItemSource->mapId
                        && std::find(feature.chestIds.begin(), feature.chestIds.end(), pItemSource->sourceIndex)
                            != feature.chestIds.end()))
                        && source.pWorld->featureVisible(feature, *source.pWorldKnowledge, guide))
                    {
                        target = feature.id; kind = WorldGuideLink::Kind::Feature;
                        break;
                    }
                if (pItemSource->position)
                    label += " (" + std::to_string((*pItemSource->position)[0]) + ", "
                        + std::to_string((*pItemSource->position)[1]) + ")";
                if (linkedSources.emplace(label, target).second)
                    entry.links.push_back({kind, label, target});
            }
            for (const CreatureLootReference &reference : loot[item.itemId])
                entry.links.push_back({WorldGuideLink::Kind::Monster,
                    reference.provenance + ": " + source.pMonsters->findStatsById(reference.monsterId)->name,
                    std::to_string(reference.monsterId)});
            if (!sources[item.itemId].empty() || !loot[item.itemId].empty())
                entry.details += "\n\nSources below describe authored data or recorded observations. "
                    "They do not guarantee remaining loot, random drops or quest availability.";
            if (identified && source.pSpells != nullptr)
                if (const auto spell = spellbookSpellId(item))
                    if (const SpellEntry *pSpell = source.pSpells->findById(int(*spell)))
                        entry.links.push_back({WorldGuideLink::Kind::Spell, "Spell: " + pSpell->name,
                            std::to_string(*spell)});
            result.push_back(std::move(entry));
        }
    }
    else if (section == WorldGuideSection::Spells && source.pSpells != nullptr)
    {
        std::map<uint32_t, std::vector<const ItemDefinition *>> books;
        if (source.pItems != nullptr)
            for (const ItemDefinition &item : source.pItems->entries())
            {
                if (item.itemId == 0 || item.name.empty()) continue;
                if (const auto spell = spellbookSpellId(item)) books[*spell].push_back(&item);
            }
        for (const SpellEntry &spell : source.pSpells->entries())
        {
            const std::vector<const ItemDefinition *> &spellbooks = books[uint32_t(spell.id)];
            const bool knownBook = std::any_of(spellbooks.begin(), spellbooks.end(),
                [&](const ItemDefinition *pBook) { return worldItemPropertiesKnown(*pBook, knowledge, false); });
            if (spell.name.empty() || (!guide && !knowledge.spells.contains(uint32_t(spell.id)) && !knownBook))
                continue;
            const auto skill = resolveMagicSkillName(uint32_t(spell.id));
            const std::string school = skill ? displaySkillName(*skill) : "Other spells";
            WorldGuideEntry entry{std::to_string(spell.id), school + " - " + spell.name};
            const std::string status = guide ? "Guide reference"
                : knowledge.spells.contains(uint32_t(spell.id)) ? "Learned by the party" : "Spellbook encountered";
            entry.details = status + "\n\n" + spell.name + " | " + school + "\n"
                + spell.description + "\n\n"
                + "Normal (" + std::to_string(spell.normalManaCost) + " SP): " + spell.normalText + "\n\n"
                + "Expert (" + std::to_string(spell.expertManaCost) + " SP): " + spell.expertText + "\n\n"
                + "Master (" + std::to_string(spell.masterManaCost) + " SP): " + spell.masterText + "\n\n"
                + "Grandmaster (" + std::to_string(spell.grandmasterManaCost) + " SP): "
                + spell.grandmasterText + "\n\n" + spell.statsText;
            for (const ItemDefinition *pBook : spellbooks)
                if (worldItemPropertiesKnown(*pBook, knowledge, guide))
                    entry.links.push_back({WorldGuideLink::Kind::Item, "Spellbook: " + pBook->name,
                        std::to_string(pBook->itemId)});
            size_t guilds = 0;
            if (source.pWorld != nullptr)
                for (const WorldRecord &record : source.pWorld->records())
                {
                    if (record.kind != WorldRecordKind::House || record.map->worldId != source.worldId
                        || (!guide && (source.pWorldKnowledge == nullptr
                            || !source.pWorld->visible(record, *source.pWorldKnowledge, false)))
                        || std::none_of(spellbooks.begin(), spellbooks.end(), [&](const ItemDefinition *pBook)
                            { return guildSpellbookStockWeight(*record.house, *pBook) > 0; }))
                        continue;
                    entry.links.push_back({WorldGuideLink::Kind::Location,
                        "Possible stock: " + record.name() + " - " + record.map->name, record.id});
                    ++guilds;
                }
            entry.details += guilds == 0
                ? "\n\nNo matching guild stock sources " + std::string(guide ? "in this world." : "discovered yet.")
                : "\n\nGuild links describe the random stock pool, not current shelves. "
                  "Stock rotates; guild access and spell-learning requirements still apply.";
            result.push_back(std::move(entry));
        }
    }
    else if (section == WorldGuideSection::Classes && source.pClasses != nullptr && source.pSkills != nullptr)
    {
        std::vector<std::pair<uint32_t, std::string>> races;
        if (source.pCharacterSelection != nullptr)
        {
            for (const auto &[raceId, name] : source.pCharacterSelection->raceNames())
                if (!name.empty() && (guide || knowledge.races.contains(raceId))) races.emplace_back(raceId, name);
            std::sort(races.begin(), races.end(), [](const auto &left, const auto &right)
            { return left.second == right.second ? left.first < right.first : left.second < right.second; });
        }
        const auto signedValue = [](int64_t value)
        { return (value > 0 ? "+" : "") + std::to_string(value); };
        std::map<std::string, std::vector<std::string>> previousPromotions;
        for (const auto &[id, definition] : source.pClasses->entries())
            for (const std::string &target : source.pSkills->promotionClassNamesFromMetadata(id))
                previousPromotions[target].push_back(id);
        for (auto &[target, parents] : previousPromotions) std::sort(parents.begin(), parents.end());
        for (const auto &[id, definition] : source.pClasses->entries())
        {
            if (!guide && !knowledge.classes.contains(id))
                continue;
            WorldGuideEntry entry{id, definition.displayName};
            entry.details = definition.displayName + "\nBase HP: " + std::to_string(definition.baseHealth)
                + " | HP/level: " + std::to_string(definition.healthPerLevel)
                + "\nBase SP: " + std::to_string(definition.baseMana)
                + " | SP/level: " + std::to_string(definition.manaPerLevel)
                + classStartingSkills(source, id, races)
                + "\n\nCurrent class mastery limits:\n";
            for (const std::string &skill : allCanonicalSkillNames())
            {
                const SkillMastery cap = source.pSkills->getClassCap(id, skill);
                if (cap == SkillMastery::None)
                    continue;
                if (cap == SkillMastery::Normal)
                {
                    entry.details += displaySkillName(skill) + ": " + masteryDisplayName(cap) + "\n";
                    continue;
                }
                entry.links.push_back({WorldGuideLink::Kind::Trainer, displaySkillName(skill)
                    + ": " + masteryDisplayName(cap) + " (find trainers)", displaySkillName(skill), uint32_t(cap) - 1});
            }
            for (const auto &[raceId, raceName] : races)
            {
                std::string differences;
                for (const std::string &skill : allCanonicalSkillNames())
                {
                    const SkillMastery base = source.pSkills->getClassCap(id, skill);
                    const SkillMastery effective = source.pSkills->getEffectiveCap(id, raceId, skill);
                    if (base == effective) continue;
                    differences += displaySkillName(skill) + ": " + masteryDisplayName(base)
                        + " -> " + masteryDisplayName(effective) + "\n";
                    if (effective >= SkillMastery::Expert)
                        entry.links.push_back({WorldGuideLink::Kind::Trainer, raceName + ": " + displaySkillName(skill)
                            + " - " + masteryDisplayName(effective) + " (find trainers)", displaySkillName(skill),
                            uint32_t(effective) - 1});
                }
                if (!differences.empty())
                    entry.details += "\n" + raceName + " mastery differences:\n" + differences;
            }
            for (const std::string &parent : previousPromotions[id])
                if (guide || knowledge.classes.contains(parent))
                    entry.links.push_back({WorldGuideLink::Kind::Class,
                        "Earlier tier: " + source.pClasses->get(parent)->displayName, parent});
            std::string promotions;
            for (const std::string &target : source.pSkills->promotionClassNamesFromMetadata(id))
            {
                const ClassMultiplierEntry *pPromotion = source.pClasses->get(target);
                if (pPromotion == nullptr || (!guide && !knowledge.classes.contains(target))) continue;
                promotions += pPromotion->displayName + ": base HP "
                    + signedValue(int64_t(pPromotion->baseHealth) - definition.baseHealth) + ", HP/level "
                    + signedValue(int64_t(pPromotion->healthPerLevel) - definition.healthPerLevel) + ", base SP "
                    + signedValue(int64_t(pPromotion->baseMana) - definition.baseMana) + ", SP/level "
                    + signedValue(int64_t(pPromotion->manaPerLevel) - definition.manaPerLevel) + "\n";
                entry.links.push_back({WorldGuideLink::Kind::Class, "Promotion: " + pPromotion->displayName, target});
            }
            if (!promotions.empty())
                entry.details += "\nNext promotion tier (growth changes):\n" + promotions
                    + "Quest completion and branching requirements still apply.\n";
            result.push_back(std::move(entry));
        }
    }
    else if (section == WorldGuideSection::Companions && source.pRoster != nullptr)
    {
        for (const RosterEntry *pMember : source.pRoster->getEntriesSortedById())
        {
            const Character *pCurrent = nullptr;
            bool inInn = false;
            if (source.pParty != nullptr)
            {
                for (const Character &member : source.pParty->members())
                    if (member.rosterId == pMember->id) pCurrent = &member;
                if (pCurrent == nullptr)
                    for (const AdventurersInnMember &member : source.pParty->adventurersInnMembers())
                        if (member.character.rosterId == pMember->id)
                        {
                            pCurrent = &member.character;
                            inInn = true;
                            break;
                        }
            }
            if (pMember->name.empty()
                || (!guide && !knowledge.roster.contains(pMember->id) && pCurrent == nullptr)) continue;
            WorldGuideEntry entry{std::to_string(pMember->id), pMember->name};
            if (pCurrent != nullptr)
                entry.details = std::string(inInn ? "In the Adventurer's Inn roster." : "Travelling with the party.")
                    + "\nCampaign character: " + pCurrent->name + " | " + displayClassName(pCurrent->className)
                    + " | Level " + std::to_string(pCurrent->level) + "\n\n";
            else entry.details = source.pParty != nullptr ? "Not in the active party or Adventurer's Inn roster.\n\n"
                : "Load a campaign to see this companion's current recruitment state.\n\n";
            entry.details += "Starting profile: " + pMember->name + " | " + displayClassName(pMember->className)
                + "\nLevel: " + std::to_string(pMember->level) + "\n\n" + pMember->blurb
                + "\n\nMight " + std::to_string(pMember->might) + " | Intellect "
                + std::to_string(pMember->intellect) + " | Personality " + std::to_string(pMember->personality)
                + "\nEndurance " + std::to_string(pMember->endurance) + " | Accuracy "
                + std::to_string(pMember->accuracy) + "\nSpeed " + std::to_string(pMember->speed)
                + " | Luck " + std::to_string(pMember->luck);
            if (guide && pCurrent == nullptr && pMember->unlockQuestBitId != 0)
            {
                entry.details += "\n\nThe Adventurer's Inn can add this companion"
                    " when their campaign unlock is recorded.";
                if (source.pParty != nullptr)
                    entry.details += source.pParty->hasQuestBit(pMember->unlockQuestBitId)
                        ? "\nInn unlock recorded in this campaign." : "\nInn unlock not recorded yet.";
            }
            appendCompanionContacts(entry, source, pMember->id, guide);
            result.push_back(std::move(entry));
        }
    }
    else if (section == WorldGuideSection::Progress && source.pHouses != nullptr && source.pMaps != nullptr)
    {
        size_t visibleTaverns = 0, wonTaverns = 0;
        for (const auto &[id, house] : source.pHouses->entries())
        {
            const MapStatsEntry *pMap = source.pMaps->findById(house.mapId);
            if (!house.arcomageRule || pMap == nullptr || pMap->worldId != source.worldId
                || (!guide && !knowledge.houses.contains(id) && !knowledge.arcomageWins.contains(id)))
                continue;
            ++visibleTaverns;
            const bool won = knowledge.arcomageWins.contains(id);
            wonTaverns += won ? 1 : 0;
            const std::string locationId = house.canonicalId.empty()
                ? pMap->worldId + ":house/" + std::to_string(id) : house.canonicalId;
            WorldGuideEntry entry{locationId, "Arcomage - "
                + (house.name.empty() ? house.buildingName : house.name)};
            const HouseEntry::ArcomageRule &rule = *house.arcomageRule;
            entry.details = entry.title + "\n" + pMap->name + "\n\n"
                + (knowledge.hasParty ? (won ? "First victory recorded.\n" : "No victory recorded here yet.\n")
                                     : "Load a campaign to see its victories.\n")
                + "\nTower to win: " + std::to_string(rule.towerToWin)
                + "\nResources to win: " + std::to_string(rule.resourceToWin)
                + "\nStarting tower: " + std::to_string(rule.towerAtStart)
                + "\nStarting wall: " + std::to_string(rule.wallAtStart);
            entry.links.push_back({WorldGuideLink::Kind::Location, "Show tavern on map", locationId});
            result.push_back(std::move(entry));
        }
        WorldGuideEntry summary{"arcomage", "Arcomage - campaign progress"};
        summary.details = "Arcomage\n\n";
        if (knowledge.hasParty)
            summary.details += "Campaign victories: " + std::to_string(knowledge.arcomageWinCount)
                + "\nCampaign losses: " + std::to_string(knowledge.arcomageLossCount)
                + "\n\nVictorious taverns in this " + (guide ? std::string("world") : std::string("discovered list"))
                + ": " + std::to_string(wonTaverns) + " / " + std::to_string(visibleTaverns)
                + "\n\nCampaign totals include all worlds. Tavern entries use the existing first-win records.";
        else
            summary.details += "Load a campaign to see its victories.\n";
        result.push_back(std::move(summary));
        if (guide || std::any_of(knowledge.arenaWins.begin(), knowledge.arenaWins.end(),
                                [](int32_t wins) { return wins > 0; }))
        {
            WorldGuideEntry arena{"arena", "Arena - campaign victories"};
            arena.details = "Arena\n\n";
            const char *ranks[] = {"Page", "Squire", "Knight", "Lord"};
            if (knowledge.hasParty)
                for (size_t i = 0; i < knowledge.arenaWins.size(); ++i)
                    arena.details += std::string(ranks[i]) + ": " + std::to_string(knowledge.arenaWins[i]) + "\n";
            else
                arena.details += "Load a campaign to see its victories.\n";
            arena.details += "\nThe existing campaign counters are shared across worlds.";
            result.push_back(std::move(arena));
        }
    }
    else if (section == WorldGuideSection::Alchemy && source.pItems != nullptr && source.pPotions != nullptr
        && source.pReagents != nullptr && source.pMixes != nullptr && source.pPotionNotes != nullptr)
    {
        std::vector<WorldAlchemyRecipe> recipes;
        std::unordered_set<uint32_t> ingredients;
        for (const MergedReagentSettingEntry &reagent : source.pReagents->entries())
        {
            ingredients.insert(reagent.itemId);
            const ItemDefinition *pReagent = source.pItems->get(reagent.itemId);
            const ItemDefinition *pResult = source.pItems->get(reagent.resultItemId);
            if (guide || (pReagent != nullptr && pResult != nullptr
                && worldItemPropertiesKnown(*pReagent, knowledge, false)
                && worldItemPropertiesKnown(*pResult, knowledge, false)))
                recipes.push_back({reagent.resultItemId, reagent.itemId, 0});
        }
        const auto &potions = source.pPotions->entries();
        for (size_t first = 0; first < potions.size(); ++first)
            for (size_t second = 0; second < potions.size(); ++second)
            {
                const uint32_t a = potions[first].itemId, b = potions[second].itemId;
                const auto mix = source.pMixes->potionCombination(a, b);
                const auto note = source.pPotionNotes->autonoteIdForMix(b, a);
                if (mix && !mix->noMix && mix->failureDamageLevel == 0 && mix->resultItemId != 0
                    && mix->resultItemId != a && mix->resultItemId != b
                    && (guide || (note && knowledge.recipes.contains(*note))))
                    recipes.push_back({mix->resultItemId, a, b});
            }
        for (const MergedPotionSettingEntry &potion : potions)
        {
            const uint32_t id = potion.itemId;
            const auto plan = planWorldAlchemy(id, recipes, ingredients);
            const bool hasFormula = std::any_of(recipes.begin(), recipes.end(),
                [id](const WorldAlchemyRecipe &recipe) { return recipe.result == id; });
            if (!guide && !hasFormula)
                continue;
            WorldGuideEntry entry{std::to_string(id), itemName(id)};
            entry.details = itemName(id) + "\n" + potion.note + "\n";
            if (potion.requiredMastery)
                entry.details += "Alchemy mastery: " + masteryDisplayName(SkillMastery(*potion.requiredMastery)) + "\n";
            entry.details += "\nOne minimum-reagent path (intermediate steps first):\n";
            std::set<uint32_t> linked;
            const auto linkInput = [&](uint32_t input)
            {
                if (const ItemDefinition *pInput = source.pItems->get(input); input != 0 && pInput != nullptr
                    && linked.insert(input).second && worldItemPropertiesKnown(*pInput, knowledge, guide))
                    entry.links.push_back({source.pPotions->getByItemId(input) != nullptr
                        ? WorldGuideLink::Kind::Alchemy : WorldGuideLink::Kind::Item,
                        "Inspect " + itemName(input), std::to_string(input)});
            };
            for (const WorldAlchemyRecipe &step : plan)
            {
                entry.details += itemName(step.first);
                if (step.second != 0)
                    entry.details += " + " + itemName(step.second);
                else
                    entry.details += " + empty bottle";
                entry.details += " -> " + itemName(step.result) + "\n";
                for (uint32_t input : {step.first, step.second}) linkInput(input);
            }
            if (plan.empty())
                entry.details += "No complete known reagent path is available.\n";
            else
            {
                std::map<uint32_t, uint64_t> amounts{{id, 1}};
                for (auto step = plan.rbegin(); step != plan.rend(); ++step)
                {
                    const uint64_t amount = amounts[step->result];
                    amounts[step->first] += amount;
                    if (step->second != 0)
                        amounts[step->second] += amount;
                }
                entry.details += "\nReagents needed for this path:\n";
                for (const auto &[ingredient, count] : amounts)
                    if (count != 0 && ingredients.contains(ingredient))
                        entry.details += std::to_string(count) + " x " + itemName(ingredient) + "\n";
            }
            entry.details += "\nDirect formulas:\n";
            std::set<std::pair<uint32_t, uint32_t>> shown;
            for (const WorldAlchemyRecipe &recipe : recipes)
                if (recipe.result == id && shown.insert(std::minmax(recipe.first, recipe.second)).second)
                {
                    entry.details += itemName(recipe.first) + " + "
                        + (recipe.second == 0 ? "empty bottle" : itemName(recipe.second)) + "\n";
                    linkInput(recipe.first);
                    linkInput(recipe.second);
                }
            entry.links.push_back({WorldGuideLink::Kind::Item, "Inspect item", std::to_string(id)});
            result.push_back(std::move(entry));
        }
    }
    std::sort(result.begin(), result.end(), [](const WorldGuideEntry &a, const WorldGuideEntry &b)
    { return a.title < b.title || (a.title == b.title && a.id < b.id); });
    return result;
}
}
