cmake_minimum_required(VERSION 3.20)
project(autopoiesis VERSION 0.1 LANGUAGES CXX)

option(AUTOPOIESIS_BUILD_GUI "Build the native raylib interface" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Threads REQUIRED)

add_library(autopoiesis_core
    src/world.cpp src/decision.cpp src/simulation.cpp src/logger.cpp src/api_budget.cpp
    src/renderer.cpp src/openai_client.cpp src/feature_request.cpp src/validation.cpp
    src/devil.cpp src/calendar.cpp src/ui_model.cpp)
target_include_directories(autopoiesis_core PUBLIC include)
target_link_libraries(autopoiesis_core PUBLIC CURL::libcurl nlohmann_json::nlohmann_json Threads::Threads)
target_compile_options(autopoiesis_core PRIVATE -Wall -Wextra -Wpedantic)

add_executable(autopoiesis src/main.cpp)
target_link_libraries(autopoiesis PRIVATE autopoiesis_core)

if(AUTOPOIESIS_BUILD_GUI)
  find_package(raylib 6.0 QUIET)
  if(NOT raylib_FOUND)
    include(FetchContent)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(raylib
      GIT_REPOSITORY https://github.com/raysan5/raylib.git
      GIT_TAG 6.0
      GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(raylib)
  endif()
  add_library(autopoiesis_raylib_ui src/raylib_interface.cpp)
  target_link_libraries(autopoiesis_raylib_ui PUBLIC autopoiesis_core raylib)
  target_compile_options(autopoiesis_raylib_ui PRIVATE -Wall -Wextra -Wpedantic)
  add_executable(autopoiesis_gui src/main.cpp)
  target_compile_definitions(autopoiesis_gui PRIVATE AUTOPOIESIS_GUI=1)
  target_link_libraries(autopoiesis_gui PRIVATE autopoiesis_raylib_ui)
  target_compile_options(autopoiesis_gui PRIVATE -Wall -Wextra -Wpedantic)
  add_executable(autopoiesis_validation_cards_preview tests/validation_cards_preview.cpp)
  target_link_libraries(autopoiesis_validation_cards_preview PRIVATE autopoiesis_raylib_ui)
  add_executable(autopoiesis_activity_preview tests/activity_preview.cpp)
  target_link_libraries(autopoiesis_activity_preview PRIVATE autopoiesis_raylib_ui)
endif()

enable_testing()
add_executable(autopoiesis_tests tests/test_core.cpp)
target_link_libraries(autopoiesis_tests PRIVATE autopoiesis_core)
add_test(NAME core COMMAND autopoiesis_tests)
add_executable(autopoiesis_budget_tests tests/budget_test.cpp)
target_link_libraries(autopoiesis_budget_tests PRIVATE autopoiesis_core)
add_test(NAME api_budget COMMAND autopoiesis_budget_tests)
add_executable(autopoiesis_goal_tests tests/test_goal.cpp)
target_link_libraries(autopoiesis_goal_tests PRIVATE autopoiesis_core)
add_test(NAME goal_decision COMMAND autopoiesis_goal_tests)
add_executable(autopoiesis_reporting_tests tests/test_reporting.cpp)
target_link_libraries(autopoiesis_reporting_tests PRIVATE autopoiesis_core)
add_test(NAME reporting_pipeline COMMAND autopoiesis_reporting_tests)
add_executable(autopoiesis_timing_tests tests/test_timing.cpp)
target_link_libraries(autopoiesis_timing_tests PRIVATE autopoiesis_core)
add_test(NAME ai_timing COMMAND autopoiesis_timing_tests)
add_executable(autopoiesis_validation_tests tests/test_validation.cpp)
target_link_libraries(autopoiesis_validation_tests PRIVATE autopoiesis_core)
add_test(NAME human_validation COMMAND autopoiesis_validation_tests)
add_executable(autopoiesis_exploration_tests tests/test_exploration.cpp)
target_link_libraries(autopoiesis_exploration_tests PRIVATE autopoiesis_core)
add_test(NAME obstacle_aware_exploration COMMAND autopoiesis_exploration_tests)
add_executable(autopoiesis_rest_tests tests/test_rest.cpp)
target_link_libraries(autopoiesis_rest_tests PRIVATE autopoiesis_core)
add_test(NAME stationary_rest COMMAND autopoiesis_rest_tests)
add_executable(autopoiesis_vital_needs_tests tests/test_vital_needs.cpp)
target_link_libraries(autopoiesis_vital_needs_tests PRIVATE autopoiesis_core)
add_test(NAME vital_needs_before_exploration COMMAND autopoiesis_vital_needs_tests)
add_executable(autopoiesis_world_complexity_tests tests/test_world_complexity.cpp)
target_link_libraries(autopoiesis_world_complexity_tests PRIVATE autopoiesis_core)
add_test(NAME world_complexity COMMAND autopoiesis_world_complexity_tests)
add_executable(autopoiesis_attribute_tests tests/test_attributes.cpp)
target_link_libraries(autopoiesis_attribute_tests PRIVATE autopoiesis_core)
add_test(NAME character_attributes COMMAND autopoiesis_attribute_tests)
add_executable(autopoiesis_survival_ai_tests tests/test_survival_ai.cpp)
target_link_libraries(autopoiesis_survival_ai_tests PRIVATE autopoiesis_core)
add_test(NAME effective_local_survival_ai COMMAND autopoiesis_survival_ai_tests)
add_executable(autopoiesis_behavior_tests tests/test_behavior.cpp)
target_link_libraries(autopoiesis_behavior_tests PRIVATE autopoiesis_core)
add_test(NAME character_behavior_model COMMAND autopoiesis_behavior_tests)
add_executable(autopoiesis_shelter_tests tests/test_shelter.cpp)
target_link_libraries(autopoiesis_shelter_tests PRIVATE autopoiesis_core)
add_test(NAME incremental_shelter_construction COMMAND autopoiesis_shelter_tests)
add_executable(autopoiesis_prompt_tests tests/test_prompts.cpp)
target_link_libraries(autopoiesis_prompt_tests PRIVATE autopoiesis_core)
add_test(NAME french_ai_prompts COMMAND autopoiesis_prompt_tests)
add_executable(autopoiesis_api_diagnostic_tests tests/test_api_diagnostics.cpp)
target_link_libraries(autopoiesis_api_diagnostic_tests PRIVATE autopoiesis_core)
add_test(NAME api_failure_diagnostics COMMAND autopoiesis_api_diagnostic_tests)
add_executable(autopoiesis_devil_tests tests/test_devil.cpp)
target_link_libraries(autopoiesis_devil_tests PRIVATE autopoiesis_core)
add_test(NAME devil_constraints COMMAND autopoiesis_devil_tests)
add_executable(autopoiesis_calendar_tests tests/test_calendar.cpp)
target_link_libraries(autopoiesis_calendar_tests PRIVATE autopoiesis_core)
add_test(NAME calendar_and_climate COMMAND autopoiesis_calendar_tests)
add_executable(autopoiesis_period_memory_tests tests/test_period_memory.cpp)
target_link_libraries(autopoiesis_period_memory_tests PRIVATE autopoiesis_core)
add_test(NAME persistent_period_memory COMMAND autopoiesis_period_memory_tests)
add_executable(autopoiesis_ui_model_tests tests/test_ui_model.cpp)
target_link_libraries(autopoiesis_ui_model_tests PRIVATE autopoiesis_core)
add_test(NAME graphical_ui_model COMMAND autopoiesis_ui_model_tests)
add_test(NAME reformulation_queue COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/test_reformulation.py)
add_test(NAME docker_verification_ownership COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/test_docker_ownership.py)
add_test(NAME macos_notifications COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/test_macos_notifications.py)
add_test(NAME run_launcher_without_arguments COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/test_run_launcher.py)

foreach(test_target
    autopoiesis_tests
    autopoiesis_budget_tests
    autopoiesis_goal_tests
    autopoiesis_reporting_tests
    autopoiesis_timing_tests
    autopoiesis_validation_tests
    autopoiesis_exploration_tests
    autopoiesis_rest_tests
    autopoiesis_vital_needs_tests
    autopoiesis_world_complexity_tests
    autopoiesis_attribute_tests
    autopoiesis_survival_ai_tests
    autopoiesis_behavior_tests
    autopoiesis_shelter_tests
    autopoiesis_prompt_tests
    autopoiesis_api_diagnostic_tests
    autopoiesis_devil_tests
    autopoiesis_calendar_tests
    autopoiesis_period_memory_tests
    autopoiesis_ui_model_tests)
  target_compile_options(${test_target} PRIVATE -UNDEBUG)
endforeach()
