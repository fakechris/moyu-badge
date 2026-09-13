# sim/CMakeLists.txt —— host SDL simulator (cross-platform: macOS / Linux / Windows).
cmake_minimum_required(VERSION 3.16)
project(deskpet-sim C)

set(CMAKE_C_STANDARD 11)

# Find SDL2: try CMake Config first (vcpkg / official SDL2Config), then fallback to sdl2-config / pkg-config
find_package(SDL2 CONFIG QUIET)
if(NOT SDL2_FOUND)
    find_package(SDL2 QUIET)
endif()

if(NOT SDL2_FOUND AND NOT TARGET SDL2::SDL2 AND NOT TARGET SDL2::SDL2-static)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(SDL2 QUIET sdl2)
    endif()
endif()

if(NOT SDL2_FOUND AND NOT TARGET SDL2::SDL2 AND NOT TARGET SDL2::SDL2-static AND NOT SDL2_LIBRARIES)
    execute_process(COMMAND sdl2-config --cflags OUTPUT_VARIABLE SDL_CFLAGS
                    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    execute_process(COMMAND sdl2-config --libs OUTPUT_VARIABLE SDL_LIBS
                    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(SDL_CFLAGS)
        separate_arguments(SDL_CFLAGS_LIST UNIX_COMMAND "${SDL_CFLAGS}")
    endif()
    if(SDL_LIBS)
        separate_arguments(SDL_LIBS_LIST UNIX_COMMAND "${SDL_LIBS}")
    endif()
endif()

file(GLOB_RECURSE LVGL_SRCS ${CMAKE_SOURCE_DIR}/third_party/lvgl/src/*.c)

add_executable(deskpet-sim
    main.c
    stubs.c
    upstream/ui_pixel.c
    ../main/app_main.c
    ../main/pet_model.c
    ../main/dungeon_model.c
    ../main/pomo_lite.c
    ../main/deskpet_i18n.c
    ../main/deskpet_store.c
    ../main/demo_deskpet.c
    ../main/deskpet_time.c
    ../main/pet_sleep.c
    ../main/serial_time.c
    ../main/app_shell.c
    ../main/hud_battery.c
    ../main/mode_pomo.c
    ../main/mode_clicker.c
    ../main/vocab_model.c
    ../main/vocab_ui.c
    ../main/mode_vocab.c
    ../main/mode_standby.c
    ../main/mode_settings.c
    ../main/clicker_hid.c
    ../main/agent_status.c
    ../main/sprite.c
    ../main/sprite_anim.c
    ../main/chiptune.c
    ../main/chiptune_songs.c
    ../main/audio_task.c
    ../main/fonts/zh_subset.c
    ../main/fonts/zh_ipa_fix.c
    ${LVGL_SRCS}
)

target_compile_definitions(deskpet-sim PRIVATE LV_CONF_INCLUDE_SIMPLE)

# Windows / MSVC compatibility
if(MSVC)
    target_compile_definitions(deskpet-sim PRIVATE _CRT_SECURE_NO_WARNINGS)
    # Avoid nested quotes: Ninja+MSVC would pass /FI"path" as a literal filename.
    target_compile_options(deskpet-sim PRIVATE
        "/FI${CMAKE_SOURCE_DIR}/stubs/win32_compat.h"
        /utf-8
        /experimental:c11atomics)
elseif(WIN32)
    target_compile_options(deskpet-sim PRIVATE -include "${CMAKE_SOURCE_DIR}/stubs/win32_compat.h")
endif()

# Same wall-clock seed as firmware: refresh build_time.h on every sim build.
add_custom_target(deskpet_build_time
    COMMAND ${CMAKE_COMMAND} -DOUT=${CMAKE_BINARY_DIR}/build_time.h
            -P ${CMAKE_SOURCE_DIR}/../tools/gen_build_time.cmake)
add_dependencies(deskpet-sim deskpet_build_time)

if(SDL_CFLAGS_LIST)
    target_compile_options(deskpet-sim PRIVATE ${SDL_CFLAGS_LIST})
endif()
if(SDL2_INCLUDE_DIRS)
    target_include_directories(deskpet-sim PRIVATE ${SDL2_INCLUDE_DIRS})
endif()

if(NOT MSVC)
    target_compile_options(deskpet-sim PRIVATE -Wno-unused-function)
endif()

target_include_directories(deskpet-sim PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/stubs
    ${CMAKE_SOURCE_DIR}/upstream
    ${CMAKE_SOURCE_DIR}/../main
    ${CMAKE_SOURCE_DIR}/third_party/lvgl
    ${CMAKE_SOURCE_DIR}/third_party
)

if(APPLE)
    target_include_directories(deskpet-sim PRIVATE /opt/homebrew/include)
endif()

# Link SDL2 and math/system libraries
if(TARGET SDL2::SDL2)
    target_link_libraries(deskpet-sim PRIVATE SDL2::SDL2)
elseif(TARGET SDL2::SDL2-static)
    target_link_libraries(deskpet-sim PRIVATE SDL2::SDL2-static)
elseif(SDL2_LIBRARIES)
    target_link_libraries(deskpet-sim PRIVATE ${SDL2_LIBRARIES})
elseif(SDL_LIBS_LIST)
    target_link_libraries(deskpet-sim PRIVATE ${SDL_LIBS_LIST})
else()
    target_link_libraries(deskpet-sim PRIVATE SDL2)
endif()

if(UNIX)
    target_link_libraries(deskpet-sim PRIVATE m)
endif()
