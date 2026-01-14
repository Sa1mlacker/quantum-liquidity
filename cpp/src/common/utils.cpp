#include "quantumliquidity/common/utils.hpp"
#include <sstream>
#include <iomanip>

namespace quantumliquidity::utils {

std::string timestamp_to_string(Timestamp ts) {
    auto time_t_val = std::chrono::system_clock::to_time_t(ts);
    auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(
        ts.time_since_epoch()
    ) % 1000000;

    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t_val), "%Y-%m-%d %H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(6) << microseconds.count();
    return oss.str();
}

Timestamp string_to_timestamp(const std::string& str) {
    // TODO: Implement robust parsing
    // For now, return current time
    return std::chrono::system_clock::now();
}

std::string side_to_string(Side side) {
    return side == Side::BUY ? "BUY" : "SELL";
}

std::string order_state_to_string(OrderState state) {
    switch (state) {
        case OrderState::PENDING_SUBMIT: return "PENDING_SUBMIT";
        case OrderState::SUBMITTED: return "SUBMITTED";
        case OrderState::ACCEPTED: return "ACCEPTED";
        case OrderState::PARTIALLY_FILLED: return "PARTIALLY_FILLED";
        case OrderState::FILLED: return "FILLED";
        case OrderState::PENDING_CANCEL: return "PENDING_CANCEL";
        case OrderState::CANCELLED: return "CANCELLED";
        case OrderState::REJECTED: return "REJECTED";
        case OrderState::EXPIRED: return "EXPIRED";
        default: return "UNKNOWN";
    }
}

} // namespace quantumliquidity::utils
