#include "doctest/doctest.h"

#include "game/gameplay/GameplaySaveLoadUiSupport.h"

using namespace OpenYAMM::Game;

TEST_CASE("save location labels resolve legacy and canonical map ids")
{
    MapStatsEntry sorpigal = {};
    sorpigal.fileName = "oute3.odm";
    sorpigal.canonicalId = "mm6:oute3.odm";
    sorpigal.name = "New Sorpigal";
    MapStatsEntry goblinwatch = {};
    goblinwatch.fileName = "6d01.blv";
    goblinwatch.canonicalId = "mm6:6d01.blv";
    goblinwatch.name = "Goblinwatch";
    const std::vector<MapStatsEntry> maps = {sorpigal, goblinwatch};

    CHECK(resolveSaveLocationName(maps, "OUTE3.ODM") == "New Sorpigal");
    CHECK(resolveSaveLocationName(maps, "MM6:OUTE3.ODM") == "New Sorpigal");
    CHECK(resolveSaveLocationName(maps, "6d01.blv") == "Goblinwatch");
    CHECK(resolveSaveLocationName(maps, "mm6:6d01.blv") == "Goblinwatch");
    CHECK(resolveSaveLocationName(maps, "custom.blv") == "custom.blv");
    CHECK(resolveSaveLocationName(maps, "").empty());
}

TEST_CASE("save game name conflict ignores target slot and empty slots")
{
    GameplayUiController::SaveGameScreenState screen = {};
    GameplayUiController::SaveSlotSummary firstSlot = {};
    firstSlot.fileLabel = "Castle Alamos";
    firstSlot.populated = true;
    screen.slots.push_back(firstSlot);

    GameplayUiController::SaveSlotSummary secondSlot = {};
    secondSlot.fileLabel = "Empty";
    secondSlot.populated = false;
    screen.slots.push_back(secondSlot);

    CHECK_FALSE(saveGameNameConflictsWithExistingSlot(screen, "Castle Alamos", 0));
    CHECK(saveGameNameConflictsWithExistingSlot(screen, "castle alamos", 1));
    CHECK_FALSE(saveGameNameConflictsWithExistingSlot(screen, "Empty", 1));
    CHECK_FALSE(saveGameNameConflictsWithExistingSlot(screen, "   ", 1));
}
