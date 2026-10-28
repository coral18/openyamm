#include "game/maps/SaveGame.h"

#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>

#include <yaml-cpp/yaml.h>

using namespace OpenYAMM::Game;

namespace
{
YAML::Node discoveriesNode(const std::unordered_map<std::string, int32_t> &variables)
{
    YAML::Node result(YAML::NodeType::Map);
    for (const auto &[name, value] : variables)
        if (name.starts_with("openyamm.discovery.")) result[name] = value;
    return result;
}

YAML::Node explorationNode(const OutdoorWorldRuntime::Snapshot &world)
{
    YAML::Node result;
    for (const auto &[name, pCells] : {
        std::pair{"fully_revealed_cells", &world.fullyRevealedCells},
        std::pair{"partially_revealed_cells", &world.partiallyRevealedCells}})
    {
        YAML::Node values(YAML::NodeType::Sequence);
        for (uint8_t cell : *pCells) values.push_back(uint32_t(cell));
        result[name] = values;
    }
    result["discoveries"] = world.eventRuntimeState
        ? discoveriesNode(world.eventRuntimeState->namedMapVars) : YAML::Node(YAML::NodeType::Map);
    return result;
}

YAML::Node itemsNode(const std::vector<InventoryItem> &items)
{
    YAML::Node result(YAML::NodeType::Sequence);
    for (const InventoryItem &item : items)
    {
        YAML::Node entry;
        entry["item_id"] = item.objectDescriptionId;
        entry["quantity"] = item.quantity;
        entry["standard_enchant"] = item.standardEnchantId;
        entry["standard_power"] = item.standardEnchantPower;
        entry["special_enchant"] = item.specialEnchantId;
        result.push_back(entry);
    }
    return result;
}

YAML::Node chestNode(const GameplayChestViewState &view)
{
    YAML::Node result;
    result["id"] = view.chestId;
    result["flags"] = view.flags;
    result["grid_width"] = uint32_t(view.gridWidth);
    result["grid_height"] = uint32_t(view.gridHeight);
    for (const bool hidden : {false, true})
    {
        YAML::Node items(YAML::NodeType::Sequence);
        for (const GameplayChestItemState &item : hidden ? view.hiddenItems : view.items)
        {
            YAML::Node entry;
            entry["item_id"] = item.itemId;
            entry["quantity"] = item.quantity;
            entry["gold"] = item.goldAmount;
            entry["is_gold"] = item.isGold;
            entry["standard_enchant"] = item.item.standardEnchantId;
            entry["standard_power"] = item.item.standardEnchantPower;
            entry["special_enchant"] = item.item.specialEnchantId;
            entry["x"] = uint32_t(item.gridX);
            entry["y"] = uint32_t(item.gridY);
            items.push_back(entry);
        }
        result[hidden ? "hidden_items" : "items"] = items;
    }
    return result;
}

YAML::Node projectilesNode(const GameplayProjectileService::Snapshot &state)
{
    YAML::Node projectiles(YAML::NodeType::Sequence);
    for (const GameplayProjectileService::ProjectileState &projectile : state.projectiles)
    {
        YAML::Node entry;
        entry["source_kind"] = uint32_t(projectile.sourceKind);
        entry["source_id"] = projectile.sourceId;
        entry["spell_id"] = projectile.spellId;
        entry["damage"] = projectile.damage;
        entry["source"] = std::vector<float>{projectile.sourceX, projectile.sourceY, projectile.sourceZ};
        entry["position"] = std::vector<float>{projectile.x, projectile.y, projectile.z};
        entry["velocity"] = std::vector<float>{projectile.velocityX, projectile.velocityY, projectile.velocityZ};
        entry["sector_id"] = projectile.sectorId;
        entry["expired"] = projectile.isExpired;
        projectiles.push_back(entry);
    }
    return projectiles;
}

template<typename World>
YAML::Node worldNode(const World &world)
{
    YAML::Node result;
    result["game_minutes"] = world.gameMinutes;
    result["chest_seed"] = world.sessionChestSeed;
    YAML::Node chests(YAML::NodeType::Sequence);
    for (const std::optional<GameplayChestViewState> &view : world.materializedChestViews)
    {
        if (view)
        {
            chests.push_back(chestNode(*view));
        }
    }
    result["materialized_chests"] = chests;
    if (world.activeChestView)
    {
        result["active_chest"] = chestNode(*world.activeChestView);
    }
    return result;
}
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: openyamm_levelgen_inspect_save SAVE.oysav\n";
        return 2;
    }
    std::string error;
    const std::optional<GameSaveData> save = loadGameDataFromPath(argv[1], error);
    if (!save)
    {
        std::cerr << error << '\n';
        return 1;
    }
    YAML::Node result;
    result["map"] = save->mapFileName;
    result["game_minutes"] = save->savedGameMinutes;
    result["gold"] = save->party.gold;
    result["food"] = save->party.food;
    result["discoveries"] = discoveriesNode(save->namedGlobalVars);
    result["outdoor_states"] = YAML::Node(YAML::NodeType::Map);
    for (const auto &[name, world] : save->outdoorWorldStates)
        result["outdoor_states"][name] = explorationNode(world);
    result["house_stock_seed"] = save->party.houseStockSeed;
    result["held_item_active"] = save->heldInventoryItemActive;
    result["held_item_id"] = save->heldInventoryItem.objectDescriptionId;
    for (const Character &member : save->party.members)
    {
        YAML::Node entry;
        entry["name"] = member.name;
        entry["health"] = member.health;
        entry["inventory"] = itemsNode(member.inventory);
        result["party"].push_back(entry);
    }
    for (const Party::HouseStockState &stock : save->party.houseStockStates)
    {
        YAML::Node entry;
        entry["house_id"] = stock.houseId;
        entry["next_refresh_minutes"] = stock.nextRefreshGameMinutes;
        entry["standard"] = itemsNode(stock.standardStock);
        entry["special"] = itemsNode(stock.specialStock);
        result["house_stocks"].push_back(entry);
    }
    if (save->hasOutdoorRuntimeState)
    {
        result["outdoor"] = worldNode(save->outdoorWorld);
        result["outdoor"]["exploration"] = explorationNode(save->outdoorWorld);
        result["outdoor"]["projectile_count"] = save->outdoorWorld.projectiles.size();
        result["outdoor"]["actor_count"] = save->outdoorWorld.mapActors.size();
        result["outdoor"]["timer_count"] = save->outdoorWorld.timers.size();
        YAML::Node timers(YAML::NodeType::Sequence);
        for (const ScriptedEventTimerState &timer : save->outdoorWorld.timers)
        {
            YAML::Node entry;
            entry["event_id"] = timer.definition.eventId;
            entry["scope"] = timer.definition.scope == ScriptedEventScope::Global ? "global" : "map";
            entry["next_alarm_minutes"] = timer.nextAlarmGameMinutes;
            timers.push_back(entry);
        }
        result["outdoor"]["timers"] = timers;
        result["outdoor"]["world_item_count"] = save->outdoorWorld.worldItems.size();
        const OutdoorMoveState &movement = save->outdoorParty.movementState;
        result["outdoor"]["party_position"] = std::vector<float>{movement.x, movement.y, movement.footZ};
    }
    if (save->hasIndoorSceneState)
    {
        result["indoor"] = worldNode(save->indoorScene.worldRuntime);
        result["indoor"]["projectiles"] = projectilesNode(save->indoorScene.worldRuntime.projectileState);
        result["indoor"]["projectile_count"] = save->indoorScene.worldRuntime.projectileState.projectiles.size();
    }
    std::cout << result << '\n';
    return 0;
}
