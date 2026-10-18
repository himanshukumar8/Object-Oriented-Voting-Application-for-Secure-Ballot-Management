#include "api/Server.hpp"
#include "third_party/httplib.h"
#include "third_party/json.hpp"
#include "security/CryptoUtils.hpp"
#include <iostream>
#include <fstream>
#include <sstream>

namespace voting::api {

namespace {

void sendJson(httplib::Response& res, int statusCode, const nlohmann::json& j) {
    res.status = statusCode;
    res.set_content(j.dump(), "application/json");
}

void sendError(httplib::Response& res, int statusCode, const std::string& title,
               const std::string& detail = "", const std::string& type = "about:blank") {
    nlohmann::json err = {
        {"status", statusCode},
        {"title", title},
        {"detail", detail.empty() ? title : detail},
        {"type", type}
    };
    sendJson(res, statusCode, err);
}

bool extractBearerToken(const httplib::Request& req, std::string& outToken) {
    auto authHeader = req.get_header_value("Authorization");
    if (authHeader.rfind("Bearer ", 0) == 0) {
        outToken = authHeader.substr(7);
        return true;
    }
    return false;
}

} // anonymous namespace

Server::Server(
    ServerConfig config,
    database::DbPool& pool,
    security::TokenService& tokenService,
    services::AuthService& authService,
    services::ElectionService& electionService,
    services::VotingService& votingService,
    database::CandidateRepository& candidateRepo,
    database::VoterRepository& voterRepo,
    database::AuditLogRepository& auditRepo
) : config(std::move(config)),
    httpServer(std::make_unique<httplib::Server>()),
    pool(pool),
    tokenService(tokenService),
    authService(authService),
    electionService(electionService),
    votingService(votingService),
    candidateRepo(candidateRepo),
    voterRepo(voterRepo),
    auditRepo(auditRepo) {
    
    configureCors();
    registerRoutes();
}

Server::~Server() {
    stop();
}

void Server::configureCors() {
    httpServer->set_pre_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With, Idempotency-Key");
        res.set_header("Access-Control-Max-Age", "86400");

        if (req.method == "OPTIONS") {
            res.status = 204;
            return httplib::Server::HandlerResponse::Handled;
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });
}

void Server::registerRoutes() {
    // --------------------------------------------------------------------------
    // 1. Health & System Diagnostic Endpoint
    // --------------------------------------------------------------------------
    httpServer->Get("/api/health", [this](const httplib::Request&, httplib::Response& res) {
        bool dbHealthy = false;
        try {
            auto conn = pool.acquire();
            auto qRes = conn->execute("SELECT 1;");
            dbHealthy = qRes.isOk();
        } catch (...) {
            dbHealthy = false;
        }

        nlohmann::json health = {
            {"status", dbHealthy ? "UP" : "DOWN"},
            {"version", "1.0.0"},
            {"database", dbHealthy ? "CONNECTED" : "DISCONNECTED"},
            {"timestamp", "2026-10-18T00:00:00Z"}
        };
        sendJson(res, dbHealthy ? 200 : 503, health);
    });

    // --------------------------------------------------------------------------
    // 2. Authentication Endpoints
    // --------------------------------------------------------------------------
    httpServer->Post("/api/auth/admin/login", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            std::string username = body.value("username", "");
            std::string password = body.value("password", "");

            if (username.empty() || password.empty()) {
                sendError(res, 400, "Username and password are required");
                return;
            }

            auto authRes = authService.adminLogin(username, password, req.remote_addr);
            if (!authRes.success) {
                sendError(res, 401, authRes.errorMessage);
                return;
            }

            nlohmann::json resp = {
                {"token", authRes.token},
                {"user", authRes.user}
            };
            sendJson(res, 200, resp);
        } catch (const std::exception& e) {
            sendError(res, 400, "Malformed request payload", e.what());
        }
    });

    httpServer->Post("/api/auth/voter/login", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            std::string voterId = body.value("voter_id", "");
            std::string password = body.value("password", "");

            if (voterId.empty() || password.empty()) {
                sendError(res, 400, "Voter ID and password are required");
                return;
            }

            auto authRes = authService.voterLogin(voterId, password, req.remote_addr);
            if (!authRes.success) {
                sendError(res, 401, authRes.errorMessage);
                return;
            }

            nlohmann::json resp = {
                {"token", authRes.token},
                {"user", authRes.user}
            };
            sendJson(res, 200, resp);
        } catch (const std::exception& e) {
            sendError(res, 400, "Malformed request payload", e.what());
        }
    });

    httpServer->Get("/api/auth/me", [this](const httplib::Request& req, httplib::Response& res) {
        std::string token;
        if (!extractBearerToken(req, token)) {
            sendError(res, 401, "Authorization bearer token required");
            return;
        }

        security::TokenClaims claims;
        if (!tokenService.verifyToken(token, claims)) {
            sendError(res, 401, "Invalid or expired token");
            return;
        }

        nlohmann::json me = {
            {"user_id", claims.userId},
            {"role", claims.role},
            {"expires_at", claims.expiresAt}
        };
        sendJson(res, 200, me);
    });

    // --------------------------------------------------------------------------
    // 3. Elections Endpoints
    // --------------------------------------------------------------------------
    httpServer->Get("/api/elections", [this](const httplib::Request& req, httplib::Response& res) {
        std::string voterId;
        std::string token;
        if (extractBearerToken(req, token)) {
            security::TokenClaims claims;
            if (tokenService.verifyToken(token, claims) && claims.role == "VOTER") {
                voterId = claims.userId;
            }
        }

        auto elections = electionService.listElections(voterId);
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& e : elections) {
            arr.push_back(e.toJson());
        }
        sendJson(res, 200, arr);
    });

    httpServer->Post("/api/elections", [this](const httplib::Request& req, httplib::Response& res) {
        std::string token;
        security::TokenClaims claims;
        if (!extractBearerToken(req, token) || !tokenService.verifyToken(token, claims) || claims.role != "ADMIN") {
            sendError(res, 403, "Administrator privileges required");
            return;
        }

        try {
            auto body = nlohmann::json::parse(req.body);
            domain::Election election = domain::Election::fromJson(body);
            std::string id = electionService.createElection(election, claims.userId);
            sendJson(res, 201, {{"id", id}, {"message", "Election created successfully"}});
        } catch (const std::exception& e) {
            sendError(res, 400, "Invalid election data", e.what());
        }
    });

    httpServer->Get(R"(/api/elections/([a-f0-9\-]+))", [this](const httplib::Request& req, httplib::Response& res) {
        std::string electionId = req.matches[1];
        std::string voterId;
        std::string token;
        if (extractBearerToken(req, token)) {
            security::TokenClaims claims;
            if (tokenService.verifyToken(token, claims) && claims.role == "VOTER") {
                voterId = claims.userId;
            }
        }

        auto electionOpt = electionService.getElection(electionId, voterId);
        if (!electionOpt) {
            sendError(res, 404, "Election not found");
            return;
        }
        sendJson(res, 200, electionOpt->toJson());
    });

    httpServer->Get(R"(/api/elections/([a-f0-9\-]+)/candidates)", [this](const httplib::Request& req, httplib::Response& res) {
        std::string electionId = req.matches[1];
        auto electionOpt = electionService.getElection(electionId);
        if (!electionOpt) {
            sendError(res, 404, "Election not found");
            return;
        }
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& c : electionOpt->candidates) {
            arr.push_back(c.toJson());
        }
        sendJson(res, 200, arr);
    });

    httpServer->Get(R"(/api/elections/([a-f0-9\-]+)/voter-status)", [this](const httplib::Request& req, httplib::Response& res) {
        std::string electionId = req.matches[1];
        std::string token;
        security::TokenClaims claims;
        if (!extractBearerToken(req, token) || !tokenService.verifyToken(token, claims)) {
            sendError(res, 401, "Authentication required");
            return;
        }

        std::string voterId = claims.userId;
        auto electionOpt = electionService.getElection(electionId, voterId);
        if (!electionOpt) {
            sendError(res, 404, "Election not found");
            return;
        }

        sendJson(res, 200, {
            {"election_id", electionId},
            {"voter_id", voterId},
            {"is_eligible", true},
            {"has_voted", electionOpt->hasVoted}
        });
    });

    auto handleStatusUpdate = [this](const httplib::Request& req, httplib::Response& res) {
        std::string electionId = req.matches[1];
        std::string token;
        security::TokenClaims claims;
        if (!extractBearerToken(req, token) || !tokenService.verifyToken(token, claims) || claims.role != "ADMIN") {
            sendError(res, 403, "Administrator privileges required");
            return;
        }

        try {
            auto body = nlohmann::json::parse(req.body);
            std::string statusStr = body.value("status", "");
            auto newStatus = domain::fromString(statusStr);

            electionService.transitionStatus(electionId, newStatus, claims.userId);
            sendJson(res, 200, {{"status", domain::toString(newStatus)}, {"message", "Status updated successfully"}});
        } catch (const std::logic_error& e) {
            sendError(res, 409, "Illegal state transition", e.what());
        } catch (const std::exception& e) {
            sendError(res, 400, "Failed to update status", e.what());
        }
    };
    httpServer->Post(R"(/api/elections/([a-f0-9\-]+)/status)", handleStatusUpdate);
    httpServer->Put(R"(/api/elections/([a-f0-9\-]+)/status)", handleStatusUpdate);

    httpServer->Post(R"(/api/elections/([a-f0-9\-]+)/candidates)", [this](const httplib::Request& req, httplib::Response& res) {
        std::string electionId = req.matches[1];
        std::string token;
        security::TokenClaims claims;
        if (!extractBearerToken(req, token) || !tokenService.verifyToken(token, claims) || claims.role != "ADMIN") {
            sendError(res, 403, "Administrator privileges required");
            return;
        }

        try {
            auto body = nlohmann::json::parse(req.body);
            std::string candidateId = body.value("candidate_id", "");
            if (candidateId.empty()) {
                sendError(res, 400, "candidate_id is required");
                return;
            }

            electionService.assignCandidate(electionId, candidateId, claims.userId);
            sendJson(res, 200, {{"message", "Candidate assigned to election"}});
        } catch (const std::logic_error& e) {
            sendError(res, 409, "Cannot assign candidate", e.what());
        } catch (const std::exception& e) {
            sendError(res, 400, "Failed to assign candidate", e.what());
        }
    });

    // --------------------------------------------------------------------------
    // 4. Candidates Endpoints
    // --------------------------------------------------------------------------
    httpServer->Get("/api/candidates", [this](const httplib::Request&, httplib::Response& res) {
        auto list = candidateRepo.findAll();
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& c : list) {
            arr.push_back(c.toJson());
        }
        sendJson(res, 200, arr);
    });

    httpServer->Post("/api/candidates", [this](const httplib::Request& req, httplib::Response& res) {
        std::string token;
        security::TokenClaims claims;
        if (!extractBearerToken(req, token) || !tokenService.verifyToken(token, claims) || claims.role != "ADMIN") {
            sendError(res, 403, "Administrator privileges required");
            return;
        }

        try {
            auto body = nlohmann::json::parse(req.body);
            domain::Candidate candidate = domain::Candidate::fromJson(body);
            std::string id = candidateRepo.create(candidate);
            sendJson(res, 201, {{"id", id}, {"message", "Candidate enrolled successfully"}});
        } catch (const std::exception& e) {
            sendError(res, 400, "Failed to enroll candidate", e.what());
        }
    });

    // --------------------------------------------------------------------------
    // 5. Voters Endpoints
    // --------------------------------------------------------------------------
    httpServer->Get("/api/voters", [this](const httplib::Request& req, httplib::Response& res) {
        std::string token;
        security::TokenClaims claims;
        if (!extractBearerToken(req, token) || !tokenService.verifyToken(token, claims) || claims.role != "ADMIN") {
            sendError(res, 403, "Administrator privileges required");
            return;
        }

        auto list = voterRepo.findAll();
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& v : list) {
            arr.push_back(v.toClientJson());
        }
        sendJson(res, 200, arr);
    });

    httpServer->Post("/api/voters", [this](const httplib::Request& req, httplib::Response& res) {
        std::string token;
        security::TokenClaims claims;
        if (!extractBearerToken(req, token) || !tokenService.verifyToken(token, claims) || claims.role != "ADMIN") {
            sendError(res, 403, "Administrator privileges required");
            return;
        }

        try {
            auto body = nlohmann::json::parse(req.body);
            std::string voterId = body.value("voter_id", "");
            std::string fullName = body.value("full_name", "");
            std::string email = body.value("email", "");
            std::string rawPassword = body.value("password", "VoterPass2026!");

            if (voterId.empty() || fullName.empty() || email.empty()) {
                sendError(res, 400, "voter_id, full_name, and email are required");
                return;
            }

            std::string passHash = security::PasswordHasher::hashPassword(rawPassword);
            domain::Voter voter("", voterId, fullName, email, passHash, true);
            std::string id = voterRepo.create(voter);
            sendJson(res, 201, {{"id", id}, {"message", "Voter registered successfully"}});
        } catch (const std::exception& e) {
            sendError(res, 400, "Failed to register voter", e.what());
        }
    });

    // --------------------------------------------------------------------------
    // 6. Ballot Submission Endpoint (Voting)
    // --------------------------------------------------------------------------
    httpServer->Post("/api/votes", [this](const httplib::Request& req, httplib::Response& res) {
        std::string token;
        security::TokenClaims claims;
        if (!extractBearerToken(req, token) || !tokenService.verifyToken(token, claims)) {
            sendError(res, 401, "Authentication required to cast a ballot");
            return;
        }

        if (claims.role != "VOTER") {
            sendError(res, 403, "Only verified voters can cast ballots");
            return;
        }

        try {
            auto body = nlohmann::json::parse(req.body);
            domain::BallotSubmission submission = domain::BallotSubmission::fromJson(body);
            
            // Check Idempotency-Key header if not in JSON body
            if (submission.idempotencyKey.empty()) {
                submission.idempotencyKey = req.get_header_value("Idempotency-Key");
            }

            domain::BallotReceipt receipt = votingService.castBallot(claims.userId, submission, req.remote_addr);
            sendJson(res, 201, receipt.toJson());
        } catch (const std::logic_error& e) {
            std::string msg = e.what();
            if (msg.find("already") != std::string::npos || msg.find("DOUBLE_VOTE") != std::string::npos) {
                sendError(res, 409, "Double Voting Prohibited", msg);
            } else if (msg.find("not registered") != std::string::npos || msg.find("eligible") != std::string::npos) {
                sendError(res, 403, "Ineligible Voter", msg);
            } else {
                sendError(res, 400, "Ballot Validation Error", msg);
            }
        } catch (const std::exception& e) {
            sendError(res, 500, "Transaction processing failed", e.what());
        }
    });

    // --------------------------------------------------------------------------
    // 7. Results & Certified Tallies Endpoint
    // --------------------------------------------------------------------------
    httpServer->Get(R"(/api/elections/([a-f0-9\-]+)/results)", [this](const httplib::Request& req, httplib::Response& res) {
        std::string electionId = req.matches[1];
        std::string userRole;
        std::string token;
        if (extractBearerToken(req, token)) {
            security::TokenClaims claims;
            if (tokenService.verifyToken(token, claims)) {
                userRole = claims.role;
            }
        }

        try {
            auto results = votingService.getElectionResults(electionId, userRole);
            sendJson(res, 200, results.toJson());
        } catch (const std::logic_error& e) {
            sendError(res, 403, "Results Confidential", e.what());
        } catch (const std::exception& e) {
            sendError(res, 404, "Results query failed", e.what());
        }
    });

    // --------------------------------------------------------------------------
    // 8. Administrative Audit Logs
    // --------------------------------------------------------------------------
    auto handleAuditLogs = [this](const httplib::Request& req, httplib::Response& res) {
        std::string token;
        security::TokenClaims claims;
        if (!extractBearerToken(req, token) || !tokenService.verifyToken(token, claims) || claims.role != "ADMIN") {
            sendError(res, 403, "Administrator privileges required");
            return;
        }

        auto logs = auditRepo.findRecent(50);
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& log : logs) {
            arr.push_back(log.toJson());
        }
        sendJson(res, 200, arr);
    };
    httpServer->Get("/api/admin/audit-logs", handleAuditLogs);
    httpServer->Get("/api/audit-logs", handleAuditLogs);

    // --------------------------------------------------------------------------
    // 9. Static File Serving (Built Frontend)
    // --------------------------------------------------------------------------
    httpServer->set_mount_point("/", config.staticDir);
}

void Server::start() {
    std::cout << "[HTTP_SERVER] Starting server on " << config.host << ":" << config.port << std::endl;
    if (!httpServer->listen(config.host, config.port)) {
        throw std::runtime_error("Failed to start HTTP server on " + config.host + ":" + std::to_string(config.port));
    }
}

void Server::stop() {
    if (httpServer && httpServer->is_running()) {
        std::cout << "[HTTP_SERVER] Stopping HTTP server..." << std::endl;
        httpServer->stop();
    }
}

} // namespace voting::api
