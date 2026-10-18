#pragma once

#include "database/DbConnection.hpp"
#include <mutex>
#include <condition_variable>
#include <queue>
#include <memory>
#include <string>

namespace voting::database {

class DbPool;

class ScopedConnection {
private:
    DbPool& pool;
    std::unique_ptr<DbConnection> conn;

public:
    ScopedConnection(DbPool& pool, std::unique_ptr<DbConnection> conn);
    ~ScopedConnection();

    ScopedConnection(const ScopedConnection&) = delete;
    ScopedConnection& operator=(const ScopedConnection&) = delete;

    ScopedConnection(ScopedConnection&&) noexcept = default;
    ScopedConnection& operator=(ScopedConnection&&) noexcept = default;

    DbConnection& get() { return *conn; }
    DbConnection* operator->() { return conn.get(); }
};

class DbPool {
private:
    std::string connStr;
    size_t poolSize;
    std::queue<std::unique_ptr<DbConnection>> availableConnections;
    std::mutex poolMutex;
    std::condition_variable cv;
    bool isShuttingDown{false};

public:
    DbPool(std::string connInfo, size_t size = 8);
    ~DbPool();

    DbPool(const DbPool&) = delete;
    DbPool& operator=(const DbPool&) = delete;

    ScopedConnection acquire();
    void release(std::unique_ptr<DbConnection> conn);

    size_t size() const { return poolSize; }
    void shutdown();
};

} // namespace voting::database
