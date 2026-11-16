#include "engine/Localization.h"
#pragma once

#include "game/events/EvtEnums.h"

#include <span>
#include <string_view>

namespace OpenYAMM::Game
{
enum class EventAttributeValueKind
{
    Number,
    Condition
};

// Stable Lua/API symbols are also the save keys. Numeric EVT aliases are not persistent identity.
struct EventAttributeInfo
{
    EvtVariable variable;
    std::string_view symbol;
    std::string_view label;
    bool partyResource;
    bool writable;
    bool referenceNumeric = true; // Reference predicates need separately audited comparison semantics.
    EventAttributeValueKind kind = EventAttributeValueKind::Number;
};

inline std::span<const EventAttributeInfo> eventAttributeInfos()
{
    static constexpr EventAttributeInfo attributes[] = {
        {EvtVariable::Gold, "Gold", Engine::trNoop("Party gold"), true, true},
        {EvtVariable::GoldInBank, "BankGold", Engine::trNoop("Banked gold"), true, true},
        {EvtVariable::Food, "Food", Engine::trNoop("Food"), true, true},
        {EvtVariable::CurrentHealth, "CurrentHealth", Engine::trNoop("Health"), false, true},
        {EvtVariable::CurrentSpellPoints, "CurrentSpellPoints", Engine::trNoop("Spell points"), false, true},
        {EvtVariable::MaxHealth, "MaxHealth", Engine::trNoop("Maximum health"), false, false},
        {EvtVariable::MaxSpellPoints, "MaxSpellPoints", Engine::trNoop("Maximum spell points"), false, false},
        {EvtVariable::BaseMight, "BaseMight", Engine::trNoop("Base Might"), false, true},
        {EvtVariable::BaseIntellect, "BaseIntellect", Engine::trNoop("Base Intellect"), false, true},
        {EvtVariable::BasePersonality, "BasePersonality", Engine::trNoop("Base Personality"), false, true},
        {EvtVariable::BaseEndurance, "BaseEndurance", Engine::trNoop("Base Endurance"), false, true},
        {EvtVariable::BaseAccuracy, "BaseAccuracy", Engine::trNoop("Base Accuracy"), false, true},
        {EvtVariable::BaseSpeed, "BaseSpeed", Engine::trNoop("Base Speed"), false, true},
        {EvtVariable::BaseLuck, "BaseLuck", Engine::trNoop("Base Luck"), false, true},
        {EvtVariable::MightBonus, "MightBonus", Engine::trNoop("Temporary Might"), false, true},
        {EvtVariable::IntellectBonus, "IntellectBonus", Engine::trNoop("Temporary Intellect"), false, true},
        {EvtVariable::PersonalityBonus, "PersonalityBonus", Engine::trNoop("Temporary Personality"), false, true},
        {EvtVariable::EnduranceBonus, "EnduranceBonus", Engine::trNoop("Temporary Endurance"), false, true},
        {EvtVariable::AccuracyBonus, "AccuracyBonus", Engine::trNoop("Temporary Accuracy"), false, true},
        {EvtVariable::SpeedBonus, "SpeedBonus", Engine::trNoop("Temporary Speed"), false, true},
        {EvtVariable::LuckBonus, "LuckBonus", Engine::trNoop("Temporary Luck"), false, true},
        {EvtVariable::NumSkillPoints, "SkillPoints", Engine::trNoop("Skill points"), false, true, false},
        {EvtVariable::ArmorClassBonus, "ArmorClassBonus", Engine::trNoop("Armor class bonus"), false, true, false},
        {EvtVariable::BaseLevel, "BaseLevel", Engine::trNoop("Level"), false, true, false},
        {EvtVariable::Age, "Age", Engine::trNoop("Age"), false, true, false},
        {EvtVariable::FireResistance, "FireResistance", Engine::trNoop("Fire resistance"), false, true},
        {EvtVariable::FireResistanceBonus, "FireResistanceBonus", Engine::trNoop("Fire resistance bonus"),
            false, true},
        {EvtVariable::AirResistance, "AirResistance", Engine::trNoop("Air resistance"), false, true},
        {EvtVariable::AirResistanceBonus, "AirResistanceBonus", Engine::trNoop("Air resistance bonus"),
            false, true},
        {EvtVariable::WaterResistance, "WaterResistance", Engine::trNoop("Water resistance"), false, true},
        {EvtVariable::WaterResistanceBonus, "WaterResistanceBonus", Engine::trNoop("Water resistance bonus"),
            false, true},
        {EvtVariable::EarthResistance, "EarthResistance", Engine::trNoop("Earth resistance"), false, true},
        {EvtVariable::EarthResistanceBonus, "EarthResistanceBonus", Engine::trNoop("Earth resistance bonus"),
            false, true},
        {EvtVariable::MindResistance, "MindResistance", Engine::trNoop("Mind resistance"), false, true},
        {EvtVariable::MindResistanceBonus, "MindResistanceBonus", Engine::trNoop("Mind resistance bonus"),
            false, true},
        {EvtVariable::BodyResistance, "BodyResistance", Engine::trNoop("Body resistance"), false, true},
        {EvtVariable::BodyResistanceBonus, "BodyResistanceBonus", Engine::trNoop("Body resistance bonus"),
            false, true},
        {EvtVariable::Cursed, "Cursed", Engine::trNoop("Curse"), false, true, false,
            EventAttributeValueKind::Condition},
        {EvtVariable::Weak, "Weak", Engine::trNoop("Weakness"), false, true, false,
            EventAttributeValueKind::Condition},
        {EvtVariable::Asleep, "Asleep", Engine::trNoop("Sleep"), false, true, false,
            EventAttributeValueKind::Condition},
        {EvtVariable::Afraid, "Afraid", Engine::trNoop("Fear"), false, true, false,
            EventAttributeValueKind::Condition},
        {EvtVariable::Drunk, "Drunk", Engine::trNoop("Drunkenness"), false, true, false,
            EventAttributeValueKind::Condition},
        {EvtVariable::Insane, "Insane", Engine::trNoop("Insanity"), false, true, false,
            EventAttributeValueKind::Condition},
        {EvtVariable::PoisonedGreen, "PoisonedGreen", Engine::trNoop("Mild poison"), false, true, false,
            EventAttributeValueKind::Condition},
        {EvtVariable::PoisonedYellow, "PoisonedYellow", Engine::trNoop("Medium poison"), false, true, false,
            EventAttributeValueKind::Condition},
        {EvtVariable::PoisonedRed, "PoisonedRed", Engine::trNoop("Severe poison"), false, true, false,
            EventAttributeValueKind::Condition},
        {EvtVariable::DiseasedGreen, "DiseasedGreen", Engine::trNoop("Mild disease"), false, true, false,
            EventAttributeValueKind::Condition},
        {EvtVariable::DiseasedYellow, "DiseasedYellow", Engine::trNoop("Medium disease"), false, true, false,
            EventAttributeValueKind::Condition},
        {EvtVariable::DiseasedRed, "DiseasedRed", Engine::trNoop("Severe disease"), false, true, false,
            EventAttributeValueKind::Condition},
        {EvtVariable::Paralyzed, "Paralyzed", Engine::trNoop("Paralysis"), false, true, false,
            EventAttributeValueKind::Condition},
        {EvtVariable::Unconscious, "Unconscious", Engine::trNoop("Unconsciousness"), false, true, false,
            EventAttributeValueKind::Condition},
        {EvtVariable::Dead, "Dead", Engine::trNoop("Death"), false, true, false,
            EventAttributeValueKind::Condition},
        {EvtVariable::Stoned, "Stoned", Engine::trNoop("Petrification"), false, true, false,
            EventAttributeValueKind::Condition},
        {EvtVariable::Eradicated, "Eradicated", Engine::trNoop("Eradication"), false, true, false,
            EventAttributeValueKind::Condition},
        {EvtVariable::ActualMight, "ActualMight", Engine::trNoop("Might"), false, false},
        {EvtVariable::ActualIntellect, "ActualIntellect", Engine::trNoop("Intellect"), false, false},
        {EvtVariable::ActualPersonality, "ActualPersonality", Engine::trNoop("Personality"), false, false},
        {EvtVariable::ActualEndurance, "ActualEndurance", Engine::trNoop("Endurance"), false, false},
        {EvtVariable::ActualAccuracy, "ActualAccuracy", Engine::trNoop("Accuracy"), false, false},
        {EvtVariable::ActualSpeed, "ActualSpeed", Engine::trNoop("Speed"), false, false},
        {EvtVariable::ActualLuck, "ActualLuck", Engine::trNoop("Luck"), false, false},
    };
    return attributes;
}

inline const EventAttributeInfo *eventAttributeInfo(std::string_view symbol)
{
    for (const EventAttributeInfo &attribute : eventAttributeInfos())
        if (attribute.symbol == symbol) return &attribute;
    return nullptr;
}
} // namespace OpenYAMM::Game
