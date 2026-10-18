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

#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <stdexcept>

namespace OpenYAMM::Game
{
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
                        || knowledge.items.contains(uint32_t(std::stoul(location.target)))))
                    {
                        entry.links.push_back({kind, role + ": " + name, target, 0,
                            learned && location.kind != JournalQuestLocation::Kind::Npc});
                    }
                }
            }
            result.push_back(std::move(entry));
        }
    }
    else if (section == WorldGuideSection::Monsters && source.pMonsters != nullptr)
    {
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
            result.push_back(std::move(entry));
        }
    }
    else if (section == WorldGuideSection::Items && source.pItems != nullptr)
    {
        for (const ItemDefinition &item : source.pItems->entries())
        {
            if (item.name.empty() || (!guide && !knowledge.items.contains(item.itemId)))
                continue;
            WorldGuideEntry entry{std::to_string(item.itemId), item.name};
            entry.details = item.name + "\n" + item.equipStat + " | " + item.skillGroup
                + "\nValue: " + std::to_string(item.value) + " gold\n";
            if (!item.mod1.empty() || !item.mod2.empty())
                entry.details += "Properties: " + item.mod1 + " " + item.mod2 + "\n";
            if (!item.materialName.empty())
                entry.details += "Material: " + item.materialName + "\n";
            entry.details += item.notes;
            result.push_back(std::move(entry));
        }
    }
    else if (section == WorldGuideSection::Spells && source.pSpells != nullptr)
    {
        for (const SpellEntry &spell : source.pSpells->entries())
        {
            if (spell.name.empty() || (!guide && !knowledge.spells.contains(uint32_t(spell.id))))
                continue;
            WorldGuideEntry entry{std::to_string(spell.id), spell.schoolName + " - " + spell.name};
            entry.details = spell.name + " | " + spell.schoolName + "\n" + spell.description + "\n\n"
                + "Normal (" + std::to_string(spell.normalManaCost) + " SP): " + spell.normalText + "\n\n"
                + "Expert (" + std::to_string(spell.expertManaCost) + " SP): " + spell.expertText + "\n\n"
                + "Master (" + std::to_string(spell.masterManaCost) + " SP): " + spell.masterText + "\n\n"
                + "Grandmaster (" + std::to_string(spell.grandmasterManaCost) + " SP): "
                + spell.grandmasterText + "\n\n" + spell.statsText;
            result.push_back(std::move(entry));
        }
    }
    else if (section == WorldGuideSection::Classes && source.pClasses != nullptr && source.pSkills != nullptr)
    {
        for (const auto &[id, definition] : source.pClasses->entries())
        {
            if (!guide && !knowledge.classes.contains(id))
                continue;
            WorldGuideEntry entry{id, definition.displayName};
            entry.details = definition.displayName + "\nBase HP: " + std::to_string(definition.baseHealth)
                + " | HP/level: " + std::to_string(definition.healthPerLevel)
                + "\nBase SP: " + std::to_string(definition.baseMana)
                + " | SP/level: " + std::to_string(definition.manaPerLevel)
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
            result.push_back(std::move(entry));
        }
    }
    else if (section == WorldGuideSection::Companions && source.pRoster != nullptr)
    {
        for (const RosterEntry *pMember : source.pRoster->getEntriesSortedById())
        {
            if (pMember->name.empty() || (!guide && !knowledge.roster.contains(pMember->id)))
                continue;
            WorldGuideEntry entry{std::to_string(pMember->id), pMember->name};
            entry.details = pMember->name + " | " + displayClassName(pMember->className)
                + "\nLevel: " + std::to_string(pMember->level) + "\n\n" + pMember->blurb
                + "\n\nMight " + std::to_string(pMember->might) + " | Intellect "
                + std::to_string(pMember->intellect) + " | Personality " + std::to_string(pMember->personality)
                + "\nEndurance " + std::to_string(pMember->endurance) + " | Accuracy "
                + std::to_string(pMember->accuracy) + "\nSpeed " + std::to_string(pMember->speed)
                + " | Luck " + std::to_string(pMember->luck);
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
            if (guide || (knowledge.items.contains(reagent.itemId) && knowledge.items.contains(reagent.resultItemId)))
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
            for (const WorldAlchemyRecipe &step : plan)
            {
                entry.details += itemName(step.first);
                if (step.second != 0)
                    entry.details += " + " + itemName(step.second);
                else
                    entry.details += " + empty bottle";
                entry.details += " -> " + itemName(step.result) + "\n";
                for (uint32_t input : {step.first, step.second})
                    if (input != 0 && linked.insert(input).second && (guide || knowledge.items.contains(input)))
                        entry.links.push_back({source.pPotions->getByItemId(input) != nullptr
                            ? WorldGuideLink::Kind::Alchemy : WorldGuideLink::Kind::Item,
                            "Inspect " + itemName(input), std::to_string(input)});
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
                    entry.details += itemName(recipe.first) + " + "
                        + (recipe.second == 0 ? "empty bottle" : itemName(recipe.second)) + "\n";
            entry.links.push_back({WorldGuideLink::Kind::Item, "Inspect item", std::to_string(id)});
            result.push_back(std::move(entry));
        }
    }
    std::sort(result.begin(), result.end(), [](const WorldGuideEntry &a, const WorldGuideEntry &b)
    { return a.title < b.title || (a.title == b.title && a.id < b.id); });
    return result;
}
}
