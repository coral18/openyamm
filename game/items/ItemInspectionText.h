#pragma once

#include <string>

namespace OpenYAMM::Game
{
struct InventoryItem;
struct ItemDefinition;

// Null state describes the base definition; instance state retains identification, damage and charges.
std::string itemInspectTypeText(const InventoryItem *pItemState, const ItemDefinition &itemDefinition);
std::string itemInspectDetailText(const InventoryItem *pItemState, const ItemDefinition &itemDefinition);
}
