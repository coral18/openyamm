#include "game/events/ScriptedEventProgram.h"

#include "engine/scripting/LuaStateOwner.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <set>
#include <string_view>

extern "C"
{
#include <lauxlib.h>
}

namespace OpenYAMM::Game
{
namespace
{
constexpr char LuaScopeMap[] = "map";
constexpr char LuaScopeGlobal[] = "global";
constexpr char LuaScopeCanShowTopic[] = "CanShowTopic";

std::atomic<uint64_t> NextScriptedEventProgramCacheId{1};

const char *luaScopeName(ScriptedEventScope scope)
{
    return scope == ScriptedEventScope::Global ? LuaScopeGlobal : LuaScopeMap;
}

void registerMetadataBindings(lua_State *pLuaState)
{
    lua_newtable(pLuaState);

    lua_newtable(pLuaState);
    lua_setfield(pLuaState, -2, LuaScopeGlobal);

    lua_newtable(pLuaState);
    lua_setfield(pLuaState, -2, LuaScopeMap);

    lua_newtable(pLuaState);
    lua_setfield(pLuaState, -2, LuaScopeCanShowTopic);

    lua_newtable(pLuaState);
    lua_newtable(pLuaState);
    lua_setfield(pLuaState, -2, LuaScopeGlobal);
    lua_newtable(pLuaState);
    lua_setfield(pLuaState, -2, LuaScopeMap);
    lua_setfield(pLuaState, -2, "meta");

    lua_setglobal(pLuaState, "evt");
}

bool pushEvtSubtable(lua_State *pLuaState, const char *pFieldName)
{
    lua_getglobal(pLuaState, "evt");

    if (!lua_istable(pLuaState, -1))
    {
        lua_pop(pLuaState, 1);
        return false;
    }

    lua_getfield(pLuaState, -1, pFieldName);

    if (!lua_istable(pLuaState, -1))
    {
        lua_pop(pLuaState, 2);
        return false;
    }

    return true;
}

bool pushEvtMetaScope(lua_State *pLuaState, ScriptedEventScope scope)
{
    lua_getglobal(pLuaState, "evt");

    if (!lua_istable(pLuaState, -1))
    {
        lua_pop(pLuaState, 1);
        return false;
    }

    lua_getfield(pLuaState, -1, "meta");

    if (!lua_istable(pLuaState, -1))
    {
        lua_pop(pLuaState, 2);
        return false;
    }

    lua_getfield(pLuaState, -1, luaScopeName(scope));

    if (!lua_istable(pLuaState, -1))
    {
        lua_pop(pLuaState, 3);
        return false;
    }

    return true;
}

template <typename TValue>
void sortAndUnique(std::vector<TValue> &values)
{
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
}

std::vector<uint16_t> readHandlerIds(lua_State *pLuaState, const char *pTableName)
{
    std::vector<uint16_t> eventIds;

    if (!pushEvtSubtable(pLuaState, pTableName))
    {
        return eventIds;
    }

    lua_pushnil(pLuaState);

    while (lua_next(pLuaState, -2) != 0)
    {
        if (lua_isinteger(pLuaState, -2) && lua_isfunction(pLuaState, -1))
        {
            eventIds.push_back(static_cast<uint16_t>(lua_tointeger(pLuaState, -2)));
        }

        lua_pop(pLuaState, 1);
    }

    lua_pop(pLuaState, 2);
    sortAndUnique(eventIds);
    return eventIds;
}

template <typename TValue>
std::vector<TValue> readIntegerArrayFromField(
    lua_State *pLuaState,
    int tableIndex,
    const char *pFieldName,
    bool preserveOrder = false)
{
    std::vector<TValue> values;
    lua_getfield(pLuaState, tableIndex, pFieldName);

    if (lua_istable(pLuaState, -1))
    {
        const size_t entryCount = lua_rawlen(pLuaState, -1);
        values.reserve(entryCount);

        for (size_t index = 1; index <= entryCount; ++index)
        {
            lua_geti(pLuaState, -1, static_cast<lua_Integer>(index));

            if (lua_isinteger(pLuaState, -1))
            {
                values.push_back(static_cast<TValue>(lua_tointeger(pLuaState, -1)));
            }

            lua_pop(pLuaState, 1);
        }
    }

    lua_pop(pLuaState, 1);
    if (!preserveOrder)
    {
        sortAndUnique(values);
    }
    return values;
}

std::vector<std::string> readStringArrayFromField(lua_State *pLuaState, int tableIndex, const char *pFieldName)
{
    std::vector<std::string> values;
    lua_getfield(pLuaState, tableIndex, pFieldName);

    if (lua_istable(pLuaState, -1))
    {
        const size_t entryCount = lua_rawlen(pLuaState, -1);
        values.reserve(entryCount);

        for (size_t index = 1; index <= entryCount; ++index)
        {
            lua_geti(pLuaState, -1, static_cast<lua_Integer>(index));

            if (lua_isstring(pLuaState, -1))
            {
                values.emplace_back(lua_tostring(pLuaState, -1));
            }

            lua_pop(pLuaState, 1);
        }
    }

    lua_pop(pLuaState, 1);
    sortAndUnique(values);
    return values;
}

std::unordered_map<uint16_t, std::string> readStringMapFromField(lua_State *pLuaState, int tableIndex, const char *pFieldName)
{
    std::unordered_map<uint16_t, std::string> values;
    lua_getfield(pLuaState, tableIndex, pFieldName);

    if (lua_istable(pLuaState, -1))
    {
        lua_pushnil(pLuaState);

        while (lua_next(pLuaState, -2) != 0)
        {
            if (lua_isinteger(pLuaState, -2) && lua_isstring(pLuaState, -1))
            {
                values.emplace(
                    static_cast<uint16_t>(lua_tointeger(pLuaState, -2)),
                    std::string(lua_tostring(pLuaState, -1)));
            }

            lua_pop(pLuaState, 1);
        }
    }

    lua_pop(pLuaState, 1);
    return values;
}

std::unordered_map<uint16_t, std::vector<uint32_t>> readOpenedChestIds(lua_State *pLuaState, int tableIndex)
{
    std::unordered_map<uint16_t, std::vector<uint32_t>> values;
    lua_getfield(pLuaState, tableIndex, "openedChestIds");

    if (lua_istable(pLuaState, -1))
    {
        lua_pushnil(pLuaState);

        while (lua_next(pLuaState, -2) != 0)
        {
            if (lua_isinteger(pLuaState, -2) && lua_istable(pLuaState, -1))
            {
                const uint16_t eventId = static_cast<uint16_t>(lua_tointeger(pLuaState, -2));
                std::vector<uint32_t> chestIds;
                const size_t chestCount = lua_rawlen(pLuaState, -1);
                chestIds.reserve(chestCount);

                for (size_t index = 1; index <= chestCount; ++index)
                {
                    lua_geti(pLuaState, -1, static_cast<lua_Integer>(index));

                    if (lua_isinteger(pLuaState, -1))
                    {
                        chestIds.push_back(static_cast<uint32_t>(lua_tointeger(pLuaState, -1)));
                    }

                    lua_pop(pLuaState, 1);
                }

                sortAndUnique(chestIds);
                values.emplace(eventId, std::move(chestIds));
            }

            lua_pop(pLuaState, 1);
        }
    }

    lua_pop(pLuaState, 1);
    return values;
}

std::optional<std::string> readOptionalStringField(lua_State *pLuaState, int tableIndex, const char *pFieldName)
{
    const int absoluteTableIndex = lua_absindex(pLuaState, tableIndex);
    lua_getfield(pLuaState, absoluteTableIndex, pFieldName);

    std::optional<std::string> value;

    if (lua_isstring(pLuaState, -1))
    {
        value = std::string(lua_tostring(pLuaState, -1));
    }

    lua_pop(pLuaState, 1);
    return value;
}

std::optional<uint32_t> readOptionalUnsignedField(lua_State *pLuaState, int tableIndex, const char *pFieldName)
{
    const int absoluteTableIndex = lua_absindex(pLuaState, tableIndex);
    lua_getfield(pLuaState, absoluteTableIndex, pFieldName);

    std::optional<uint32_t> value;

    if (lua_isinteger(pLuaState, -1))
    {
        const lua_Integer rawValue = lua_tointeger(pLuaState, -1);

        if (rawValue >= 0)
        {
            value = static_cast<uint32_t>(rawValue);
        }
    }

    lua_pop(pLuaState, 1);
    return value;
}

std::optional<double> readOptionalNumberField(lua_State *pLuaState, int tableIndex, const char *pFieldName)
{
    const int absoluteTableIndex = lua_absindex(pLuaState, tableIndex);
    lua_getfield(pLuaState, absoluteTableIndex, pFieldName);

    std::optional<double> value;

    if (lua_isnumber(pLuaState, -1))
    {
        value = static_cast<double>(lua_tonumber(pLuaState, -1));
    }

    lua_pop(pLuaState, 1);
    return value;
}

bool readBooleanField(lua_State *pLuaState, int tableIndex, const char *pFieldName)
{
    const int absoluteTableIndex = lua_absindex(pLuaState, tableIndex);
    lua_getfield(pLuaState, absoluteTableIndex, pFieldName);
    const bool value = lua_toboolean(pLuaState, -1) != 0;
    lua_pop(pLuaState, 1);
    return value;
}

std::optional<ScriptedEventProgram::ContextActionMetadata> readContextActionMetadata(
    lua_State *pLuaState,
    int tableIndex)
{
    if (!lua_istable(pLuaState, tableIndex))
    {
        return std::nullopt;
    }

    ScriptedEventProgram::ContextActionMetadata metadata = {};
    const int absoluteTableIndex = lua_absindex(pLuaState, tableIndex);

    metadata.kind = readOptionalStringField(pLuaState, absoluteTableIndex, "kind").value_or("");
    metadata.source = readOptionalStringField(pLuaState, absoluteTableIndex, "source").value_or("");
    metadata.houseId = readOptionalUnsignedField(pLuaState, absoluteTableIndex, "houseId");
    metadata.targetMap = readOptionalStringField(pLuaState, absoluteTableIndex, "targetMap");
    metadata.targetName = readOptionalStringField(pLuaState, absoluteTableIndex, "targetName");
    metadata.chestIds = readIntegerArrayFromField<uint32_t>(pLuaState, absoluteTableIndex, "chestIds");

    lua_getfield(pLuaState, absoluteTableIndex, "hidden");
    metadata.hidden = lua_toboolean(pLuaState, -1) != 0;
    lua_pop(pLuaState, 1);

    if (metadata.kind.empty())
    {
        return std::nullopt;
    }

    return metadata;
}

std::unordered_map<uint16_t, ScriptedEventProgram::ContextActionMetadata> readContextActionMetadataByEventId(
    lua_State *pLuaState,
    int tableIndex)
{
    std::unordered_map<uint16_t, ScriptedEventProgram::ContextActionMetadata> values;
    lua_getfield(pLuaState, tableIndex, "contextActions");

    if (lua_istable(pLuaState, -1))
    {
        lua_pushnil(pLuaState);

        while (lua_next(pLuaState, -2) != 0)
        {
            if (lua_isinteger(pLuaState, -2) && lua_istable(pLuaState, -1))
            {
                const std::optional<ScriptedEventProgram::ContextActionMetadata> metadata =
                    readContextActionMetadata(pLuaState, -1);

                if (metadata)
                {
                    values.emplace(static_cast<uint16_t>(lua_tointeger(pLuaState, -2)), *metadata);
                }
            }

            lua_pop(pLuaState, 1);
        }
    }

    lua_pop(pLuaState, 1);
    return values;
}

ScriptedEventTimerScheduleKind timerScheduleKindFromString(const std::string &value)
{
    if (value == "interval")
    {
        return ScriptedEventTimerScheduleKind::Interval;
    }

    if (value == "daily")
    {
        return ScriptedEventTimerScheduleKind::Daily;
    }

    if (value == "weekly")
    {
        return ScriptedEventTimerScheduleKind::Weekly;
    }

    if (value == "monthly")
    {
        return ScriptedEventTimerScheduleKind::Monthly;
    }

    if (value == "yearly")
    {
        return ScriptedEventTimerScheduleKind::Yearly;
    }

    return ScriptedEventTimerScheduleKind::Relative;
}

std::vector<ScriptedEventProgram::TimerTrigger> readTimerTriggers(
    lua_State *pLuaState,
    int tableIndex,
    ScriptedEventScope scope)
{
    std::vector<ScriptedEventProgram::TimerTrigger> timers;
    std::unordered_map<uint16_t, uint32_t> nativeRegistrationCounts;
    lua_getfield(pLuaState, tableIndex, "timers");

    if (lua_istable(pLuaState, -1))
    {
        const size_t timerCount = lua_rawlen(pLuaState, -1);
        timers.reserve(timerCount);

        for (size_t index = 1; index <= timerCount; ++index)
        {
            lua_geti(pLuaState, -1, static_cast<lua_Integer>(index));

            if (lua_istable(pLuaState, -1))
            {
                ScriptedEventProgram::TimerTrigger timer = {};
                const int timerTableIndex = lua_absindex(pLuaState, -1);
                timer.scope = scope;

                if (const std::optional<uint32_t> eventId =
                        readOptionalUnsignedField(pLuaState, timerTableIndex, "eventId"))
                {
                    timer.eventId = static_cast<uint16_t>(*eventId);
                }

                if (const std::optional<uint32_t> sourceEventId =
                        readOptionalUnsignedField(pLuaState, timerTableIndex, "sourceEventId"))
                {
                    timer.sourceEventId = static_cast<uint16_t>(*sourceEventId);
                }
                else
                {
                    timer.sourceEventId = timer.eventId;
                }

                const std::string origin =
                    readOptionalStringField(pLuaState, timerTableIndex, "origin").value_or("native");
                timer.origin = origin == "legacy"
                    ? ScriptedEventTimerOrigin::Legacy
                    : ScriptedEventTimerOrigin::Native;

                const std::string scheduleKind =
                    readOptionalStringField(pLuaState, timerTableIndex, "scheduleKind").value_or("relative");
                timer.scheduleKind = timerScheduleKindFromString(scheduleKind);

                if (timer.origin == ScriptedEventTimerOrigin::Legacy)
                {
                    if (timer.scheduleKind == ScriptedEventTimerScheduleKind::Relative)
                    {
                        timer.scheduleKind = ScriptedEventTimerScheduleKind::Daily;
                    }

                    const std::string triggerKind =
                        readOptionalStringField(pLuaState, timerTableIndex, "triggerKind").value_or("timer");
                    timer.triggerKind = triggerKind == "long"
                        ? ScriptedEventTimerTriggerKind::LongTimer
                        : ScriptedEventTimerTriggerKind::Timer;
                    timer.repeating = true;

                    if (const std::optional<uint32_t> triggerStep =
                            readOptionalUnsignedField(pLuaState, timerTableIndex, "triggerStep"))
                    {
                        timer.triggerStep = static_cast<uint8_t>(*triggerStep);
                    }

                    if (const std::optional<uint32_t> intervalHalfMinutes =
                            readOptionalUnsignedField(pLuaState, timerTableIndex, "intervalHalfMinutes"))
                    {
                        timer.intervalHalfMinutes = static_cast<uint16_t>(*intervalHalfMinutes);
                    }

                    timer.startHour = static_cast<int>(
                        readOptionalUnsignedField(pLuaState, timerTableIndex, "startHour").value_or(0));
                    timer.startMinute = static_cast<int>(
                        readOptionalUnsignedField(pLuaState, timerTableIndex, "startMinute").value_or(0));
                    timer.startSecond = static_cast<int>(
                        readOptionalUnsignedField(pLuaState, timerTableIndex, "startSecond").value_or(0));
                }
                else
                {
                    timer.registrationIndex = ++nativeRegistrationCounts[timer.sourceEventId];
                    timer.repeating = readBooleanField(pLuaState, timerTableIndex, "repeating");
                    timer.intervalGameMinutes =
                        readOptionalNumberField(pLuaState, timerTableIndex, "intervalGameMinutes").value_or(0.0);
                    timer.initialDelayGameMinutes =
                        readOptionalNumberField(pLuaState, timerTableIndex, "initialDelayGameMinutes")
                            .value_or(
                                readOptionalNumberField(
                                    pLuaState,
                                    timerTableIndex,
                                    "remainingGameMinutes").value_or(timer.intervalGameMinutes));
                }

                if (timer.eventId != 0)
                {
                    timers.push_back(std::move(timer));
                }
            }

            lua_pop(pLuaState, 1);
        }
    }

    lua_pop(pLuaState, 1);
    return timers;
}

}

namespace
{
bool readMonsterSpawns(lua_State *pLuaState, int metadataIndex, ScriptedEventScope scope,
    std::unordered_map<uint16_t, std::vector<ScriptedEventProgram::MonsterSpawn>> &result, std::string &error)
{
    const int stackTop = lua_gettop(pLuaState);
    const auto fail = [&]()
    {
        error = "Invalid monsterSpawns metadata: expected callback-owned arrays of SummonMonsters arguments";
        lua_settop(pLuaState, stackTop);
        return false;
    };
    lua_getfield(pLuaState, metadataIndex, "monsterSpawns");
    if (lua_isnil(pLuaState, -1))
    {
        lua_pop(pLuaState, 1);
        return true;
    }
    if (scope != ScriptedEventScope::Map || !lua_istable(pLuaState, -1)) return fail();
    lua_pushnil(pLuaState);
    while (lua_next(pLuaState, -2) != 0)
    {
        if (!lua_isinteger(pLuaState, -2) || lua_tointeger(pLuaState, -2) <= 0
            || lua_tointeger(pLuaState, -2) > UINT16_MAX || !lua_istable(pLuaState, -1)) return fail();
        const uint16_t eventId = uint16_t(lua_tointeger(pLuaState, -2));
        lua_getfield(pLuaState, -1, "handler");
        if (!lua_isfunction(pLuaState, -1)) return fail();
        lua_getglobal(pLuaState, "evt");
        lua_getfield(pLuaState, -1, "map");
        if (!lua_istable(pLuaState, -1)) return fail();
        lua_rawgeti(pLuaState, -1, eventId);
        const bool ownsHandler = lua_rawequal(pLuaState, -1, -4);
        lua_pop(pLuaState, 4);
        if (!ownsHandler)
        {
            lua_pop(pLuaState, 1);
            continue;
        }
        lua_getfield(pLuaState, -1, "spawns");
        if (!lua_istable(pLuaState, -1)) return fail();
        const size_t count = lua_rawlen(pLuaState, -1);
        std::vector<ScriptedEventProgram::MonsterSpawn> spawns(count);
        size_t entries = 0;
        lua_pushnil(pLuaState);
        while (lua_next(pLuaState, -2) != 0)
        {
            if (!lua_isinteger(pLuaState, -2) || lua_tointeger(pLuaState, -2) <= 0
                || uint64_t(lua_tointeger(pLuaState, -2)) > count || !lua_istable(pLuaState, -1)
                || lua_rawlen(pLuaState, -1) != 8) return fail();
            const size_t index = size_t(lua_tointeger(pLuaState, -2)) - 1;
            std::array<lua_Integer, 8> args{};
            size_t fields = 0;
            lua_pushnil(pLuaState);
            while (lua_next(pLuaState, -2) != 0)
            {
                if (!lua_isinteger(pLuaState, -2) || lua_tointeger(pLuaState, -2) < 1
                    || lua_tointeger(pLuaState, -2) > 8 || !lua_isinteger(pLuaState, -1)) return fail();
                args[size_t(lua_tointeger(pLuaState, -2)) - 1] = lua_tointeger(pLuaState, -1);
                ++fields;
                lua_pop(pLuaState, 1);
            }
            if (fields != 8 || args[0] < 1 || args[0] > 3 || args[1] < 0 || args[1] > UINT32_MAX
                || args[2] <= 0 || args[2] > INT32_MAX || args[6] < 0 || args[6] > UINT32_MAX
                || args[7] < 0 || args[7] > UINT32_MAX) return fail();
            for (size_t coordinate = 3; coordinate <= 5; ++coordinate)
                if (args[coordinate] < INT32_MIN || args[coordinate] > INT32_MAX) return fail();
            spawns[index] = {uint32_t(args[0]), uint32_t(args[1]), uint32_t(args[2]),
                int32_t(args[3]), int32_t(args[4]), int32_t(args[5]), uint32_t(args[6]), uint32_t(args[7])};
            ++entries;
            lua_pop(pLuaState, 1);
        }
        if (entries != count) return fail();
        if (!spawns.empty()) result.emplace(eventId, std::move(spawns));
        lua_pop(pLuaState, 2);
    }
    lua_settop(pLuaState, stackTop);
    return true;
}

bool readSiteReferences(lua_State *pLuaState, int metadataIndex, ScriptedEventScope scope,
    std::unordered_map<uint16_t, std::string> &result,
    std::unordered_map<uint16_t, std::string> &kinds, std::string &error)
{
    const int stackTop = lua_gettop(pLuaState);
    const auto fail = [&]()
    {
        error = "Invalid siteReferences metadata: expected registered handlers and reference text";
        lua_settop(pLuaState, stackTop);
        return false;
    };
    lua_getfield(pLuaState, metadataIndex, "siteReferences");
    if (lua_isnil(pLuaState, -1))
    {
        lua_pop(pLuaState, 1);
        return true;
    }
    if (!lua_istable(pLuaState, -1)) return fail();
    lua_pushnil(pLuaState);
    while (lua_next(pLuaState, -2) != 0)
    {
        if (!lua_isinteger(pLuaState, -2) || lua_tointeger(pLuaState, -2) <= 0
            || lua_tointeger(pLuaState, -2) > UINT16_MAX || !lua_istable(pLuaState, -1)) return fail();
        const uint16_t eventId = uint16_t(lua_tointeger(pLuaState, -2));
        lua_getfield(pLuaState, -1, "handler");
        if (!lua_isfunction(pLuaState, -1)) return fail();
        lua_getglobal(pLuaState, "evt");
        lua_getfield(pLuaState, -1, luaScopeName(scope));
        if (!lua_istable(pLuaState, -1)) return fail();
        lua_rawgeti(pLuaState, -1, eventId);
        bool ownsHandler = lua_rawequal(pLuaState, -1, -4);
        lua_pop(pLuaState, 4);
        if (ownsHandler)
        {
            lua_getfield(pLuaState, -1, "dependencies");
            if (!lua_isnil(pLuaState, -1))
            {
                if (scope != ScriptedEventScope::Map || !lua_istable(pLuaState, -1)) return fail();
                size_t count = 0;
                lua_pushnil(pLuaState);
                while (lua_next(pLuaState, -2) != 0)
                {
                    if (++count > 64 || !lua_isinteger(pLuaState, -2)
                        || lua_tointeger(pLuaState, -2) <= 0 || lua_tointeger(pLuaState, -2) > UINT16_MAX
                        || lua_tointeger(pLuaState, -2) == eventId || !lua_isfunction(pLuaState, -1)) return fail();
                    const uint16_t dependency = uint16_t(lua_tointeger(pLuaState, -2));
                    lua_getglobal(pLuaState, "evt");
                    lua_getfield(pLuaState, -1, "map");
                    if (!lua_istable(pLuaState, -1)) return fail();
                    lua_rawgeti(pLuaState, -1, dependency);
                    ownsHandler = lua_rawequal(pLuaState, -1, -4) && ownsHandler;
                    lua_pop(pLuaState, 4);
                }
            }
            lua_pop(pLuaState, 1);
        }
        if (ownsHandler)
        {
            lua_getfield(pLuaState, -1, "text");
            if (lua_type(pLuaState, -1) != LUA_TSTRING) return fail();
            size_t length = 0;
            const char *pText = lua_tolstring(pLuaState, -1, &length);
            if (length > 16384 || std::string_view(pText, length).find('\0') != std::string_view::npos) return fail();
            if (length != 0) result[eventId] = std::string(pText, length);
            lua_pop(pLuaState, 1);
            lua_getfield(pLuaState, -1, "kind");
            if (!lua_isnil(pLuaState, -1))
            {
                if (scope != ScriptedEventScope::Map || lua_type(pLuaState, -1) != LUA_TSTRING) return fail();
                const char *pKind = lua_tolstring(pLuaState, -1, &length);
                if (std::string_view(pKind, length) != "drink") return fail();
                kinds[eventId] = std::string(pKind, length);
            }
            lua_pop(pLuaState, 1);
        }
        lua_pop(pLuaState, 1);
    }
    lua_pop(pLuaState, 1);
    return true;
}

bool readInteractionReferences(lua_State *pLuaState, int metadataIndex, ScriptedEventScope scope,
    std::unordered_map<uint16_t, SiteInteractionReference> &result, std::string &error)
{
    const int stackTop = lua_gettop(pLuaState);
    const auto fail = [&]()
    {
        error = "Invalid interactionReferences metadata: expected a callback-owned typed reference";
        lua_settop(pLuaState, stackTop);
        return false;
    };
    lua_getfield(pLuaState, metadataIndex, "interactionReferences");
    if (lua_isnil(pLuaState, -1))
    {
        lua_pop(pLuaState, 1);
        return true;
    }
    if (scope != ScriptedEventScope::Map || !lua_istable(pLuaState, -1)) return fail();
    lua_pushnil(pLuaState);
    while (lua_next(pLuaState, -2) != 0)
    {
        if (!lua_isinteger(pLuaState, -2) || lua_tointeger(pLuaState, -2) <= 0
            || lua_tointeger(pLuaState, -2) > UINT16_MAX || !lua_istable(pLuaState, -1)) return fail();
        const uint16_t eventId = uint16_t(lua_tointeger(pLuaState, -2));
        lua_getfield(pLuaState, -1, "handler");
        lua_getglobal(pLuaState, "evt");
        lua_getfield(pLuaState, -1, "map");
        lua_rawgeti(pLuaState, -1, eventId);
        const bool ownsHandler = lua_isfunction(pLuaState, -4) && lua_rawequal(pLuaState, -1, -4);
        lua_pop(pLuaState, 4);
        if (ownsHandler)
        {
            lua_getfield(pLuaState, -1, "reference");
            if (lua_isnil(pLuaState, -1))
            {
                lua_pop(pLuaState, 2);
                continue;
            }
            if (!lua_istable(pLuaState, -1)) return fail();
            SiteInteractionReference reference;
            bool valid = true;
            lua_pushnil(pLuaState);
            while (lua_next(pLuaState, -2) != 0)
            {
                if (lua_type(pLuaState, -2) != LUA_TSTRING) return fail();
                size_t size = 0;
                const char *pKey = lua_tolstring(pLuaState, -2, &size);
                const std::string_view key(pKey, size);
                uint32_t *pNumber = key == "itemId" ? &reference.itemId : key == "spellId" ? &reference.spellId
                    : key == "npcId" ? &reference.npcId : key == "spriteId" ? &reference.spriteId
                    : key == "completionQBit" ? &reference.completionQBit
                    : key == "openedQBit" ? &reference.openedQBit : nullptr;
                if (pNumber != nullptr)
                {
                    if (!lua_isinteger(pLuaState, -1) || lua_tointeger(pLuaState, -1) <= 0
                        || lua_tointeger(pLuaState, -1) > INT32_MAX) return fail();
                    *pNumber = uint32_t(lua_tointeger(pLuaState, -1));
                }
                else if (key == "consumesItem" || key == "addsFollower")
                {
                    if (!lua_isboolean(pLuaState, -1)) return fail();
                    (key == "consumesItem" ? reference.consumesItem : reference.addsFollower)
                        = lua_toboolean(pLuaState, -1);
                }
                else if (key == "kind" || key == "openedMapFlag" || key == "targetMap" || key == "network"
                    || key == "itemScope")
                {
                    if (lua_type(pLuaState, -1) != LUA_TSTRING) return fail();
                    const char *pValue = lua_tolstring(pLuaState, -1, &size);
                    if (size == 0 || size > 128 || std::string_view(pValue, size).find('\0') != std::string_view::npos)
                        return fail();
                    std::string &value = key == "kind" ? reference.kind : key == "network" ? reference.network
                        : key == "openedMapFlag" ? reference.openedMapFlag
                        : key == "itemScope" ? reference.itemScope : reference.targetMap;
                    value.assign(pValue, size);
                }
                else return fail();
                lua_pop(pLuaState, 1);
            }
            if (!reference.openedMapFlag.empty())
                for (char character : reference.openedMapFlag)
                    valid = valid && ((character >= 'a' && character <= 'z')
                        || (character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9')
                        || character == '_');
            if (!reference.targetMap.empty())
            {
                valid = valid && reference.targetMap.size() > 4
                    && (reference.targetMap.ends_with(".blv") || reference.targetMap.ends_with(".odm"));
                for (size_t index = 0; index + 4 < reference.targetMap.size(); ++index)
                {
                    char &character = reference.targetMap[index];
                    valid = valid && ((character >= 'a' && character <= 'z')
                        || (character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9')
                        || character == '_' || character == '-');
                    if (character >= 'A' && character <= 'Z') character += 'a' - 'A';
                }
            }
            if (reference.kind != "dimension_door" && !reference.network.empty()) valid = false;
            const bool passage = reference.kind == "enter_dungeon" || reference.kind == "leave_dungeon";
            if (!passage && (!reference.itemScope.empty() || reference.openedQBit != 0)) valid = false;
            if (passage)
                valid = valid && (reference.itemScope.empty() || reference.itemScope == "party"
                    || reference.itemScope == "each_member" || reference.itemScope == "selected")
                    && (reference.openedQBit == 0 || reference.openedMapFlag.empty());
            if (reference.kind == "dimension_door")
                valid = valid && !reference.network.empty() && reference.network.front() != '.'
                    && reference.network.back() != '.'
                    && reference.network.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789._-")
                        == std::string::npos && reference.network.find("..") == std::string::npos
                    && reference.itemId == 0 && reference.spellId == 0 && reference.npcId == 0
                    && reference.spriteId == 0 && reference.completionQBit == 0
                    && !reference.consumesItem && !reference.addsFollower
                    && reference.targetMap.empty() && reference.openedMapFlag.empty();
            else if (passage)
                valid = valid && reference.itemId > 0 && !reference.targetMap.empty()
                    && reference.npcId == 0 && reference.spriteId == 0
                    && reference.spellId == 0 && reference.completionQBit == 0
                    && !reference.consumesItem && !reference.addsFollower;
            else if (reference.kind == "rescue" || reference.kind == "stone_to_flesh")
                valid = valid && reference.itemId > 0 && reference.npcId > 0 && reference.spriteId > 0
                    && reference.targetMap.empty() && reference.openedMapFlag.empty()
                    && (reference.kind == "rescue" ? reference.completionQBit > 0 && reference.spellId == 0
                        : reference.spellId > 0 && reference.completionQBit == 0 && !reference.addsFollower);
            else valid = false;
            if (!valid) return fail();
            result.emplace(eventId, std::move(reference));
            lua_pop(pLuaState, 1);
        }
        lua_pop(pLuaState, 1);
    }
    lua_pop(pLuaState, 1);
    return true;
}

bool readConnectionTargets(lua_State *pLuaState, int metadataIndex, ScriptedEventScope scope,
    std::unordered_map<uint16_t, std::vector<std::string>> &result, std::string &error)
{
    const int stackTop = lua_gettop(pLuaState);
    const auto fail = [&]()
    {
        error = "Invalid connectionTargets: expected callback-owned dense arrays of distinct map filenames";
        lua_settop(pLuaState, stackTop);
        return false;
    };
    lua_getfield(pLuaState, metadataIndex, "connectionTargets");
    if (lua_isnil(pLuaState, -1)) { lua_pop(pLuaState, 1); return true; }
    if (scope != ScriptedEventScope::Map || !lua_istable(pLuaState, -1)) return fail();
    lua_pushnil(pLuaState);
    while (lua_next(pLuaState, -2) != 0)
    {
        if (!lua_isinteger(pLuaState, -2) || lua_tointeger(pLuaState, -2) <= 0
            || lua_tointeger(pLuaState, -2) > UINT16_MAX || !lua_istable(pLuaState, -1)) return fail();
        const uint16_t eventId = uint16_t(lua_tointeger(pLuaState, -2));
        lua_getfield(pLuaState, -1, "handler");
        lua_getglobal(pLuaState, "evt");
        lua_getfield(pLuaState, -1, "map");
        lua_rawgeti(pLuaState, -1, eventId);
        const bool ownsHandler = lua_isfunction(pLuaState, -4) && lua_rawequal(pLuaState, -1, -4);
        lua_pop(pLuaState, 4);
        if (!ownsHandler) { lua_pop(pLuaState, 1); continue; }
        lua_getfield(pLuaState, -1, "maps");
        if (!lua_isnil(pLuaState, -1))
        {
            if (!lua_istable(pLuaState, -1) || lua_rawlen(pLuaState, -1) > 64) return fail();
            const size_t count = lua_rawlen(pLuaState, -1);
            size_t entries = 0;
            lua_pushnil(pLuaState);
            while (lua_next(pLuaState, -2) != 0)
            {
                if (!lua_isinteger(pLuaState, -2) || lua_tointeger(pLuaState, -2) <= 0
                    || uint64_t(lua_tointeger(pLuaState, -2)) > count) return fail();
                ++entries;
                lua_pop(pLuaState, 1);
            }
            if (entries != count) return fail();
            std::vector<std::string> maps;
            std::set<std::string> used;
            for (size_t index = 1; index <= count; ++index)
            {
                lua_rawgeti(pLuaState, -1, lua_Integer(index));
                if (lua_type(pLuaState, -1) != LUA_TSTRING) return fail();
                size_t size = 0;
                const char *pValue = lua_tolstring(pLuaState, -1, &size);
                const std::string map(pValue, size);
                if (size <= 4 || size > 128 || (!map.ends_with(".blv") && !map.ends_with(".odm"))
                    || map.substr(0, size - 4).find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_-")
                        != std::string::npos || !used.insert(map).second) return fail();
                maps.push_back(map);
                lua_pop(pLuaState, 1);
            }
            if (!maps.empty()) result.emplace(eventId, std::move(maps));
        }
        lua_pop(pLuaState, 2);
    }
    lua_pop(pLuaState, 1);
    return true;
}

bool readSiteGroups(lua_State *pLuaState, int metadataIndex, ScriptedEventScope scope,
    std::unordered_map<uint16_t, std::vector<std::vector<std::string>>> &result, std::string &error)
{
    const int stackTop = lua_gettop(pLuaState);
    const auto fail = [&]()
    {
        error = "Invalid siteGroups metadata: expected callback-owned disjoint arrays of physical site IDs";
        lua_settop(pLuaState, stackTop);
        return false;
    };
    lua_getfield(pLuaState, metadataIndex, "siteGroups");
    if (lua_isnil(pLuaState, -1)) { lua_pop(pLuaState, 1); return true; }
    if (scope != ScriptedEventScope::Map || !lua_istable(pLuaState, -1)) return fail();
    const auto validSiteId = [](std::string_view id)
    {
        const auto number = [](std::string_view value)
        {
            uint32_t parsed = 0;
            const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
            return !value.empty() && result.ec == std::errc{} && result.ptr == value.data() + value.size()
                && std::to_string(parsed) == value;
        };
        if (id.starts_with("bmodel/"))
        {
            id.remove_prefix(7);
            const size_t slash = id.find('/');
            if (slash == std::string_view::npos || !number(id.substr(0, slash))) return false;
            id.remove_prefix(slash + 1);
            return id.starts_with("surface/") && number(id.substr(8));
        }
        if (id.starts_with("surface/")) return number(id.substr(8));
        return id.starts_with("decoration/") && number(id.substr(11));
    };
    const auto arraySize = [&](int index, size_t minimum, size_t maximum) -> std::optional<size_t>
    {
        if (!lua_istable(pLuaState, index)) return std::nullopt;
        index = lua_absindex(pLuaState, index);
        const size_t count = lua_rawlen(pLuaState, index);
        if (count < minimum || count > maximum) return std::nullopt;
        size_t entries = 0;
        lua_pushnil(pLuaState);
        while (lua_next(pLuaState, index) != 0)
        {
            const bool valid = lua_isinteger(pLuaState, -2) && lua_tointeger(pLuaState, -2) > 0
                && uint64_t(lua_tointeger(pLuaState, -2)) <= count;
            lua_pop(pLuaState, 1);
            if (!valid) { lua_pop(pLuaState, 1); return std::nullopt; }
            ++entries;
        }
        return entries == count ? std::optional(count) : std::nullopt;
    };
    lua_pushnil(pLuaState);
    while (lua_next(pLuaState, -2) != 0)
    {
        if (!lua_isinteger(pLuaState, -2) || lua_tointeger(pLuaState, -2) <= 0
            || lua_tointeger(pLuaState, -2) > UINT16_MAX || !lua_istable(pLuaState, -1)) return fail();
        const uint16_t eventId = uint16_t(lua_tointeger(pLuaState, -2));
        lua_getfield(pLuaState, -1, "handler");
        lua_getglobal(pLuaState, "evt");
        lua_getfield(pLuaState, -1, "map");
        lua_rawgeti(pLuaState, -1, eventId);
        const bool ownsHandler = lua_isfunction(pLuaState, -4) && lua_rawequal(pLuaState, -1, -4);
        lua_pop(pLuaState, 4);
        if (!ownsHandler) { lua_pop(pLuaState, 1); continue; }
        lua_getfield(pLuaState, -1, "groups");
        if (!lua_isnil(pLuaState, -1))
        {
            const auto count = arraySize(-1, 0, 64);
            if (!count) return fail();
            std::vector<std::vector<std::string>> groups;
            std::set<std::string> used;
            for (size_t groupIndex = 1; groupIndex <= *count; ++groupIndex)
            {
                lua_rawgeti(pLuaState, -1, lua_Integer(groupIndex));
                const auto members = arraySize(-1, 2, 256);
                if (!members) return fail();
                std::vector<std::string> group;
                for (size_t index = 1; index <= *members; ++index)
                {
                    lua_rawgeti(pLuaState, -1, lua_Integer(index));
                    if (lua_type(pLuaState, -1) != LUA_TSTRING) return fail();
                    size_t length = 0;
                    const char *pValue = lua_tolstring(pLuaState, -1, &length);
                    if (length == 0 || length > 80) return fail();
                    const std::string value(pValue, length);
                    if (!validSiteId(value) || !used.insert(value).second) return fail();
                    group.push_back(value);
                    lua_pop(pLuaState, 1);
                }
                groups.push_back(std::move(group));
                lua_pop(pLuaState, 1);
            }
            if (!groups.empty()) result.emplace(eventId, std::move(groups));
        }
        lua_pop(pLuaState, 2);
    }
    lua_settop(pLuaState, stackTop);
    return true;
}

bool readIdReferences(lua_State *pLuaState, int metadataIndex, ScriptedEventScope scope,
    const char *pField, const char *pValues, uint32_t maximumId,
    std::unordered_map<uint16_t, std::vector<uint32_t>> &result, std::string &error)
{
    const int stackTop = lua_gettop(pLuaState);
    const auto fail = [&]()
    {
        error = "Invalid " + std::string(pField)
            + " metadata: expected registered handlers and arrays of positive IDs up to " + std::to_string(maximumId);
        lua_settop(pLuaState, stackTop);
        return false;
    };
    lua_getfield(pLuaState, metadataIndex, pField);
    if (lua_isnil(pLuaState, -1))
    {
        lua_pop(pLuaState, 1);
        return true;
    }
    if (!lua_istable(pLuaState, -1)) return fail();
    lua_pushnil(pLuaState);
    while (lua_next(pLuaState, -2) != 0)
    {
        if (!lua_isinteger(pLuaState, -2) || lua_tointeger(pLuaState, -2) <= 0
            || lua_tointeger(pLuaState, -2) > UINT16_MAX || !lua_istable(pLuaState, -1)) return fail();
        const uint16_t eventId = uint16_t(lua_tointeger(pLuaState, -2));
        lua_getfield(pLuaState, -1, "handler");
        if (!lua_isfunction(pLuaState, -1)) return fail();
        lua_getglobal(pLuaState, "evt");
        lua_getfield(pLuaState, -1, luaScopeName(scope));
        if (!lua_istable(pLuaState, -1)) return fail();
        lua_rawgeti(pLuaState, -1, eventId);
        const bool ownsHandler = lua_rawequal(pLuaState, -1, -4);
        lua_pop(pLuaState, 4);
        if (!ownsHandler)
        {
            // The callback was replaced or removed. Only explicitly redeclared metadata can describe it.
            lua_pop(pLuaState, 1);
            continue;
        }
        lua_getfield(pLuaState, -1, pValues);
        if (!lua_istable(pLuaState, -1)) return fail();
        const size_t count = lua_rawlen(pLuaState, -1);
        size_t entries = 0;
        std::vector<uint32_t> items;
        lua_pushnil(pLuaState);
        while (lua_next(pLuaState, -2) != 0)
        {
            if (!lua_isinteger(pLuaState, -2) || lua_tointeger(pLuaState, -2) <= 0
                || uint64_t(lua_tointeger(pLuaState, -2)) > count || !lua_isinteger(pLuaState, -1)
                || lua_tointeger(pLuaState, -1) <= 0 || uint64_t(lua_tointeger(pLuaState, -1)) > maximumId)
                return fail();
            ++entries;
            items.push_back(uint32_t(lua_tointeger(pLuaState, -1)));
            lua_pop(pLuaState, 1);
        }
        if (entries != count) return fail();
        std::sort(items.begin(), items.end());
        items.erase(std::unique(items.begin(), items.end()), items.end());
        if (!items.empty()) result.emplace(eventId, std::move(items));
        lua_pop(pLuaState, 2);
    }
    lua_settop(pLuaState, stackTop);
    return true;
}

template <typename Reference, typename Less>
bool readNpcReferences(lua_State *pLuaState, int metadataIndex, ScriptedEventScope scope,
    const char *pField, const char *pValues,
    std::unordered_map<uint16_t, std::vector<Reference>> &result, Less less, std::string &error)
{
    const int stackTop = lua_gettop(pLuaState);
    const auto fail = [&]()
    {
        error = "Invalid " + std::string(pField)
            + " metadata: expected registered handlers and arrays of positive NPC/target ID pairs";
        lua_settop(pLuaState, stackTop);
        return false;
    };
    lua_getfield(pLuaState, metadataIndex, pField);
    if (lua_isnil(pLuaState, -1))
    {
        lua_pop(pLuaState, 1);
        return true;
    }
    if (!lua_istable(pLuaState, -1)) return fail();
    lua_pushnil(pLuaState);
    while (lua_next(pLuaState, -2) != 0)
    {
        if (!lua_isinteger(pLuaState, -2) || lua_tointeger(pLuaState, -2) <= 0
            || lua_tointeger(pLuaState, -2) > UINT16_MAX || !lua_istable(pLuaState, -1)) return fail();
        const uint16_t eventId = uint16_t(lua_tointeger(pLuaState, -2));
        lua_getfield(pLuaState, -1, "handler");
        if (!lua_isfunction(pLuaState, -1)) return fail();
        lua_getglobal(pLuaState, "evt");
        lua_getfield(pLuaState, -1, luaScopeName(scope));
        if (!lua_istable(pLuaState, -1)) return fail();
        lua_rawgeti(pLuaState, -1, eventId);
        const bool ownsHandler = lua_rawequal(pLuaState, -1, -4);
        lua_pop(pLuaState, 4);
        if (!ownsHandler)
        {
            lua_pop(pLuaState, 1);
            continue;
        }
        lua_getfield(pLuaState, -1, pValues);
        if (!lua_istable(pLuaState, -1)) return fail();
        const size_t count = lua_rawlen(pLuaState, -1);
        size_t entries = 0;
        std::vector<Reference> references;
        lua_pushnil(pLuaState);
        while (lua_next(pLuaState, -2) != 0)
        {
            if (!lua_isinteger(pLuaState, -2) || lua_tointeger(pLuaState, -2) <= 0
                || uint64_t(lua_tointeger(pLuaState, -2)) > count || !lua_istable(pLuaState, -1)
                || lua_rawlen(pLuaState, -1) != 2) return fail();
            ++entries;
            size_t fields = 0;
            lua_pushnil(pLuaState);
            while (lua_next(pLuaState, -2) != 0)
            {
                if (!lua_isinteger(pLuaState, -2) || lua_tointeger(pLuaState, -2) < 1
                    || lua_tointeger(pLuaState, -2) > 2 || !lua_isinteger(pLuaState, -1)
                    || lua_tointeger(pLuaState, -1) <= 0 || uint64_t(lua_tointeger(pLuaState, -1)) > UINT32_MAX)
                    return fail();
                ++fields;
                lua_pop(pLuaState, 1);
            }
            if (fields != 2) return fail();
            lua_rawgeti(pLuaState, -1, 1);
            const uint32_t npcId = uint32_t(lua_tointeger(pLuaState, -1));
            lua_pop(pLuaState, 1);
            lua_rawgeti(pLuaState, -1, 2);
            const uint32_t targetId = uint32_t(lua_tointeger(pLuaState, -1));
            lua_pop(pLuaState, 1);
            references.push_back({npcId, targetId});
            lua_pop(pLuaState, 1);
        }
        if (entries != count) return fail();
        std::sort(references.begin(), references.end(), less);
        references.erase(std::unique(references.begin(), references.end()), references.end());
        if (!references.empty()) result.emplace(eventId, std::move(references));
        lua_pop(pLuaState, 2);
    }
    lua_settop(pLuaState, stackTop);
    return true;
}
}

bool ScriptedEventProgram::populateMetadataFromLua(
    lua_State *pLuaState,
    ScriptedEventScope scope,
    ScriptedEventProgram &program,
    std::string &error)
{
    program.m_eventIds = readHandlerIds(pLuaState, luaScopeName(scope));

    if (scope == ScriptedEventScope::Global)
    {
        program.m_canShowTopicEventIds = readHandlerIds(pLuaState, LuaScopeCanShowTopic);
    }

    if (!pushEvtMetaScope(pLuaState, scope))
    {
        error = "missing evt.meta." + std::string(luaScopeName(scope));
        return false;
    }

    program.m_onLoadEventIds = readIntegerArrayFromField<uint16_t>(pLuaState, -1, "onLoad", true);
    program.m_onLeaveEventIds = readIntegerArrayFromField<uint16_t>(pLuaState, -1, "onLeave", true);
    program.m_monsterKilledHookEventIds =
        readIntegerArrayFromField<uint16_t>(pLuaState, -1, "monsterKilledHooks", true);
    program.m_monsterDamageHookEventIds =
        readIntegerArrayFromField<uint16_t>(pLuaState, -1, "monsterDamageHooks", true);
    program.m_hints = readStringMapFromField(pLuaState, -1, "hint");
    program.m_summaries = readStringMapFromField(pLuaState, -1, "title");

    if (program.m_summaries.empty())
    {
        program.m_summaries = readStringMapFromField(pLuaState, -1, "summary");
    }

    program.m_openedChestIdsByEventId = readOpenedChestIds(pLuaState, -1);
    program.m_contextActionsByEventId = readContextActionMetadataByEventId(pLuaState, -1);
    if (!readSiteReferences(pLuaState, lua_absindex(pLuaState, -1), scope,
        program.m_siteReferences, program.m_siteReferenceKinds, error))
        return false;
    if (!readInteractionReferences(pLuaState, lua_absindex(pLuaState, -1), scope,
        program.m_interactionReferences, error)) return false;
    if (!readSiteGroups(pLuaState, lua_absindex(pLuaState, -1), scope, program.m_siteGroups, error)) return false;
    if (!readConnectionTargets(pLuaState, lua_absindex(pLuaState, -1), scope,
        program.m_connectionTargets, error)) return false;
    if (!readMonsterSpawns(pLuaState, lua_absindex(pLuaState, -1), scope,
        program.m_monsterSpawns, error)) return false;
    program.m_textureNames = readStringArrayFromField(pLuaState, -1, "textureNames");
    program.m_spriteNames = readStringArrayFromField(pLuaState, -1, "spriteNames");
    program.m_castSpellIds = readIntegerArrayFromField<uint32_t>(pLuaState, -1, "castSpellIds");
    if (!readIdReferences(pLuaState, lua_absindex(pLuaState, -1), scope,
        "itemRewards", "items", UINT32_MAX, program.m_itemRewards, error)) return false;
    if (!readIdReferences(pLuaState, lua_absindex(pLuaState, -1), scope,
        "autonoteReferences", "notes", UINT16_MAX, program.m_autonoteReferences, error)) return false;
    if (!readIdReferences(pLuaState, lua_absindex(pLuaState, -1), scope,
        "qbitGrants", "qbits", UINT32_MAX, program.m_qbitGrants, error)) return false;
    if (!readNpcReferences(pLuaState, lua_absindex(pLuaState, -1), scope, "npcLocations", "locations",
        program.m_npcLocations, [](const NpcLocation &a, const NpcLocation &b)
        { return a.npcId < b.npcId || (a.npcId == b.npcId && a.houseId < b.houseId); }, error)) return false;
    if (!readNpcReferences(pLuaState, lua_absindex(pLuaState, -1), scope, "npcTopicReferences", "topics",
        program.m_npcTopicReferences, [](const NpcTopicReference &a, const NpcTopicReference &b)
        { return a.npcId < b.npcId || (a.npcId == b.npcId && a.topicId < b.topicId); }, error)) return false;
    program.m_levitateTrapFaceMask = readOptionalUnsignedField(pLuaState, -1, "levitateTrapFaceMask").value_or(0);
    program.m_levitateTrapEvents = readIntegerArrayFromField<uint16_t>(pLuaState, -1, "levitateTrapEvents", true);
    program.m_timerTriggers = readTimerTriggers(pLuaState, -1, scope);
    lua_pop(pLuaState, 3);
    return true;
}

const std::unordered_map<uint16_t, std::vector<uint32_t>> &ScriptedEventProgram::itemRewards() const
{
    return m_itemRewards;
}

const std::unordered_map<uint16_t, std::vector<ScriptedEventProgram::MonsterSpawn>> &
ScriptedEventProgram::monsterSpawns() const
{
    return m_monsterSpawns;
}

const std::unordered_map<uint16_t, std::vector<std::string>> &ScriptedEventProgram::connectionTargets() const
{
    return m_connectionTargets;
}

const std::unordered_map<uint16_t, SiteInteractionReference> &ScriptedEventProgram::interactionReferences() const
{
    return m_interactionReferences;
}

const std::unordered_map<uint16_t, std::vector<std::vector<std::string>>> &ScriptedEventProgram::siteGroups() const
{
    return m_siteGroups;
}

const std::unordered_map<uint16_t, std::string> &ScriptedEventProgram::siteReferences() const
{
    return m_siteReferences;
}

const std::unordered_map<uint16_t, std::vector<uint32_t>> &ScriptedEventProgram::autonoteReferences() const
{
    return m_autonoteReferences;
}

const std::unordered_map<uint16_t, std::vector<ScriptedEventProgram::NpcLocation>> &
ScriptedEventProgram::npcLocations() const
{
    return m_npcLocations;
}

const std::unordered_map<uint16_t, std::vector<ScriptedEventProgram::NpcTopicReference>> &
ScriptedEventProgram::npcTopicReferences() const
{
    return m_npcTopicReferences;
}

const std::unordered_map<uint16_t, std::vector<uint32_t>> &ScriptedEventProgram::qbitGrants() const
{
    return m_qbitGrants;
}

std::optional<ScriptedEventProgram> ScriptedEventProgram::loadFromLuaText(
    const std::string &luaSourceText,
    const std::string &luaSourceName,
    ScriptedEventScope scope,
    std::string &error)
{
    error.clear();
    Engine::LuaStateOwner lua = {};

    if (!lua.isValid())
    {
        error = "lua state unavailable";
        return std::nullopt;
    }

    lua.openApprovedLibraries();
    registerMetadataBindings(lua.state());

    std::optional<std::string> runtimeError;

    if (!lua.runChunk(luaSourceText, luaSourceName, runtimeError))
    {
        error = runtimeError.value_or("failed to execute lua chunk");
        return std::nullopt;
    }

    ScriptedEventProgram program = {};
    program.m_luaSourceText = luaSourceText;
    program.m_luaSourceName = luaSourceName;
    program.m_cacheId = NextScriptedEventProgramCacheId.fetch_add(1, std::memory_order_relaxed);
    program.m_scope = scope;

    if (!populateMetadataFromLua(lua.state(), scope, program, error))
    {
        return std::nullopt;
    }

    return program;
}

const std::optional<std::string> &ScriptedEventProgram::luaSourceText() const
{
    return m_luaSourceText;
}

const std::optional<std::string> &ScriptedEventProgram::luaSourceName() const
{
    return m_luaSourceName;
}

uint64_t ScriptedEventProgram::cacheId() const
{
    return m_cacheId;
}

ScriptedEventScope ScriptedEventProgram::scope() const
{
    return m_scope;
}

const std::vector<uint16_t> &ScriptedEventProgram::eventIds() const
{
    return m_eventIds;
}

const std::vector<uint16_t> &ScriptedEventProgram::canShowTopicEventIds() const
{
    return m_canShowTopicEventIds;
}

const std::vector<uint16_t> &ScriptedEventProgram::onLoadEventIds() const
{
    return m_onLoadEventIds;
}

const std::vector<uint16_t> &ScriptedEventProgram::onLeaveEventIds() const
{
    return m_onLeaveEventIds;
}

const std::vector<uint16_t> &ScriptedEventProgram::monsterKilledHookEventIds() const
{
    return m_monsterKilledHookEventIds;
}

const std::vector<uint16_t> &ScriptedEventProgram::monsterDamageHookEventIds() const
{
    return m_monsterDamageHookEventIds;
}

const std::vector<std::string> &ScriptedEventProgram::textureNames() const
{
    return m_textureNames;
}

const std::vector<std::string> &ScriptedEventProgram::spriteNames() const
{
    return m_spriteNames;
}

const std::vector<uint32_t> &ScriptedEventProgram::castSpellIds() const
{
    return m_castSpellIds;
}

const std::vector<ScriptedEventProgram::TimerTrigger> &ScriptedEventProgram::timerTriggers() const
{
    return m_timerTriggers;
}

bool ScriptedEventProgram::isLevitateSensitivePressurePlate(uint16_t eventId, uint32_t faceAttributes) const
{
    return (faceAttributes & m_levitateTrapFaceMask) != 0
        || std::find(m_levitateTrapEvents.begin(), m_levitateTrapEvents.end(), eventId) != m_levitateTrapEvents.end();
}

bool ScriptedEventProgram::hasEvent(uint16_t eventId) const
{
    return std::find(m_eventIds.begin(), m_eventIds.end(), eventId) != m_eventIds.end();
}

bool ScriptedEventProgram::isHintOnlyEvent(uint16_t eventId) const
{
    return !hasEvent(eventId) && getHint(eventId).has_value();
}

std::optional<std::string> ScriptedEventProgram::getHint(uint16_t eventId) const
{
    const auto iterator = m_hints.find(eventId);
    return iterator != m_hints.end() ? std::optional<std::string>(iterator->second) : std::nullopt;
}

std::optional<std::string> ScriptedEventProgram::summarizeEvent(uint16_t eventId) const
{
    const auto iterator = m_summaries.find(eventId);
    return iterator != m_summaries.end() ? std::optional<std::string>(iterator->second) : std::nullopt;
}

std::vector<uint32_t> ScriptedEventProgram::getOpenedChestIds(uint16_t eventId) const
{
    const auto iterator = m_openedChestIdsByEventId.find(eventId);
    return iterator != m_openedChestIdsByEventId.end() ? iterator->second : std::vector<uint32_t>{};
}

std::optional<ScriptedEventProgram::ContextActionMetadata> ScriptedEventProgram::getContextActionMetadata(
    uint16_t eventId) const
{
    const auto iterator = m_contextActionsByEventId.find(eventId);
    std::optional<ContextActionMetadata> metadata = iterator != m_contextActionsByEventId.end()
        ? std::optional<ContextActionMetadata>(iterator->second)
        : std::nullopt;
    if (const auto targets = m_connectionTargets.find(eventId); targets != m_connectionTargets.end())
    {
        if (!metadata)
        {
            metadata.emplace();
            metadata->kind = "passage";
            metadata->source = "reference";
        }
        metadata->targetMaps = targets->second;
        metadata->targetMap = targets->second.size() == 1
            ? std::optional(targets->second.front()) : std::nullopt;
        if (targets->second.size() != 1) metadata->targetName.reset();
    }
    const auto kind = m_siteReferenceKinds.find(eventId);
    if (kind != m_siteReferenceKinds.end() && (!metadata
        || (metadata->kind == "generic_event" && metadata->source != "script" && metadata->source != "override")
        || metadata->source == "title"))
    {
        if (!metadata) metadata.emplace();
        metadata->kind = kind->second;
        metadata->source = "reference";
    }
    if (const auto reference = m_interactionReferences.find(eventId); reference != m_interactionReferences.end())
    {
        const bool inferContext = !metadata || metadata->source == "title"
            || (metadata->kind == "generic_event" && metadata->source != "script" && metadata->source != "override");
        if (!metadata) metadata.emplace();
        metadata->interactionReference = reference->second;
        if (inferContext)
        {
            metadata->kind = reference->second.kind;
            metadata->source = "script";
            if (!reference->second.targetMap.empty()) metadata->targetMap = reference->second.targetMap;
        }
    }
    return metadata;
}

std::vector<ScriptedEventTimerDefinition> scriptedEventTimerDefinitionsFromPrograms(
    const std::optional<ScriptedEventProgram> &localProgram,
    const std::optional<ScriptedEventProgram> &globalProgram)
{
    std::vector<ScriptedEventTimerDefinition> definitions;

    const auto appendDefinitions =
        [&definitions](const std::optional<ScriptedEventProgram> &program)
        {
            if (!program)
            {
                return;
            }

            definitions.insert(
                definitions.end(),
                program->timerTriggers().begin(),
                program->timerTriggers().end());
        };

    appendDefinitions(localProgram);
    appendDefinitions(globalProgram);
    return definitions;
}
}
