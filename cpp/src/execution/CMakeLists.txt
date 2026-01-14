add_library(execution OBJECT
    execution_engine.cpp
    order_manager.cpp
    # Provider implementations (TODO)
    # providers/fix_provider.cpp
    # providers/rest_provider.cpp
)

target_include_directories(execution PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(execution PUBLIC
    common
    risk
)
