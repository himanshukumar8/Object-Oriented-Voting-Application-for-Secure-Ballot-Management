#include "database/VoterRepository.hpp"
#include <iostream>
#include <stdexcept>

namespace voting::database {

namespace {

domain::Voter rowToVoter(const DbResult& res, int row) {
    domain::Voter v;
    v.id = res.getValue(row, "id");
    v.voterId = res.getValue(row, "voter_id");
    v.fullName = res.getValue(row, "full_name");
    v.email = res.getValue(row, "email");
    v.passwordHash = res.getValue(row, "password_hash");
    v.isActive = (res.getValue(row, "is_active") == "t");
    v.createdAt = res.getValue(row, "created_at");
    return v;
}

} // anonymous namespace

std::string VoterRepository::create(const domain::Voter& voter) {
    auto conn = pool.acquire();
    std::string sql = 
        "INSERT INTO voters (voter_id, full_name, email, password_hash, is_active) "
        "VALUES ($1, $2, $3, $4, $5) RETURNING id;";
    
    std::vector<std::string> params = {
        voter.voterId,
        voter.fullName,
        voter.email,
        voter.passwordHash,
        voter.isActive ? "true" : "false"
    };

    auto res = conn->executeParams(sql, params);
    if (!res.isOk() || res.rowCount() == 0) {
        throw std::runtime_error("Failed to insert voter: " + res.errorMessage());
    }

    std::string newId = res.getValue(0, 0);
    // Auto-enroll voter into active/draft/scheduled elections
    conn->executeParams(
        "INSERT INTO election_voters (election_id, voter_id, is_eligible) "
        "SELECT id, $1::uuid, TRUE FROM elections WHERE status IN ('DRAFT', 'SCHEDULED', 'OPEN') "
        "ON CONFLICT DO NOTHING;",
        {newId}
    );

    return newId;
}

std::optional<domain::Voter> VoterRepository::findById(const std::string& id) {
    auto conn = pool.acquire();
    std::string sql = "SELECT * FROM voters WHERE id = $1::uuid;";
    auto res = conn->executeParams(sql, {id});
    if (!res.isOk() || res.rowCount() == 0) {
        return std::nullopt;
    }
    return rowToVoter(res, 0);
}

std::optional<domain::Voter> VoterRepository::findByVoterId(const std::string& voterId) {
    auto conn = pool.acquire();
    std::string sql = "SELECT * FROM voters WHERE voter_id = $1;";
    auto res = conn->executeParams(sql, {voterId});
    if (!res.isOk() || res.rowCount() == 0) {
        return std::nullopt;
    }
    return rowToVoter(res, 0);
}

std::optional<domain::Voter> VoterRepository::findByEmail(const std::string& email) {
    auto conn = pool.acquire();
    std::string sql = "SELECT * FROM voters WHERE email = $1;";
    auto res = conn->executeParams(sql, {email});
    if (!res.isOk() || res.rowCount() == 0) {
        return std::nullopt;
    }
    return rowToVoter(res, 0);
}

std::vector<domain::Voter> VoterRepository::findAll() {
    auto conn = pool.acquire();
    std::string sql = "SELECT * FROM voters ORDER BY voter_id ASC;";
    auto res = conn->execute(sql);
    
    std::vector<domain::Voter> list;
    if (!res.isOk()) return list;

    list.reserve(res.rowCount());
    for (int i = 0; i < res.rowCount(); ++i) {
        list.push_back(rowToVoter(res, i));
    }
    return list;
}

bool VoterRepository::isEligibleForElection(const std::string& electionId, const std::string& voterId) {
    auto conn = pool.acquire();
    std::string sql = 
        "SELECT 1 FROM election_voters ev "
        "JOIN voters v ON ev.voter_id = v.id "
        "WHERE ev.election_id = $1::uuid AND (CAST(v.id AS TEXT) = $2 OR v.voter_id = $2) AND ev.is_eligible = TRUE AND v.is_active = TRUE;";
    
    auto res = conn->executeParams(sql, {electionId, voterId});
    if (!res.isOk()) {
        std::cerr << "[VOTER_REPO_ERROR] isEligibleForElection failed: " << res.errorMessage() << std::endl;
        return false;
    }
    return res.rowCount() > 0;
}

} // namespace voting::database
