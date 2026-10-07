#pragma once

#include "game/content/ContentManifest.h"
#include "game/tables/ClassMultiplierTable.h"

#include <optional>
#include <compare>
#include <string>
#include <vector>

namespace OpenYAMM::Game
{
struct ModVersion
{
    uint32_t major = 0, minor = 0, patch = 0;
    auto operator<=>(const ModVersion &) const = default;
    std::string text() const;
};

struct ModDependency
{
    std::string id;
    std::optional<ModVersion> minimumVersion;
};

struct ModClassPatch
{
    std::string classId;
    std::string path;
    ClassProgressionField field = ClassProgressionField::HealthPerLevel;
    ClassPatchOperation operation = ClassPatchOperation::AddNumber;
    int value = 0;
};

struct ModManifest
{
    std::string id;
    std::string name;
    ModVersion version;
    std::string source;
    std::vector<ModDependency> dependencies;
    std::vector<std::string> conflicts;
    std::vector<std::string> loadAfter;
    std::vector<ContentIdRangeDefinition> idRanges;
    std::vector<ModClassPatch> classPatches;
};

struct ModPatchProvenance
{
    std::string modId;
    std::string classId;
    std::string path;
    ClassPatchOperation operation = ClassPatchOperation::AddNumber;
    int before = 0, after = 0;
};

struct ModLoadPlan
{
    std::vector<ModManifest> mods;
    std::vector<ModPatchProvenance> provenance;
};

std::optional<ModManifest> parseModManifest(const std::string &text, const std::string &source, std::string &error);
// No profile means all installed mods. An explicit profile is an ordered selection, never auto-enabling dependencies.
bool resolveModPlan(const std::vector<ModManifest> &installed, const std::optional<std::string> &profile,
    const std::vector<WorldManifest> &worlds, ModLoadPlan &plan, std::string &error);
bool loadModPlan(const Engine::AssetFileSystem &assets, const std::vector<WorldManifest> &worlds,
    ModLoadPlan &plan, std::string &error);
// Transactional: a bad patch leaves both the table and plan's provenance unchanged.
bool applyModClassPatches(ModLoadPlan &plan, ClassMultiplierTable &table, std::string &error);
std::string modSavePackageId(const ModManifest &mod);
}
