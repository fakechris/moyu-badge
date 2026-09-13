# Windows IDF_PATH uses backslashes; CMake string escapes would break \U...
file(TO_CMAKE_PATH "$ENV{IDF_PATH}" _DESKPET_IDF_PATH)
idf_component_register(
    SRCS "recovery_boot_hook.c"
    PRIV_REQUIRES bootloader_support
    PRIV_INCLUDE_DIRS "${_DESKPET_IDF_PATH}/components/bootloader_support/private_include"
)
