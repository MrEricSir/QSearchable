cmake_minimum_required(VERSION 3.16)
project(QSearchable VERSION 1.0.0 LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(QSearchable STATIC
    src/QSearchableItem.cpp
    src/QSearchableItem.h
    src/QSearchableIndex.cpp
    src/QSearchableIndex.h
    src/backends/QSearchableIndexBackend.h
    src/backends/NoOpBackend.cpp
    src/backends/NoOpBackend.h
)
add_library(QSearchable::QSearchable ALIAS QSearchable)

target_include_directories(QSearchable PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(QSearchable PUBLIC Qt6::Core)

if(APPLE)
    enable_language(OBJCXX)
    target_sources(QSearchable PRIVATE
        src/backends/CoreSpotlightBackend.mm
        src/backends/CoreSpotlightBackend.h
    )
    target_link_libraries(QSearchable PRIVATE
        "-framework CoreSpotlight"
        "-framework CoreServices"
        "-framework UniformTypeIdentifiers"
    )
elseif(WIN32)
    target_sources(QSearchable PRIVATE
        src/backends/WindowsSearchBackend.cpp
        src/backends/WindowsSearchBackend.h
    )
    target_link_libraries(QSearchable PRIVATE shell32 ole32 uuid)
elseif(UNIX)
    find_package(Qt6 REQUIRED COMPONENTS DBus)
    target_sources(QSearchable PRIVATE
        src/backends/SearchableItemStore.cpp
        src/backends/SearchableItemStore.h
        src/backends/GnomeSearchBackend.cpp
        src/backends/GnomeSearchBackend.h
        src/backends/KRunnerBackend.cpp
        src/backends/KRunnerBackend.h
    )
    target_link_libraries(QSearchable PRIVATE Qt6::DBus)
endif()

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    enable_testing()
    add_subdirectory(tests)
    add_subdirectory(example)
endif()
