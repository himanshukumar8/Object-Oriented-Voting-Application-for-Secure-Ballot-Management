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

    std::string appEnv = getEnvOrDefault("APP_ENV", "development");
    std::string host = getEnvOrDefault("HOST", "0.0.0.0");
    int port = std::stoi(getEnvOrDefault("PORT", "8080"));
    std::string staticDir = getEnvOrDefault("STATIC_DIR", "frontend/dist");

    std::string dbHost = getEnvOrDefault("DB_HOST", "localhost");
    std::string dbPort = getEnvOrDefault("DB_PORT", "5432");
    std::string dbName = getEnvOrDefault("DB_NAME", "secure_voting_db");
    std::string dbUser = getEnvOrDefault("DB_USER", "subscribly_user");

    const char* envDbPass = std::getenv("DB_PASSWORD");
    if (!envDbPass || !*envDbPass) {
        std::cerr << "[FATAL CONFIG ERROR] DB_PASSWORD environment variable is required to start the server." << std::endl;
        return 1;
    }
    std::string dbPass = envDbPass;

    const char* envJwtSecret = std::getenv("JWT_SECRET");
    std::string jwtSecret;
    if (appEnv == "production") {
        if (!envJwtSecret || std::string(envJwtSecret).length() < 32 || std::string(envJwtSecret).find("change_me") != std::string::npos) {
            std::cerr << "[FATAL CONFIG ERROR] In production mode, JWT_SECRET must be explicitly supplied with at least 32 characters and cannot use placeholder strings." << std::endl;
            return 1;
        }
        jwtSecret = envJwtSecret;
    } else {
        if (envJwtSecret && *envJwtSecret) {
            jwtSecret = envJwtSecret;
        } else {
            jwtSecret = "demo_jwt_secret_development_only_2026";
            std::cout << "[SECURITY WARNING] Running in " << appEnv << " mode with fallback demo JWT_SECRET. Do NOT use in production." << std::endl;
        }
    }

    const char* envReceiptSecret = std::getenv("RECEIPT_SECRET");
    std::string receiptSecret;
    if (appEnv == "production") {
        if (!envReceiptSecret || std::string(envReceiptSecret).length() < 32 || std::string(envReceiptSecret).find("change_me") != std::string::npos) {
            std::cerr << "[FATAL CONFIG ERROR] In production mode, RECEIPT_SECRET must be explicitly supplied with at least 32 characters." << std::endl;
            return 1;
        }
        receiptSecret = envReceiptSecret;
    } else {
        receiptSecret = (envReceiptSecret && *envReceiptSecret) ? envReceiptSecret : "demo_receipt_secret_development_only_2026";
    }

    std::string connStr = 
        "host=" + dbHost + " port=" + dbPort + 
        " dbname=" + dbName + " user=" + dbUser + 
        " password=" + dbPass;

    std::cout << "[CONFIG] Environment Mode: " << appEnv << std::endl;
    std::cout << "[CONFIG] Target Database: " << dbName << " on " << dbHost << ":" << dbPort << " (user: " << dbUser << ")" << std::endl;
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
        voting::services::VotingService votingService(voteRepo, voterRepo, electionRepo, auditRepo, receiptSecret);

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
