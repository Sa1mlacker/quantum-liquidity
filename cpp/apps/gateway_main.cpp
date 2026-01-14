#include "quantumliquidity/market_data/feed_interface.hpp"
#include "quantumliquidity/common/logger.hpp"
#include <iostream>
#include <csignal>
#include <atomic>

std::atomic<bool> running{true};

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nShutdown signal received..." << std::endl;
        running = false;
    }
}

int main(int argc, char** argv) {
    using namespace quantumliquidity;

    // Setup signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    Logger::info("gateway", "QuantumLiquidity Gateway starting...");

    // TODO: Load configuration from YAML
    // TODO: Initialize market data feeds
    // TODO: Connect to PostgreSQL
    // TODO: Connect to Redis
    // TODO: Subscribe to instruments
    // TODO: Start event loop

    Logger::info("gateway", "Gateway started. Press Ctrl+C to stop.");

    while (running) {
        // Event loop
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    Logger::info("gateway", "Gateway shutting down...");

    // TODO: Cleanup: disconnect feeds, flush buffers

    return 0;
}
