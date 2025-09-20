cmake_minimum_required(VERSION 3.16)

# Source files
set(DROD_SOURCES
    BloodEffect.cpp
    BoundingBox.cpp
    BrowserScreen.cpp
    CharacterDialogWidget.cpp
    CharacterOptionsWidget.cpp
    Chat.cpp
    ChatScreen.cpp
    ClockWidget.cpp
    Color.cpp
    CommandListBoxWidget.cpp
    CreditsScreen.cpp
    DamagePreviewEffect.cpp
    DebrisEffect.cpp
    DemoScreen.cpp
    DemosScreen.cpp
    DrodBitmapManager.cpp
    DrodDialogs.cpp
    DrodFileDialogWidget.cpp
    DrodFontManager.cpp
    DrodScreen.cpp
    DrodScreenManager.cpp
    DrodSound.cpp
    EditRoomScreen.cpp
    EditRoomWidget.cpp
    EditSelectScreen.cpp
    EntranceSelectDialogWidget.cpp
    EquipmentDescription.cpp
    ExplosionEffect.cpp
    FaceWidget.cpp
    FiretrapEffect.cpp
    GameScreen.cpp
    HoldSelectScreen.cpp
    IceMeltEffect.cpp
    ImageOverlayEffect.cpp
    KeypressDialogWidget.cpp
    LevelStartScreen.cpp
    Light.cpp
    Main.cpp
    MapWidget.cpp
    ModScreen.cpp
    NewPlayerScreen.cpp
    ParticleExplosionEffect.cpp
    PendingBuildEffect.cpp
    PendingPlotEffect.cpp
    Point.cpp
    PuffExplosionEffect.cpp
    RaindropEffect.cpp
    Rectangle.cpp
    RestoreScreen.cpp
    RoomEffectList.cpp
    RoomScreen.cpp
    RoomWidget.cpp
    Scene.cpp
    SelectMediaDialogWidget.cpp
    SelectPlayerScreen.cpp
    SellScreen.cpp
    SettingsScreen.cpp
    SnowflakeEffect.cpp
    SparkEffect.cpp
    Sphere.cpp
    SplashEffect.cpp
    SteamEffect.cpp
    StrikeOrbEffect.cpp
    SwordsmanSwirlEffect.cpp
    SwordSwingEffect.cpp
    TarStabEffect.cpp
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
    ZombieGazeEffect.cpp
)

# Create executable
add_executable(drodrpg ${DROD_SOURCES})

# Link libraries - static first, then dynamic (like ninjamaker)
set(DROD_LIBRARIES
    DRODLib
    FrontEndLib
    BackEndLib
    # Static libraries (in ninjamaker order)
    SDL2_mixer SDL2_ttf SDL2 theora vorbisfile vorbis ogg
    jpeg mk4 freetype png16 curl ssl crypto expat jsoncpp
    # Dynamic libraries are already in CMAKE_EXE_LINKER_FLAGS
)

# Add CaravelNet conditionally
if(ENABLE_CARAVELNET)
    list(APPEND DROD_LIBRARIES CaravelNet)
endif()

# Add Steam libraries if building Steam version
if(BUILD_STEAM AND STEAM_LIBRARIES)
    list(APPEND DROD_LIBRARIES ${STEAM_LIBRARIES})
endif()

target_link_libraries(drodrpg ${DROD_LIBRARIES})

# Compiler definitions
target_compile_definitions(drodrpg PRIVATE -DUSE_SDL_MIXER)

# Include directories
target_include_directories(DRODLib PUBLIC .)
target_include_directories(DRODLib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../BackEndLib)
target_include_directories(DRODLib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../FrontEndLib)
target_include_directories(drodrpg PRIVATE ${SDL2_INCLUDE_DIRS})

