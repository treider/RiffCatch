cmake_minimum_required(VERSION 3.22)
project(RiffCatch VERSION 0.33 LANGUAGES C CXX)   # bump the minor version every iteration
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.4
  GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

# LAME (LGPL) MP3 encoder: official source extracted by the user to third_party/lame (not committed).
# LAME has no CMake build, so the static library is described here.
set(LAME_DIR "${CMAKE_SOURCE_DIR}/third_party/lame" CACHE PATH "Path to LAME source (lame-3.100)")
if(EXISTS "${LAME_DIR}/libmp3lame/lame.c" AND EXISTS "${LAME_DIR}/configMS.h")
  set(RC_MP3 1)
  message(STATUS "LAME found: ${LAME_DIR}")
  file(GLOB LAME_SOURCES "${LAME_DIR}/libmp3lame/*.c")
  list(APPEND LAME_SOURCES "${LAME_DIR}/libmp3lame/vector/xmm_quantize_sub.c")
  configure_file("${LAME_DIR}/configMS.h" "${CMAKE_BINARY_DIR}/lame_config/config.h" COPYONLY)
  add_library(mp3lame STATIC ${LAME_SOURCES})
  target_include_directories(mp3lame
    PUBLIC  "${LAME_DIR}/include"
    PRIVATE "${CMAKE_BINARY_DIR}/lame_config" "${LAME_DIR}/libmp3lame" "${LAME_DIR}")
  target_compile_definitions(mp3lame PRIVATE HAVE_CONFIG_H HAVE_XMMINTRIN_H TAKEHIRO_IEEE754_HACK FLOAT8=float REAL_IS_FLOAT=1
    _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE)
  if(MSVC)
    target_compile_options(mp3lame PRIVATE /w)
  endif()
else()
  set(RC_MP3 0)
  message(WARNING "LAME not found at ${LAME_DIR}; building without MP3 export.")
endif()
# ASIO SDK is NOT in the repo (Steinberg license). Default location below.
set(ASIO_SDK_DIR "${CMAKE_SOURCE_DIR}/third_party/asio_sdk" CACHE PATH "Path to Steinberg ASIO SDK")
if(EXISTS "${ASIO_SDK_DIR}/common/iasiodrv.h")
  set(RC_ASIO 1)
  message(STATUS "ASIO SDK found: ${ASIO_SDK_DIR}")
else()
  set(RC_ASIO 0)
  message(WARNING "ASIO SDK not found at ${ASIO_SDK_DIR}; building without ASIO (WASAPI only).")
endif()

juce_add_gui_app(RiffCatch
  PRODUCT_NAME "RiffCatch"
  COMPANY_NAME "RiffCatch"
  ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png"
  ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png")

target_sources(RiffCatch PRIVATE
  Source/Main.cpp
  Source/MainComponent.cpp
  Source/WinTitleBar.cpp
  Source/audio/AudioEngine.cpp
  Source/audio/HitEditor.cpp
  Source/project/ProjectIO.cpp
  Source/project/Exporter.cpp
  Source/ui/TrackRow.cpp
  Source/ui/DrumPanel.cpp
  Source/ui/MixerPanel.cpp)

target_include_directories(RiffCatch PRIVATE Source)

target_compile_definitions(RiffCatch PRIVATE
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:RiffCatch,JUCE_PRODUCT_NAME>"
  JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}"
  JUCE_ASIO=${RC_ASIO}
  RC_MP3=${RC_MP3})

if(RC_MP3)
  target_link_libraries(RiffCatch PRIVATE mp3lame)
endif()

if(RC_ASIO)
  target_include_directories(RiffCatch PRIVATE
    "${ASIO_SDK_DIR}/common" "${ASIO_SDK_DIR}/host" "${ASIO_SDK_DIR}/host/pc")
endif()

target_link_libraries(RiffCatch PRIVATE
  juce::juce_audio_utils
  juce::juce_audio_devices
  juce::juce_audio_formats
  juce::juce_recommended_config_flags
  juce::juce_recommended_warning_flags)
