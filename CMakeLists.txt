cmake_minimum_required(VERSION 3.22)

# Mac: Apple 실리콘 + Intel 모두 지원, macOS 11 이상
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
endif()

project(ChordLink VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(CL_BUILD_CLAP "CLAP 형식도 빌드" ON)
option(CL_COPY_PLUGINS "빌드 후 플러그인 폴더에 자동 복사 (Mac)" ON)

include(FetchContent)
# JUCE: -DJUCE_PATH=... 로 로컬 경로를 줄 수 있고, 없으면 자동으로 받음
if(DEFINED JUCE_PATH)
    add_subdirectory(${JUCE_PATH} JUCE)
else()
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4 GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

if(CL_BUILD_CLAP)
    if(DEFINED CLAP_JUCE_PATH)
        add_subdirectory(${CLAP_JUCE_PATH} clap-juce-extensions EXCLUDE_FROM_ALL)
    else()
        FetchContent_Declare(clap-juce-extensions
            GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
            GIT_TAG main GIT_SHALLOW ON GIT_SUBMODULES_RECURSE ON)
        FetchContent_MakeAvailable(clap-juce-extensions)
    endif()
endif()

set(CL_SOURCES
    Source/ChordEngine.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

function(cl_add_plugin target product code instrument)
    if(instrument)
        set(fmts VST3 Standalone)
        set(vst3cat Instrument Synth)
    else()
        set(fmts VST3)
        set(vst3cat Fx)
    endif()
    if(APPLE)
        list(APPEND fmts AU)
    endif()
    if(APPLE AND CL_COPY_PLUGINS)
        set(copy TRUE)
    else()
        set(copy FALSE)
    endif()
    if(instrument)
        set(midifx FALSE)
    else()
        set(midifx TRUE)
    endif()

    juce_add_plugin(${target}
        COMPANY_NAME "Chord Link"
        COMPANY_WEBSITE "https://example.com/chordlink"
        PRODUCT_NAME "${product}"
        BUNDLE_ID "com.chordlink.${code}"
        PLUGIN_MANUFACTURER_CODE Chlk
        PLUGIN_CODE ${code}
        VERSION ${PROJECT_VERSION}
        IS_SYNTH ${instrument}
        IS_MIDI_EFFECT ${midifx}
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT TRUE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD ${copy}
        VST3_CATEGORIES ${vst3cat}
        FORMATS ${fmts})

    target_sources(${target} PRIVATE ${CL_SOURCES})
    target_compile_definitions(${target} PUBLIC
        CL_INSTRUMENT=$<IF:$<BOOL:${instrument}>,1,0>
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)
    if(UNIX AND NOT APPLE)
        # Linux 테스트 빌드용 (화면 확장 라이브러리·ALSA 없이)
        target_compile_definitions(${target} PUBLIC JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_USE_XSHM=0 JUCE_ALSA=0 JUCE_JACK=0)
    endif()
    target_link_libraries(${target}
        PRIVATE juce::juce_audio_utils
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

    if(CL_BUILD_CLAP)
        if(instrument)
            clap_juce_extensions_plugin(TARGET ${target} CLAP_ID "com.chordlink.${code}" CLAP_FEATURES instrument note-effect)
        else()
            clap_juce_extensions_plugin(TARGET ${target} CLAP_ID "com.chordlink.${code}" CLAP_FEATURES note-effect)
        endif()
    endif()
endfunction()

# 1) 악기형: 내장 소리 + MIDI 출력 (Ableton Live, Logic 악기 슬롯)
cl_add_plugin(ChordLink "Chord Link" CLin TRUE)
# 2) MIDI FX: Logic 의 MIDI FX 슬롯용 (소리 없음, MIDI 만 출력)
cl_add_plugin(ChordLinkFX "Chord Link MIDI FX" CLfx FALSE)

# 개발용: 화면 캡처 테스트 (-DCL_BUILD_TESTS=ON)
option(CL_BUILD_TESTS "개발용 테스트 프로그램" OFF)
if(CL_BUILD_TESTS)
    juce_add_console_app(ClEditorShot PRODUCT_NAME "ClEditorShot")
    target_sources(ClEditorShot PRIVATE tests/editor_shot.cpp)
    target_link_libraries(ClEditorShot PRIVATE ChordLink juce::juce_audio_utils juce::juce_recommended_config_flags)
endif()
