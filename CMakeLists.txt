cmake_minimum_required(VERSION 3.20)
project(GravityShips CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(OpenGL_GL_PREFERENCE GLVND)
find_package(OpenGL REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(GLFW REQUIRED IMPORTED_TARGET glfw3)

add_library(gravityships_core STATIC src/ship.cpp src/sound.cpp src/gameplay.cpp src/ai.cpp src/vectorfont.cpp)
target_include_directories(gravityships_core PUBLIC src)
target_include_directories(gravityships_core SYSTEM PUBLIC third_party)
target_compile_definitions(gravityships_core PUBLIC GL_SILENCE_DEPRECATION)
target_link_libraries(gravityships_core PUBLIC PkgConfig::GLFW OpenGL::GL m ${CMAKE_DL_LIBS} Threads::Threads)
add_executable(gravityships src/main.cpp)
target_link_libraries(gravityships PRIVATE gravityships_core)

include(CTest)
if(BUILD_TESTING)
  add_executable(ai_tests tests/ai_tests.cpp)
  target_link_libraries(ai_tests PRIVATE gravityships_core)
  add_test(NAME ai_unit COMMAND ai_tests)
  set_tests_properties(ai_unit PROPERTIES LABELS unit)
  add_executable(ai_scenarios tests/ai_scenarios.cpp)
  target_link_libraries(ai_scenarios PRIVATE gravityships_core)
  add_test(NAME ai_scenarios COMMAND ai_scenarios)
  set_tests_properties(ai_scenarios PROPERTIES LABELS graphics TIMEOUT 300)
endif()

add_custom_command(TARGET gravityships POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:gravityships>/assets)
