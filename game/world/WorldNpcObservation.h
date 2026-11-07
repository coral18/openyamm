#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace OpenYAMM::Game
{
// A remembered encounter, not a live tracker or an authored spawn guarantee.
struct WorldNpcActorLocation
{
    std::string mapId;
    uint32_t actorIndex = 0;
    int32_t x = 0, y = 0, z = 0;
    int32_t gameMinutes = 0;
    bool operator==(const WorldNpcActorLocation &) const = default;
};

// The generator's NPC slot is map-local and can identify somebody else on another map.
// Freeze perceived identity at the encounter rather than reading later presentation overrides.
struct WorldGeneratedNpcObservation
{
    std::string actorKey;
    uint32_t npcId = 0;
    std::string name;
    uint32_t pictureId = 0;
    uint32_t professionId = 0;
    WorldNpcActorLocation location;
    std::vector<uint32_t> teacherTopics;
    bool operator==(const WorldGeneratedNpcObservation &) const = default;
};
}
