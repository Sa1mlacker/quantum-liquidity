add_library(market_data OBJECT
    feed_manager.cpp
    bar_aggregator.cpp
    replay_feed.cpp
    redis_publisher.cpp
    # Provider implementations (TODO)
    # providers/darwinex_feed.cpp
    # providers/mt5_feed.cpp
    # providers/ib_feed.cpp
)

target_include_directories(market_data PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(market_data PUBLIC
    common
)
