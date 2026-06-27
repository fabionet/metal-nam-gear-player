if (MSVC)
	add_compile_options(
		"$<$<CONFIG:DEBUG>:/W4>"
		"$<$<CONFIG:RELEASE>:/O2>"
	)
else()
	add_compile_options(
		-Wall 
		# -Wpedantic -Wextra -Wstrict-aliasing -Wunreachable-code -Weffc++ -Wno-unused-parameter
		"$<$<CONFIG:DEBUG>:-Og;-ggdb>"
		"$<$<CONFIG:RELWITHDEBINFO>:-Ofast>"
		"$<$<CONFIG:RELEASE>:-Ofast>"
	)
endif()


if (CMAKE_SYSTEM_PROCESSOR MATCHES "(amd64)|(AMD64)|(x86_64)")
	option(USE_NATIVE_ARCH "Enable architecture-specific optimizations" OFF)

	if (USE_NATIVE_ARCH)
		if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
			add_compile_options(/arch:AVX2)
			message(STATUS "Enabling /arch:AVX2")
		 
		else()
			add_compile_options(-march=x86-64-v3)
			message(STATUS "Enabling -march=x86-64-v3")
		endif()
	endif (USE_NATIVE_ARCH)
endif ()

add_subdirectory(../deps/NeuralAudio NeuralAudio)

# --- nam_dsp: local DSP static lib (Biquad EQ, Depth, IR convolver) ---
set(NAM_DSP_SOURCES
	dsp/Biquad.cpp
	dsp/Biquad.h
	dsp/EQ.cpp
	dsp/EQ.h
	dsp/DepthFilter.cpp
	dsp/DepthFilter.h
	dsp/IRConvolver.cpp
	dsp/IRConvolver.h
	dsp/dr_wav.h
	# FFTConvolver bundled sources (no CMakeLists upstream)
	../deps/FFTConvolver/AudioFFT.cpp
	../deps/FFTConvolver/FFTConvolver.cpp
	../deps/FFTConvolver/TwoStageFFTConvolver.cpp
	../deps/FFTConvolver/Utilities.cpp)

add_library(nam_dsp STATIC ${NAM_DSP_SOURCES})
target_include_directories(nam_dsp PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/dsp
	${CMAKE_CURRENT_SOURCE_DIR}/../deps/FFTConvolver)
set_target_properties(nam_dsp PROPERTIES
	CXX_STANDARD 17
	POSITION_INDEPENDENT_CODE ON)

set(SOURCES nam_lv2.cpp
	nam_plugin.cpp
	nam_plugin.h)

set(NA_SOURCES ../deps/NeuralAudio/NeuralAudio/NeuralModel.h)

add_library(neural_amp_modeler SHARED ${SOURCES} ${NA_SOURCES})

target_include_directories(neural_amp_modeler PUBLIC ../deps/NeuralAudio)
target_include_directories(neural_amp_modeler PUBLIC ../deps/lv2/include)
target_include_directories(neural_amp_modeler PUBLIC ../deps/denormal)

target_link_libraries(neural_amp_modeler PRIVATE NeuralAudio nam_dsp)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES})
source_group(NAM ${CMAKE_CURRENT_SOURCE_DIR} FILES ${NA_SOURCES})

option(DISABLE_DENORMALS "Disable floating point denormals" ON)

if (DISABLE_DENORMALS)
	add_definitions(-DDISABLE_DENORMALS)
endif (DISABLE_DENORMALS)

option(SMART_BYPASS_ENABLED "Enable auto-bypass on silence" OFF)

if (SMART_BYPASS_ENABLED)
	add_definitions(-DSMART_BYPASS_ENABLED)
	message(STATUS "Smart Bypass enabled")
else()
	message(STATUS "Smart Bypass NOT enabled")
endif (SMART_BYPASS_ENABLED)

set_target_properties(neural_amp_modeler
	PROPERTIES
	CXX_VISIBILITY_PRESET hidden
	INTERPROCEDURAL_OPTIMIZATION TRUE
	PREFIX ""
)

# Platform

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	target_compile_definitions(neural_amp_modeler PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
