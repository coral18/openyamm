#include "doctest/doctest.h"
#include "game/items/ItemInspectionText.h"
#include "game/party/Party.h"
#include "game/tables/ItemTable.h"

using namespace OpenYAMM::Game;

TEST_CASE("shared item inspection describes weapon definitions and preserves broken instance details")
{
    ItemDefinition item;
    item.equipStat = "Weapon";
    item.mod1 = "2d3";
    item.mod2 = "3";
    CHECK(itemInspectDetailText(nullptr, item) == "Attack: +3   Damage: 2d3+3");
    InventoryItem instance;
    instance.broken = true;
    CHECK(itemInspectDetailText(&instance, item) == "Broken   Attack: +3   Damage: 2d3+3");
    item.mod2 = "-2";
    CHECK(itemInspectDetailText(nullptr, item) == "Attack: -2   Damage: 2d3-2");
    item.equipStat = "Missile";
    CHECK(itemInspectDetailText(nullptr, item) == "Shoot: -2   Damage: 2d3-2");
    item.mod2 = "0";
    CHECK(itemInspectDetailText(nullptr, item) == "Shoot: +0   Damage: 2d3");
}

TEST_CASE("shared item inspection retains actual wand and potion state without identifying unknown items")
{
    ItemDefinition item;
    item.equipStat = "WeaponW";
    item.name = "Flame Wand";
    item.unidentifiedName = "Wand";
    item.identifyRepairDifficulty = 5;
    item.mod2 = "12";
    InventoryItem instance;
    instance.currentCharges = 3;
    instance.maxCharges = 10;
    CHECK(itemInspectDetailText(nullptr, item) == "Charges: 12/12");
    CHECK(itemInspectDetailText(&instance, item) == "Charges: 3/10");
    instance.identified = false;
    CHECK(itemInspectTypeText(&instance, item) == "Not identified");
    CHECK(itemInspectDetailText(&instance, item) == "Not identified");
    CHECK_FALSE(instance.identified);
    CHECK(instance.currentCharges == 3);
    instance.identified = true;
    item.equipStat = "Bottle";
    instance.standardEnchantPower = 37;
    CHECK(itemInspectDetailText(&instance, item) == "Power: 37");
    CHECK(itemInspectDetailText(nullptr, item).empty());
}

TEST_CASE("shared item inspection formats signed armor values with widened arithmetic")
{
    ItemDefinition item;
    item.equipStat = "Armor";
    item.mod1 = "4";
    item.mod2 = "3";
    CHECK(itemInspectDetailText(nullptr, item) == "Armor: +7");
    item.mod2 = "-6";
    CHECK(itemInspectDetailText(nullptr, item) == "Armor: -2");
    item.mod1 = item.mod2 = "2147483647";
    CHECK(itemInspectDetailText(nullptr, item) == "Armor: +4294967294");
    item.mod1 = item.mod2 = "not a number";
    CHECK(itemInspectDetailText(nullptr, item).empty());
}
