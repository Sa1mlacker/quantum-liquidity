#pragma once

#include "quantumliquidity/common/types.hpp"
#include <string>

namespace quantumliquidity::utils {

// Timestamp conversion
std::string timestamp_to_string(Timestamp ts);
Timestamp string_to_timestamp(const std::string& str);

// Enum to string
std::string side_to_string(Side side);
std::string order_state_to_string(OrderState state);

// Price formatting
inline std::string format_price(Price price, int decimals = 5) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.*f", decimals, price);
    return std::string(buffer);
}

// Quantity formatting
inline std::string format_quantity(Quantity qty, int decimals = 2) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.*f", decimals, qty);
    return std::string(buffer);
}

} // namespace quantumliquidity::utils
