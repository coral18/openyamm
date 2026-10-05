#include "engine/AssetFileSystem.h"
#include "game/data/GameDataLoader.h"
#include "game/ui/screens/NewGameScreen.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>

namespace OpenYAMM::Game
{
struct NewGameScreenTestAccess
{
    static void checkAllContinents(NewGameScreen &screen, const GameDataRepository &data,
                                   std::vector<Character> &createdParty, uint32_t &startingContinentId)
    {
        std::vector<uint32_t> expectedCharacterIds;
        for (const auto &[id, entry] : data.characterDollTable().characters())
        {
            if (entry.availableAtStart)
            {
                expectedCharacterIds.push_back(id);
            }
        }
        std::sort(expectedCharacterIds.begin(), expectedCharacterIds.end());
        const std::vector<uint32_t> humanClassIds = {0, 4, 5, 12, 16, 22, 26, 30, 34, 42, 44};
        const std::vector<uint32_t> dragonClassIds = {10};
        const std::vector<uint32_t> expectedDragonIds = {25, 26, 72, 75};

        for (const MergedCharacterSelectionContinent &continent : data.mergedCharacterSelectionTable().continents())
        {
            CAPTURE(continent.key);
            screen.selectContinent(continent.key);
            std::vector<uint32_t> characterIds;
            std::vector<size_t> dragonIndices;
            bool foundHuman = false;
            for (size_t index = 0; index < screen.m_candidates.size(); ++index)
            {
                const NewGameScreen::CreationCandidate &candidate = screen.m_candidates[index];
                characterIds.push_back(candidate.characterDataId);
                if (candidate.raceName == "Human")
                {
                    CHECK(candidate.availableClassIds == humanClassIds);
                    foundHuman = true;
                }
                if (candidate.raceName == "Dragon")
                {
                    CHECK(candidate.availableClassIds == dragonClassIds);
                    dragonIndices.push_back(index);
                }
            }
            CHECK(std::search(characterIds.begin(), characterIds.end(),
                              expectedDragonIds.begin(), expectedDragonIds.end()) != characterIds.end());
            std::sort(characterIds.begin(), characterIds.end());
            CHECK(characterIds == expectedCharacterIds);
            CHECK(foundHuman);
            REQUIRE_EQ(dragonIndices.size(), expectedDragonIds.size());

            for (size_t dragonIndex : dragonIndices)
            {
                createdParty.clear();
                startingContinentId = 0;
                screen.resetStateForCandidate(dragonIndex);
                screen.confirmCreation();
                REQUIRE_EQ(createdParty.size(), 1);
                CHECK_EQ(createdParty.front().characterDataId, screen.m_candidates[dragonIndex].characterDataId);
                CHECK_EQ(createdParty.front().raceId, 5);
                CHECK_EQ(createdParty.front().className, "Dragon");
                CHECK_EQ(startingContinentId, continent.id);
            }
        }
    }
};
}

TEST_CASE("party creation offers all starting characters and classes on every continent")
{
    using namespace OpenYAMM::Game;
    const std::filesystem::path sourceRoot = OPENYAMM_SOURCE_DIR;
    OpenYAMM::Engine::AssetFileSystem assets;
    REQUIRE(assets.initialize(sourceRoot, sourceRoot / "assets_dev", OpenYAMM::Engine::AssetScaleTier::X1, "mm6"));
    GameDataLoader loader;
    REQUIRE(loader.loadForHeadlessGameplay(assets));
    GameDataRepository data;
    data.bind(loader);
    std::vector<Character> createdParty;
    uint32_t startingContinentId = 0;
    NewGameScreen screen(assets, nullptr, data, false, false, true,
        [&](const std::vector<Character> &characters, uint32_t continentId, bool preserveDebugLoadout)
        {
            createdParty = characters;
            startingContinentId = continentId;
            CHECK_FALSE(preserveDebugLoadout);
        }, {});
    NewGameScreenTestAccess::checkAllContinents(screen, data, createdParty, startingContinentId);
}
