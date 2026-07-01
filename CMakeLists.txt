cmake_minimum_required(VERSION 3.22)

project(NAMCustomWin VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN _USE_MATH_DEFINES)
    # MinGW on case-sensitive filesystems: Steinberg VST3 SDK includes <Windows.h>
    # (capitalized), but mingw-w64 ships only <windows.h>. Provide a shim dir first
    # on the include path.
    include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/compat)
endif()

# --- Shared DSP static lib -------------------------------------------------
add_subdirectory(deps/NeuralAudio NeuralAudio)

set(NAM_DSP_SOURCES
    src/dsp/Biquad.cpp
    src/dsp/EQ.cpp
    src/dsp/DepthFilter.cpp
    src/dsp/IRConvolver.cpp
    deps/FFTConvolver/AudioFFT.cpp
    deps/FFTConvolver/FFTConvolver.cpp
    deps/FFTConvolver/TwoStageFFTConvolver.cpp
    deps/FFTConvolver/Utilities.cpp)

add_library(nam_dsp STATIC ${NAM_DSP_SOURCES})
target_include_directories(nam_dsp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/FFTConvolver)
set_target_properties(nam_dsp PROPERTIES
    CXX_STANDARD 17
    POSITION_INDEPENDENT_CODE ON)

# --- JUCE + plugin ---------------------------------------------------------
add_subdirectory(juce)
