#pragma once

#include <string>
#include "third_party/json.hpp"

namespace voting::domain {

class Voter {
public:
    std::string id;
    std::string voterId;
    std::string fullName;
    std::string email;
    std::string passwordHash;
    bool isActive{true};
    std::string createdAt;

    Voter() = default;

    Voter(std::string id, std::string voterId, std::string fullName,
          std::string email, std::string passwordHash = "",
          bool isActive = true, std::string createdAt = "")
        : id(std::move(id)),
          voterId(std::move(voterId)),
          fullName(std::move(fullName)),
          email(std::move(email)),
          passwordHash(std::move(passwordHash)),
          isActive(isActive),
          createdAt(std::move(createdAt)) {}

    /**
     * Serializes voter information for client responses, safely omitting password hashes.
     */
    nlohmann::json toClientJson() const {
        return {
            {"id", id},
            {"voter_id", voterId},
            {"full_name", fullName},
            {"email", email},
            {"is_active", isActive},
            {"created_at", createdAt}
        };
    }

    nlohmann::json toInternalJson() const {
        auto j = toClientJson();
        j["password_hash"] = passwordHash;
        return j;
    }
};

inline void to_json(nlohmann::json& j, const Voter& v) {
    j = v.toClientJson();
}

} // namespace voting::domain
