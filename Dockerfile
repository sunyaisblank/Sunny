# Sunny MCP Server Docker Image
# Multi-stage build with uv for fast dependency installation

# =============================================================================
# Build Stage
# =============================================================================
FROM python:3.12-slim-bookworm AS builder

# Install uv
COPY --from=ghcr.io/astral-sh/uv:latest /uv /usr/local/bin/uv

WORKDIR /build

# Copy dependency files first for caching
COPY pyproject.toml uv.lock* ./

# Create virtual environment and install dependencies
RUN uv venv /opt/venv && \
    uv pip install --python /opt/venv/bin/python -e . --no-cache

# Copy source code
COPY src ./src

# =============================================================================
# Runtime Stage
# =============================================================================
FROM python:3.12-slim-bookworm AS runtime

# Install runtime dependencies (none needed for Sunny currently)
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Create non-root user
RUN useradd --create-home --shell /bin/bash sunny

WORKDIR /app

# Copy virtual environment from builder
COPY --from=builder /opt/venv /opt/venv

# Copy source and config
COPY --from=builder /build/src /app/src
COPY sunny.toml.example /app/sunny.toml.example

# Create directories
RUN mkdir -p /app/snapshots /app/audit && \
    chown -R sunny:sunny /app

# Set environment
ENV PATH="/opt/venv/bin:$PATH"
ENV PYTHONPATH="/app/src"
ENV SUNNY_SNAPSHOT_DIR="/app/snapshots"

USER sunny

# Expose default ports (for documentation - not actually used in stdio mode)
# TCP for Ableton commands
EXPOSE 9876
# UDP for OSC modulation
EXPOSE 9877

# Health check - verify Python can import sunny
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD python -c "import sunny.server.main" || exit 1

# Run the MCP server
ENTRYPOINT ["python", "-m", "sunny.server"]

# =============================================================================
# Labels
# =============================================================================
LABEL org.opencontainers.image.title="Sunny"
LABEL org.opencontainers.image.description="AI MCP Server for Ableton Live"
LABEL org.opencontainers.image.vendor="Sunny Project"
