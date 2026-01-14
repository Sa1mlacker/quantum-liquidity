# Examples

add_executable(market_data_example
    market_data_example.cpp
)

target_link_libraries(market_data_example
    quantumliquidity_core
)

# Install examples (optional)
install(TARGETS market_data_example
    RUNTIME DESTINATION bin/examples
)
