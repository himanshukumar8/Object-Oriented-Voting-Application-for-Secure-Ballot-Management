#include "database/AuditLogRepository.hpp"
#include <iostream>

namespace voting::database {

void AuditLogRepository::log(
    const std::string& action,
    const std::string& actorType,
    const std::string& actorId,
    const std::string& ipAddress,
    const nlohmann::json& metadata
) {
    try {
        auto conn = pool.acquire();
        std::string sql = 
            "INSERT INTO audit_logs (action, actor_type, actor_id, ip_address, metadata) "
            "VALUES ($1, $2, $3, $4, $5::jsonb);";
        
        std::string metaStr = metadata.empty() ? "{}" : metadata.dump();
        conn->executeParams(sql, {action, actorType, actorId, ipAddress, metaStr});
    } catch (const std::exception& e) {
        // Logging should not crash application flow; write to stderr as fallback
        std::cerr << "[AUDIT_LOG_ERROR] " << e.what() << " | Action: " << action << std::endl;
    }
}

std::vector<AuditLogEntry> AuditLogRepository::findRecent(int limit) {
    auto conn = pool.acquire();
    std::string sql = 
        "SELECT * FROM audit_logs ORDER BY created_at DESC LIMIT $1;";
    
    auto res = conn->executeParams(sql, {std::to_string(limit)});
    std::vector<AuditLogEntry> logs;
    if (!res.isOk()) return logs;

    logs.reserve(res.rowCount());
    for (int i = 0; i < res.rowCount(); ++i) {
        AuditLogEntry entry;
        entry.id = res.getValue(i, "id");
        entry.action = res.getValue(i, "action");
        entry.actorType = res.getValue(i, "actor_type");
        entry.actorId = res.getValue(i, "actor_id");
        entry.ipAddress = res.getValue(i, "ip_address");
        
        std::string metaRaw = res.getValue(i, "metadata");
        if (!metaRaw.empty()) {
            try {
                entry.metadata = nlohmann::json::parse(metaRaw);
            } catch (...) {
                entry.metadata = metaRaw;
            }
        }
        entry.createdAt = res.getValue(i, "created_at");
        logs.push_back(std::move(entry));
    }
    return logs;
}

} // namespace voting::database
