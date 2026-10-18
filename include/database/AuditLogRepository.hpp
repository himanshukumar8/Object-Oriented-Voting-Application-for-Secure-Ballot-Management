#pragma once

#include "database/DbPool.hpp"
#include "third_party/json.hpp"
#include <string>
#include <vector>

namespace voting::database {

struct AuditLogEntry {
    std::string id;
    std::string action;
    std::string actorType;
    std::string actorId;
    std::string ipAddress;
    nlohmann::json metadata;
    std::string createdAt;

    nlohmann::json toJson() const {
        return {
            {"id", id},
            {"action", action},
            {"actor_type", actorType},
            {"actor_id", actorId},
            {"ip_address", ipAddress},
            {"metadata", metadata},
            {"created_at", createdAt}
        };
    }
};

class AuditLogRepository {
private:
    DbPool& pool;

public:
    explicit AuditLogRepository(DbPool& pool) : pool(pool) {}

    void log(const std::string& action, const std::string& actorType,
             const std::string& actorId, const std::string& ipAddress,
             const nlohmann::json& metadata = {});

    std::vector<AuditLogEntry> findRecent(int limit = 50);
};

} // namespace voting::database
