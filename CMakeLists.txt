cmake_minimum_required(VERSION 3.20)

file(STRINGS "../extrachain-core/extrachain_version" EXTRACHAIN_VERSION)
project(extrachain-console LANGUAGES C CXX VERSION ${EXTRACHAIN_VERSION})

set(EXTRACHAIN_STATIC_BUILD true)
include(../extrachain-core/CMakeLists.txt)

set(EXTRACHAIN_CONSOLE_SOURCES
    headers/console/console_manager.h
    headers/console/push_manager.h
    headers/console/console_input.h
    sources/console/console_manager.cpp
    sources/console/push_manager.cpp
    sources/console/console_input.cpp
    main.cpp
    )

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Library)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Library)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Build)

add_executable(extrachain-console
    ${EXTRACHAIN_CONSOLE_SOURCES}
    )
add_definitions(-DECONSOLE)
target_include_directories(extrachain-console PUBLIC ${CMAKE_CURRENT_LIST_DIR}/headers ${EXTRACHAIN_CORE_INCLUDES})

# git
execute_process(COMMAND git --git-dir ${CMAKE_CURRENT_LIST_DIR}/.git --work-tree $$PWD describe --always --tags OUTPUT_VARIABLE GIT_COMMIT)
execute_process(COMMAND git --git-dir ${CMAKE_CURRENT_LIST_DIR}/.git --work-tree $$PWD symbolic-ref --short HEAD OUTPUT_VARIABLE GIT_BRANCH)
string(STRIP "${GIT_COMMIT}" GIT_COMMIT)
string(STRIP "${GIT_BRANCH}" GIT_BRANCH)
add_compile_definitions(GIT_COMMIT="${GIT_COMMIT}" GIT_BRANCH="${GIT_BRANCH}")

target_link_libraries(extrachain-console PRIVATE Qt6::Core Qt6::Network extrachain)

if (EXTRACHAIN_ENABLE_RUST)
    target_link_libraries(extrachain-console PRIVATE rextrachain)
endif (EXTRACHAIN_ENABLE_RUST)

set_property(TARGET extrachain-console PROPERTY POSITION_INDEPENDENT_CODE 1)
