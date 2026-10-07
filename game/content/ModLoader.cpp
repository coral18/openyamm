#include "game/content/ModLoader.h"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <charconv>
#include <limits>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <unordered_set>

namespace OpenYAMM::Game
{
namespace
{
void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

void fields(const YAML::Node &node, std::initializer_list<const char *> allowed)
{
    require(node.IsMap(), "Expected a mapping");
    std::set<std::string> keys;
    for (const auto &entry : node)
    {
        require(entry.first.IsScalar(), "Mapping keys must be strings");
        const std::string key = entry.first.as<std::string>();
        require(keys.insert(key).second, "Duplicate field: " + key);
        require(std::any_of(allowed.begin(), allowed.end(), [&](const char *pKey) { return key == pKey; }),
            "Unknown field: " + key);
    }
}

std::string scalar(const YAML::Node &node)
{
    require(node && node.IsScalar(), "Expected a scalar value");
    return node.as<std::string>();
}

bool validId(const std::string &id)
{
    if (id.empty() || id.front() < 'a' || id.front() > 'z' || id.back() == '.' || id.find('.') == std::string::npos)
    {
        return false;
    }
    char previous = 0;
    for (char c : id)
    {
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_')
            || (c == '.' && previous == '.'))
        {
            return false;
        }
        previous = c;
    }
    return true;
}

ModVersion version(const std::string &text)
{
    ModVersion result;
    uint32_t *parts[] = {&result.major, &result.minor, &result.patch};
    size_t begin = 0;
    for (size_t i = 0; i < 3; ++i)
    {
        const size_t end = text.find('.', begin);
        require((i < 2 && end != std::string::npos) || (i == 2 && end == std::string::npos),
            "Version must be MAJOR.MINOR.PATCH: " + text);
        const std::string part = text.substr(begin, end == std::string::npos ? end : end - begin);
        require(!part.empty() && (part.size() == 1 || part.front() != '0'), "Invalid version: " + text);
        const auto parsed = std::from_chars(part.data(), part.data() + part.size(), *parts[i]);
        require(parsed.ec == std::errc() && parsed.ptr == part.data() + part.size(), "Invalid version: " + text);
        begin = end == std::string::npos ? text.size() : end + 1;
    }
    return result;
}

std::vector<std::string> ids(const YAML::Node &node)
{
    if (!node)
    {
        return {};
    }
    require(node.IsSequence(), "Expected a sequence of mod IDs");
    std::vector<std::string> result;
    std::unordered_set<std::string> seen;
    for (const YAML::Node &entry : node)
    {
        const std::string id = scalar(entry);
        require(validId(id) && seen.insert(id).second, "Invalid or duplicate mod ID: " + id);
        result.push_back(id);
    }
    return result;
}

bool overlaps(const ContentIdRangeDefinition &a, const ContentIdRangeDefinition &b)
{
    return a.domain == b.domain && a.begin <= b.end && b.begin <= a.end;
}
}

std::string ModVersion::text() const
{
    return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
}

std::optional<ModManifest> parseModManifest(const std::string &text, const std::string &source, std::string &error)
{
    error.clear();
    try
    {
        const YAML::Node root = YAML::Load(text);
        fields(root, {"schema_version", "id", "version", "name", "dependencies", "conflicts", "load_after",
                      "qbits", "patches"});
        require(root["schema_version"] && root["schema_version"].as<int>() == 1, "Unsupported mod schema_version");
        ModManifest mod;
        mod.source = source;
        mod.id = scalar(root["id"]);
        require(validId(mod.id), "Mod ID must be a lowercase namespaced identifier: " + mod.id);
        mod.version = version(scalar(root["version"]));
        mod.name = root["name"] ? scalar(root["name"]) : mod.id;
        mod.conflicts = ids(root["conflicts"]);
        mod.loadAfter = ids(root["load_after"]);
        const YAML::Node dependencies = root["dependencies"];
        if (dependencies)
        {
            require(dependencies.IsSequence(), "dependencies must be a sequence");
            std::set<std::string> seen;
            for (const YAML::Node &node : dependencies)
            {
                fields(node, {"id", "min_version"});
                ModDependency dependency;
                dependency.id = scalar(node["id"]);
                require(validId(dependency.id) && seen.insert(dependency.id).second,
                    "Invalid or duplicate dependency: " + dependency.id);
                if (node["min_version"])
                {
                    dependency.minimumVersion = version(scalar(node["min_version"]));
                }
                mod.dependencies.push_back(dependency);
            }
        }
        const YAML::Node qbits = root["qbits"];
        if (qbits)
        {
            fields(qbits, {"begin", "end"});
            ContentIdRangeDefinition range{"qbits", qbits["begin"].as<uint32_t>(), qbits["end"].as<uint32_t>()};
            require(range.begin >= 10000 && range.begin <= range.end
                && range.end <= uint32_t(std::numeric_limits<int32_t>::max()),
                "QBit range must be ordered and start at 10000 or higher");
            mod.idRanges.push_back(range);
        }
        const YAML::Node patches = root["patches"];
        if (patches)
        {
            require(patches.IsSequence(), "patches must be a sequence");
            for (const YAML::Node &patch : patches)
            {
                fields(patch, {"target", "operations"});
                fields(patch["target"], {"type", "id"});
                require(scalar(patch["target"]["type"]) == "class", "v1 only supports typed class patches");
                std::string classId = scalar(patch["target"]["id"]);
                // Unqualified legacy aliases remain accepted at the existing table boundary.
                if (classId.starts_with("engine."))
                {
                    classId.erase(0, 7);
                }
                require(!classId.empty() && classId.find_first_of("./\\:") == std::string::npos,
                    "Unknown class namespace or invalid class ID");
                const YAML::Node operations = patch["operations"];
                require(operations && operations.IsSequence() && operations.size() > 0,
                    "A patch needs at least one operation");
                for (const YAML::Node &node : operations)
                {
                    fields(node, {"op", "path", "value"});
                    ModClassPatch operation;
                    operation.classId = classId;
                    operation.path = scalar(node["path"]);
                    require(operation.path == "progression.hp_per_level" || operation.path == "progression.base_hp",
                        "Unsupported typed class path: " + operation.path);
                    operation.field = operation.path == "progression.base_hp"
                        ? ClassProgressionField::BaseHealth : ClassProgressionField::HealthPerLevel;
                    const std::string op = scalar(node["op"]);
                    require(op == "add_number" || op == "set", "Unsupported patch operation: " + op);
                    operation.operation = op == "set" ? ClassPatchOperation::Set : ClassPatchOperation::AddNumber;
                    const std::string number = scalar(node["value"]);
                    const auto parsed = std::from_chars(number.data(), number.data() + number.size(), operation.value);
                    require(parsed.ec == std::errc() && parsed.ptr == number.data() + number.size(),
                        "Patch value must be an integer within the engine range");
                    mod.classPatches.push_back(operation);
                }
            }
        }
        return mod;
    }
    catch (const std::exception &exception)
    {
        error = source + ": " + exception.what();
        return std::nullopt;
    }
}

bool resolveModPlan(const std::vector<ModManifest> &installed, const std::optional<std::string> &profile,
    const std::vector<WorldManifest> &worlds, ModLoadPlan &plan, std::string &error)
{
    error.clear();
    try
    {
        std::map<std::string, const ModManifest *> available;
        for (const ModManifest &mod : installed)
        {
            require(available.emplace(mod.id, &mod).second, "Duplicate mod ID: " + mod.id);
        }
        std::vector<std::string> selected;
        if (profile)
        {
            const YAML::Node root = YAML::Load(*profile);
            fields(root, {"enabled"});
            require(bool(root["enabled"]), "profile.yaml must declare enabled");
            selected = ids(root["enabled"]);
        }
        else
        {
            for (const auto &[id, mod] : available)
            {
                (void)mod;
                selected.push_back(id);
            }
        }
        const std::set<std::string> active(selected.begin(), selected.end());
        for (const std::string &id : selected)
        {
            require(available.contains(id), "Profile enables missing mod: " + id);
        }
        std::map<std::string, std::set<std::string>> prerequisites;
        std::vector<std::pair<std::string, ContentIdRangeDefinition>> ranges;
        for (const WorldManifest &world : worlds)
        {
            if (world.qbits.declared)
            {
                ranges.push_back({world.id, {"qbits", world.qbits.begin, world.qbits.end}});
            }
            for (const ContentIdRangeDefinition &range : world.idRanges)
            {
                ranges.push_back({world.id, range});
            }
        }
        for (const std::string &id : selected)
        {
            require(available.contains(id), "Profile enables missing mod: " + id);
            const ModManifest &mod = *available.at(id);
            for (const ModDependency &dependency : mod.dependencies)
            {
                require(active.contains(dependency.id), "Mod " + id + " requires missing or disabled mod "
                    + dependency.id);
                require(!dependency.minimumVersion
                    || available.at(dependency.id)->version >= *dependency.minimumVersion,
                    "Mod " + id + " requires a newer version of " + dependency.id);
                prerequisites[id].insert(dependency.id);
            }
            for (const std::string &after : mod.loadAfter)
            {
                if (active.contains(after))
                {
                    prerequisites[id].insert(after);
                }
            }
            for (const std::string &conflict : mod.conflicts)
            {
                require(!active.contains(conflict), "Mod conflict: " + id + " and " + conflict);
            }
            for (const ContentIdRangeDefinition &range : mod.idRanges)
            {
                for (const auto &[owner, existing] : ranges)
                {
                    require(!overlaps(range, existing), "QBit collision: " + id + " and " + owner);
                }
                ranges.push_back({id, range});
            }
        }
        ModLoadPlan resolved;
        std::set<std::string> loaded;
        while (loaded.size() < selected.size())
        {
            bool progressed = false;
            for (const std::string &id : selected)
            {
                if (!loaded.contains(id) && std::all_of(prerequisites[id].begin(), prerequisites[id].end(),
                    [&](const std::string &dependency) { return loaded.contains(dependency); }))
                {
                    resolved.mods.push_back(*available.at(id));
                    loaded.insert(id);
                    progressed = true;
                    break; // Re-evaluate ready mods using the profile's priority order.
                }
            }
            require(progressed, "Mod dependency/load-order cycle");
        }
        plan = std::move(resolved);
        return true;
    }
    catch (const std::exception &exception)
    {
        error = exception.what();
        return false;
    }
}

bool loadModPlan(const Engine::AssetFileSystem &assets, const std::vector<WorldManifest> &worlds,
    ModLoadPlan &plan, std::string &error)
{
    std::vector<ModManifest> installed;
    for (const std::string &directory : assets.enumerate("mods"))
    {
        const std::string path = "mods/" + directory + "/mod.yaml";
        if (!assets.exists(path))
        {
            continue;
        }
        const auto source = assets.readTextFile(path);
        if (!source)
        {
            error = "Could not read mod manifest: " + path;
            return false;
        }
        const auto mod = parseModManifest(*source, path, error);
        if (!mod)
        {
            return false;
        }
        installed.push_back(*mod);
        std::cout << "Found mod " << mod->id << ' ' << mod->version.text() << '\n';
    }
    std::optional<std::string> profile;
    if (assets.exists("mods/profile.yaml"))
    {
        profile = assets.readTextFile("mods/profile.yaml");
        if (!profile)
        {
            error = "Could not read mods/profile.yaml";
            return false;
        }
    }
    return resolveModPlan(installed, profile, worlds, plan, error);
}

bool applyModClassPatches(ModLoadPlan &plan, ClassMultiplierTable &table, std::string &error)
{
    ClassMultiplierTable staged = table;
    std::vector<ModPatchProvenance> provenance;
    std::map<std::pair<std::string, ClassProgressionField>, std::pair<std::string, int>> assignments;
    for (const ModManifest &mod : plan.mods)
    {
        for (const ModClassPatch &patch : mod.classPatches)
        {
            const ClassMultiplierEntry *pClass = staged.get(patch.classId);
            if (pClass == nullptr)
            {
                error = mod.id + ": Unknown class: " + patch.classId;
                return false;
            }
            const auto key = std::make_pair(pClass->className, patch.field);
            if (patch.operation == ClassPatchOperation::Set)
            {
                const auto previous = assignments.find(key);
                if (previous != assignments.end() && previous->second.first != mod.id
                    && previous->second.second != patch.value)
                {
                    error = "Conflicting set patches from " + previous->second.first + " and " + mod.id
                        + " at " + pClass->className + "." + patch.path;
                    return false;
                }
                assignments[key] = {mod.id, patch.value};
            }
            ModPatchProvenance change{mod.id, pClass->className, patch.path, patch.operation};
            if (!staged.patchProgression(patch.classId, patch.field, patch.operation,
                patch.value, change.before, change.after, error))
            {
                error = mod.id + ": " + error;
                return false;
            }
            provenance.push_back(change);
        }
    }
    table = std::move(staged);
    plan.provenance = std::move(provenance);
    error.clear();
    return true;
}

std::string modSavePackageId(const ModManifest &mod)
{
    return "mods/" + mod.id + "@" + mod.version.text();
}
}
