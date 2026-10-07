#include "game/tables/ClassMultiplierTable.h"

#include "game/party/SkillData.h"
#include "game/StringUtils.h"

#include <cctype>
#include <cstdlib>
#include <utility>
#include <limits>

namespace OpenYAMM::Game
{
namespace
{
std::string trimCopy(const std::string &text)
{
    size_t begin = 0;

    while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])) != 0)
    {
        ++begin;
    }

    size_t end = text.size();

    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0)
    {
        --end;
    }

    return text.substr(begin, end - begin);
}

bool parseInteger(const std::string &text, int &value)
{
    const std::string trimmed = trimCopy(text);

    if (trimmed.empty())
    {
        return false;
    }

    char *pEnd = nullptr;
    const long parsed = std::strtol(trimmed.c_str(), &pEnd, 10);

    if (pEnd == trimmed.c_str() || *pEnd != '\0')
    {
        return false;
    }

    value = static_cast<int>(parsed);
    return true;
}

ClassManaMode parseManaMode(const std::string &text)
{
    const std::string mode = toLowerCopy(trimCopy(text));

    if (mode == "intellect" || mode == "intelligence")
    {
        return ClassManaMode::Intellect;
    }

    if (mode == "personality")
    {
        return ClassManaMode::Personality;
    }

    if (mode == "mixed")
    {
        return ClassManaMode::Mixed;
    }

    if (mode == "level")
    {
        return ClassManaMode::Level;
    }

    return ClassManaMode::None;
}
}

bool ClassMultiplierTable::loadFromRows(const std::vector<std::vector<std::string>> &rows)
{
    m_entries.clear();

    if (rows.size() < 2)
    {
        return false;
    }

    for (size_t rowIndex = 1; rowIndex < rows.size(); ++rowIndex)
    {
        const std::vector<std::string> &row = rows[rowIndex];

        if (row.size() < 6)
        {
            continue;
        }

        const std::string className = canonicalClassName(row[0]);

        if (className.empty())
        {
            continue;
        }

        ClassMultiplierEntry entry = {};
        entry.classId = static_cast<uint32_t>(rowIndex - 1);
        entry.className = className;
        entry.displayName = trimCopy(row[0]);
        entry.manaMode = parseManaMode(row[5]);

        if (!parseInteger(row[1], entry.baseHealth)
            || !parseInteger(row[2], entry.healthPerLevel)
            || !parseInteger(row[3], entry.baseMana)
            || !parseInteger(row[4], entry.manaPerLevel))
        {
            continue;
        }

        m_entries[className] = std::move(entry);
    }

    return !m_entries.empty();
}

bool ClassMultiplierTable::applyClassExtraRows(const std::vector<std::vector<std::string>> &rows)
{
    if (rows.size() < 2)
    {
        return false;
    }

    bool appliedAny = false;

    for (size_t rowIndex = 1; rowIndex < rows.size(); ++rowIndex)
    {
        const std::vector<std::string> &row = rows[rowIndex];

        if (row.size() < 4)
        {
            continue;
        }

        int classId = 0;
        int classKind = 0;
        int promotionStep = 0;

        if (!parseInteger(row[0], classId)
            || !parseInteger(row[1], classKind)
            || !parseInteger(row[2], promotionStep))
        {
            continue;
        }

        const std::string className = canonicalClassName(row[3]);

        if (className.empty())
        {
            continue;
        }

        ClassMultiplierEntry &entry = m_entries[className];
        entry.classId = static_cast<uint32_t>(std::max(0, classId));
        entry.className = className;
        entry.displayName = trimCopy(row[3]);
        entry.classKind = static_cast<uint32_t>(std::max(0, classKind));
        entry.promotionStep = static_cast<uint32_t>(std::max(0, promotionStep));
        appliedAny = true;
    }

    return appliedAny;
}

const ClassMultiplierEntry *ClassMultiplierTable::get(const std::string &className) const
{
    const std::unordered_map<std::string, ClassMultiplierEntry>::const_iterator it =
        m_entries.find(canonicalClassName(className));

    if (it == m_entries.end())
    {
        return nullptr;
    }

    return &it->second;
}

const std::unordered_map<std::string, ClassMultiplierEntry> &ClassMultiplierTable::entries() const
{
    return m_entries;
}

bool ClassMultiplierTable::patchProgression(const std::string &className, ClassProgressionField field,
    ClassPatchOperation operation, int value, int &before, int &after, std::string &error)
{
    error.clear();
    const auto entry = m_entries.find(canonicalClassName(className));
    if (entry == m_entries.end())
    {
        error = "Unknown class: " + className;
        return false;
    }
    int &property = field == ClassProgressionField::BaseHealth
        ? entry->second.baseHealth : entry->second.healthPerLevel;
    const int64_t result = operation == ClassPatchOperation::Set ? int64_t(value) : int64_t(property) + value;
    if (result < 0 || result > std::numeric_limits<int>::max())
    {
        error = "Class health progression must remain a non-negative integer within the engine range";
        return false;
    }
    before = property;
    after = int(result);
    property = after;
    return true;
}
}
