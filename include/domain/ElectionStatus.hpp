#pragma once

#include <string>
#include <stdexcept>

namespace voting::domain {

enum class ElectionStatus {
    Draft,
    Scheduled,
    Open,
    Closed,
    ResultsPublished
};

inline std::string toString(ElectionStatus status) {
    switch (status) {
        case ElectionStatus::Draft:            return "DRAFT";
        case ElectionStatus::Scheduled:        return "SCHEDULED";
        case ElectionStatus::Open:             return "OPEN";
        case ElectionStatus::Closed:           return "CLOSED";
        case ElectionStatus::ResultsPublished: return "RESULTS_PUBLISHED";
    }
    return "UNKNOWN";
}

inline ElectionStatus fromString(const std::string& str) {
    if (str == "DRAFT")             return ElectionStatus::Draft;
    if (str == "SCHEDULED")         return ElectionStatus::Scheduled;
    if (str == "OPEN")              return ElectionStatus::Open;
    if (str == "CLOSED")            return ElectionStatus::Closed;
    if (str == "RESULTS_PUBLISHED") return ElectionStatus::ResultsPublished;
    throw std::invalid_argument("Invalid election status: " + str);
}

/**
 * Validates allowed state transitions in the election lifecycle.
 * Allowed deterministic graph:
 * DRAFT -> SCHEDULED
 * SCHEDULED -> OPEN
 * OPEN -> CLOSED
 * CLOSED -> RESULTS_PUBLISHED
 */
inline bool canTransition(ElectionStatus from, ElectionStatus to) {
    if (from == to) return true; // Idempotent no-op
    switch (from) {
        case ElectionStatus::Draft:
            return to == ElectionStatus::Scheduled;
        case ElectionStatus::Scheduled:
            return to == ElectionStatus::Open;
        case ElectionStatus::Open:
            return to == ElectionStatus::Closed;
        case ElectionStatus::Closed:
            return to == ElectionStatus::ResultsPublished;
        case ElectionStatus::ResultsPublished:
            return false; // Terminal state; no transitions permitted
    }
    return false;
}

} // namespace voting::domain
