#pragma once

#include <string>
#include <vector>
#include "domain/ElectionStatus.hpp"
#include "domain/Candidate.hpp"
#include "third_party/json.hpp"

namespace voting::domain {

class Election {
public:
    std::string id;
    std::string title;
    std::string description;
    ElectionStatus status{ElectionStatus::Draft};
    std::string startTime;
    std::string endTime;
    std::string createdAt;
    std::vector<Candidate> candidates;
    bool hasVoted{false}; // User-specific participation flag

    Election() = default;

    Election(std::string id, std::string title, std::string description,
             ElectionStatus status, std::string startTime, std::string endTime,
             std::string createdAt = "")
        : id(std::move(id)),
          title(std::move(title)),
          description(std::move(description)),
          status(status),
          startTime(std::move(startTime)),
          endTime(std::move(endTime)),
          createdAt(std::move(createdAt)) {}

    bool isVotingAllowed() const {
        return status == ElectionStatus::Open;
    }

    bool canTransitionTo(ElectionStatus newStatus) const {
        return canTransition(status, newStatus);
    }

    nlohmann::json toJson() const {
        nlohmann::json candJson = nlohmann::json::array();
        for (const auto& c : candidates) {
            candJson.push_back(c.toJson());
        }

        return {
            {"id", id},
            {"title", title},
            {"description", description},
            {"status", toString(status)},
            {"start_time", startTime},
            {"end_time", endTime},
            {"created_at", createdAt},
            {"candidates", candJson},
            {"has_voted", hasVoted}
        };
    }

    static Election fromJson(const nlohmann::json& j) {
        Election e;
        if (j.contains("id")) e.id = j["id"].get<std::string>();
        if (j.contains("title")) e.title = j["title"].get<std::string>();
        if (j.contains("description") && !j["description"].is_null()) e.description = j["description"].get<std::string>();
        if (j.contains("status")) e.status = fromString(j["status"].get<std::string>());
        if (j.contains("start_time")) e.startTime = j["start_time"].get<std::string>();
        if (j.contains("end_time")) e.endTime = j["end_time"].get<std::string>();
        if (j.contains("created_at") && !j["created_at"].is_null()) e.createdAt = j["created_at"].get<std::string>();
        return e;
    }
};

inline void to_json(nlohmann::json& j, const Election& e) {
    j = e.toJson();
}

inline void from_json(const nlohmann::json& j, Election& e) {
    e = Election::fromJson(j);
}

} // namespace voting::domain
