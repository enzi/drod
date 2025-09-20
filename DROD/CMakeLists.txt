cmake_minimum_required(VERSION 3.16)

# Source files
set(DROD_SOURCES
    AumtlichGazeEffect.cpp
    BloodEffect.cpp
    BoundingBox.cpp
    BrowserScreen.cpp
    CharacterDialogWidget.cpp
    CharacterOptionsWidget.cpp
    Chat.cpp
    ChatScreen.cpp
    CheckpointEffect.cpp
    ClockWidget.cpp
    Color.cpp
    CreditsScreen.cpp
    DebrisEffect.cpp
    DemoScreen.cpp
    DemosScreen.cpp
    DrodBitmapManager.cpp
    DrodFileDialogWidget.cpp
    DrodFontManager.cpp
    DrodScreen.cpp
    DrodScreenManager.cpp
    DrodSound.cpp
    EditRoomScreen.cpp
    EditRoomWidget.cpp
    EditSelectScreen.cpp
    EntranceSelectDialogWidget.cpp
    EvilEyeGazeEffect.cpp
    ExplosionEffect.cpp
    FaceWidget.cpp
    FiretrapEffect.cpp
    GameScreen.cpp
    GridEffect.cpp
    HoldSelectScreen.cpp
    IceMeltEffect.cpp
    ImageOverlayEffect.cpp
    LevelStartScreen.cpp
    Light.cpp
    Main.cpp
    MapWidget.cpp
    ModScreen.cpp
    NewPlayerScreen.cpp
    NoticeEffect.cpp
    ParticleExplosionEffect.cpp
    PendingBuildEffect.cpp
    PendingPlotEffect.cpp
    Point.cpp
    PuffExplosionEffect.cpp
    RaindropEffect.cpp
    Rectangle.cpp
    RestoreScreen.cpp
    RoomDrawStatsEffect.cpp
    RoomEffectList.cpp
    RoomScreen.cpp
    RoomWidget.cpp
    Scene.cpp
    SelectPlayerScreen.cpp
    SellScreen.cpp
    SellScreenTCB.cpp
    SettingsScreen.cpp
    SnowflakeEffect.cpp
    SparkEffect.cpp
    Sphere.cpp
    SpikeEffect.cpp
    SplashEffect.cpp
    SteamEffect.cpp
    StrikeOrbEffect.cpp
    StunEffect.cpp
    SwordsmanSwirlEffect.cpp
    SwordSwingEffect.cpp
    TarStabEffect.cpp
    TemporalMoveEffect.cpp
    TileImageCalcs.cpp
    TitleScreen.cpp
    TrapdoorFallEffect.cpp
    VarMonitorEffect.cpp
    VerminEffect.cpp
    WadeEffect.cpp
    WeatherDialogWidget.cpp
    WinStartScreen.cpp
    WorldMapScreen.cpp
    WorldMapWidget.cpp
)

# Create executable
add_executable(drod ${DROD_SOURCES})

# Link libraries - static first, then dynamic (like ninjamaker)
set(DROD_LIBRARIES
    DRODLib
    FrontEndLib
    BackEndLib
    # Static libraries (in ninjamaker order)
    SDL2_mixer SDL2_ttf SDL2 theora vorbisfile vorbis ogg
    jpeg mk4 freetype png16 curl ssl crypto expat
    # Dynamic libraries are already in CMAKE_EXE_LINKER_FLAGS
)

# Add CaravelNet conditionally
if(ENABLE_CARAVELNET)
    list(APPEND DROD_LIBRARIES CaravelNet)
    # jsoncpp must come after CaravelNet since CaravelNet depends on it
    list(APPEND DROD_LIBRARIES jsoncpp)
endif()

# Add Steam libraries if building Steam version
if(BUILD_STEAM AND STEAM_LIBRARIES)
    list(APPEND DROD_LIBRARIES ${STEAM_LIBRARIES})
endif()

target_link_libraries(drod ${DROD_LIBRARIES})

# Compiler definitions
target_compile_definitions(drod PRIVATE -DUSE_SDL_MIXER)

# Include directories
target_include_directories(DRODLib PUBLIC .)
target_include_directories(DRODLib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/BackEndLib)
target_include_directories(DRODLib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/FrontEndLib)
target_include_directories(drod PRIVATE ${SDL2_INCLUDE_DIRS})

