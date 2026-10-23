#include "game/items/ItemInspectionText.h"

#include "game/items/ItemRuntime.h"
#include "game/tables/ItemTable.h"

namespace OpenYAMM::Game
{
namespace
{
bool tryParseInteger(const std::string &value, int &parsedValue)
{
    if (value.empty())
    {
        return false;
    }

    size_t parsedCharacters = 0;

    try
    {
        parsedValue = std::stoi(value, &parsedCharacters);
    }
    catch (...)
    {
        return false;
    }

    return parsedCharacters == value.size();
}

std::string formatItemInspectDamageText(const std::string &damageDice, int bonus)
{
    if (damageDice.empty() || damageDice == "0")
    {
        if (bonus <= 0)
        {
            return {};
        }

        return std::to_string(bonus);
    }

    if (bonus == 0)
    {
        return damageDice;
    }

    return damageDice + (bonus > 0 ? "+" : "") + std::to_string(bonus);
}

}

std::string itemInspectTypeText(const InventoryItem *pItemState, const ItemDefinition &itemDefinition)
{
    if (pItemState != nullptr && !pItemState->identified && ItemRuntime::requiresIdentification(itemDefinition))
    {
        return "Not identified";
    }

    if (!itemDefinition.unidentifiedName.empty()
        && itemDefinition.unidentifiedName != "0"
        && itemDefinition.unidentifiedName != "N / A")
    {
        return itemDefinition.unidentifiedName;
    }

    if (!itemDefinition.skillGroup.empty()
        && itemDefinition.skillGroup != "0"
        && itemDefinition.skillGroup != "Misc")
    {
        return itemDefinition.skillGroup;
    }

    if (!itemDefinition.equipStat.empty()
        && itemDefinition.equipStat != "0"
        && itemDefinition.equipStat != "N / A")
    {
        return itemDefinition.equipStat;
    }

    return "Misc";
}

std::string itemInspectDetailText(const InventoryItem *pItemState, const ItemDefinition &itemDefinition)
{
    if (pItemState != nullptr && !pItemState->identified && ItemRuntime::requiresIdentification(itemDefinition))
        return "Not identified";
    const bool isBroken = pItemState != nullptr && pItemState->broken;
    const std::string &equipStat = itemDefinition.equipStat;
    int mod1Value = 0;
    int mod2Value = 0;
    const bool hasMod1Int = tryParseInteger(itemDefinition.mod1, mod1Value);
    const bool hasMod2Int = tryParseInteger(itemDefinition.mod2, mod2Value);

    if (equipStat == "Bottle" && pItemState != nullptr && pItemState->standardEnchantPower > 0)
    {
        return "Power: " + std::to_string(pItemState->standardEnchantPower);
    }

    if (equipStat == "Reagent" && hasMod1Int && mod1Value > 0)
    {
        return "Power: " + std::to_string(mod1Value);
    }

    if (equipStat == "Weapon" || equipStat == "Weapon2" || equipStat == "Weapon1or2")
    {
        const int attackBonus = hasMod2Int ? mod2Value : 0;
        const std::string damageText = formatItemInspectDamageText(itemDefinition.mod1, attackBonus);

        if (damageText.empty())
        {
            return isBroken ? "Broken" : std::string {};
        }

        const std::string detail = "Attack: " + std::string(attackBonus >= 0 ? "+" : "") + std::to_string(attackBonus)
            + "   Damage: " + damageText;
        return isBroken ? "Broken   " + detail : detail;
    }

    if (equipStat == "Missile")
    {
        const int shootBonus = hasMod2Int ? mod2Value : 0;
        const std::string damageText = formatItemInspectDamageText(itemDefinition.mod1, shootBonus);

        if (damageText.empty())
        {
            return isBroken ? "Broken" : std::string {};
        }

        const std::string detail = "Shoot: " + std::string(shootBonus >= 0 ? "+" : "") + std::to_string(shootBonus)
            + "   Damage: " + damageText;
        return isBroken ? "Broken   " + detail : detail;
    }

    if (equipStat == "WeaponW")
    {
        const int currentCharges =
            pItemState != nullptr
                ? pItemState->currentCharges
                : (hasMod2Int ? mod2Value : 0);
        const int maxCharges =
            pItemState != nullptr && pItemState->maxCharges > 0
                ? pItemState->maxCharges
                : (hasMod2Int ? mod2Value : currentCharges);

        if (maxCharges > 0)
        {
            const std::string detail =
                "Charges: " + std::to_string(currentCharges) + "/" + std::to_string(maxCharges);
            return isBroken ? "Broken   " + detail : detail;
        }

        return isBroken ? "Broken" : std::string {};
    }

    if (equipStat == "Armor"
        || equipStat == "Shield"
        || equipStat == "Helm"
        || equipStat == "Belt"
        || equipStat == "Cloak"
        || equipStat == "Gauntlets"
        || equipStat == "Boots"
        || equipStat == "Ring"
        || equipStat == "Amulet")
    {
        const int64_t armorValue = int64_t(hasMod1Int ? mod1Value : 0) + (hasMod2Int ? mod2Value : 0);

        if (armorValue != 0)
        {
            const std::string detail = "Armor: " + std::string(armorValue > 0 ? "+" : "") + std::to_string(armorValue);
            return isBroken ? "Broken   " + detail : detail;
        }

        return isBroken ? "Broken" : std::string {};
    }

    return isBroken ? "Broken" : std::string {};
}

} // namespace OpenYAMM::Game
