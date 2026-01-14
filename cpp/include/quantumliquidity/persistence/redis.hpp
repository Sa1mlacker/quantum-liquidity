#pragma once

#include "quantumliquidity/common/types.hpp"
#include <string>
#include <functional>
#include <memory>
#include <vector>

namespace quantumliquidity::persistence {

/**
 * @brief Redis configuration
 */
struct RedisConfig {
    std::string host = "localhost";
    int port = 6379;
    int db = 0;
    std::string password;
    int timeout_ms = 1000;
};

/**
 * @brief Redis pub/sub subscriber interface
 */
class IRedisSubscriber {
public:
    virtual ~IRedisSubscriber() = default;

    // Subscribe to channel
    virtual void subscribe(const std::string& channel) = 0;
    virtual void psubscribe(const std::string& pattern) = 0;  // Pattern subscribe

    // Unsubscribe
    virtual void unsubscribe(const std::string& channel) = 0;
    virtual void punsubscribe(const std::string& pattern) = 0;

    // Message callback
    using MessageCallback = std::function<void(const std::string& channel, const std::string& message)>;
    virtual void set_message_callback(MessageCallback callback) = 0;

    // Lifecycle
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual bool is_running() const = 0;
};

/**
 * @brief Redis pub/sub publisher interface
 */
class IRedisPublisher {
public:
    virtual ~IRedisPublisher() = default;

    // Publish message to channel
    virtual void publish(const std::string& channel, const std::string& message) = 0;

    // Batch publish
    virtual void publish_batch(const std::vector<std::pair<std::string, std::string>>& messages) = 0;

    // Connection
    virtual void connect() = 0;
    virtual void disconnect() = 0;
    virtual bool is_connected() const = 0;
};

/**
 * @brief Redis client interface (GET/SET/etc.)
 */
class IRedisClient {
public:
    virtual ~IRedisClient() = default;

    // Basic operations
    virtual void set(const std::string& key, const std::string& value) = 0;
    virtual std::string get(const std::string& key) = 0;
    virtual void del(const std::string& key) = 0;

    // Expiration
    virtual void setex(const std::string& key, const std::string& value, int seconds) = 0;
    virtual void expire(const std::string& key, int seconds) = 0;

    // Hash operations
    virtual void hset(const std::string& key, const std::string& field, const std::string& value) = 0;
    virtual std::string hget(const std::string& key, const std::string& field) = 0;

    // Connection
    virtual void connect() = 0;
    virtual void disconnect() = 0;
    virtual bool is_connected() const = 0;
};

} // namespace quantumliquidity::persistence
