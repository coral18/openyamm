#include "game/tables/JournalQuestTable.h"

#include "game/party/Party.h"
#include "game/tables/MapStats.h"
#include "game/tables/NpcDialogTable.h"
#include "game/tables/HouseTable.h"
#include "game/tables/ItemTable.h"
#include "game/maps/MapIdentity.h"
#include <charconv>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <set>

namespace OpenYAMM::Game
{
namespace
{
std::string getCell(const std::vector<std::string> &row, size_t index)
{
    return index < row.size() ? row[index] : "";
}

bool parseQBitList(
    const std::string &text,
    std::vector<uint32_t> &qbitIds,
    size_t rowIndex,
    const char *pColumnName)
{
    size_t tokenBegin = 0;

    while (tokenBegin < text.size())
    {
        while (tokenBegin < text.size()
            && (text[tokenBegin] == ' ' || text[tokenBegin] == ',' || text[tokenBegin] == ';'))
        {
            ++tokenBegin;
        }

        if (tokenBegin >= text.size())
        {
            break;
        }

        size_t tokenEnd = tokenBegin;
        while (tokenEnd < text.size() && text[tokenEnd] != ',' && text[tokenEnd] != ';')
        {
            ++tokenEnd;
        }

        size_t trimmedEnd = tokenEnd;
        while (trimmedEnd > tokenBegin && text[trimmedEnd - 1] == ' ')
        {
            --trimmedEnd;
        }

        const std::string token = text.substr(tokenBegin, trimmedEnd - tokenBegin);
        char *pEnd = nullptr;
        const unsigned long parsedValue = std::strtoul(token.c_str(), &pEnd, 10);

        if (token.empty()
            || pEnd == token.c_str()
            || *pEnd != '\0'
            || parsedValue == 0
            || parsedValue > std::numeric_limits<uint32_t>::max())
        {
            std::cerr << "Invalid " << pColumnName << " QBit at quests.txt row " << (rowIndex + 1)
                      << ": " << token << '\n';
            return false;
        }

        const uint32_t qbitId = static_cast<uint32_t>(parsedValue);
        if (std::find(qbitIds.begin(), qbitIds.end(), qbitId) == qbitIds.end())
        {
            qbitIds.push_back(qbitId);
        }

        tokenBegin = tokenEnd + 1;
    }

    return true;
}
}

bool JournalQuestTable::loadFromRows(const std::vector<std::vector<std::string>> &rows)
{
    m_entries.clear();
    std::set<uint32_t> seenQBitIds;
    bool hasValidationError = false;

    for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex)
    {
        const std::vector<std::string> &row = rows[rowIndex];

        if (row.empty() || row[0].empty() || row[0][0] == '#')
        {
            continue;
        }

        char *pEnd = nullptr;
        const unsigned long parsedId = std::strtoul(row[0].c_str(), &pEnd, 10);

        if (pEnd == row[0].c_str() || *pEnd != '\0')
        {
            if (row[0] != "Q Bit")
            {
                std::cerr << "Invalid quests.txt QBit id at row " << (rowIndex + 1) << ": " << row[0] << '\n';
                hasValidationError = true;
            }

            continue;
        }

        if (parsedId > std::numeric_limits<uint32_t>::max())
        {
            std::cerr << "Out-of-range quests.txt QBit id at row " << (rowIndex + 1) << ": " << row[0] << '\n';
            hasValidationError = true;
            continue;
        }

        const uint32_t qbitId = static_cast<uint32_t>(parsedId);

        if (!seenQBitIds.insert(qbitId).second)
        {
            std::cerr << "Duplicate quests.txt QBit id at row " << (rowIndex + 1) << ": " << qbitId << '\n';
            hasValidationError = true;
        }

        JournalQuestEntry entry = {};
        entry.qbitId = qbitId;
        entry.text = getCell(row, 1);
        entry.notes = getCell(row, 2);
        entry.owner = getCell(row, 3);
        entry.usesExplicitConditions = row.size() >= 6;

        if (entry.usesExplicitConditions
            && (!parseQBitList(getCell(row, 4), entry.requiredQBitIds, rowIndex, "required")
                || !parseQBitList(getCell(row, 5), entry.forbiddenQBitIds, rowIndex, "forbidden")))
        {
            hasValidationError = true;
            continue;
        }

        if (entry.text.empty())
        {
            continue;
        }

        m_entries.push_back(std::move(entry));
    }

    if (hasValidationError)
    {
        m_entries.clear();
        return false;
    }

    std::sort(
        m_entries.begin(),
        m_entries.end(),
        [](const JournalQuestEntry &left, const JournalQuestEntry &right)
        {
            return left.qbitId < right.qbitId;
        });

    return !m_entries.empty();
}

bool JournalQuestTable::loadLocationRows(const std::vector<std::vector<std::string>> &rows)
{
    // Transactional: malformed mod metadata cannot leave partially attached relationships.
    std::vector<JournalQuestEntry> entries = m_entries;
    for (JournalQuestEntry &entry : entries)
    {
        entry.worldId.clear();
        entry.locations.clear();
    }
    std::set<std::string> seen;
    for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex)
    {
        const auto &row = rows[rowIndex];
        if (row.empty() || row[0].empty() || row[0][0] == '#' || row[0] == "QBit")
        {
            continue;
        }
        const auto fail = [&]()
        {
            std::cerr << "Invalid quest_locations.txt relationship at row " << rowIndex + 1 << '\n';
            return false;
        };
        uint32_t id = 0;
        const auto parsed = std::from_chars(row[0].data(), row[0].data() + row[0].size(), id);
        if (row.size() != 6 || parsed.ec != std::errc() || parsed.ptr != row[0].data() + row[0].size()
            || row[0] != std::to_string(id) || row[1] != normalizeWorldId(row[1]))
        {
            return fail();
        }
        const auto found = std::find_if(entries.begin(), entries.end(),
            [id](const JournalQuestEntry &entry) { return entry.qbitId == id; });
        if (found == entries.end() || row[1].empty() || row[4].empty()
            || (!found->worldId.empty() && found->worldId != row[1]))
        {
            return fail();
        }
        JournalQuestLocation location;
        if (row[2] == "giver") location.role = JournalQuestLocation::Role::Giver;
        else if (row[2] == "objective") location.role = JournalQuestLocation::Role::Objective;
        else if (row[2] == "return") location.role = JournalQuestLocation::Role::Return;
        else return fail();
        if (row[3] == "map") location.kind = JournalQuestLocation::Kind::Map;
        else if (row[3] == "npc") location.kind = JournalQuestLocation::Kind::Npc;
        else if (row[3] == "house") location.kind = JournalQuestLocation::Kind::House;
        else if (row[3] == "item") location.kind = JournalQuestLocation::Kind::Item;
        else return fail();
        if (row[5] != "journal" && row[5] != "guide") return fail();
        location.disclosed = row[5] == "journal";
        location.target = row[4];
        if (location.kind != JournalQuestLocation::Kind::Map)
        {
            uint32_t targetId = 0;
            const auto target = std::from_chars(row[4].data(), row[4].data() + row[4].size(), targetId);
            if (target.ec != std::errc() || target.ptr != row[4].data() + row[4].size()
                || targetId == 0 || row[4] != std::to_string(targetId))
            {
                return fail();
            }
        }
        const std::string key = row[0] + "/" + row[2] + "/" + row[3] + "/" + row[4];
        if (!seen.insert(key).second) return fail();
        found->worldId = row[1];
        found->locations.push_back(std::move(location));
    }
    m_entries = std::move(entries);
    return true;
}

bool JournalQuestTable::validateLocations(const MapStats &maps, const NpcDialogTable &npcs,
    const HouseTable &houses, const ItemTable &items) const
{
    for (const JournalQuestEntry &entry : m_entries)
    {
        if (!entry.locations.empty() && std::none_of(maps.getEntries().begin(), maps.getEntries().end(),
            [&](const MapStatsEntry &map) { return map.worldId == entry.worldId; }))
        {
            std::cerr << "Unknown quest world: " << entry.worldId << '\n';
            return false;
        }
        for (const JournalQuestLocation &location : entry.locations)
        {
            bool valid = false;
            const auto kind = location.kind;
            if (kind == JournalQuestLocation::Kind::Map)
            {
                valid = std::any_of(maps.getEntries().begin(), maps.getEntries().end(), [&](const MapStatsEntry &map)
                {
                    const std::string id = map.canonicalId.empty()
                        ? buildCanonicalMapId(map.worldId, map.fileName) : map.canonicalId;
                    return map.worldId == entry.worldId && id == location.target;
                });
            }
            else
            {
                const uint32_t id = uint32_t(std::stoul(location.target));
                if (kind == JournalQuestLocation::Kind::Npc) valid = npcs.getNpc(id) != nullptr;
                if (kind == JournalQuestLocation::Kind::Item)
                {
                    const ItemDefinition *pItem = items.get(id);
                    valid = pItem != nullptr && !pItem->name.empty();
                }
                if (kind == JournalQuestLocation::Kind::House)
                {
                    const HouseEntry *pHouse = houses.get(id);
                    const MapStatsEntry *pMap = pHouse != nullptr ? maps.findById(pHouse->mapId) : nullptr;
                    valid = pMap != nullptr && pMap->worldId == entry.worldId && pHouse->type != "Dungeon Ent";
                }
            }
            if (!valid)
            {
                std::cerr << "Unknown quest location reference for QBit " << entry.qbitId
                          << ": " << location.target << '\n';
                return false;
            }
        }
    }
    return true;
}

const std::vector<JournalQuestEntry> &JournalQuestTable::entries() const
{
    return m_entries;
}

bool JournalQuestTable::hasQuestText(uint32_t qbitId) const
{
    const std::vector<JournalQuestEntry>::const_iterator iterator = std::lower_bound(
        m_entries.begin(),
        m_entries.end(),
        qbitId,
        [](const JournalQuestEntry &entry, uint32_t value)
        {
            return entry.qbitId < value;
        });

    return iterator != m_entries.end() && iterator->qbitId == qbitId && !iterator->text.empty();
}

bool JournalQuestTable::isVisible(const JournalQuestEntry &entry, const Party &party) const
{
    return isVisible(entry, [&party](uint32_t id) { return party.hasQuestBit(id); });
}

bool JournalQuestTable::isVisible(const JournalQuestEntry &entry, const QBitReader &hasQBit) const
{
    if (!entry.usesExplicitConditions)
    {
        return hasQBit(entry.qbitId);
    }
    return std::all_of(entry.requiredQBitIds.begin(), entry.requiredQBitIds.end(), hasQBit)
        && std::none_of(entry.forbiddenQBitIds.begin(), entry.forbiddenQBitIds.end(), hasQBit);
}

std::string JournalQuestTable::discoveryKey(uint32_t qbitId)
{
    return "openyamm.discovery.quest/" + std::to_string(qbitId);
}

std::vector<uint32_t> JournalQuestTable::questsKnownAroundChange(uint32_t qbitId, bool wasSet, bool isSet,
    const QBitReader &hasQBit) const
{
    std::vector<uint32_t> result;
    if (wasSet == isSet)
    {
        return result;
    }
    for (const JournalQuestEntry &entry : m_entries)
    {
        const bool affected = entry.usesExplicitConditions
            ? std::find(entry.requiredQBitIds.begin(), entry.requiredQBitIds.end(), qbitId)
                    != entry.requiredQBitIds.end()
                || std::find(entry.forbiddenQBitIds.begin(), entry.forbiddenQBitIds.end(), qbitId)
                    != entry.forbiddenQBitIds.end()
            : entry.qbitId == qbitId;
        if (affected && (isVisible(entry, hasQBit)
            || isVisible(entry, [&](uint32_t id) { return id == qbitId ? wasSet : hasQBit(id); })))
        {
            result.push_back(entry.qbitId);
        }
    }
    return result;
}

bool JournalQuestTable::qbitChangeRevealsQuest(
    uint32_t qbitId,
    bool wasSet,
    bool isSet,
    const Party &party) const
{
    if (wasSet == isSet)
    {
        return false;
    }

    for (const JournalQuestEntry &entry : m_entries)
    {
        if (!entry.usesExplicitConditions)
        {
            if (!wasSet && isSet && entry.qbitId == qbitId)
            {
                return true;
            }
            continue;
        }

        const std::vector<uint32_t> &revealingConditions = isSet
            ? entry.requiredQBitIds
            : entry.forbiddenQBitIds;
        if (std::find(revealingConditions.begin(), revealingConditions.end(), qbitId)
                != revealingConditions.end()
            && isVisible(entry, party))
        {
            return true;
        }
    }

    return false;
}
}
