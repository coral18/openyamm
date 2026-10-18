#include "game/gameplay/GuildSpellbookRules.h"
#include "game/tables/HouseTable.h"
#include "game/tables/ItemTable.h"

#include <algorithm>
#include <charconv>

namespace OpenYAMM::Game
{
std::optional<uint32_t> spellbookSpellId(const ItemDefinition &item)
{
    if (item.equipStat != "Book" || item.mod1.size() < 2 || item.mod1[0] != 'S')
    {
        return std::nullopt;
    }
    uint32_t spell = 0;
    const char *pEnd = item.mod1.data() + item.mod1.size();
    const auto parsed = std::from_chars(item.mod1.data() + 1, pEnd, spell);
    if (parsed.ec != std::errc() || parsed.ptr != pEnd || spell == 0)
    {
        return std::nullopt;
    }
    return spell;
}

namespace
{
bool isGuildType(const HouseEntry &house, const char *pType)
{
    return house.type == pType;
}

bool schoolAllowsSpellbook(const HouseEntry &houseEntry, const ItemDefinition &itemDefinition)
{
    const std::optional<uint32_t> spellId = spellbookSpellId(itemDefinition);

    if (!spellId)
    {
        return false;
    }

    if (isGuildType(houseEntry, "Elemental Guild"))
    {
        return *spellId >= 1 && *spellId <= 44;
    }

    if (isGuildType(houseEntry, "Fire Guild"))
    {
        return *spellId >= 1 && *spellId <= 11;
    }

    if (isGuildType(houseEntry, "Air Guild"))
    {
        return *spellId >= 12 && *spellId <= 22;
    }

    if (isGuildType(houseEntry, "Water Guild"))
    {
        return *spellId >= 23 && *spellId <= 33;
    }

    if (isGuildType(houseEntry, "Earth Guild"))
    {
        return *spellId >= 34 && *spellId <= 44;
    }

    if (isGuildType(houseEntry, "Self Guild"))
    {
        return *spellId >= 45 && *spellId <= 77;
    }

    if (isGuildType(houseEntry, "Spirit Guild"))
    {
        return *spellId >= 45 && *spellId <= 55;
    }

    if (isGuildType(houseEntry, "Mind Guild"))
    {
        return *spellId >= 56 && *spellId <= 66;
    }

    if (isGuildType(houseEntry, "Body Guild"))
    {
        return *spellId >= 67 && *spellId <= 77;
    }

    if (isGuildType(houseEntry, "Light Guild"))
    {
        return *spellId >= 78 && *spellId <= 88;
    }

    if (isGuildType(houseEntry, "Dark Guild"))
    {
        return *spellId >= 89 && *spellId <= 99;
    }

    if (isGuildType(houseEntry, "Spell Shop"))
    {
        return *spellId >= 1 && *spellId <= 99;
    }

    return false;
}

int spellbookQualityFromItemValue(const ItemDefinition &entry)
{
    if (entry.equipStat != "Book")
    {
        return 0;
    }

    if (entry.value > 3000)
    {
        return 4;
    }

    if (entry.value > 1000)
    {
        return 3;
    }

    if (entry.value > 400)
    {
        return 2;
    }

    return 1;
}

}

int guildSpellbookStockWeight(const HouseEntry &house, const ItemDefinition &item)
{
    const int tier = std::clamp(house.spellbookStockRule.quality, 0, 11);
    if (tier == 0 || house.vendorStockProfile != VendorStockProfile::None || item.itemId == 0
        || item.name.empty() || item.hasContentFlag("NoShop") || !schoolAllowsSpellbook(house, item))
    {
        return 0;
    }
    int weight = 0;
    for (int index = 0; index < std::min(tier, 6); ++index)
    {
        weight += item.randomTreasureWeights[size_t(index)];
    }
    if (weight > 0)
    {
        return weight;
    }
    const int quality = spellbookQualityFromItemValue(item);
    return quality > 0 && quality <= tier ? 1 : 0;
}
}
