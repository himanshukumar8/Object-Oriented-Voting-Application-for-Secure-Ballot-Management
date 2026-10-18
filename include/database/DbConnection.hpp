#pragma once

#include <libpq-fe.h>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>

namespace voting::database {

class DbResult {
private:
    PGresult* res{nullptr};

public:
    explicit DbResult(PGresult* res) : res(res) {}
    ~DbResult() {
        if (res) PQclear(res);
    }

    DbResult(const DbResult&) = delete;
    DbResult& operator=(const DbResult&) = delete;

    DbResult(DbResult&& other) noexcept : res(other.res) {
        other.res = nullptr;
    }

    DbResult& operator=(DbResult&& other) noexcept {
        if (this != &other) {
            if (res) PQclear(res);
            res = other.res;
            other.res = nullptr;
        }
        return *this;
    }

    ExecStatusType status() const {
        return res ? PQresultStatus(res) : PGRES_FATAL_ERROR;
    }

    bool isOk() const {
        auto s = status();
        return s == PGRES_COMMAND_OK || s == PGRES_TUPLES_OK;
    }

    int rowCount() const {
        return res ? PQntuples(res) : 0;
    }

    int colCount() const {
        return res ? PQnfields(res) : 0;
    }

    std::string getValue(int row, int col) const {
        if (!res || row >= rowCount() || col >= colCount() || PQgetisnull(res, row, col)) {
            return "";
        }
        return PQgetvalue(res, row, col);
    }

    std::string getValue(int row, const std::string& colName) const {
        if (!res) return "";
        int colIndex = PQfnumber(res, colName.c_str());
        if (colIndex < 0) return "";
        return getValue(row, colIndex);
    }

    bool isNull(int row, const std::string& colName) const {
        if (!res) return true;
        int colIndex = PQfnumber(res, colName.c_str());
        if (colIndex < 0) return true;
        return PQgetisnull(res, row, colIndex) != 0;
    }

    std::string errorMessage() const {
        return res ? PQresultErrorMessage(res) : "No result";
    }

    std::string errorCode() const {
        if (!res) return "";
        const char* code = PQresultErrorField(res, PG_DIAG_SQLSTATE);
        return code ? std::string(code) : "";
    }
};

class DbConnection {
private:
    PGconn* conn{nullptr};
    std::string connStr;

public:
    explicit DbConnection(std::string connInfo);
    ~DbConnection();

    DbConnection(const DbConnection&) = delete;
    DbConnection& operator=(const DbConnection&) = delete;

    DbConnection(DbConnection&& other) noexcept;
    DbConnection& operator=(DbConnection&& other) noexcept;

    bool isConnected() const;
    void reconnect();

    DbResult execute(const std::string& sql);
    DbResult executeParams(const std::string& sql, const std::vector<std::string>& params);

    void beginTransaction();
    void commitTransaction();
    void rollbackTransaction();

    std::string lastError() const;
    bool isInTransaction() const { return inTransaction; }
private:
    bool inTransaction{false};
};

} // namespace voting::database
