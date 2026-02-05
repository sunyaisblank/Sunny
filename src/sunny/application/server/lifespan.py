"""Server Lifecycle Management.

Component: SVLF001A
Domain: SV (Server) | Category: LF (Lifespan)

Manages server startup and shutdown, including:
- Ableton connection establishment
- Theory engine initialization
- Snapshot manager setup
- Security configuration
"""

from __future__ import annotations

import logging
import os
from contextlib import asynccontextmanager
from pathlib import Path
from typing import TYPE_CHECKING, Any, AsyncIterator

from mcp.server.fastmcp import FastMCP

from sunny.infrastructure.audit import (
    AuditConfig,
    AuditEntry,
    AuditFormat,
    ActionCategory,
    Outcome,
    Severity,
    audit_log,
    init_global_logger,
)
from sunny.infrastructure.security import (
    SecurityConfig,
    TrustModel,
    RateLimiter,
    get_security_config_from_env,
)

logger = logging.getLogger("sunny")

# =============================================================================
# Audit and Security Initialization
# =============================================================================

def _init_audit_logging() -> None:
    """Initialize the global audit logger from environment."""
    audit_path = os.getenv("SUNNY_AUDIT_LOG")
    audit_format = AuditFormat.JSON_LINES if os.getenv("SUNNY_AUDIT_JSON") else AuditFormat.TEXT

    if audit_path:
        init_global_logger(
            AuditConfig(
                log_path=Path(audit_path),
                echo_stdout=os.getenv("SUNNY_LOG_LEVEL", "").upper() == "DEBUG",
                format=audit_format,
            )
        )
    else:
        init_global_logger(
            AuditConfig(
                echo_stdout=False,
                format=audit_format,
            )
        )


def _init_security() -> tuple[SecurityConfig, RateLimiter | None]:
    """Initialize security configuration from environment.

    Returns:
        Tuple of (security_config, rate_limiter or None).
    """
    config = get_security_config_from_env()
    rate_limiter = RateLimiter(max_requests=config.rate_limit) if config.rate_limit > 0 else None

    audit_log(
        AuditEntry(
            action="SECURITY_INIT",
            entity_type="Security",
            entity_id="config",
            description=f"Security initialized: trust_model={config.trust_model.value}",
            category=ActionCategory.SYSTEM,
            severity=Severity.INFO if config.trust_model != TrustModel.PERMISSIVE else Severity.WARNING,
            metadata={
                "trust_model": config.trust_model.value,
                "rate_limit": str(config.rate_limit),
                "strict_validation": str(config.strict_validation),
            },
        )
    )

    return config, rate_limiter


# Initialize on module load
_init_audit_logging()
_security_config, _rate_limiter = _init_security()


# =============================================================================
# Lifespan Context Manager
# =============================================================================


@asynccontextmanager
async def lifespan(server: FastMCP) -> AsyncIterator[dict[str, Any]]:
    """Manage server lifecycle and shared resources.

    This context manager handles:
    - Server startup logging
    - Ableton connection establishment
    - Theory engine initialization
    - Snapshot manager setup
    - Graceful shutdown

    Yields:
        Dictionary containing shared resources:
        - ableton: AbletonConnection instance
        - theory: TheoryEngine instance
        - snapshots: SnapshotManager instance
        - security: SecurityConfig instance
        - rate_limiter: RateLimiter instance or None
    """
    logger.info("Sunny starting...")
    audit_log(
        AuditEntry(
            action="STARTUP",
            entity_type="Server",
            entity_id="sunny",
            description="MCP server starting",
            category=ActionCategory.SYSTEM,
        )
    )

    # Import here to avoid circular imports
    from sunny.host.transport import AbletonConnection
    from sunny.core.engine import TheoryEngine
    from sunny.server.snapshot import SnapshotManager

    # Initialize shared resources
    ableton = AbletonConnection()
    theory = TheoryEngine()
    snapshots = SnapshotManager()

    # Connect snapshot manager to transport for state capture/restore
    snapshots.set_transport(ableton)

    try:
        # Attempt connection to Ableton
        connected = await ableton.connect()
        if connected:
            logger.info("Connected to Ableton Live")
            audit_log(
                AuditEntry(
                    action="CONNECT",
                    entity_type="Ableton",
                    entity_id=f"{ableton.tcp.host}:{ableton.tcp.port}",
                    description="Connected to Ableton Live",
                    category=ActionCategory.CONNECTION,
                    outcome=Outcome.SUCCESS,
                )
            )
        else:
            logger.warning("Could not connect to Ableton Live - running in offline mode")
            audit_log(
                AuditEntry(
                    action="CONNECT",
                    entity_type="Ableton",
                    entity_id="offline",
                    description="Running in offline mode (Ableton not available)",
                    category=ActionCategory.CONNECTION,
                    severity=Severity.WARNING,
                    outcome=Outcome.PARTIAL,
                )
            )

        yield {
            "ableton": ableton,
            "theory": theory,
            "snapshots": snapshots,
            "security": _security_config,
            "rate_limiter": _rate_limiter,
        }
    finally:
        await ableton.disconnect()
        logger.info("Sunny stopped")
        audit_log(
            AuditEntry(
                action="SHUTDOWN",
                entity_type="Server",
                entity_id="sunny",
                description="MCP server stopped",
                category=ActionCategory.SYSTEM,
            )
        )


def get_security_config() -> SecurityConfig:
    """Get the global security configuration.

    Returns:
        The initialized security configuration.
    """
    return _security_config


def get_global_rate_limiter() -> RateLimiter | None:
    """Get the global rate limiter.

    Returns:
        The rate limiter or None if disabled.
    """
    return _rate_limiter
