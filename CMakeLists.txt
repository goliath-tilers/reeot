# reeot - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
if(APPLE)
    project(reeot LANGUAGES C CXX OBJC)
else()
    project(reeot LANGUAGES C CXX)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

set(NFD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(NFD_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(thirdparty/nativefiledialog-extended)
add_library(ddspp INTERFACE)
target_include_directories(ddspp INTERFACE thirdparty/ddspp)

add_library(xenon_patcher STATIC
    thirdparty/xenon_utils/xex_patcher.cpp
    thirdparty/xenon_utils/memory_mapped_file.cpp
    thirdparty/libmspack/libmspack/mspack/lzxd.c
    thirdparty/tiny-AES-c/aes.c
)
target_include_directories(xenon_patcher PUBLIC
    thirdparty/xenon_utils
    thirdparty/libmspack/libmspack/mspack
    thirdparty/tiny-AES-c
    thirdparty/TinySHA1
)

# Sources
set(REEOT_SOURCES
    src/main.cpp
    src/goliath_engine/common/cvars.cpp
    src/goliath_engine/common/misc.cpp # Eventually merge this into cvars.cpp
    src/goliath_engine/installer/installer_dialog.cpp
    src/goliath_engine/installer/install_manifest.cpp
    src/goliath_engine/installer/install_paths.cpp
    src/goliath_engine/installer/installer.cpp
    src/goliath_engine/installer/source_vfs.cpp
)

if(WIN32)
    add_executable(reeot WIN32 ${REEOT_SOURCES})
else()
    add_executable(reeot ${REEOT_SOURCES})
endif()

rexglue_setup_target(reeot GPU_PLUGINS xenos)
target_link_libraries(reeot PRIVATE nfd::nfd ddspp xenon_patcher)

if(APPLE AND TARGET reeot_GameLogic)
    add_custom_command(TARGET reeot_GameLogic POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            "$<TARGET_FILE_NAME:reeot_GameLogic>"
            "$<TARGET_FILE_DIR:reeot_GameLogic>/reeot_GameLogic"
        COMMENT "Creating macOS dlopen alias for reeot_GameLogic"
    )
endif()
