cmake_minimum_required(VERSION 3.16)

project(RadioHandler LANGUAGES C)

find_package(Threads REQUIRED)

if(NOT TARGET ccsds AND NOT CCSDS_INCLUDE_DIRS)
    add_subdirectory(ccsds)
endif()

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

# Linux controls the Pi GPIOs; macOS uses a preconfigured radio over USB serial.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_path(PIGPIOD_IF2_INCLUDE_DIR NAMES pigpiod_if2.h)
    find_library(PIGPIOD_IF2_LIBRARY NAMES pigpiod_if2)
endif()

if(APPLE OR (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND
             PIGPIOD_IF2_INCLUDE_DIR AND PIGPIOD_IF2_LIBRARY))
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
    )
    target_link_libraries(RadioHandler PUBLIC RadioHandler_common)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_include_directories(RadioHandler PRIVATE "${PIGPIOD_IF2_INCLUDE_DIR}")
        target_link_libraries(RadioHandler PRIVATE "${PIGPIOD_IF2_LIBRARY}")
    else()
        target_compile_definitions(RadioHandler PRIVATE _DARWIN_C_SOURCE)
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(STATUS "pigpiod_if2 not found: RadioHandler hardware driver disabled.")
else()
    message(STATUS "Unsupported system (${CMAKE_SYSTEM_NAME}): RadioHandler hardware driver disabled.")
endif()

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    include(CTest)
endif()
if(BUILD_TESTING AND APPLE AND CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    add_executable(radio_serial_test tests/radio_serial_test.c)
    target_link_libraries(radio_serial_test PRIVATE RadioHandler)
    foreach(case IN ITEMS roundtrip missing-device invalid-device swapped-uart invalid-delay)
        add_test(NAME radio_serial_${case} COMMAND radio_serial_test ${case})
        set_tests_properties(radio_serial_${case} PROPERTIES TIMEOUT 15)
    endforeach()
    set_tests_properties(radio_serial_missing-device PROPERTIES
        PASS_REGULAR_EXPRESSION "set E22_UART_DEVICE")
    set_tests_properties(radio_serial_invalid-device PROPERTIES
        PASS_REGULAR_EXPRESSION "open uart")
    set_tests_properties(radio_serial_swapped-uart PROPERTIES
        PASS_REGULAR_EXPRESSION "E22_SWAPPED_UART requires Linux")
    set_tests_properties(radio_serial_invalid-delay PROPERTIES
        PASS_REGULAR_EXPRESSION "E22_TX_DELAY_MS must be between")
endif()
