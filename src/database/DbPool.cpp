#include "database/DbPool.hpp"
#include <stdexcept>

namespace voting::database {

ScopedConnection::ScopedConnection(DbPool& pool, std::unique_ptr<DbConnection> conn)
    : pool(pool), conn(std::move(conn)) {}

ScopedConnection::~ScopedConnection() {
    if (conn) {
        pool.release(std::move(conn));
    }
}

DbPool::DbPool(std::string connInfo, size_t size)
    : connStr(std::move(connInfo)), poolSize(size) {
    for (size_t i = 0; i < poolSize; ++i) {
        auto conn = std::make_unique<DbConnection>(connStr);
        availableConnections.push(std::move(conn));
    }
}

DbPool::~DbPool() {
    shutdown();
}

ScopedConnection DbPool::acquire() {
    std::unique_lock<std::mutex> lock(poolMutex);
    cv.wait(lock, [this]() {
        return !availableConnections.empty() || isShuttingDown;
    });

    if (isShuttingDown) {
        throw std::runtime_error("DbPool is shutting down; cannot acquire connection");
    }

    auto conn = std::move(availableConnections.front());
    availableConnections.pop();
    return ScopedConnection(*this, std::move(conn));
}

void DbPool::release(std::unique_ptr<DbConnection> conn) {
    if (!conn) return;
    {
        std::lock_guard<std::mutex> lock(poolMutex);
        if (!conn->isConnected()) {
            try {
                conn->reconnect();
            } catch (...) {
                // If reconnection fails, replace with a fresh connection attempt
            }
        }
        availableConnections.push(std::move(conn));
    }
    cv.notify_one();
}

void DbPool::shutdown() {
    std::lock_guard<std::mutex> lock(poolMutex);
    isShuttingDown = true;
    while (!availableConnections.empty()) {
        availableConnections.pop();
    }
    cv.notify_all();
}

} // namespace voting::database
