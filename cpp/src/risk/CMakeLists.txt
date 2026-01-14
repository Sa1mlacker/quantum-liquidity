add_library(risk OBJECT
    risk_engine.cpp
    position_manager.cpp
    risk_rules.cpp
)

target_include_directories(risk PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(risk PUBLIC
    common
)
