cmake_minimum_required(VERSION 3.16)
project(gamepad-tester VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(GAMEPAD_TESTER_BUNDLED_SDL "Download SDL3 and link it statically instead of using the system library" OFF)
option(GAMEPAD_TESTER_WERROR "Treat compiler warnings as errors" OFF)

if(GAMEPAD_TESTER_BUNDLED_SDL)
  # For distributions without SDL3 >= 3.4 and for the portable release builds.
  # The archive is pinned by its SHA-256 checksum (the one published on the SDL release page).
  include(FetchContent)
  if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
  endif()
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  set(SDL_TESTS OFF CACHE BOOL "" FORCE)
  set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
  set(SDL_AUDIO OFF CACHE BOOL "" FORCE)
  set(SDL_CAMERA OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(SDL3
    URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.20/SDL3-3.4.20.tar.gz
    URL_HASH SHA256=f3bb06f55bed9557227a700af2ea62e49497ebdf58037732a731a5fb3ef8581f
  )
  FetchContent_MakeAvailable(SDL3)
  set(SDL_LIBRARY SDL3::SDL3-static)
else()
  find_package(SDL3 3.4 REQUIRED CONFIG COMPONENTS SDL3-shared)
  set(SDL_LIBRARY SDL3::SDL3)
endif()

add_executable(gamepad-tester
  src/main.c
  src/ui.c
  src/eventlog.c
  src/pads.c
  src/padinfo.c
  src/input.c
  src/pollrate.c
  src/trail.c
  src/devtools.c
  src/tabs/info.c
  src/tabs/buttons.c
  src/tabs/sticks.c
  src/tabs/drift.c
  src/tabs/rumble.c
  src/tabs/gyro.c
  src/tabs/extras.c
)
target_include_directories(gamepad-tester PRIVATE src)
target_compile_definitions(gamepad-tester PRIVATE GAMEPAD_TESTER_VERSION="${PROJECT_VERSION}")
target_link_libraries(gamepad-tester PRIVATE ${SDL_LIBRARY} m)
target_compile_options(gamepad-tester PRIVATE -Wall -Wextra)
if(GAMEPAD_TESTER_WERROR)
  target_compile_options(gamepad-tester PRIVATE -Werror)
endif()

include(GNUInstallDirs)
install(TARGETS gamepad-tester RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES data/gamepad-tester.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
