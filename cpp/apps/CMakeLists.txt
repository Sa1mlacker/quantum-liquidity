# Main applications

# Automatic market data daemon (YAML-configured)
add_executable(market_data_daemon
    market_data_daemon.cpp
)

target_link_libraries(market_data_daemon
    quantumliquidity_core
    curl
    yaml-cpp  # For YAML config parsing
)

# Market data gateway app
add_executable(quantumliquidity_gateway
    gateway_main.cpp
)

target_link_libraries(quantumliquidity_gateway
    quantumliquidity_core
)

# Strategy runner app
add_executable(quantumliquidity_strategy_runner
    strategy_runner_main.cpp
)

target_link_libraries(quantumliquidity_strategy_runner
    quantumliquidity_core
)

# Backtest runner app
add_executable(quantumliquidity_backtest
    backtest_main.cpp
)

target_link_libraries(quantumliquidity_backtest
    quantumliquidity_core
)

install(TARGETS
    market_data_daemon
    quantumliquidity_gateway
    quantumliquidity_strategy_runner
    quantumliquidity_backtest
    RUNTIME DESTINATION bin
)
