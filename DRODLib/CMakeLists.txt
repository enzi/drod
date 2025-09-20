cmake_minimum_required(VERSION 3.16)

# Source files
set(DRODLib_SOURCES
    Architect.cpp
    Aumtlich.cpp
    BlueSerpent.cpp
    Brain.cpp
    Briar.cpp
    Bridge.cpp
    Building.cpp
    BuildUtil.cpp
    Character.cpp
    CharacterCommand.cpp
    Citizen.cpp
    Clone.cpp
    Construct.cpp
    CueEvents.cpp
    CurrentGame.cpp
    Db.cpp
    DbBase.cpp
    DbCommands.cpp
    DbData.cpp
    DbDemos.cpp
    DbHolds.cpp
    DbLevels.cpp
    DbMessageText.cpp
    DbPackedVars.cpp
    DbPlayers.cpp
    DbRefs.cpp
    DbRooms.cpp
    DbSavedGames.cpp
    DbSpeech.cpp
    DbXML.cpp
    Decoy.cpp
    EntranceData.cpp
    EvilEye.cpp
    Fegundo.cpp
    FegundoAshes.cpp
    FluffBaby.cpp
    GameConstants.cpp
    Gentryii.cpp
    Goblin.cpp
    GreenSerpent.cpp
    Guard.cpp
    Halph.cpp
    ImportInfo.cpp
    Mimic.cpp
    Monster.cpp
    MonsterFactory.cpp
    MonsterMessage.cpp
    MonsterPiece.cpp
    Neather.cpp
    NetInterface.cpp
    OrbUtil.cpp
    PathMap.cpp
    Platform.cpp
    PlayerDouble.cpp
    PlayerStats.cpp
    RedSerpent.cpp
    Roach.cpp
    RoachEgg.cpp
    RoachQueen.cpp
    RockGiant.cpp
    RockGolem.cpp
    Seep.cpp
    Serpent.cpp
    SettingsKeys.cpp
    Slayer.cpp
    Spider.cpp
    Stalwart.cpp
    Station.cpp
    Swordsman.cpp
    TarBaby.cpp
    TarMother.cpp
    TemporalClone.cpp
    Waterskipper.cpp
    WaterskipperNest.cpp
    Weapons.cpp
    WraithWing.cpp
    Wubba.cpp
)

# Create static library
add_library(DRODLib STATIC ${DRODLib_SOURCES})

# Include directories
target_include_directories(DRODLib PUBLIC .)
target_include_directories(DRODLib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/BackEndLib)
target_include_directories(DRODLib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/FrontEndLib)
target_include_directories(DRODLib PRIVATE ${SDL2_INCLUDE_DIRS})

# Link libraries
target_link_libraries(DRODLib BackEndLib FrontEndLib)

# Make sure drod-revision.h is generated before building DRODLib
add_dependencies(DRODLib update_revision)
