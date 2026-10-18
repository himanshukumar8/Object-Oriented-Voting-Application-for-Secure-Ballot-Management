#pragma once

#include <string>
#include <vector>
#include "third_party/json.hpp"

namespace voting::domain {

struct BallotSubmission {
    std::string electionId;
    std::string candidateId;
    std::string idempotencyKey;

    static BallotSubmission fromJson(const nlohmann::json& j) {
        BallotSubmission s;
        if (j.contains("election_id")) s.electionId = j["election_id"].get<std::string>();
        if (j.contains("candidate_id")) s.candidateId = j["candidate_id"].get<std::string>();
        if (j.contains("idempotency_key")) s.idempotencyKey = j["idempotency_key"].get<std::string>();
        return s;
    }
};

struct BallotReceipt {
    std::string id;
    std::string electionId;
    std::string voterId;
    std::string idempotencyKey;
    std::string receiptHash;
    std::string castAt;

    nlohmann::json toJson() const {
        return {
            {"receipt_id", id},
            {"election_id", electionId},
            {"voter_id", voterId},
            {"receipt_hash", receiptHash},
            {"cast_at", castAt},
            {"status", "CONFIRMED"}
        };
    }
};

struct CandidateTally {
    std::string candidateId;
    std::string candidateCode;
    std::string fullName;
    std::string partyName;
    std::string partySymbol;
    int voteCount{0};
    double percentage{0.0};

    nlohmann::json toJson() const {
        return {
            {"candidate_id", candidateId},
            {"candidate_code", candidateCode},
            {"full_name", fullName},
            {"party_name", partyName},
            {"party_symbol", partySymbol},
            {"vote_count", voteCount},
            {"percentage", percentage}
        };
    }
};

struct ElectionResult {
    std::string electionId;
    std::string title;
    std::string status;
    int totalVotes{0};
    int totalEligibleVoters{0};
    double turnoutPercentage{0.0};
    std::vector<CandidateTally> tallies;
    std::string certifiedAt;

    nlohmann::json toJson() const {
        nlohmann::json tallyArray = nlohmann::json::array();
        for (const auto& t : tallies) {
            tallyArray.push_back(t.toJson());
        }

        return {
            {"election_id", electionId},
            {"title", title},
            {"status", status},
            {"total_votes", totalVotes},
            {"total_eligible_voters", totalEligibleVoters},
            {"turnout_percentage", turnoutPercentage},
            {"tallies", tallyArray},
            {"certified_at", certifiedAt}
        };
    }
};

} // namespace voting::domain
