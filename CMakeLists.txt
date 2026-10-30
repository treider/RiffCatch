cmake_minimum_required(VERSION 3.22)
project(RiffCatch VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.4
  GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

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
  COMPANY_NAME "RiffCatch")

target_sources(RiffCatch PRIVATE
  Source/Main.cpp
  Source/MainComponent.cpp
  Source/audio/AudioEngine.cpp
  Source/project/ProjectIO.cpp
  Source/ui/ChannelStrip.cpp)

target_include_directories(RiffCatch PRIVATE Source)

target_compile_definitions(RiffCatch PRIVATE
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:RiffCatch,JUCE_PRODUCT_NAME>"
  JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}"
  JUCE_ASIO=${RC_ASIO})

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
