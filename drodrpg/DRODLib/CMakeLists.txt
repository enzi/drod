cmake_minimum_required(VERSION 3.16)

# Source files
set(DRODLib_SOURCES
    Ant.cpp
    AntHill.cpp
    BlueSerpent.cpp
    Brain.cpp
    Briar.cpp
    Bridge.cpp
    Building.cpp
    BuildUtil.cpp
    CharacterCommand.cpp
    Character.cpp
    Citizen.cpp
    Clone.cpp
    Combat.cpp
    Construct.cpp
    CueEvents.cpp
    CurrentGame.cpp
    DbBase.cpp
    DbCommands.cpp
    Db.cpp
    DbData.cpp
    DbDemos.cpp
    DbHolds.cpp
    DbLevels.cpp
    DbLocalHighScores.cpp
    DbMessageText.cpp
    DbPackedVars.cpp
    DbPlayers.cpp
    DbRefs.cpp
    DbRooms.cpp
    DbSavedGameMoves.cpp
    DbSavedGames.cpp
    DbSpeech.cpp
    DbXML.cpp
    Decoy.cpp
    EntranceData.cpp
    EvilEye.cpp
    FluffBaby.cpp
    GameConstants.cpp
    Goblin.cpp
    GreenSerpent.cpp
    Guard.cpp
    Halph.cpp
    I18N.cpp
    ImportInfo.cpp
    Mimic.cpp
    Monster.cpp
    MonsterFactory.cpp
    MonsterMessage.cpp
    MonsterPiece.cpp
    NetInterface.cpp
    OrbUtil.cpp
    PathMap.cpp
    PhoenixAshes.cpp
    Phoenix.cpp
    Platform.cpp
    PlayerDouble.cpp
    PlayerStats.cpp
    RedSerpent.cpp
    Roach.cpp
    RoachEgg.cpp
    RoachQueen.cpp
    RockGiant.cpp
    Seep.cpp
    Serpent.cpp
    SettingsKeys.cpp
    Slayer.cpp
    Spider.cpp
    Splitter.cpp
    Stalwart.cpp
    Swordsman.cpp
    TarBaby.cpp
    TarMother.cpp
    TotalMapStates.cpp
    WraithWing.cpp
    Wubba.cpp
    Zombie.cpp
)

# Create static library
add_library(DRODLib STATIC ${DRODLib_SOURCES})

# Include directories
target_include_directories(DRODLib PUBLIC .)
target_include_directories(DRODLib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../BackEndLib)
target_include_directories(DRODLib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../FrontEndLib)
target_include_directories(DRODLib PRIVATE ${SDL2_INCLUDE_DIRS})

# Link libraries
target_link_libraries(DRODLib BackEndLib FrontEndLib)

# Make sure drod-revision.h is generated before building DRODLib
add_dependencies(DRODLib update_revision)
