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

add_executable(gravityships src/main.cpp src/ship.cpp src/sound.cpp)
target_include_directories(gravityships PRIVATE src)
target_include_directories(gravityships SYSTEM PRIVATE third_party)
target_compile_definitions(gravityships PRIVATE GL_SILENCE_DEPRECATION)
target_link_libraries(gravityships PRIVATE PkgConfig::GLFW OpenGL::GL m ${CMAKE_DL_LIBS} Threads::Threads)

add_custom_command(TARGET gravityships POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:gravityships>/assets)
