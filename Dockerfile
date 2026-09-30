# Sunny MCP Server Docker Image
# Builds the C++ sunny-mcp binary; the container speaks MCP on stdio and
# connects to Sunny's Ableton Remote Script when SUNNY_ABLETON_HOST is set.

# =============================================================================
# Build Stage
# =============================================================================
FROM ubuntu:24.04 AS builder

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        g++ cmake ninja-build git ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build

COPY CMakeLists.txt CMakePresets.json ./
COPY apps ./apps
COPY cmake ./cmake
COPY include ./include
COPY src ./src

# Lean server build: no tests, no Python bindings
RUN cmake -B .bin -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DSUNNY_BUILD_TESTS=OFF \
        -DSUNNY_BUILD_PYTHON_BINDINGS=OFF \
    && cmake --build .bin --target sunny-mcp

# =============================================================================
# Runtime Stage
# =============================================================================
FROM ubuntu:24.04 AS runtime

RUN apt-get update && \
    apt-get install -y --no-install-recommends libstdc++6 \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --create-home --shell /bin/bash sunny

COPY --from=builder /build/.bin/sunny-mcp /usr/local/bin/sunny-mcp

USER sunny

# MCP protocol runs on stdio; the Ableton TCP connection is outbound and
# opt-in via SUNNY_ABLETON_HOST / SUNNY_TCP_PORT (see .env.example).
ENTRYPOINT ["sunny-mcp"]

# =============================================================================
# Labels
# =============================================================================
LABEL org.opencontainers.image.title="Sunny"
LABEL org.opencontainers.image.description="Music theory MCP server with Ableton Live integration"
LABEL org.opencontainers.image.vendor="Sunny Project"
