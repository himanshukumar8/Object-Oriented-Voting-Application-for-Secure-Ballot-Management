#include "database/ElectionRepository.hpp"
#include <stdexcept>

namespace voting::database {

namespace {

domain::Election rowToElection(const DbResult& res, int row) {
    domain::Election e;
    e.id = res.getValue(row, "id");
    e.title = res.getValue(row, "title");
    e.description = res.getValue(row, "description");
    e.status = domain::fromString(res.getValue(row, "status"));
    e.startTime = res.getValue(row, "start_time");
    e.endTime = res.getValue(row, "end_time");
    e.createdAt = res.getValue(row, "created_at");
    if (!res.isNull(row, "has_voted")) {
        e.hasVoted = (res.getValue(row, "has_voted") == "t" || res.getValue(row, "has_voted") == "1");
    }
    return e;
}

} // anonymous namespace

std::string ElectionRepository::create(const domain::Election& election) {
    auto conn = pool.acquire();
    std::string sql = 
        "INSERT INTO elections (title, description, status, start_time, end_time) "
        "VALUES ($1, $2, $3, $4::timestamptz, $5::timestamptz) RETURNING id;";
    
    std::vector<std::string> params = {
        election.title,
        election.description,
        domain::toString(election.status),
        election.startTime,
        election.endTime
    };

    auto res = conn->executeParams(sql, params);
    if (!res.isOk() || res.rowCount() == 0) {
        throw std::runtime_error("Failed to insert election: " + res.errorMessage());
    }

    return res.getValue(0, 0);
}

std::optional<domain::Election> ElectionRepository::findById(const std::string& id, const std::string& voterId) {
    auto conn = pool.acquire();
    std::string sql;
    std::vector<std::string> params;

    if (!voterId.empty()) {
        sql = 
            "SELECT e.*, "
            "EXISTS(SELECT 1 FROM ballot_receipts br JOIN voters v ON br.voter_id = v.id "
            "       WHERE br.election_id = e.id AND (CAST(v.id AS TEXT) = $2 OR v.voter_id = $2)) AS has_voted "
            "FROM elections e WHERE e.id = $1::uuid;";
        params = {id, voterId};
    } else {
        sql = "SELECT e.*, FALSE as has_voted FROM elections e WHERE e.id = $1::uuid;";
        params = {id};
    }

    auto res = conn->executeParams(sql, params);
    if (!res.isOk() || res.rowCount() == 0) {
        return std::nullopt;
    }

    auto election = rowToElection(res, 0);

    // Populate assigned candidates
    std::string candSql = 
        "SELECT c.* FROM candidates c "
        "JOIN election_candidates ec ON c.id = ec.candidate_id "
        "WHERE ec.election_id = $1::uuid ORDER BY c.candidate_code ASC;";
    
    auto candRes = conn->executeParams(candSql, {election.id});
    if (candRes.isOk()) {
        for (int i = 0; i < candRes.rowCount(); ++i) {
            domain::Candidate c;
            c.id = candRes.getValue(i, "id");
            c.candidateCode = candRes.getValue(i, "candidate_code");
            c.fullName = candRes.getValue(i, "full_name");
            c.partyName = candRes.getValue(i, "party_name");
            c.partySymbol = candRes.getValue(i, "party_symbol");
            c.biography = candRes.getValue(i, "biography");
            c.createdAt = candRes.getValue(i, "created_at");
            election.candidates.push_back(std::move(c));
        }
    }

    return election;
}

std::vector<domain::Election> ElectionRepository::findAll(const std::string& voterId) {
    auto conn = pool.acquire();
    std::string sql;
    std::vector<std::string> params;

    if (!voterId.empty()) {
        sql = 
            "SELECT e.*, "
            "EXISTS(SELECT 1 FROM ballot_receipts br JOIN voters v ON br.voter_id = v.id "
            "       WHERE br.election_id = e.id AND (CAST(v.id AS TEXT) = $1 OR v.voter_id = $1)) AS has_voted "
            "FROM elections e ORDER BY e.created_at DESC;";
        params = {voterId};
    } else {
        sql = "SELECT e.*, FALSE as has_voted FROM elections e ORDER BY e.created_at DESC;";
    }

    auto res = params.empty() ? conn->execute(sql) : conn->executeParams(sql, params);
    std::vector<domain::Election> list;
    if (!res.isOk()) return list;

    list.reserve(res.rowCount());
    for (int i = 0; i < res.rowCount(); ++i) {
        auto election = rowToElection(res, i);

        // Fetch candidate count/slate for each election
        std::string candSql = 
            "SELECT c.* FROM candidates c "
            "JOIN election_candidates ec ON c.id = ec.candidate_id "
            "WHERE ec.election_id = $1::uuid ORDER BY c.candidate_code ASC;";
        auto candRes = conn->executeParams(candSql, {election.id});
        if (candRes.isOk()) {
            for (int c = 0; c < candRes.rowCount(); ++c) {
                domain::Candidate cand;
                cand.id = candRes.getValue(c, "id");
                cand.candidateCode = candRes.getValue(c, "candidate_code");
                cand.fullName = candRes.getValue(c, "full_name");
                cand.partyName = candRes.getValue(c, "party_name");
                cand.partySymbol = candRes.getValue(c, "party_symbol");
                election.candidates.push_back(std::move(cand));
            }
        }
        list.push_back(std::move(election));
    }

    return list;
}

void ElectionRepository::updateStatus(const std::string& id, domain::ElectionStatus newStatus) {
    auto conn = pool.acquire();
    std::string sql = "UPDATE elections SET status = $1 WHERE id = $2::uuid;";
    auto res = conn->executeParams(sql, {domain::toString(newStatus), id});
    if (!res.isOk()) {
        throw std::runtime_error("Failed to update election status: " + res.errorMessage());
    }
}

void ElectionRepository::assignCandidate(const std::string& electionId, const std::string& candidateId) {
    auto conn = pool.acquire();
    std::string sql = 
        "INSERT INTO election_candidates (election_id, candidate_id) "
        "VALUES ($1::uuid, $2::uuid) ON CONFLICT DO NOTHING;";
    auto res = conn->executeParams(sql, {electionId, candidateId});
    if (!res.isOk()) {
        throw std::runtime_error("Failed to assign candidate to election: " + res.errorMessage());
    }
}

void ElectionRepository::assignVoterEligibility(const std::string& electionId, const std::string& voterId) {
    auto conn = pool.acquire();
    std::string sql = 
        "INSERT INTO election_voters (election_id, voter_id, is_eligible) "
        "VALUES ($1::uuid, $2::uuid, TRUE) ON CONFLICT DO NOTHING;";
    auto res = conn->executeParams(sql, {electionId, voterId});
    if (!res.isOk()) {
        throw std::runtime_error("Failed to assign voter eligibility: " + res.errorMessage());
    }
}

} // namespace voting::database
