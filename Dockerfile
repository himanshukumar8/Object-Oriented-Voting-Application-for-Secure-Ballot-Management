# ==============================================================================
# AegisVote Multi-Stage Production Dockerfile
# Builds C++20 Backend + React Vite Frontend in unified high-performance container
# ==============================================================================

# ------------------------------------------------------------------------------
# STAGE 1: Frontend Build (Node.js 22 LTS)
# ------------------------------------------------------------------------------
FROM node:22-alpine AS frontend-builder
WORKDIR /app/frontend

COPY frontend/package*.json ./
RUN npm ci

COPY frontend/ ./
RUN npm run build

# ------------------------------------------------------------------------------
# STAGE 2: C++20 Backend Build (Debian Bookworm GCC 12+)
# ------------------------------------------------------------------------------
FROM debian:bookworm-slim AS backend-builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    libpq-dev \
    libssl-dev \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy CMake and C++ source code
COPY CMakeLists.txt ./
COPY include/ ./include/
COPY src/ ./src/
COPY tests/ ./tests/

# Build backend binaries with Ninja
RUN cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
RUN cmake --build build --target voting_server test_invariants test_crypto test_concurrency

# ------------------------------------------------------------------------------
# STAGE 3: Minimal Production Runtime
# ------------------------------------------------------------------------------
FROM debian:bookworm-slim AS runtime

# Install minimal runtime dependencies (libpq5, libssl3)
RUN apt-get update && apt-get install -y --no-install-recommends \
    libpq5 \
    libssl3 \
    ca-certificates \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Create unprivileged service user
RUN groupadd -r aegisvote && useradd -r -g aegisvote -d /app -s /sbin/nologin aegisvote

WORKDIR /app

# Copy compiled C++ server binary and test utilities
COPY --from=backend-builder /app/build/voting_server ./voting_server
COPY --from=backend-builder /app/build/test_invariants ./test_invariants
COPY --from=backend-builder /app/build/test_crypto ./test_crypto
COPY --from=backend-builder /app/build/test_concurrency ./test_concurrency

# Copy static frontend bundle built in Stage 1
COPY --from=frontend-builder /app/frontend/dist ./frontend/dist

# Copy database migrations and seed schemas
COPY database/ ./database/

# Set file permissions for non-root execution
RUN chown -R aegisvote:aegisvote /app && chmod +x ./voting_server

USER aegisvote

EXPOSE 8080

HEALTHCHECK --interval=15s --timeout=3s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:8080/api/health || exit 1

ENTRYPOINT ["./voting_server"]
