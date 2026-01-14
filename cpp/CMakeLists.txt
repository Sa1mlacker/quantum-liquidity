cmake_minimum_required(VERSION 3.20)
project(QuantumLiquidity VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -fsanitize=address -fsanitize=undefined")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG -march=native")

# Find dependencies
find_package(Threads REQUIRED)
find_package(Protobuf REQUIRED)

# Third-party libraries (assuming vcpkg or system install)
# TODO: Add find_package calls for:
# - spdlog (logging)
# - fmt (formatting)
# - nlohmann_json (JSON parsing)
# - hiredis (Redis client)
# - libpq (PostgreSQL client)
# - googletest (testing)
# - benchmark (performance testing)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${Protobuf_INCLUDE_DIRS}
)

# Subdirectories
add_subdirectory(src/common)
add_subdirectory(src/market_data)
add_subdirectory(src/execution)
add_subdirectory(src/risk)
add_subdirectory(src/strategy)
add_subdirectory(src/persistence)

# Main library: libquantumliquidity_core
add_library(quantumliquidity_core STATIC
    $<TARGET_OBJECTS:common>
    $<TARGET_OBJECTS:market_data>
    $<TARGET_OBJECTS:execution>
    $<TARGET_OBJECTS:risk>
    $<TARGET_OBJECTS:strategy>
    $<TARGET_OBJECTS:persistence>
)

target_link_libraries(quantumliquidity_core
    PUBLIC
        Threads::Threads
        ${Protobuf_LIBRARIES}
    # TODO: Add other libraries
)

# Python bindings (pybind11)
# TODO: Add pybind11 subdirectory
# add_subdirectory(bindings/python)

# Executables
add_subdirectory(apps)

# Tests
option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Install rules
install(TARGETS quantumliquidity_core
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.hpp"
)
