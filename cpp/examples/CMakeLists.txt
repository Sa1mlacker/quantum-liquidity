# Examples

add_executable(market_data_example
    market_data_example.cpp
)

target_link_libraries(market_data_example
    quantumliquidity_core
)

add_executable(oanda_live_example
    oanda_live_example.cpp
)

target_link_libraries(oanda_live_example
    quantumliquidity_core
    curl  # libcurl for OANDA HTTP/streaming
)

# Install examples (optional)
install(TARGETS market_data_example oanda_live_example
    RUNTIME DESTINATION bin/examples
)
