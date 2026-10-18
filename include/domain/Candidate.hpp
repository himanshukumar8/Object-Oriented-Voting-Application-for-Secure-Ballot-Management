#pragma once

#include <string>
#include "third_party/json.hpp"

namespace voting::domain {

class Candidate {
public:
    std::string id;
    std::string candidateCode;
    std::string fullName;
    std::string partyName;
    std::string partySymbol;
    std::string biography;
    std::string createdAt;

    Candidate() = default;

    Candidate(std::string id, std::string candidateCode, std::string fullName,
              std::string partyName, std::string partySymbol,
              std::string biography = "", std::string createdAt = "")
        : id(std::move(id)),
          candidateCode(std::move(candidateCode)),
          fullName(std::move(fullName)),
          partyName(std::move(partyName)),
          partySymbol(std::move(partySymbol)),
          biography(std::move(biography)),
          createdAt(std::move(createdAt)) {}

    nlohmann::json toJson() const {
        return {
            {"id", id},
            {"candidate_code", candidateCode},
            {"full_name", fullName},
            {"party_name", partyName},
            {"party_symbol", partySymbol},
            {"biography", biography},
            {"created_at", createdAt}
        };
    }

    static Candidate fromJson(const nlohmann::json& j) {
        Candidate c;
        if (j.contains("id")) c.id = j["id"].get<std::string>();
        if (j.contains("candidate_code")) c.candidateCode = j["candidate_code"].get<std::string>();
        if (j.contains("full_name")) c.fullName = j["full_name"].get<std::string>();
        if (j.contains("party_name")) c.partyName = j["party_name"].get<std::string>();
        if (j.contains("party_symbol")) c.partySymbol = j["party_symbol"].get<std::string>();
        if (j.contains("biography") && !j["biography"].is_null()) c.biography = j["biography"].get<std::string>();
        if (j.contains("created_at") && !j["created_at"].is_null()) c.createdAt = j["created_at"].get<std::string>();
        return c;
    }
};

inline void to_json(nlohmann::json& j, const Candidate& c) {
    j = c.toJson();
}

inline void from_json(const nlohmann::json& j, Candidate& c) {
    c = Candidate::fromJson(j);
}

} // namespace voting::domain
