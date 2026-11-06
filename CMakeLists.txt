cmake_minimum_required(VERSION 3.16)
project(populous C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew" "/usr/local")
endif()

find_package(SDL2 REQUIRED)

set(POPULOUS_PORT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/populous_c_port)
file(TO_CMAKE_PATH "${CMAKE_SOURCE_DIR}/binaries/populous-extracted" POPULOUS_ASSET_DIR)

add_executable(populous_init_port
    ${POPULOUS_PORT_DIR}/populous_init.c
    ${POPULOUS_PORT_DIR}/populous_prg.c
    ${POPULOUS_PORT_DIR}/sdl_lowlevel.c
    ${POPULOUS_PORT_DIR}/land_data.c
    ${POPULOUS_PORT_DIR}/level_data.c
    ${POPULOUS_PORT_DIR}/graphics.c
    ${POPULOUS_PORT_DIR}/audio.c
    ${POPULOUS_PORT_DIR}/title_music.c
    ${POPULOUS_PORT_DIR}/intro_data.c
    ${POPULOUS_PORT_DIR}/pop_log.c
)

set_property(SOURCE ${POPULOUS_PORT_DIR}/populous_prg.c APPEND PROPERTY OBJECT_DEPENDS
    ${POPULOUS_PORT_DIR}/a4_state.h
    ${POPULOUS_PORT_DIR}/modules/01_preamble.c
    ${POPULOUS_PORT_DIR}/modules/02_rules.c
    ${POPULOUS_PORT_DIR}/modules/03_ui_save.c
    ${POPULOUS_PORT_DIR}/modules/04_render_input.c
)

target_include_directories(populous_init_port PRIVATE ${POPULOUS_PORT_DIR})
target_compile_definitions(populous_init_port PRIVATE
    "POPULOUS_DEFAULT_ASSET_DIR=\"${POPULOUS_ASSET_DIR}\""
)

if(APPLE)
    target_compile_definitions(populous_init_port PRIVATE _DARWIN_C_SOURCE)
endif()

if(MSVC)
    target_compile_options(populous_init_port PRIVATE /W3 /utf-8)
else()
    target_compile_options(populous_init_port PRIVATE -Wall -Wextra)
endif()

if(TARGET SDL2::SDL2main)
    target_link_libraries(populous_init_port PRIVATE SDL2::SDL2main)
endif()
if(TARGET SDL2::SDL2)
    target_link_libraries(populous_init_port PRIVATE SDL2::SDL2)
else()
    target_include_directories(populous_init_port PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(populous_init_port PRIVATE ${SDL2_LIBRARIES})
endif()

enable_testing()
add_test(NAME populous_init_port_exists
    COMMAND ${CMAKE_COMMAND} -E md5sum $<TARGET_FILE:populous_init_port>)