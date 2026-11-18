cmake_minimum_required(VERSION 3.16)
project(gamepad-tester C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(SDL3 3.4 REQUIRED CONFIG COMPONENTS SDL3-shared)

add_executable(gamepad-tester
  src/main.c
  src/ui.c
  src/eventlog.c
  src/pads.c
  src/padinfo.c
  src/input.c
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
target_link_libraries(gamepad-tester PRIVATE SDL3::SDL3 m)
target_compile_options(gamepad-tester PRIVATE -Wall -Wextra)
