# Main applications

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
    quantumliquidity_gateway
    quantumliquidity_strategy_runner
    quantumliquidity_backtest
    RUNTIME DESTINATION bin
)
