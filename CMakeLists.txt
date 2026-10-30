cmake_minimum_required(VERSION 3.22)
project(Giro VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: use a local checkout if given (-DGIRO_JUCE_DIR=/path/to/JUCE), otherwise fetch 8.0.4 from GitHub.
if(GIRO_JUCE_DIR)
  add_subdirectory(${GIRO_JUCE_DIR} juce)
else()
  include(FetchContent)
  FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4 GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(JUCE)
endif()

set(GIRO_CORE core/theory.cpp core/chart.cpp core/midi.cpp core/patterns.cpp core/ireal.cpp)

# --- tests (plain C++, no JUCE)
add_executable(giro_tests tests/test_core.cpp ${GIRO_CORE})
enable_testing()
add_test(NAME core COMMAND giro_tests)
add_executable(giro_ireal_tests tests/test_ireal.cpp ${GIRO_CORE})
add_test(NAME ireal COMMAND giro_ireal_tests WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)

# --- the app
juce_add_gui_app(Giro
  PRODUCT_NAME "Giro"
  COMPANY_NAME "Massimo Danieli"
  BUNDLE_ID com.massimodanieli.giro
  ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/app/icon.png"
  PLUGINHOST_AU TRUE
  MICROPHONE_PERMISSION_ENABLED FALSE
  DOCUMENT_EXTENSIONS "mid;midi")

target_sources(Giro PRIVATE
  app/Main.cpp app/MainComponent.cpp app/ChartComponent.cpp app/Engine.cpp app/Library.cpp
  ${GIRO_CORE})

target_compile_definitions(Giro PRIVATE
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
  JUCE_PLUGINHOST_AU=1 JUCE_PLUGINHOST_VST3=0 JUCE_PLUGINHOST_LADSPA=0
  JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:Giro,JUCE_PRODUCT_NAME>"
  JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:Giro,JUCE_VERSION>"
  JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Giro PRIVATE
  juce::juce_gui_extra juce::juce_audio_utils juce::juce_audio_processors
  juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# --- engine test (offline render, no audio device)
juce_add_console_app(giro_engine_test PRODUCT_NAME "giro_engine_test")
target_sources(giro_engine_test PRIVATE tests/test_engine.cpp app/Engine.cpp ${GIRO_CORE})
target_compile_definitions(giro_engine_test PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_PLUGINHOST_AU=1 JUCE_PLUGINHOST_VST3=0 JUCE_PLUGINHOST_LADSPA=0)
target_link_libraries(giro_engine_test PRIVATE juce::juce_audio_utils juce::juce_audio_processors juce::juce_recommended_config_flags)
add_test(NAME engine COMMAND giro_engine_test)
