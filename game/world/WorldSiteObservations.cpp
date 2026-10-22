#include "game/world/WorldSiteObservations.h"

#include "game/events/EventAttributeInfo.h"
#include "game/events/EventRuntime.h"
#include "game/party/Party.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <set>

namespace OpenYAMM::Game
{
namespace
{
constexpr std::string_view Root = "openyamm.discovery.site-effects.v1/";

std::string prefix(const std::string &siteId, uint16_t eventId)
{
    return std::string(Root) + siteId + "/event/" + std::to_string(eventId) + "/";
}

bool validSite(const std::string &siteId)
{
    return !siteId.empty() && siteId.size() <= 256 && siteId.find("/event/") == std::string::npos
        && siteId.find('\0') == std::string::npos;
}

WorldSiteObservation readObservation(const EventRuntimeState &state, const std::string &siteId, uint16_t eventId)
{
    WorldSiteObservation result;
    result.siteId = siteId;
    result.eventId = eventId;
    const std::string key = prefix(siteId, eventId);
    const auto field = [&](const std::string &name) -> std::optional<int32_t>
    {
        const auto value = state.namedMapVars.find(key + name);
        if (value == state.namedMapVars.end()) return std::nullopt;
        return value->second;
    };
    const auto time = field("time"), changed = field("changed"), seen = field("seen");
    if (!time || *time < 0 || !changed || (*changed != 0 && *changed != 1) || seen != 1)
        result.error = "Invalid recorded outcome data for this site.";
    else
    {
        result.gameMinutes = *time;
        result.lastChanged = *changed != 0;
    }
    const std::string stats = key + "stat/";
    std::set<std::string> symbols;
    for (const auto &[name, value] : state.namedMapVars)
    {
        if (!name.starts_with(stats)) continue;
        const size_t separator = name.find('/', stats.size());
        if (separator == std::string::npos)
            result.error = "Invalid recorded outcome attribute for this site.";
        else
            symbols.insert(name.substr(stats.size(), separator - stats.size()));
    }
    for (const std::string &symbol : symbols)
    {
        const EventAttributeInfo *pInfo = eventAttributeInfo(symbol);
        const std::string stat = "stat/" + symbol + "/";
        if (field(stat + "seen") != 1 || pInfo == nullptr || !pInfo->writable)
        {
            result.error = "Invalid recorded outcome attribute for this site.";
            continue;
        }
        const auto minBefore = field(stat + "min-before"), minAfter = field(stat + "min-after");
        const auto maxBefore = field(stat + "max-before"), maxAfter = field(stat + "max-after");
        if (!minBefore || !minAfter || !maxBefore || !maxAfter)
        {
            result.error = "Incomplete recorded outcome data for this site.";
            continue;
        }
        const WorldSiteEffectRange range{symbol, *minBefore, *minAfter, *maxBefore, *maxAfter};
        if (pInfo->kind == EventAttributeValueKind::Condition
            && (*minBefore < 0 || *minBefore > 1 || *minAfter < 0 || *minAfter > 1
                || *maxBefore < 0 || *maxBefore > 1 || *maxAfter < 0 || *maxAfter > 1))
            result.error = "Invalid recorded condition change for this site.";
        if (range.minDelta() == 0 || range.maxDelta() == 0 || range.minDelta() > range.maxDelta())
            result.error = "Invalid recorded outcome range for this site.";
        else
            result.effects.push_back(range);
    }
    std::sort(result.effects.begin(), result.effects.end(), [](const auto &a, const auto &b)
    {
        return a.attribute < b.attribute;
    });
    return result;
}
}

WorldSiteValueSnapshot captureWorldSiteValues(const EventRuntimeState &state, const Party &party)
{
    WorldSiteValueSnapshot result;
    for (const Character &member : party.members())
        result.members.push_back(member.name + "/" + std::to_string(member.rosterId) + "/"
            + std::to_string(member.characterDataId) + "/" + std::to_string(member.portraitPictureId));
    for (const EventAttributeInfo &attribute : eventAttributeInfos())
    {
        if (!attribute.writable) continue;
        const EventRuntime::VariableRef variable = EventRuntime::decodeVariable(uint32_t(attribute.variable));
        if (attribute.partyResource)
            result.values.push_back({std::string(attribute.symbol), std::nullopt,
                EventRuntime::getVariableValue(state, variable, &party, std::nullopt)});
        else
            for (size_t i = 0; i < party.memberCount(); ++i)
                result.values.push_back({std::string(attribute.symbol), i,
                    EventRuntime::getVariableValue(state, variable, &party, i)});
    }
    return result;
}

bool recordWorldSiteObservation(EventRuntimeState &state, const std::string &siteId, uint16_t eventId,
    const WorldSiteValueSnapshot &before, const WorldSiteValueSnapshot &after, float gameMinutes)
{
    const double time = std::floor(double(gameMinutes));
    if (!validSite(siteId) || eventId == 0 || !std::isfinite(time) || time < 0 || time > INT32_MAX) return false;
    const std::string key = prefix(siteId, eventId);
    WorldSiteObservation observation;
    if (std::any_of(state.namedMapVars.begin(), state.namedMapVars.end(), [&](const auto &field)
        { return field.first.starts_with(key); }))
    {
        observation = readObservation(state, siteId, eventId);
        if (!observation.error.empty()) return false;
    }
    bool changed = false;
    for (const WorldSiteValue &value : before.values)
    {
        const EventAttributeInfo *pInfo = eventAttributeInfo(value.attribute);
        if (pInfo == nullptr || !pInfo->writable || pInfo->partyResource != !value.member.has_value()) return false;
        if (value.member && *value.member >= before.members.size()) return false;
        if (value.member && before.members != after.members) continue;
        const auto next = std::find_if(after.values.begin(), after.values.end(), [&](const WorldSiteValue &candidate)
        { return candidate.attribute == value.attribute && candidate.member == value.member; });
        if (next == after.values.end()) return false;
        if (pInfo->kind == EventAttributeValueKind::Condition
            && (value.value < 0 || value.value > 1 || next->value < 0 || next->value > 1)) return false;
        if (value.value == next->value) continue;
        changed = true;
        const int64_t delta = int64_t(next->value) - value.value;
        auto range = std::find_if(observation.effects.begin(), observation.effects.end(), [&](const auto &effect)
        { return effect.attribute == value.attribute; });
        if (range == observation.effects.end())
            observation.effects.push_back({value.attribute, value.value, next->value, value.value, next->value});
        else
        {
            if (delta < range->minDelta())
            {
                range->minBefore = value.value;
                range->minAfter = next->value;
            }
            if (delta > range->maxDelta())
            {
                range->maxBefore = value.value;
                range->maxAfter = next->value;
            }
        }
    }
    state.namedMapVars[key + "seen"] = 1;
    state.namedMapVars[key + "time"] = int32_t(time);
    state.namedMapVars[key + "changed"] = changed ? 1 : 0;
    for (const WorldSiteEffectRange &range : observation.effects)
    {
        const std::string stat = key + "stat/" + range.attribute + "/";
        state.namedMapVars[stat + "seen"] = 1;
        state.namedMapVars[stat + "min-before"] = range.minBefore;
        state.namedMapVars[stat + "min-after"] = range.minAfter;
        state.namedMapVars[stat + "max-before"] = range.maxBefore;
        state.namedMapVars[stat + "max-after"] = range.maxAfter;
    }
    return true;
}

std::vector<WorldSiteObservation> worldSiteObservations(const EventRuntimeState &state)
{
    std::vector<WorldSiteObservation> result;
    std::set<std::pair<std::string, uint16_t>> identities;
    for (const auto &[key, value] : state.namedMapVars)
    {
        if (!key.starts_with(Root)) continue;
        const size_t separator = key.find("/event/", Root.size());
        if (separator == std::string::npos) continue;
        const size_t end = key.find('/', separator + 7);
        if (end == std::string::npos) continue;
        const std::string siteId = key.substr(Root.size(), separator - Root.size());
        const std::string number = key.substr(separator + 7, end - separator - 7);
        uint32_t eventId = 0;
        const auto parsed = std::from_chars(number.data(), number.data() + number.size(), eventId);
        if (parsed.ec != std::errc() || parsed.ptr != number.data() + number.size()) continue;
        if (!validSite(siteId) || eventId == 0 || eventId > UINT16_MAX || number != std::to_string(eventId)) continue;
        identities.emplace(siteId, uint16_t(eventId));
    }
    for (const auto &[siteId, eventId] : identities)
        result.push_back(readObservation(state, siteId, eventId));
    std::sort(result.begin(), result.end(), [](const auto &a, const auto &b)
    {
        return std::pair{a.siteId, a.eventId} < std::pair{b.siteId, b.eventId};
    });
    return result;
}

std::string describeWorldSiteObservation(const WorldSiteObservation &observation)
{
    if (!observation.error.empty()) return observation.error;
    std::string text = "Recorded use: day " + std::to_string(observation.gameMinutes / (24 * 60) + 1) + ".";
    if (!observation.lastChanged) text += " No additional changes recorded.";
    const auto amount = [](int64_t value)
    {
        return (value > 0 ? "+" : "") + std::to_string(value);
    };
    for (const bool party : {true, false})
    {
        bool heading = false;
        for (const WorldSiteEffectRange &range : observation.effects)
        {
            const EventAttributeInfo *pInfo = eventAttributeInfo(range.attribute);
            if (pInfo == nullptr || pInfo->partyResource != party) continue;
            if (!heading)
            {
                text += party ? "\nParty resources observed:" : "\nCharacter changes observed:";
                heading = true;
            }
            if (pInfo->kind == EventAttributeValueKind::Condition)
            {
                const std::string action = range.minDelta() < 0 && range.maxDelta() > 0
                    ? "Applied and removed: " : range.minDelta() < 0 ? "Removed: " : "Applied: ";
                text += "\n" + action + std::string(pInfo->label);
            }
            else
            {
                text += "\n" + std::string(pInfo->label) + " " + amount(range.minDelta());
                if (range.minDelta() != range.maxDelta())
                    text += " to " + amount(range.maxDelta()) + " (observed range)";
            }
        }
    }
    return text;
}
} // namespace OpenYAMM::Game
