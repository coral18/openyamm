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
        {EvtVariable::Gold, "Gold", "Party gold", true, true},
        {EvtVariable::GoldInBank, "BankGold", "Banked gold", true, true},
        {EvtVariable::Food, "Food", "Food", true, true},
        {EvtVariable::CurrentHealth, "CurrentHealth", "Health", false, true},
        {EvtVariable::CurrentSpellPoints, "CurrentSpellPoints", "Spell points", false, true},
        {EvtVariable::MaxHealth, "MaxHealth", "Maximum health", false, false},
        {EvtVariable::MaxSpellPoints, "MaxSpellPoints", "Maximum spell points", false, false},
        {EvtVariable::BaseMight, "BaseMight", "Base Might", false, true},
        {EvtVariable::BaseIntellect, "BaseIntellect", "Base Intellect", false, true},
        {EvtVariable::BasePersonality, "BasePersonality", "Base Personality", false, true},
        {EvtVariable::BaseEndurance, "BaseEndurance", "Base Endurance", false, true},
        {EvtVariable::BaseAccuracy, "BaseAccuracy", "Base Accuracy", false, true},
        {EvtVariable::BaseSpeed, "BaseSpeed", "Base Speed", false, true},
        {EvtVariable::BaseLuck, "BaseLuck", "Base Luck", false, true},
        {EvtVariable::MightBonus, "MightBonus", "Temporary Might", false, true},
        {EvtVariable::IntellectBonus, "IntellectBonus", "Temporary Intellect", false, true},
        {EvtVariable::PersonalityBonus, "PersonalityBonus", "Temporary Personality", false, true},
        {EvtVariable::EnduranceBonus, "EnduranceBonus", "Temporary Endurance", false, true},
        {EvtVariable::AccuracyBonus, "AccuracyBonus", "Temporary Accuracy", false, true},
        {EvtVariable::SpeedBonus, "SpeedBonus", "Temporary Speed", false, true},
        {EvtVariable::LuckBonus, "LuckBonus", "Temporary Luck", false, true},
        {EvtVariable::NumSkillPoints, "SkillPoints", "Skill points", false, true, false},
        {EvtVariable::ArmorClassBonus, "ArmorClassBonus", "Armor class bonus", false, true, false},
        {EvtVariable::BaseLevel, "BaseLevel", "Level", false, true, false},
        {EvtVariable::Age, "Age", "Age", false, true, false},
        {EvtVariable::FireResistance, "FireResistance", "Fire resistance", false, true, false},
        {EvtVariable::FireResistanceBonus, "FireResistanceBonus", "Fire resistance bonus",
            false, true, false},
        {EvtVariable::AirResistance, "AirResistance", "Air resistance", false, true, false},
        {EvtVariable::AirResistanceBonus, "AirResistanceBonus", "Air resistance bonus",
            false, true, false},
        {EvtVariable::WaterResistance, "WaterResistance", "Water resistance", false, true, false},
        {EvtVariable::WaterResistanceBonus, "WaterResistanceBonus", "Water resistance bonus",
            false, true, false},
        {EvtVariable::EarthResistance, "EarthResistance", "Earth resistance", false, true, false},
        {EvtVariable::EarthResistanceBonus, "EarthResistanceBonus", "Earth resistance bonus",
            false, true, false},
        {EvtVariable::MindResistance, "MindResistance", "Mind resistance", false, true, false},
        {EvtVariable::MindResistanceBonus, "MindResistanceBonus", "Mind resistance bonus",
            false, true, false},
        {EvtVariable::BodyResistance, "BodyResistance", "Body resistance", false, true, false},
        {EvtVariable::BodyResistanceBonus, "BodyResistanceBonus", "Body resistance bonus",
            false, true, false},
        {EvtVariable::Cursed, "Cursed", "Curse", false, true, false,
            EventAttributeValueKind::Condition},
        {EvtVariable::Weak, "Weak", "Weakness", false, true, false,
            EventAttributeValueKind::Condition},
        {EvtVariable::Asleep, "Asleep", "Sleep", false, true, false,
            EventAttributeValueKind::Condition},
        {EvtVariable::Afraid, "Afraid", "Fear", false, true, false,
            EventAttributeValueKind::Condition},
        {EvtVariable::Drunk, "Drunk", "Drunkenness", false, true, false,
            EventAttributeValueKind::Condition},
        {EvtVariable::Insane, "Insane", "Insanity", false, true, false,
            EventAttributeValueKind::Condition},
        {EvtVariable::PoisonedGreen, "PoisonedGreen", "Mild poison", false, true, false,
            EventAttributeValueKind::Condition},
        {EvtVariable::PoisonedYellow, "PoisonedYellow", "Medium poison", false, true, false,
            EventAttributeValueKind::Condition},
        {EvtVariable::PoisonedRed, "PoisonedRed", "Severe poison", false, true, false,
            EventAttributeValueKind::Condition},
        {EvtVariable::DiseasedGreen, "DiseasedGreen", "Mild disease", false, true, false,
            EventAttributeValueKind::Condition},
        {EvtVariable::DiseasedYellow, "DiseasedYellow", "Medium disease", false, true, false,
            EventAttributeValueKind::Condition},
        {EvtVariable::DiseasedRed, "DiseasedRed", "Severe disease", false, true, false,
            EventAttributeValueKind::Condition},
        {EvtVariable::Paralyzed, "Paralyzed", "Paralysis", false, true, false,
            EventAttributeValueKind::Condition},
        {EvtVariable::Unconscious, "Unconscious", "Unconsciousness", false, true, false,
            EventAttributeValueKind::Condition},
        {EvtVariable::Dead, "Dead", "Death", false, true, false,
            EventAttributeValueKind::Condition},
        {EvtVariable::Stoned, "Stoned", "Petrification", false, true, false,
            EventAttributeValueKind::Condition},
        {EvtVariable::Eradicated, "Eradicated", "Eradication", false, true, false,
            EventAttributeValueKind::Condition},
        {EvtVariable::ActualMight, "ActualMight", "Might", false, false},
        {EvtVariable::ActualIntellect, "ActualIntellect", "Intellect", false, false},
        {EvtVariable::ActualPersonality, "ActualPersonality", "Personality", false, false},
        {EvtVariable::ActualEndurance, "ActualEndurance", "Endurance", false, false},
        {EvtVariable::ActualAccuracy, "ActualAccuracy", "Accuracy", false, false},
        {EvtVariable::ActualSpeed, "ActualSpeed", "Speed", false, false},
        {EvtVariable::ActualLuck, "ActualLuck", "Luck", false, false},
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
