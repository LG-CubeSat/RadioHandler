cmake_minimum_required(VERSION 3.16)

project(RadioHandler LANGUAGES C)

find_package(Threads REQUIRED)

add_library(RadioHandler_interface INTERFACE)
target_include_directories(RadioHandler_interface INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# Common radio queuing and CCSDS packet deframing (used across sim, real hardware, and ground station)
add_library(RadioHandler_common STATIC
    src/radio_queue.c
    src/radio_deframer.c
    include/radio_queue.h
    include/radio_deframer.h
)
add_library(RadioHandler::common ALIAS RadioHandler_common)

target_compile_features(RadioHandler_common PRIVATE c_std_11)
target_compile_definitions(RadioHandler_common PRIVATE
    _DEFAULT_SOURCE
    _POSIX_C_SOURCE=200809L
)
target_include_directories(RadioHandler_common
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

if(TARGET ccsds)
    target_link_libraries(RadioHandler_common PUBLIC ccsds)
elseif(CCSDS_INCLUDE_DIRS)
    target_include_directories(RadioHandler_common PUBLIC ${CCSDS_INCLUDE_DIRS})
    if(CCSDS_LIBRARIES)
        target_link_libraries(RadioHandler_common PUBLIC ${CCSDS_LIBRARIES})
    endif()
endif()

target_link_libraries(RadioHandler_common PUBLIC
    Threads::Threads
)

# Real E22 hardware driver (requires Linux GPIO chardev, termios, and pigpiod_if2)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_path(PIGPIOD_IF2_INCLUDE_DIR NAMES pigpiod_if2.h)
    find_library(PIGPIOD_IF2_LIBRARY NAMES pigpiod_if2)
    if(PIGPIOD_IF2_INCLUDE_DIR AND PIGPIOD_IF2_LIBRARY)
        add_library(RadioHandler STATIC
            src/radio.c
            include/radio.h
        )
        add_library(RadioHandler::RadioHandler ALIAS RadioHandler)

        target_compile_features(RadioHandler PRIVATE c_std_11)
        target_compile_definitions(RadioHandler PRIVATE
            _DEFAULT_SOURCE
            _POSIX_C_SOURCE=200809L
        )
        target_include_directories(RadioHandler
            PUBLIC
                "${CMAKE_CURRENT_SOURCE_DIR}/include"
            PRIVATE
                "${PIGPIOD_IF2_INCLUDE_DIR}"
        )
        target_link_libraries(RadioHandler
            PUBLIC
                RadioHandler_common
            PRIVATE
                "${PIGPIOD_IF2_LIBRARY}"
        )
    else()
        message(STATUS "pigpiod_if2 not found: RadioHandler hardware driver disabled.")
    endif()
else()
    message(STATUS "Non-Linux system (${CMAKE_SYSTEM_NAME}): RadioHandler hardware driver disabled.")
endif()
