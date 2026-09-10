# sim/CMakeLists.txt —— host SDL simulator (macOS). Same game sources as firmware.
cmake_minimum_required(VERSION 3.16)
project(deskpet-sim C)

set(CMAKE_C_STANDARD 11)

execute_process(COMMAND sdl2-config --cflags OUTPUT_VARIABLE SDL_CFLAGS
                OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND sdl2-config --libs OUTPUT_VARIABLE SDL_LIBS
                OUTPUT_STRIP_TRAILING_WHITESPACE)

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
# Same wall-clock seed as firmware: refresh build_time.h on every sim build.
add_custom_target(deskpet_build_time
    COMMAND ${CMAKE_COMMAND} -DOUT=${CMAKE_BINARY_DIR}/build_time.h
            -P ${CMAKE_SOURCE_DIR}/../tools/gen_build_time.cmake)
add_dependencies(deskpet-sim deskpet_build_time)
separate_arguments(SDL_CFLAGS_LIST UNIX_COMMAND "${SDL_CFLAGS}")
separate_arguments(SDL_LIBS_LIST UNIX_COMMAND "${SDL_LIBS}")
target_compile_options(deskpet-sim PRIVATE ${SDL_CFLAGS_LIST} -Wno-unused-function)
target_include_directories(deskpet-sim PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/stubs
    ${CMAKE_SOURCE_DIR}/upstream
    ${CMAKE_SOURCE_DIR}/../main
    ${CMAKE_SOURCE_DIR}/third_party/lvgl
    ${CMAKE_SOURCE_DIR}/third_party
    /opt/homebrew/include
)
target_link_libraries(deskpet-sim PRIVATE ${SDL_LIBS_LIST} m)
