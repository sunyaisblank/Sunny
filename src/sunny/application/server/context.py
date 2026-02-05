"""Request Context Helpers.

Component: SVCX001A
Domain: SV (Server) | Category: CX (Context)

Helper functions for accessing shared resources from MCP request context.
"""

from __future__ import annotations

from typing import TYPE_CHECKING, Any

from mcp.server.fastmcp import Context

from sunny.infrastructure.audit import (
    AuditEntry,
    ActionCategory,
    Outcome,
    Severity,
    audit_log,
)
from sunny.infrastructure.security import SecurityConfig

if TYPE_CHECKING:
    from sunny.host.transport import AbletonConnection
    from sunny.core.engine import TheoryEngine
    from sunny.server.snapshot import SnapshotManager
    from sunny.infrastructure.security import RateLimiter


def get_ableton(ctx: Context) -> "AbletonConnection":
    """Get Ableton connection from context.

    Args:
        ctx: MCP request context.

    Returns:
        The Ableton connection instance.

    Raises:
        RuntimeError: If context not initialized or connection unavailable.
    """
    lifespan_ctx = ctx.request_context.lifespan_context
    if lifespan_ctx is None:
        raise RuntimeError("Lifespan context not initialized - server may still be starting")

    ableton = lifespan_ctx.get("ableton")
    if ableton is None:
        raise RuntimeError("Ableton connection not available")

    return ableton


def get_theory(ctx: Context) -> "TheoryEngine":
    """Get theory engine from context.

    Args:
        ctx: MCP request context.

    Returns:
        The theory engine instance.

    Raises:
        RuntimeError: If context not initialized or engine unavailable.
    """
    lifespan_ctx = ctx.request_context.lifespan_context
    if lifespan_ctx is None:
        raise RuntimeError("Lifespan context not initialized - server may still be starting")

    theory = lifespan_ctx.get("theory")
    if theory is None:
        raise RuntimeError("Theory engine not available")

    return theory


def get_snapshots(ctx: Context) -> "SnapshotManager":
    """Get snapshot manager from context.

    Args:
        ctx: MCP request context.

    Returns:
        The snapshot manager instance.

    Raises:
        RuntimeError: If context not initialized or manager unavailable.
    """
    lifespan_ctx = ctx.request_context.lifespan_context
    if lifespan_ctx is None:
        raise RuntimeError("Lifespan context not initialized - server may still be starting")

    snapshots = lifespan_ctx.get("snapshots")
    if snapshots is None:
        raise RuntimeError("Snapshot manager not available")

    return snapshots


def get_security(ctx: Context) -> SecurityConfig:
    """Get security configuration from context.

    Args:
        ctx: MCP request context.

    Returns:
        The security configuration.
    """
    from sunny.application.server.lifespan import get_security_config

    lifespan_ctx = ctx.request_context.lifespan_context
    if lifespan_ctx is None:
        return get_security_config()

    return lifespan_ctx.get("security", get_security_config())


def get_rate_limiter(ctx: Context) -> "RateLimiter | None":
    """Get rate limiter from context.

    Args:
        ctx: MCP request context.

    Returns:
        The rate limiter or None if disabled.
    """
    lifespan_ctx = ctx.request_context.lifespan_context
    if lifespan_ctx is None:
        return None

    return lifespan_ctx.get("rate_limiter")


def check_rate_limit(ctx: Context, client_id: str = "default") -> None:
    """Check rate limit and raise if exceeded.

    Args:
        ctx: MCP context.
        client_id: Client identifier for rate limiting.

    Raises:
        RuntimeError: If rate limit exceeded.
    """
    rate_limiter = get_rate_limiter(ctx)
    if rate_limiter and not rate_limiter.is_allowed(client_id):
        audit_log(
            AuditEntry(
                action="RATE_LIMIT",
                entity_type="Client",
                entity_id=client_id,
                description="Rate limit exceeded",
                category=ActionCategory.SYSTEM,
                severity=Severity.WARNING,
                outcome=Outcome.FAILURE,
            )
        )
        raise RuntimeError("Rate limit exceeded. Please wait before making more requests.")


def validate_input_strict(ctx: Context) -> bool:
    """Check if strict input validation is enabled.

    Args:
        ctx: MCP request context.

    Returns:
        True if strict validation should be enforced.
    """
    security = get_security(ctx)
    return security.strict_validation
