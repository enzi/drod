cmake_minimum_required(VERSION 3.16)

# Source files
set(FrontEndLib_SOURCES
    AnimatedTileEffect.cpp
    BitmapManager.cpp
    Bolt.cpp
    BumpObstacleEffect.cpp
    ButtonWidget.cpp
    Colors.cpp
    DialogWidget.cpp
    Effect.cpp
    EffectList.cpp
    EventHandlerWidget.cpp
    ExpandingTextEffect.cpp
    Fade.cpp
    FadeTileEffect.cpp
    FileDialogWidget.cpp
    FlashMessageEffect.cpp
    FlashShadeEffect.cpp
    FloatEffect.cpp
    FloatTextEffect.cpp
    FocusWidget.cpp
    FontManager.cpp
    FrameRateEffect.cpp
    FrameWidget.cpp
    HTMLWidget.cpp
    HyperLinkWidget.cpp
    ImageWidget.cpp
    Inset.cpp
    JpegHandler.cpp
    KeypressDialogWidget.cpp
    LabelWidget.cpp
    ListBoxWidget.cpp
    MarqueeWidget.cpp
    MenuWidget.cpp
    MovingTileEffect.cpp
    ObjectMenuWidget.cpp
    OptionButtonWidget.cpp
    Outline.cpp
    Pan.cpp
    PNGHandler.cpp
    ProgressBarWidget.cpp
    RotateTileEffect.cpp
    ScalerWidget.cpp
    ScaleTileEffect.cpp
    Screen.cpp
    ScreenManager.cpp
    ScrollableWidget.cpp
    ScrollingTextWidget.cpp
    ShadeEffect.cpp
    SliderWidget.cpp
    Sound.cpp
    SubtitleEffect.cpp
    TabbedMenuWidget.cpp
    TextBox2DWidget.cpp
    TextBoxWidget.cpp
    TextEffect.cpp
    TheoraPlayer.cpp
    TilesWidget.cpp
    ToolTipEffect.cpp
    TransTileEffect.cpp
    Widget.cpp
)

# Create static library
add_library(FrontEndLib STATIC ${FrontEndLib_SOURCES})

# Include directories
target_include_directories(FrontEndLib PUBLIC .)
target_include_directories(FrontEndLib PRIVATE ${SDL2_INCLUDE_DIRS})
target_include_directories(FrontEndLib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../BackEndLib)

# Link libraries
target_link_libraries(FrontEndLib BackEndLib ${SDL2_LIBRARIES})
