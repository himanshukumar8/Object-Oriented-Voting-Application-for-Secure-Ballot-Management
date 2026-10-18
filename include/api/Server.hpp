#pragma once

#include "database/DbPool.hpp"
#include "services/AuthService.hpp"
#include "services/ElectionService.hpp"
#include "services/VotingService.hpp"
#include "database/CandidateRepository.hpp"
#include "database/VoterRepository.hpp"
#include "database/AuditLogRepository.hpp"
#include "security/TokenService.hpp"
#include <string>
#include <memory>

namespace httplib {
    class Server;
}

namespace voting::api {

struct ServerConfig {
    std::string host{"0.0.0.0"};
    int port{8080};
    std::string staticDir{"frontend/dist"};
};

class Server {
private:
    ServerConfig config;
    std::unique_ptr<httplib::Server> httpServer;
    
    database::DbPool& pool;
    security::TokenService& tokenService;
    services::AuthService& authService;
    services::ElectionService& electionService;
    services::VotingService& votingService;
    database::CandidateRepository& candidateRepo;
    database::VoterRepository& voterRepo;
    database::AuditLogRepository& auditRepo;

    void registerRoutes();
    void configureCors();

public:
    Server(
        ServerConfig config,
        database::DbPool& pool,
        security::TokenService& tokenService,
        services::AuthService& authService,
        services::ElectionService& electionService,
        services::VotingService& votingService,
        database::CandidateRepository& candidateRepo,
        database::VoterRepository& voterRepo,
        database::AuditLogRepository& auditRepo
    );

    ~Server();

    void start();
    void stop();
};

} // namespace voting::api
