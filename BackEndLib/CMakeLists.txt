cmake_minimum_required(VERSION 3.16)

# Source files
set(BackEndLib_SOURCES
    Assert.cpp
    AttachableObject.cpp
    Base64.cpp
    Browser.cpp
    Clipboard.cpp
    Coord.cpp
    Date.cpp
    Dyn.cpp
    Files.cpp
    GameStream.cpp
    Heap.cpp
    IDList.cpp
    IniFile.cpp
    InputKey.cpp
    Internet.cpp
    MessageIDs.cpp
    Metadata.cpp
    Ports.cpp
    PostData.cpp
    StretchyBuffer.cpp
    SysTimer.cpp
    Wchar.cpp
)

# Create static library
add_library(BackEndLib STATIC ${BackEndLib_SOURCES})

# Include directories
target_include_directories(BackEndLib PUBLIC .)
target_include_directories(BackEndLib PRIVATE ${SDL2_INCLUDE_DIRS})

# Link libraries
target_link_libraries(BackEndLib ${SDL2_LIBRARIES})
