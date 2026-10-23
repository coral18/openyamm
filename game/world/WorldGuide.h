#pragma once

#include "game/party/SkillData.h"

#include <cstdint>
#include <array>
#include <string>
#include <utility>
#include <unordered_set>
#include <vector>

namespace OpenYAMM::Game
{
class ItemTable;
class SpellTable;
class ClassMultiplierTable;
class ClassSkillTable;
class PotionMixingTable;
class PotionNoteTable;
class MergedPotionSettingTable;
class MergedReagentSettingTable;
class RosterTable;
class HouseTable;
class MapStats;
class MonsterTable;
class JournalQuestTable;
class NpcDialogTable;
class WorldDatabase;
class MergedCharacterSelectionTable;
struct ClassMultiplierEntry;
struct ItemDefinition;
struct WorldKnowledge;

enum class WorldGuideSection { Items, Spells, Classes, Alchemy, Companions, Progress, Monsters, Quests };

// A read-only view of the loaded game registries, never a second set of content definitions.
struct WorldGuideSource
{
    const ItemTable *pItems = nullptr;
    const SpellTable *pSpells = nullptr;
    const ClassMultiplierTable *pClasses = nullptr;
    const ClassSkillTable *pSkills = nullptr;
    const MergedPotionSettingTable *pPotions = nullptr;
    const MergedReagentSettingTable *pReagents = nullptr;
    const PotionMixingTable *pMixes = nullptr;
    const PotionNoteTable *pPotionNotes = nullptr;
    const RosterTable *pRoster = nullptr;
    const HouseTable *pHouses = nullptr;
    const MapStats *pMaps = nullptr;
    std::string worldId;
    const MonsterTable *pMonsters = nullptr;
    const JournalQuestTable *pQuests = nullptr;
    const NpcDialogTable *pNpcs = nullptr;
    const WorldDatabase *pWorld = nullptr;
    const WorldKnowledge *pWorldKnowledge = nullptr;
    const MergedCharacterSelectionTable *pCharacterSelection = nullptr;
};

struct WorldGuideKnowledge
{
    std::unordered_set<uint32_t> items, spells, recipes, roster;
    std::unordered_set<std::string> classes;
    std::unordered_set<uint32_t> houses, arcomageWins;
    std::array<int32_t, 4> arenaWins{};
    uint32_t arcomageWinCount = 0, arcomageLossCount = 0;
    bool hasParty = false;
    std::unordered_set<int16_t> monsters;
    std::unordered_set<uint32_t> quests, activeQuests;
    std::unordered_set<uint32_t> questBits;
    std::unordered_set<std::string> questLocations;
    std::unordered_set<uint32_t> races;
};

struct WorldGuideLink
{
    enum class Kind { Trainer, Alchemy, Item, Location, Spell, Class, Feature, Monster };
    Kind kind = Kind::Item;
    std::string label;
    std::string target;
    // Teacher-topic ranks: 1 Expert, 2 Master, 3 Grandmaster (not SkillMastery values).
    uint32_t mastery = 0;
    // A disclosed quest target may be known without being visited or explored.
    bool informed = false;
};

// Derived reading text and links for a single view; values always come from the loaded tables.
struct WorldGuideEntry
{
    std::string id;
    std::string title;
    std::string details;
    std::vector<WorldGuideLink> links;
};

struct WorldAlchemyRecipe
{
    uint32_t result = 0, first = 0, second = 0;
};

struct WorldClassComparisonColumn
{
    std::string id;
    const ClassMultiplierEntry *pDefinition = nullptr;
};

struct WorldClassComparisonRow
{
    std::string skill;
    std::vector<SkillMastery> caps;
};

// Definitions refer to the active class registry, which must outlive this read-only view.
struct WorldClassComparison
{
    std::vector<WorldClassComparisonColumn> classes;
    std::vector<WorldClassComparisonRow> skills;
    std::vector<std::pair<uint32_t, std::string>> races;
    std::optional<uint32_t> raceId;
};

// Base definitions from the active item registry, which must outlive this read-only view.
struct WorldItemComparison
{
    std::array<const ItemDefinition *, 2> items{};
    std::array<std::string, 2> details;
};

std::optional<WorldItemComparison> buildWorldItemComparison(const WorldGuideSource &source,
    const WorldGuideKnowledge &knowledge, bool guide, uint32_t pinnedItem, uint32_t selectedItem);

std::optional<WorldClassComparison> buildWorldClassComparison(const WorldGuideSource &source,
    const WorldGuideKnowledge &knowledge, bool guide, const std::string &anchorClass,
    std::optional<uint32_t> raceId = std::nullopt);

// Cheapest known reagent path. Cycles/unreachable recipes cannot create an infinite expansion.
std::vector<WorldAlchemyRecipe> planWorldAlchemy(uint32_t target, const std::vector<WorldAlchemyRecipe> &recipes,
    const std::unordered_set<uint32_t> &ingredients);
std::vector<WorldGuideEntry> buildWorldGuide(const WorldGuideSource &source, WorldGuideSection section,
    const WorldGuideKnowledge &knowledge, bool guide);
}
