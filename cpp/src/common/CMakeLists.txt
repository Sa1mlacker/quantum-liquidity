add_library(common OBJECT
    types.cpp
    logger.cpp
    config.cpp
    utils.cpp
)

target_include_directories(common PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)
