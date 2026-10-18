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

# Find dependencies with CMake discovery modules
find_package(PostgreSQL QUIET)
find_package(OpenSSL QUIET)

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

target_include_directories(voting_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# PostgreSQL include and linkage configuration
if(PostgreSQL_FOUND)
    target_include_directories(voting_core PUBLIC ${PostgreSQL_INCLUDE_DIRS})
    target_link_libraries(voting_core PUBLIC ${PostgreSQL_LIBRARIES})
else()
    if(WIN32 AND EXISTS "C:/Program Files/PostgreSQL/18/include")
        target_include_directories(voting_core PUBLIC "C:/Program Files/PostgreSQL/18/include")
        target_link_libraries(voting_core PUBLIC "C:/Program Files/PostgreSQL/18/bin/libpq.dll")
    else()
        target_link_libraries(voting_core PUBLIC pq)
    endif()
endif()

# OpenSSL include and linkage configuration
if(OpenSSL_FOUND)
    target_link_libraries(voting_core PUBLIC OpenSSL::Crypto OpenSSL::SSL)
    if(WIN32)
        target_link_libraries(voting_core PUBLIC ws2_32 crypt32)
    endif()
else()
    if(WIN32)
        if(EXISTS "C:/msys64/ucrt64/include")
            target_include_directories(voting_core PUBLIC "C:/msys64/ucrt64/include")
        endif()
        target_link_libraries(voting_core PUBLIC crypto ssl ws2_32 crypt32)
    else()
        target_link_libraries(voting_core PUBLIC crypto ssl pthread)
    endif()
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
