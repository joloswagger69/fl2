cmake_minimum_required(VERSION 3.22)
project(NeoWaveAudio VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(MSVC)
    add_compile_options(/utf-8 /permissive- /MP)
endif()

# JUCE: standardmaessig per FetchContent. Fuer Offline-/Wiederholungs-Builds kann ein lokaler Checkout
# angegeben werden:  cmake -DNWA_JUCE_SOURCE_DIR=/pfad/zu/JUCE ...
set(NWA_JUCE_SOURCE_DIR "" CACHE PATH "Optionaler lokaler JUCE-Checkout (8.0.4)")
include(FetchContent)
if(NWA_JUCE_SOURCE_DIR)
    set(FETCHCONTENT_SOURCE_DIR_JUCE "${NWA_JUCE_SOURCE_DIR}")
endif()
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

enable_testing()

add_subdirectory(core)
add_subdirectory(plugins/RaveStation)
add_subdirectory(plugins/MelodyStudio)
add_subdirectory(plugins/SpaceFX)
add_subdirectory(plugins/NeoWave)
add_subdirectory(tests)
