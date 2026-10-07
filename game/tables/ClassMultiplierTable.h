#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace OpenYAMM::Game
{
enum class ClassManaMode
{
    None,
    Intellect,
    Personality,
    Mixed,
    Level,
};

enum class ClassProgressionField { BaseHealth, HealthPerLevel };
enum class ClassPatchOperation { Set, AddNumber };

struct ClassMultiplierEntry
{
    uint32_t classId = 0;
    std::string className;
    std::string displayName;
    uint32_t classKind = 0;
    uint32_t promotionStep = 0;
    int baseHealth = 0;
    int healthPerLevel = 0;
    int baseMana = 0;
    int manaPerLevel = 0;
    ClassManaMode manaMode = ClassManaMode::None;
};

class ClassMultiplierTable
{
public:
    bool loadFromRows(const std::vector<std::vector<std::string>> &rows);
    bool applyClassExtraRows(const std::vector<std::vector<std::string>> &rows);
    const ClassMultiplierEntry *get(const std::string &className) const;
    const std::unordered_map<std::string, ClassMultiplierEntry> &entries() const;
    bool patchProgression(const std::string &className, ClassProgressionField field,
        ClassPatchOperation operation, int value, int &before, int &after, std::string &error);

private:
    std::unordered_map<std::string, ClassMultiplierEntry> m_entries;
};
}
