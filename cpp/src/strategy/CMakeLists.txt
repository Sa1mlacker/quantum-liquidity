add_library(strategy OBJECT
    strategy_engine.cpp
    strategy_context.cpp
    # Built-in strategies
    strategies/orb_strategy.cpp
    strategies/mean_reversion_strategy.cpp
    strategies/sentiment_filter.cpp
)

target_include_directories(strategy PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(strategy PUBLIC
    common
    execution
    risk
)
