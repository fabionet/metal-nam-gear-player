cmake_minimum_required(VERSION 3.10)

project(NeuralAmpModelerLv2 VERSION 0.2.1)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
set(LIB_INSTALL_DIR ${CMAKE_INSTALL_PREFIX}/lib CACHE PATH "The library install dir (default: lib)")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED OFF)
set(CMAKE_CXX_EXTENSIONS OFF)

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	include_directories(SYSTEM /usr/local/include)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
	link_libraries( "$<$<AND:$<CXX_COMPILER_ID:GNU>,$<VERSION_LESS:$<CXX_COMPILER_VERSION>,9.0>>:-lstdc++fs>" )
elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
else()
	message(FATAL_ERROR "Unrecognized Platform!")
endif()

set(NAM_LV2_ID http://github.com/mikeoliphant/neural-amp-modeler-lv2)

option(NAM_BUILD_LV2_LEGACY "Build the original hand-written LV2 plugin" ON)
option(NAM_BUILD_JUCE       "Build the JUCE-based plugin (VST3/LV2/Standalone)" ON)

# --- Shared deps (NeuralAudio + nam_dsp static lib) used by both targets ---
add_subdirectory(deps/NeuralAudio NeuralAudio)

set(NAM_DSP_SOURCES
	src/dsp/Biquad.cpp
	src/dsp/Biquad.h
	src/dsp/EQ.cpp
	src/dsp/EQ.h
	src/dsp/DepthFilter.cpp
	src/dsp/DepthFilter.h
	src/dsp/IRConvolver.cpp
	src/dsp/IRConvolver.h
	src/dsp/dr_wav.h
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

if(NAM_BUILD_LV2_LEGACY)
	add_subdirectory(src)

	# create neural_amp_modeler.lv2
	add_custom_target(copy_binaries ALL
		${CMAKE_COMMAND} -E copy "$<TARGET_FILE:neural_amp_modeler>" neural_amp_modeler.lv2/
		DEPENDS neural_amp_modeler
	)

	file(COPY resources/modgui.ttl DESTINATION neural_amp_modeler.lv2)
	file(COPY resources/modgui DESTINATION neural_amp_modeler.lv2)

	configure_file(resources/manifest.ttl.in neural_amp_modeler.lv2/manifest.ttl)
	configure_file(resources/neural_amp_modeler.ttl.in neural_amp_modeler.lv2/neural_amp_modeler.ttl)

	install (DIRECTORY ${CMAKE_BINARY_DIR}/neural_amp_modeler.lv2
		DESTINATION ${LIB_INSTALL_DIR}/lv2
	)
endif()

if(NAM_BUILD_JUCE)
	add_subdirectory(juce)
endif()

set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "oliphant@nostatic.org>")
set(CPACK_PACKAGE_VENDOR "Mike Oliphant")
set(CPACK_PACKAGE_DESCRIPTION "Neural Amp Modeler LV2 plugin implementation")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Neural Amp Modeler LV2 plugin")
set(CPACK_DEBIAN_PACKAGE_SECTION sound)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION TRUE)
set(CPACK_PACKAGING_INSTALL_PREFIX /usr)
set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set (CPACK_STRIP_FILES ON)

include(CPack)
