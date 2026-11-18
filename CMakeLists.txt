cmake_minimum_required(VERSION 3.22)
project(FxSoundPlugin VERSION 0.0.5 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Point to central JUCE 9.0.5 installation
list(APPEND CMAKE_PREFIX_PATH "C:/JUCE" "C:/JUCE/lib/cmake/JUCE-9.0.5")

# Find system-installed JUCE 9.0.5 (installed in C:/JUCE)
find_package(JUCE CONFIG REQUIRED)

# Load unofficial CLAP plugin extensions for JUCE
add_subdirectory(libs/clap-juce-extensions EXCLUDE_FROM_ALL)

# Add JUCE Plugin Target (VST3 + Standalone)
juce_add_plugin(FxSound
    COMPANY_NAME "ArmoniaPush"
    PLUGIN_MANUFACTURER_CODE "ArmP"
    PLUGIN_CODE "FxSd"
    FORMATS VST3 Standalone
    PRODUCT_NAME "FxSound"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(FxSound PRIVATE
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp
)

target_include_directories(FxSound PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/submodule/dsp/include
    ${CMAKE_CURRENT_SOURCE_DIR}/submodule/dsp/ptutil/include
    ${CMAKE_CURRENT_SOURCE_DIR}/submodule/audiopassthru/include
)

target_compile_definitions(FxSound PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    PT_NON_MFC
    DSPSOFT_TARGET
    PT_DSP_BUILD=PT_DSP_DFX
)

target_link_libraries(FxSound PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/submodule/lib/Release-x64/DfxDsp.lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/submodule/lib/Release-x64/audiopassthru.lib"
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# Enable CLAP format build target (FxSound_CLAP)
clap_juce_extensions_plugin(TARGET FxSound
    CLAP_ID "com.armoniapush.fxsound"
    CLAP_FEATURES audio-effect equalizer
)

# Automatic Post-Build Deployment to System Plugin Directories
add_custom_command(TARGET FxSound_VST3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:FxSound_VST3>"
        "C:/Program Files/Common Files/VST3/FxSound.vst3"
    COMMENT "Auto-deploying VST3 to C:/Program Files/Common Files/VST3/FxSound.vst3"
)

add_custom_command(TARGET FxSound_CLAP POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:FxSound_CLAP>"
        "C:/Program Files/Common Files/CLAP/FxSound.clap"
    COMMENT "Auto-deploying CLAP to C:/Program Files/Common Files/CLAP/FxSound.clap"
)
