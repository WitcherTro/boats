cmake_minimum_required(VERSION 3.25)
project(boats C)

set(CMAKE_C_STANDARD 11)

# Ensure UTF-8 source and execution charset
if (MSVC)
	add_compile_options(/utf-8)
elseif (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
	add_compile_options(-finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

include_directories(
    src/common
    src/client
    src/client/cli
    src/client/gui
    src/server
)

set(SOURCES_COMMON
	src/common/game.c
	src/common/game.h
	src/common/common.c
	src/common/common.h
    src/common/generic_queue.c
    src/common/generic_queue.h
)

set(SOURCES_CLIENT_CORE
    src/client/client_globals.c
	src/client/client_api.c
	src/client/client_recv.c
	src/client/client_commands.c
)

set(SOURCES_CLIENT_CLI_FILES
	src/client/cli/client_cli.c
	src/client/cli/client_ui.c
	src/client/cli/cli_ui.c
	src/client/cli/cli_callbacks.c
)

set(SOURCES_CLIENT
    ${SOURCES_CLIENT_CORE}
    ${SOURCES_CLIENT_CLI_FILES}
)

set(SOURCES_GUI
	src/client/gui/raylib_ui.c
	src/client/gui/gui_state.c
	src/client/gui/gui_draw.c
	src/client/gui/gui_input.c
	${SOURCES_CLIENT}
	src/common/common.c
)

set(SOURCES_SERVER
	src/server/server.c
	src/server/server_state.c
	src/server/server_state.h
	src/server/server_message.c
	src/server/server_message.h
	src/server/server_client.c
	src/server/server_client.h
	src/server/server_commands.c
	src/server/server_commands.h
)

add_executable(server
	src/server/main_server.c
	${SOURCES_SERVER}
	${SOURCES_COMMON}
)

add_executable(client_cli
	src/client/main_client.c
	${SOURCES_CLIENT}
	src/common/common.c
)

target_include_directories(server PRIVATE ${CMAKE_SOURCE_DIR}/src/server ${CMAKE_SOURCE_DIR}/src/common)
target_include_directories(client_cli PRIVATE ${CMAKE_SOURCE_DIR}/src/client ${CMAKE_SOURCE_DIR}/src/client/cli ${CMAKE_SOURCE_DIR}/src/common)

find_package(Threads REQUIRED)
target_link_libraries(server PRIVATE Threads::Threads)
target_link_libraries(client_cli PRIVATE Threads::Threads)

if(WIN32)
	target_link_libraries(server PRIVATE ws2_32)
	target_link_libraries(client_cli PRIVATE ws2_32)
    if(MINGW)
        target_link_options(server PRIVATE -static)
        target_link_options(client_cli PRIVATE -static)
    endif()
endif()

# Optional raylib GUI build
option(BUILD_GUI "Build raylib GUI client (raylib_client)" ON)

if(BUILD_GUI)
    if(WIN32)
        # Windows: Use local bundled raylib if available
        set(raylib_ROOT "${CMAKE_SOURCE_DIR}/lib/raylib")
        set(raylib_INCLUDE_DIR "${raylib_ROOT}/include")
        set(raylib_LIB_DIR "${raylib_ROOT}/lib")
        
        if(EXISTS "${raylib_INCLUDE_DIR}/raylib.h" AND EXISTS "${raylib_LIB_DIR}/libraylib.a")
            message(STATUS "raylib found at ${raylib_ROOT}, building GUI client")
            add_executable(client_gui
                src/client/main_client.c
                ${SOURCES_GUI}
            )
            target_compile_definitions(client_gui PRIVATE BUILD_GUI=1)
            target_include_directories(client_gui PRIVATE ${CMAKE_SOURCE_DIR}/src/client ${CMAKE_SOURCE_DIR}/src/client/cli ${CMAKE_SOURCE_DIR}/src/client/gui ${CMAKE_SOURCE_DIR}/src/common ${raylib_INCLUDE_DIR})
            target_link_directories(client_gui PRIVATE ${raylib_LIB_DIR})
            target_link_libraries(client_gui PRIVATE raylib Threads::Threads)
            target_link_libraries(client_gui PRIVATE ws2_32 gdi32 winmm)
            if(MINGW)
                target_link_options(client_gui PRIVATE -static)
            endif()
        else()
            message(STATUS "raylib not found at ${raylib_ROOT}. Set BUILD_GUI=OFF or install raylib")
        endif()
    elseif(APPLE)
        # macOS: Check for local static library (lib/raylib_macos)
        # Assuming the library here is a Universal Binary (Fat Library) supporting both x86_64 and arm64
        set(raylib_macos_ROOT "${CMAKE_SOURCE_DIR}/lib/raylib_macos")
        set(raylib_macos_LIB "${raylib_macos_ROOT}/lib/libraylib.a")
        set(raylib_INCLUDE_DIR "${raylib_macos_ROOT}/include")

        if(EXISTS "${raylib_macos_LIB}")
            message(STATUS "Found local macOS raylib at ${raylib_macos_LIB}")
            add_executable(client_gui src/client/main_client.c ${SOURCES_GUI})
            target_compile_definitions(client_gui PRIVATE BUILD_GUI=1)
            target_include_directories(client_gui PRIVATE ${CMAKE_SOURCE_DIR}/src/client ${CMAKE_SOURCE_DIR}/src/client/cli ${CMAKE_SOURCE_DIR}/src/client/gui ${CMAKE_SOURCE_DIR}/src/common ${raylib_INCLUDE_DIR})
            
            # macOS requires linking against specific frameworks
            target_link_libraries(client_gui PRIVATE ${raylib_macos_LIB} Threads::Threads "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
        else()
            message(WARNING "raylib not found at ${raylib_macos_ROOT}. Please add raylib libs or install via brew.")
        endif()
    elseif(UNIX)
        # Linux: Check for local static library first (lib/raylib_linux)
        set(raylib_linux_ROOT "${CMAKE_SOURCE_DIR}/lib/raylib_linux")
        set(raylib_linux_LIB "${raylib_linux_ROOT}/lib/libraylib.a")
        
        # Use the headers from the Linux folder
        set(raylib_INCLUDE_DIR "${raylib_linux_ROOT}/include")

        if(EXISTS "${raylib_linux_LIB}")
            message(STATUS "Found local Linux raylib at ${raylib_linux_LIB}")
            add_executable(client_gui src/client/main_client.c ${SOURCES_GUI})
            target_compile_definitions(client_gui PRIVATE BUILD_GUI=1)
            target_include_directories(client_gui PRIVATE ${CMAKE_SOURCE_DIR}/src/client ${CMAKE_SOURCE_DIR}/src/client/cli ${CMAKE_SOURCE_DIR}/src/client/gui ${CMAKE_SOURCE_DIR}/src/common ${raylib_INCLUDE_DIR})
            
            # Link against the local static library
            target_link_libraries(client_gui PRIVATE ${raylib_linux_LIB} Threads::Threads m pthread dl rt X11 GL)
        else()
            # Fallback: Try to find installed package via pkg-config or cmake
            find_package(raylib QUIET)
            if(raylib_FOUND)
                message(STATUS "Found system raylib")
                add_executable(client_gui src/client/main_client.c ${SOURCES_GUI})
                target_compile_definitions(client_gui PRIVATE BUILD_GUI=1)
                target_include_directories(client_gui PRIVATE ${CMAKE_SOURCE_DIR}/src/client ${CMAKE_SOURCE_DIR}/src/client/cli ${CMAKE_SOURCE_DIR}/src/client/gui ${CMAKE_SOURCE_DIR}/src/common)
                target_link_libraries(client_gui PRIVATE raylib Threads::Threads m pthread dl rt X11 GL)
            else()
                find_package(PkgConfig QUIET)
                pkg_check_modules(RAYLIB raylib)
                if(RAYLIB_FOUND)
                     message(STATUS "Found raylib via pkg-config")
                     add_executable(client_gui src/client/main_client.c ${SOURCES_GUI})
                     target_compile_definitions(client_gui PRIVATE BUILD_GUI=1)
                     target_include_directories(client_gui PRIVATE ${CMAKE_SOURCE_DIR}/src/client ${CMAKE_SOURCE_DIR}/src/client/cli ${CMAKE_SOURCE_DIR}/src/client/gui ${CMAKE_SOURCE_DIR}/src/common ${RAYLIB_INCLUDE_DIRS})
                     target_link_libraries(client_gui PRIVATE ${RAYLIB_LIBRARIES} Threads::Threads m pthread dl rt X11 GL)
                else()
                     message(WARNING "raylib not found on Linux (checked local lib/raylib_linux and system). GUI client will not be built.")
                endif()
            endif()
        endif()
    endif()
endif()
