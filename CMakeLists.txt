cmake_minimum_required(VERSION 3.20)
project(SecureVotingPlatform VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable rich compiler warnings
if(MSVC)
    add_compile_options(/W4 /EHsc /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Core include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    "C:/Program Files/PostgreSQL/18/include"
    "C:/msys64/ucrt64/include"
)

# Core source files (excluding main.cpp for library reuse in tests)
set(CORE_SOURCES
    src/security/CryptoUtils.cpp
    src/security/PasswordHasher.cpp
    src/security/TokenService.cpp
    src/database/DbConnection.cpp
    src/database/DbPool.cpp
    src/database/CandidateRepository.cpp
    src/database/VoterRepository.cpp
    src/database/ElectionRepository.cpp
    src/database/VoteRepository.cpp
    src/database/AuditLogRepository.cpp
    src/services/AuthService.cpp
    src/services/ElectionService.cpp
    src/services/VotingService.cpp
    src/api/Server.cpp
)

# Create core static library for testability
add_library(voting_core STATIC ${CORE_SOURCES})

# Platform-specific library linking
if(WIN32)
    target_link_libraries(voting_core PUBLIC
        "C:/Program Files/PostgreSQL/18/bin/libpq.dll"
        crypto
        ssl
        ws2_32
        crypt32
    )
else()
    target_link_libraries(voting_core PUBLIC
        pq
        crypto
        ssl
        pthread
    )
endif()

# Main backend executable
add_executable(voting_server src/main.cpp)
target_link_libraries(voting_server PRIVATE voting_core)

# Unit & Invariant Test Suite
enable_testing()

add_executable(test_invariants tests/unit/test_invariants.cpp)
target_link_libraries(test_invariants PRIVATE voting_core)
add_test(NAME InvariantTests COMMAND test_invariants)

add_executable(test_crypto tests/unit/test_crypto.cpp)
target_link_libraries(test_crypto PRIVATE voting_core)
add_test(NAME CryptoTests COMMAND test_crypto)

add_executable(test_concurrency tests/concurrency/test_concurrency.cpp)
target_link_libraries(test_concurrency PRIVATE voting_core)
add_test(NAME ConcurrencyRaceTests COMMAND test_concurrency)
