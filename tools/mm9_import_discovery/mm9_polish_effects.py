"""Polish rendering of the current MM9 item effect lists; never reuse obsolete retail balance values."""
from __future__ import annotations

import re


NUMBER = re.compile(r"[-+]?\d+(?:\.\d+)?")
TERMS = {
    "{n} Might": "{0} do Mocy",
    "{n} Intellect": "{0} do Intelektu",
    "{n} Personality": "{0} do Osobowości",
    "{n} Endurance": "{0} do Wytrzymałości",
    "{n} Accuracy": "{0} do Celności",
    "{n} Speed": "{0} do Szybkości",
    "{n} Luck": "{0} do Szczęścia",
    "{n} HP": "{0} do PŻ",
    "{n} SP": "{0} do PM",
    "{n} AC": "{0} do KP",
    "{n} Armsmaster": "{0} do Władania bronią",
    "{n} Axe": "{0} do Topora",
    "{n} Bow": "{0} do Łuku",
    "{n} Mace": "{0} do Maczugi",
    "{n} Spear": "{0} do Włóczni",
    "{n} Staff": "{0} do Kija",
    "{n} Light": "{0} do Magii Światła",
    "{n} Spirit Magic": "{0} do Magii Ducha",
    "{n} Light Magic": "{0} do Magii Światła",
    "{n} Dark Magic": "{0} do Magii Ciemności",
    "{n} Disarm Traps": "{0} do Rozbrajania pułapek",
    "{n} Dodging": "{0} do Uników",
    "{n} Learning": "{0} do Uczenia się",
    "{n} Perception": "{0} do Percepcji",
    "{n} Meditation": "{0} do Medytacji",
    "{n} Repair Item": "{0} do Naprawy przedmiotów",
    "{n} damage": "{0} do obrażeń",
    "{n} damage per hit": "{0} obrażeń przy trafieniu",
    "{n} poison damage per hit": "{0} obrażeń od trucizny przy trafieniu",
    "{n} disease damage per hit": "{0} obrażeń od choroby przy trafieniu",
    "{n} to hit": "{0} do trafienia",
    "{n} to all stats": "{0} do wszystkich statystyk",
    "{n} to all resistances": "{0} do wszystkich odporności",
    "{n} to all magic skills": "{0} do wszystkich umiejętności magicznych",
    "{n} to the armor skills": "{0} do umiejętności posługiwania się pancerzem",
    "{n} AC against magic": "{0} do KP przeciw magii",
    "{n} AC against melee attacks": "{0} do KP przeciw atakom wręcz",
    "{n} AC against missiles": "{0} do KP przeciw pociskom",
    "While carried": "Gdy przedmiot jest noszony",
    "for the whole party: {n} Might": "dla całej drużyny: {0} do Mocy",
    "{n} Endurance for the whole party": "{0} do Wytrzymałości dla całej drużyny",
    "{n},{n} gold each week": "{0} {1} szt. złota tygodniowo",
    "Vampiric": "Wampiryczny",
    "never breaks": "niezniszczalny",
    "always hits": "zawsze trafia",
    "faster recovery": "krótszy czas gotowości",
    "slower recovery": "dłuższy czas gotowości",
    "regenerates HP": "regeneruje PŻ",
    "three projectiles per throw": "trzy pociski przy każdym rzucie",
    "halves the target's AC": "zmniejsza KP celu o połowę",
    "Breaks after its first attack": "psuje się po pierwszym ataku",
    "stuns the target on every hit": "ogłusza cel przy każdym trafieniu",
    "Divine Intervention always fails": "Boska interwencja zawsze zawodzi",
    "immune to all conditions": "odporność na wszystkie negatywne stany",
    "immune to weakness and drunkenness": "odporność na osłabienie i upojenie alkoholem",
    "random encounters {n}% more likely": "szansa losowych spotkań większa o {0}%",
    "{n}% less experience": "o {0}% mniej doświadczenia",
    "{n}% less physical damage taken": "o {0}% mniejsze otrzymywane obrażenia fizyczne",
    "{n}% chance each week to lose a tenth of HP": "{0}% szans tygodniowo na utratę jednej dziesiątej PŻ",
    "{n}% chance of insanity each week": "{0}% szans na szaleństwo w każdym tygodniu",
    "{n}% chance per shot to break and hurt the wielder": "{0}% szans przy każdym strzale na uszkodzenie broni"
        " i zranienie właściciela",
    "{n}% chance to enrage the target at Grandmaster": "{0}% szans na rozwścieczenie celu na poziomie arcymistrza",
    "{n}% chance to poison the target at Grandmaster": "{0}% szans na zatrucie celu na poziomie arcymistrza",
    "{n}% chance to frighten the target on a hit": "{0}% szans na przestraszenie celu przy trafieniu",
    "{n}% chance to ignore the target's AC": "{0}% szans na pominięcie KP celu",
    "{n}% chance to kill the target outright": "{0}% szans na natychmiastowe zabicie celu",
    "{n}% chance to stun the target": "{0}% szans na ogłuszenie celu",
    "{n}% chance to knock the wielder unconscious when a hit does not stun": "{0}% szans na utratę przytomności"
        " przez właściciela, jeśli trafienie nie ogłuszy celu",
    "{n} Blade; {n} Might (of Regeneration with complete set.)": "{0} do Broni siecznej; {1} do Mocy"
        " (Regeneracja przy pełnym zestawie)",
    "{n} Body Building; {n} AC (of Regeneration with complete set.)": "{0} do Kulturystyki; {1} do KP"
        " (Regeneracja przy pełnym zestawie)",
    "{n} Light; {n} Might (of Body with complete set)": "{0} do Magii Światła; {1} do Mocy"
        " (efekt Ciała przy pełnym zestawie)",
    "{n} Light; {n} Speed ({n} to Speed with complete set)": "{0} do Magii Światła; {1} do Szybkości"
        " ({2} do Szybkości przy pełnym zestawie)",
    "{n} Spirit ({n} all stats with complete set)": "{0} do Magii Ducha"
        " ({1} do wszystkich statystyk przy pełnym zestawie)",
    "{n} Meditation ({n} all resistances with complete set)": "{0} do Medytacji"
        " ({1} do wszystkich odporności przy pełnym zestawie)",
    "{n} Repair Item (of Skill with complete set)": "{0} do Naprawy przedmiotów"
        " (efekt Biegłości przy pełnym zestawie)",
}


def polish_effect_description(english: str, retail_polish: str) -> str | None:
    """Keep official narrative, render the current numeric effect list, refuse unfamiliar wording or missing markers."""
    flavor = re.split(r"\((?:Efekty|Premie|Premia):", retail_polish, maxsplit=1)
    if len(flavor) != 2 or not flavor[0].strip() or "(Effects:" not in english or not english.rstrip().endswith(")"):
        return None
    suffix = english.split("(Effects:", 1)[1].strip()[:-1]
    translated = []
    # A thousands separator is followed by digits, not a space, so it stays inside its phrase.
    for part in suffix.split(", "):
        part = part.strip()
        template = TERMS.get(NUMBER.sub("{n}", part))
        if template is None:
            return None
        translated.append(template.format(*NUMBER.findall(part)))
    result = "(Efekty: " + ", ".join(translated) + ")"
    if NUMBER.findall(suffix) != NUMBER.findall(result):
        raise ValueError("Polish item effect translation changed a number or its sign")
    return flavor[0].strip() + " " + result
