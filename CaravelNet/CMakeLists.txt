cmake_minimum_required(VERSION 3.16)

# Source files
set(CaravelNet_SOURCES
    CaravelNetInterface.cpp
)

# Create static library
add_library(CaravelNet STATIC ${CaravelNet_SOURCES})

# Include directories
target_include_directories(CaravelNet PUBLIC .)
target_include_directories(CaravelNet PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/BackEndLib)
target_include_directories(CaravelNet PRIVATE ${SDL2_INCLUDE_DIRS})

# Link libraries
target_link_libraries(CaravelNet BackEndLib)
