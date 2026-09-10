# DeskPet standalone firmware. BSP comes from the official checkout; point
# DESKPET_BSP_DIR at its components dir (default: sibling my-ai-passport checkout).
cmake_minimum_required(VERSION 3.16)

if(DEFINED ENV{DESKPET_BSP_DIR})
    set(DESKPET_BSP_DIR "$ENV{DESKPET_BSP_DIR}")
else()
    set(DESKPET_BSP_DIR "${CMAKE_SOURCE_DIR}/../my-ai-passport/ai-passport/components")
endif()
list(APPEND EXTRA_COMPONENT_DIRS "${DESKPET_BSP_DIR}")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(deskpet-game)
