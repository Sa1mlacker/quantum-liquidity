add_library(market_data OBJECT
    feed_manager.cpp
    bar_aggregator.cpp
    replay_feed.cpp
    redis_publisher.cpp
    # Provider implementations
    providers/csv_feed.cpp
    providers/oanda_feed.cpp
    # providers/darwinex_feed.cpp  # TODO
    # providers/mt5_feed.cpp  # TODO
    # providers/ib_feed.cpp  # TODO
)

target_include_directories(market_data PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(market_data PUBLIC
    common
)
