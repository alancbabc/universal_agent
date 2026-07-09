cmake_minimum_required(VERSION 3.21)

project(vi_agent_workspace LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

option(VI_AGENT_BUILD_DEMO "Build demo host executable" ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets Sql)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Sql)

add_library(sqlite3_amalgamation STATIC
    sqlite_3.53.0/sqlite3.c
)
target_include_directories(sqlite3_amalgamation
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/sqlite_3.53.0
)
target_compile_definitions(sqlite3_amalgamation
    PRIVATE
        SQLITE_THREADSAFE=1
        SQLITE_OMIT_LOAD_EXTENSION
)

add_library(vi_agent SHARED
    include/vi_agent_api.h
    src/vi_agent/AgentConfig.h
    src/vi_agent/AgentConfig.cpp
    src/vi_agent/AgentRuntime.h
    src/vi_agent/AgentRuntime.cpp
    src/vi_agent/MetricsRepository.h
    src/vi_agent/MetricsRepository.cpp
    src/vi_agent/PathResolver.h
    src/vi_agent/PathResolver.cpp
    src/vi_agent/ProjectProfile.h
    src/vi_agent/ProjectProfile.cpp
    src/vi_agent/SQLiteConnection.h
    src/vi_agent/SQLiteConnection.cpp
    src/vi_agent/ViAgentDialog.h
    src/vi_agent/ViAgentDialog.cpp
    src/vi_agent/exported_api.cpp
)

target_compile_definitions(vi_agent PRIVATE VI_AGENT_BUILD_DLL)
target_include_directories(vi_agent
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vi_agent
)
target_link_libraries(vi_agent
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Sql
        sqlite3_amalgamation
)

if(WIN32)
    target_link_libraries(vi_agent PRIVATE user32)
endif()

if(MSVC)
    target_compile_options(vi_agent PRIVATE /utf-8)
endif()

if(VI_AGENT_BUILD_DEMO)
    add_executable(vi_agent_demo
        src/demo_main/main.cpp
    )

    target_include_directories(vi_agent_demo
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(vi_agent_demo
        PRIVATE
            Qt${QT_VERSION_MAJOR}::Core
            Qt${QT_VERSION_MAJOR}::Widgets
            vi_agent
    )

    if(MSVC)
        target_compile_options(vi_agent_demo PRIVATE /utf-8)
    endif()
endif()