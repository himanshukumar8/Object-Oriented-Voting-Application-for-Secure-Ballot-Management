#include <iostream>
#include <string>
#include <cstdlib>
#include <csignal>
#include <memory>

#include "database/DbPool.hpp"
#include "database/VoterRepository.hpp"
#include "database/CandidateRepository.hpp"
#include "database/ElectionRepository.hpp"
#include "database/VoteRepository.hpp"
#include "database/AuditLogRepository.hpp"
#include "security/TokenService.hpp"
#include "services/AuthService.hpp"
#include "services/ElectionService.hpp"
#include "services/VotingService.hpp"
#include "api/Server.hpp"

namespace {
    std::function<void(int)> shutdownHandler;
    void signalHandler(int signal) {
        if (shutdownHandler) shutdownHandler(signal);
    }

    std::string getEnvOrDefault(const char* var, const std::string& defaultValue) {
        const char* val = std::getenv(var);
        return val ? std::string(val) : defaultValue;
    }
}

int main() {
    std::cout << "===================================================================" << std::endl;
    std::cout << "      SECURE BALLOT MANAGEMENT PLATFORM - C++20 BACKEND ENGINE    " << std::endl;
    std::cout << "===================================================================" << std::endl;

    std::string host = getEnvOrDefault("HOST", "0.0.0.0");
    int port = std::stoi(getEnvOrDefault("PORT", "8080"));
    std::string staticDir = getEnvOrDefault("STATIC_DIR", "frontend/dist");

    std::string dbHost = getEnvOrDefault("DB_HOST", "localhost");
    std::string dbPort = getEnvOrDefault("DB_PORT", "5432");
    std::string dbName = getEnvOrDefault("DB_NAME", "secure_voting_db");
    std::string dbUser = getEnvOrDefault("DB_USER", "subscribly_user");
    std::string dbPass = getEnvOrDefault("DB_PASSWORD", "1");
    std::string jwtSecret = getEnvOrDefault("JWT_SECRET", "super_secure_voting_secret_token_key_2026_production");

    std::string connStr = 
        "host=" + dbHost + " port=" + dbPort + 
        " dbname=" + dbName + " user=" + dbUser + 
        " password=" + dbPass;

    std::cout << "[CONFIG] Target Database: " << dbName << " on " << dbHost << ":" << dbPort << std::endl;
    std::cout << "[CONFIG] HTTP Listener: " << host << ":" << port << std::endl;
    std::cout << "[CONFIG] Static Assets: " << staticDir << std::endl;

    try {
        std::cout << "[INIT] Initializing PostgreSQL connection pool..." << std::endl;
        voting::database::DbPool pool(connStr, 8);
        std::cout << "[INIT] Database connection pool online (8 connections ready)." << std::endl;

        // Repositories
        voting::database::VoterRepository voterRepo(pool);
        voting::database::CandidateRepository candidateRepo(pool);
        voting::database::ElectionRepository electionRepo(pool);
        voting::database::VoteRepository voteRepo(pool);
        voting::database::AuditLogRepository auditRepo(pool);

        // Security
        voting::security::TokenService tokenService(jwtSecret);

        // Services
        voting::services::AuthService authService(pool, tokenService, voterRepo, auditRepo);
        voting::services::ElectionService electionService(electionRepo, candidateRepo, auditRepo);
        voting::services::VotingService votingService(voteRepo, voterRepo, electionRepo, auditRepo);

        // API Server
        voting::api::ServerConfig serverConfig{host, port, staticDir};
        voting::api::Server server(
            serverConfig,
            pool,
            tokenService,
            authService,
            electionService,
            votingService,
            candidateRepo,
            voterRepo,
            auditRepo
        );

        shutdownHandler = [&server](int) {
            std::cout << "\n[SIGNAL] Received shutdown signal. Terminating gracefully..." << std::endl;
            server.stop();
        };

        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);

        std::cout << "[INIT] Bootstrapping REST API server..." << std::endl;
        server.start();

    } catch (const std::exception& e) {
        std::cerr << "[FATAL] Server initialization failed: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "[SHUTDOWN] Server terminated cleanly." << std::endl;
    return 0;
}
