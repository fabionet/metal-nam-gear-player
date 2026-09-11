# JUCE-based plugin (VST3 + LV2 + Standalone) — wraps existing nam_dsp + NeuralAudio.
# Stage 1: skeleton only — empty processor, host-generic UI.

add_subdirectory(${CMAKE_SOURCE_DIR}/deps/JUCE JUCE)

# JUCE does not officially support MinGW for VST3, and its LV2 manifest helper
# does not build against mingw-w64 headers. When cross-compiling for Windows
# via MinGW, ship only the Standalone .exe.
if(MINGW)
    set(NAMCUSTOM_FORMATS Standalone)
else()
    set(NAMCUSTOM_FORMATS VST3 LV2 Standalone)
endif()

juce_add_plugin(NAMCustom
    VERSION               0.2.0
    PRODUCT_NAME          "NAM Custom"
    COMPANY_NAME          "fabionet"
    BUNDLE_ID             "com.fabionet.namcustom"
    PLUGIN_MANUFACTURER_CODE Fbnt
    PLUGIN_CODE              Nmcu
    FORMATS                  ${NAMCUSTOM_FORMATS}
    LV2URI                   "https://github.com/mikeoliphant/neural-amp-modeler-lv2#namcustom-juce"
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    COPY_PLUGIN_AFTER_BUILD  FALSE
)

target_sources(NAMCustom PRIVATE
    PluginProcessor.cpp
    PluginProcessor.h
    PluginEditor.cpp
    PluginEditor.h
    NAMLookAndFeel.cpp
    NAMLookAndFeel.h
    NAMPipeline.cpp
    NAMPipeline.h
    PreampFX.h
    PresetManager.cpp
    PresetManager.h
    PresetPanelComponent.cpp
    EQAnalyserComponent.cpp
    LCDDisplayComponent.cpp
    PresetPanelComponent.h
    GlobalSettings.cpp
    GlobalSettings.h
    MeterStripComponent.cpp
    MeterStripComponent.h
)

juce_add_binary_data(NAMFonts SOURCES
    fonts/Nosifer-Regular.ttf
    fonts/MetalMania-Regular.ttf
    fonts/PirataOne-Regular.ttf
)

juce_add_binary_data(NAMPresets
    NAMESPACE NAMPresetData
    HEADER_NAME NAMPresetData.h
    SOURCES
        Presets/Factory/Clean.nampreset
        Presets/Factory/Crunch.nampreset
        Presets/Factory/Lead.nampreset
        Presets/Factory/Bass.nampreset
        Presets/Factory/ModernDjent.nampreset
        Presets/Factory/ThrashRhythm.nampreset
        Presets/Factory/DoomSludge.nampreset
        Presets/Factory/BlackMetal.nampreset
        Presets/Factory/DemoClean.nampreset
        Presets/Factory/Default.nampreset
        Presets/Assets/nam/demo_wavenet_a1.nam
)

target_compile_definitions(NAMCustom PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(NAMCustom PRIVATE
    nam_dsp
    NeuralAudio
    NAMFonts
    NAMPresets
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_audio_processors
    PRIVATE
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

target_include_directories(NAMCustom PRIVATE
    ${CMAKE_SOURCE_DIR}/deps/NeuralAudio
    ${CMAKE_SOURCE_DIR}/deps/denormal
)

# Le architetture del core NAM (WaveNet, LSTM, Linear, ConvNet,
# SlimmableContainer) si registrano nel ConfigParserRegistry tramite oggetti
# statici definiti nei rispettivi .cpp. NeuralAudio e' una OBJECT library: i
# suoi oggetti confluiscono nell'archivio SharedCode di JUCE e, quando i target
# finali linkano quell'archivio, il linker scarta i membri di cui nessuno
# referenzia un simbolo. I registratori non girano mai e il caricamento muore
# con "No config parser registered for architecture: WaveNet".
# Aggiungere gli oggetti direttamente ai target finali li rende non scartabili;
# i membri dell'archivio restano inutilizzati perche' i simboli sono gia'
# definiti, quindi non si creano duplicati.
foreach(_fmt ${NAMCUSTOM_FORMATS})
    if(TARGET NAMCustom_${_fmt})
        target_sources(NAMCustom_${_fmt} PRIVATE $<TARGET_OBJECTS:NeuralAudio>)
    endif()
endforeach()
