file(GLOB STELLA_COMMON_SRCS "stella/src/common/*.cxx")
list(REMOVE_ITEM STELLA_COMMON_SRCS "${CMAKE_CURRENT_LIST_DIR}/stella/src/common/MouseControl.cxx")

file(GLOB STELLA_EMUCORE_SRCS "stella/src/emucore/*.cxx")

set(COMPONENT_SRCS
    ${STELLA_COMMON_SRCS}
    ${STELLA_EMUCORE_SRCS}
    "libretro.cxx"
    "libretro-common/compat/compat_posix_string.c"
    "libretro-common/compat/compat_strcasestr.c"
    "libretro-common/compat/compat_snprintf.c"
    "libretro-common/compat/compat_strl.c"
    "libretro-common/compat/fopen_utf8.c"
    "libretro-common/encodings/encoding_utf.c"
    "libretro-common/file/file_path.c"
    "libretro-common/file/file_path_io.c"
    "libretro-common/streams/file_stream.c"
    "libretro-common/streams/file_stream_transforms.c"
    "libretro-common/string/stdstring.c"
    "libretro-common/time/rtime.c"
    "libretro-common/vfs/vfs_implementation.c"
)

set(COMPONENT_ADD_INCLUDEDIRS
    "."
    "stella/src/common"
    "stella/src/emucore"
    "stella/stubs"
    "libretro-common/include"
)

set(COMPONENT_REQUIRES "retro-go")

register_component()

target_compile_options(${COMPONENT_LIB} PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++11>
)

rg_setup_compile_options(
    -O2
    -fno-rtti
    -fno-exceptions
    -Wno-error=format
    -Wno-error=implicit-fallthrough
    -Wno-error=parentheses
    -Wno-error=template-id-cdtor
    -Wno-unused-function
    -Wno-unused-variable
    -Wno-sign-compare
    -Wno-missing-field-initializers
)
