#include "database/DbConnection.hpp"
#include <iostream>

namespace voting::database {

DbConnection::DbConnection(std::string connInfo)
    : connStr(std::move(connInfo)) {
    conn = PQconnectdb(connStr.c_str());
    if (PQstatus(conn) != CONNECTION_OK) {
        std::string err = PQerrorMessage(conn);
        PQfinish(conn);
        conn = nullptr;
        throw std::runtime_error("Database connection failure: " + err);
    }
}

DbConnection::~DbConnection() {
    if (conn) {
        PQfinish(conn);
        conn = nullptr;
    }
}

DbConnection::DbConnection(DbConnection&& other) noexcept
    : conn(other.conn), connStr(std::move(other.connStr)) {
    other.conn = nullptr;
}

DbConnection& DbConnection::operator=(DbConnection&& other) noexcept {
    if (this != &other) {
        if (conn) PQfinish(conn);
        conn = other.conn;
        connStr = std::move(other.connStr);
        other.conn = nullptr;
    }
    return *this;
}

bool DbConnection::isConnected() const {
    return conn && PQstatus(conn) == CONNECTION_OK;
}

void DbConnection::reconnect() {
    if (conn) {
        PQfinish(conn);
        conn = nullptr;
    }
    conn = PQconnectdb(connStr.c_str());
    if (PQstatus(conn) != CONNECTION_OK) {
        std::string err = PQerrorMessage(conn);
        PQfinish(conn);
        conn = nullptr;
        throw std::runtime_error("Database reconnection failure: " + err);
    }
}

DbResult DbConnection::execute(const std::string& sql) {
    if (!isConnected()) {
        reconnect();
    }
    PGresult* res = PQexec(conn, sql.c_str());
    return DbResult(res);
}

DbResult DbConnection::executeParams(const std::string& sql, const std::vector<std::string>& params) {
    if (!isConnected()) {
        reconnect();
    }

    std::vector<const char*> paramValues;
    paramValues.reserve(params.size());
    for (const auto& p : params) {
        paramValues.push_back(p.c_str());
    }

    PGresult* res = PQexecParams(
        conn,
        sql.c_str(),
        static_cast<int>(params.size()),
        nullptr, // Let PostgreSQL infer parameter types
        paramValues.data(),
        nullptr, // Text format parameters
        nullptr, // Text format parameters
        0        // Text result format
    );

    return DbResult(res);
}

void DbConnection::beginTransaction() {
    auto res = execute("BEGIN;");
    if (!res.isOk()) {
        throw std::runtime_error("Failed to begin transaction: " + res.errorMessage());
    }
    inTransaction = true;
}

void DbConnection::commitTransaction() {
    auto res = execute("COMMIT;");
    inTransaction = false;
    if (!res.isOk()) {
        throw std::runtime_error("Failed to commit transaction: " + res.errorMessage());
    }
}

void DbConnection::rollbackTransaction() {
    if (inTransaction) {
        execute("ROLLBACK;");
        inTransaction = false;
    }
}

std::string DbConnection::lastError() const {
    return conn ? PQerrorMessage(conn) : "Not connected";
}

} // namespace voting::database
