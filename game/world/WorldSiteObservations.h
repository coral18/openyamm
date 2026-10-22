#pragma once

#include <cstdint>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace OpenYAMM::Game
{
class Party;
struct EventRuntimeState;

struct WorldSiteValue
{
    std::string attribute;
    std::optional<size_t> member;
    int32_t value = 0;
};

struct WorldSiteValueSnapshot
{
    std::vector<std::string> members;
    std::vector<WorldSiteValue> values;
};

struct WorldSiteEffectRange
{
    std::string attribute;
    int32_t minBefore = 0, minAfter = 0;
    int32_t maxBefore = 0, maxAfter = 0;
    int64_t minDelta() const
    {
        return int64_t(minAfter) - minBefore;
    }
    int64_t maxDelta() const
    {
        return int64_t(maxAfter) - maxBefore;
    }
};

struct WorldSiteObservation
{
    std::string siteId;
    uint16_t eventId = 0;
    int32_t gameMinutes = 0;
    bool lastChanged = false;
    std::vector<WorldSiteEffectRange> effects;
    std::string error;
};

WorldSiteValueSnapshot captureWorldSiteValues(const EventRuntimeState &state, const Party &party);
// Synchronous before/after values only. Does not inspect possible branches or infer unobserved conditions.
// Numeric named map state uses stable attribute symbols, survives refill and requires no save-format change.
bool recordWorldSiteObservation(EventRuntimeState &state, const std::string &siteId, uint16_t eventId,
    const WorldSiteValueSnapshot &before, const WorldSiteValueSnapshot &after, float gameMinutes);
std::vector<WorldSiteObservation> worldSiteObservations(const EventRuntimeState &state);
std::string describeWorldSiteObservation(const WorldSiteObservation &observation);
} // namespace OpenYAMM::Game
