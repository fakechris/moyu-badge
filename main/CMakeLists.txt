include(${CMAKE_CURRENT_LIST_DIR}/sprites_embed.cmake)
# Hero art flavor: embed the shared world art plus exactly one hero set.
# generic = public release; oc = family-private build (files are gitignored).
if(CONFIG_DESKPET_ART_FLAVOR_OC)
    set(DESKPET_ART_SPRITES ${DESKPET_SPRITES_OC})
    if(NOT DESKPET_ART_SPRITES)
        message(FATAL_ERROR "Art flavor 'oc' selected but main/sprites/oc is "
                "empty — the private hero set is not present in this checkout.")
    endif()
else()
    set(DESKPET_ART_SPRITES ${DESKPET_SPRITES_GENERIC})
    if(NOT DESKPET_ART_SPRITES)
        message(FATAL_ERROR "Art flavor 'generic' selected but main/sprites/generic "
                "is empty — build it with moyu-playbook tools/build_generic_heroes.py.")
    endif()
endif()
idf_component_register(
    SRCS "app_main.c"
         "ui_pixel.c"
         "sprite.c"
         "sprite_anim.c"
         "pet_model.c"
         "dungeon_model.c"
         "pomo_lite.c"
         "deskpet_i18n.c"
         "deskpet_store.c"
         "demo_deskpet.c"
         "deskpet_time.c"
         "pet_sleep.c"
         "serial_time.c"
         "app_shell.c"
         "hud_battery.c"
         "mode_pomo.c"
         "mode_clicker.c"
         "vocab_model.c"
         "vocab_ui.c"
         "mode_vocab.c"
         "mode_standby.c"
         "mode_settings.c"
         "clicker_hid_ble.c"
         "agent_status.c"
         "agent_status_gatts.c"
         "chiptune.c"
         "chiptune_songs.c"
         "audio_task.c"
         "fonts/zh_subset.c"
         "fonts/zh_ipa_fix.c"
     INCLUDE_DIRS "."
     REQUIRES bsp bt esp_event esp_hid esp_hw_support esp_netif esp_timer esp_wifi nvs_flash
     PRIV_REQUIRES esp_driver_gpio esp_driver_usb_serial_jtag esp_lcd
     EMBED_FILES ${DESKPET_SPRITES_SHARED} ${DESKPET_ART_SPRITES}
)

# Wall-clock seed: regenerated on every build (target has no output file, so
# it is always stale); the flashed image boots with build/flash-time wall clock.
add_custom_target(deskpet_build_time
    COMMAND ${CMAKE_COMMAND} -DOUT=${CMAKE_BINARY_DIR}/build_time.h
            -P ${CMAKE_CURRENT_LIST_DIR}/../tools/gen_build_time.cmake)
add_dependencies(${COMPONENT_LIB} deskpet_build_time)
target_include_directories(${COMPONENT_LIB} PRIVATE ${CMAKE_BINARY_DIR})
