#include "database/CandidateRepository.hpp"
#include <stdexcept>

namespace voting::database {

namespace {

domain::Candidate rowToCandidate(const DbResult& res, int row) {
    domain::Candidate c;
    c.id = res.getValue(row, "id");
    c.candidateCode = res.getValue(row, "candidate_code");
    c.fullName = res.getValue(row, "full_name");
    c.partyName = res.getValue(row, "party_name");
    c.partySymbol = res.getValue(row, "party_symbol");
    c.biography = res.getValue(row, "biography");
    c.createdAt = res.getValue(row, "created_at");
    return c;
}

} // anonymous namespace

std::string CandidateRepository::create(const domain::Candidate& candidate) {
    auto conn = pool.acquire();
    std::string sql = 
        "INSERT INTO candidates (candidate_code, full_name, party_name, party_symbol, biography) "
        "VALUES ($1, $2, $3, $4, $5) RETURNING id;";
    
    std::vector<std::string> params = {
        candidate.candidateCode,
        candidate.fullName,
        candidate.partyName,
        candidate.partySymbol,
        candidate.biography
    };

    auto res = conn->executeParams(sql, params);
    if (!res.isOk() || res.rowCount() == 0) {
        throw std::runtime_error("Failed to insert candidate: " + res.errorMessage());
    }

    return res.getValue(0, 0);
}

std::optional<domain::Candidate> CandidateRepository::findById(const std::string& id) {
    auto conn = pool.acquire();
    std::string sql = "SELECT * FROM candidates WHERE id = $1::uuid;";
    auto res = conn->executeParams(sql, {id});
    if (!res.isOk() || res.rowCount() == 0) {
        return std::nullopt;
    }
    return rowToCandidate(res, 0);
}

std::optional<domain::Candidate> CandidateRepository::findByCode(const std::string& code) {
    auto conn = pool.acquire();
    std::string sql = "SELECT * FROM candidates WHERE candidate_code = $1;";
    auto res = conn->executeParams(sql, {code});
    if (!res.isOk() || res.rowCount() == 0) {
        return std::nullopt;
    }
    return rowToCandidate(res, 0);
}

std::vector<domain::Candidate> CandidateRepository::findAll() {
    auto conn = pool.acquire();
    std::string sql = "SELECT * FROM candidates ORDER BY candidate_code ASC;";
    auto res = conn->execute(sql);
    
    std::vector<domain::Candidate> list;
    if (!res.isOk()) return list;

    list.reserve(res.rowCount());
    for (int i = 0; i < res.rowCount(); ++i) {
        list.push_back(rowToCandidate(res, i));
    }
    return list;
}

std::vector<domain::Candidate> CandidateRepository::findForElection(const std::string& electionId) {
    auto conn = pool.acquire();
    std::string sql = 
        "SELECT c.* FROM candidates c "
        "JOIN election_candidates ec ON c.id = ec.candidate_id "
        "WHERE ec.election_id = $1::uuid ORDER BY c.candidate_code ASC;";
    
    auto res = conn->executeParams(sql, {electionId});
    std::vector<domain::Candidate> list;
    if (!res.isOk()) return list;

    list.reserve(res.rowCount());
    for (int i = 0; i < res.rowCount(); ++i) {
        list.push_back(rowToCandidate(res, i));
    }
    return list;
}

} // namespace voting::database
