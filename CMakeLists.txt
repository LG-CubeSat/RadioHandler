cmake_minimum_required(VERSION 3.16)

project(RadioHandler LANGUAGES C)

add_library(RadioHandler_interface INTERFACE)
target_include_directories(RadioHandler_interface INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(Threads REQUIRED)
    find_path(PIGPIOD_IF2_INCLUDE_DIR NAMES pigpiod_if2.h)
    find_library(PIGPIOD_IF2_LIBRARY NAMES pigpiod_if2)
    if(PIGPIOD_IF2_INCLUDE_DIR AND PIGPIOD_IF2_LIBRARY)
        # Supply radio_deframer, radio_queue, and their CCSDS dependencies from the
        # consuming project; those files are not included in this repository.
        set(RADIOHANDLER_SUPPORT_SOURCES "" CACHE STRING
            "Semicolon-separated supporting C source files to compile into RadioHandler")
        set(RADIOHANDLER_SUPPORT_INCLUDE_DIRS "" CACHE STRING
            "Semicolon-separated include directories for the supporting headers")

        add_library(RadioHandler STATIC
            src/radio.c
            include/radio.h
            ${RADIOHANDLER_SUPPORT_SOURCES}
        )
        add_library(RadioHandler::RadioHandler ALIAS RadioHandler)

        target_compile_features(RadioHandler PRIVATE c_std_11)
        target_compile_definitions(RadioHandler PRIVATE
            _DEFAULT_SOURCE
            _POSIX_C_SOURCE=200809L
        )
        target_include_directories(RadioHandler
            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
            PRIVATE
                "${PIGPIOD_IF2_INCLUDE_DIR}"
                ${RADIOHANDLER_SUPPORT_INCLUDE_DIRS}
        )
        target_link_libraries(RadioHandler PRIVATE
            Threads::Threads
            "${PIGPIOD_IF2_LIBRARY}"
        )
    else()
        message(STATUS "pigpiod_if2 not found: RadioHandler hardware driver disabled.")
    endif()
else()
    message(STATUS "Non-Linux system (${CMAKE_SYSTEM_NAME}): RadioHandler hardware driver disabled.")
endif()
