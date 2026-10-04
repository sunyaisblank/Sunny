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

COPY CMakeLists.txt CMakePresets.json pyproject.toml ./
COPY apps ./apps
COPY cmake ./cmake
COPY include ./include
COPY src ./src
# Configure-time inputs: the bridge contract generates the protocol header,
# and the version gate compares the Python and Max package metadata.
COPY remote_script/Sunny ./remote_script/Sunny
COPY python/sunny/__init__.py ./python/sunny/__init__.py
COPY max-package/CMakeLists.txt max-package/package-info.json ./max-package/

# Lean server build: no tests, no Python bindings
RUN cmake -B .bin -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DSUNNY_BUILD_TESTS=OFF \
        -DSUNNY_BUILD_PYTHON_BINDINGS=OFF \
    && cmake --build .bin --target sunny-mcp --parallel 2

# =============================================================================
# Runtime Stage
# =============================================================================
FROM ubuntu:24.04 AS runtime

RUN apt-get update && \
    apt-get install -y --no-install-recommends libstdc++6 \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --create-home --shell /bin/bash sunny \
    && mkdir /data && chown sunny:sunny /data

COPY --from=builder /build/.bin/sunny-mcp /usr/local/bin/sunny-mcp
COPY --from=builder /build/.bin/remote_script/Sunny /opt/sunny/remote-script/Sunny

USER sunny

# Mount a named volume at /data. Explicit workspace_save retains authored state;
# a later container restores the supported main file before accepting MCP requests.
ENV SUNNY_WORKSPACE_PATH=/data/workspace.sunny.json

# MCP protocol runs on stdio; the Ableton TCP connection is outbound and
# opt-in via SUNNY_ABLETON_HOST / SUNNY_TCP_PORT (see .env.example).
ENTRYPOINT ["sunny-mcp"]

# =============================================================================
# Labels
# =============================================================================
LABEL org.opencontainers.image.title="Sunny"
LABEL org.opencontainers.image.description="Music theory MCP server with Ableton Live integration"
LABEL org.opencontainers.image.vendor="Sunny Project"
