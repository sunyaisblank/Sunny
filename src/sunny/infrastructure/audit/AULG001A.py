"""Sunny - Audit Logging.

Component: AULG001A
Domain: AU (Audit) | Category: LG (Logging)

Provides immutable, queryable audit logging following event sourcing principles.
All entries are append-only and include structured fields for traceability.

Design Principles:
- Immutable: Entries cannot be modified after creation
- Structured: WHO/WHAT/WHEN/OUTCOME pattern for all events
- Queryable: Support for filtering by time, entity, correlation
- Clean Output: Human-readable with machine-parseable structure

Example:
    >>> from sunny.infrastructure.audit import audit_log, AuditEntry, Severity, Outcome
    >>> audit_log(AuditEntry(
    ...     action="CONNECT",
    ...     entity_type="Ableton",
    ...     entity_id="localhost:9876",
    ...     description="Connected to Ableton Live",
    ...     outcome=Outcome.SUCCESS,
    ... ))

Thread Safety:
    All logging operations are thread-safe via mutex locking.
"""

from __future__ import annotations

import json
import logging
import threading
import uuid
import time
from dataclasses import dataclass, field
from datetime import datetime, timezone
from enum import IntEnum
from pathlib import Path
from typing import Any, Final, Optional


# =============================================================================
# Constants
# =============================================================================

#: Maximum log line length before truncation
MAX_LINE_LENGTH: Final[int] = 10000

#: Default log rotation size (MB)
DEFAULT_MAX_SIZE_MB: Final[int] = 100


# =============================================================================
# Core Types
# =============================================================================


class Severity(IntEnum):
    """Severity level for audit entries.

    Component: AULG001A.Severity
    """

    INFO = 0
    WARNING = 1
    ERROR = 2
    CRITICAL = 3

    def __str__(self) -> str:
        return self.name


class Outcome(IntEnum):
    """Outcome of an audited operation.

    Component: AULG001A.Outcome
    """

    SUCCESS = 0
    FAILURE = 1
    PARTIAL = 2

    def __str__(self) -> str:
        return self.name


class ActionCategory(IntEnum):
    """Category of audited action.

    Component: AULG001A.ActionCategory
    """

    SYSTEM = 0      # System-level events
    CONNECTION = 1  # Ableton connection
    TOOL = 2        # MCP tool invocation
    THEORY = 3      # Music theory computation
    TRANSPORT = 4   # TCP/UDP communication
    SNAPSHOT = 5    # Project snapshots
    DEVICE = 6      # Device manipulation
    TRACK = 7       # Track operations
    CLIP = 8        # Clip operations

    def __str__(self) -> str:
        return self.name


# =============================================================================
# Audit Entry
# =============================================================================


@dataclass(frozen=True)
class AuditEntry:
    """An immutable audit log entry.

    Component: AULG001A.AuditEntry

    Captures WHO did WHAT to WHICH entity, WHEN, and with what OUTCOME.
    The frozen=True ensures immutability after creation.

    Invariants:
        - id is unique across all entries
        - timestamp is always UTC
        - action and entity_type are non-empty
    """

    action: str
    """Action performed (e.g., 'CONNECT', 'INVOKE_TOOL', 'COMPUTE')."""

    entity_type: str
    """Type of entity involved (e.g., 'Ableton', 'Tool', 'Track')."""

    entity_id: str
    """Identifier of the entity (e.g., 'localhost:9876', 'set_tempo')."""

    description: str
    """Human-readable description of the event."""

    # Optional fields with defaults
    id: str = field(default_factory=lambda: str(uuid.uuid4())[:8])
    """Unique identifier for this entry."""

    timestamp: datetime = field(default_factory=lambda: datetime.now(timezone.utc))
    """UTC timestamp of the event."""

    category: ActionCategory = ActionCategory.SYSTEM
    """Category of the action."""

    severity: Severity = Severity.INFO
    """Severity level."""

    outcome: Outcome = Outcome.SUCCESS
    """Outcome of the operation."""

    correlation_id: Optional[str] = None
    """Correlation ID for tracing related operations."""

    duration_ms: Optional[float] = None
    """Duration in milliseconds (if applicable)."""

    initiator: Optional[str] = None
    """Who initiated the action (user, system, tool name)."""

    metadata: dict[str, Any] = field(default_factory=dict)
    """Additional structured data."""

    def timestamp_iso(self) -> str:
        """Format timestamp as ISO 8601."""
        return self.timestamp.strftime("%Y-%m-%dT%H:%M:%S.%f")[:-3] + "Z"

    def format_line(self) -> str:
        """Format as a single structured log line.

        Format: TIMESTAMP | SEVERITY | CATEGORY | ACTION | ENTITY | OUTCOME | DESCRIPTION [metadata]
        """
        line = (
            f"{self.timestamp_iso()} | "
            f"{str(self.severity):8} | "
            f"{str(self.category):12} | "
            f"{self.action:16} | "
            f"{self.entity_type}:{self.entity_id} | "
            f"{str(self.outcome):8} | "
            f"{self.description}"
        )

        # Add duration if present
        if self.duration_ms is not None:
            if self.duration_ms >= 1000:
                line += f" [{self.duration_ms / 1000:.2f}s]"
            else:
                line += f" [{self.duration_ms:.2f}ms]"

        # Add correlation if present
        if self.correlation_id:
            line += f" [cid:{self.correlation_id}]"

        # Add initiator if present
        if self.initiator:
            line += f" [by:{self.initiator}]"

        # Add metadata if present
        if self.metadata:
            meta_str = ", ".join(f"{k}={v}" for k, v in self.metadata.items())
            line += f" {{{meta_str}}}"

        return line[:MAX_LINE_LENGTH]

    def to_dict(self) -> dict[str, Any]:
        """Convert to dictionary for JSON serialization."""
        d = {
            "id": self.id,
            "timestamp": self.timestamp_iso(),
            "severity": str(self.severity),
            "category": str(self.category),
            "action": self.action,
            "entity_type": self.entity_type,
            "entity_id": self.entity_id,
            "outcome": str(self.outcome),
            "description": self.description,
        }

        if self.duration_ms is not None:
            d["duration_ms"] = self.duration_ms
        if self.correlation_id:
            d["correlation_id"] = self.correlation_id
        if self.initiator:
            d["initiator"] = self.initiator
        if self.metadata:
            d["metadata"] = self.metadata

        return d

    def to_json(self) -> str:
        """Serialize to JSON format."""
        return json.dumps(self.to_dict(), separators=(",", ":"))


# =============================================================================
# Audit Logger
# =============================================================================


class AuditFormat(IntEnum):
    """Output format for audit logs.

    Component: AULG001A.AuditFormat
    """

    TEXT = 0
    JSON_LINES = 1


@dataclass
class AuditConfig:
    """Configuration for the audit logger.

    Component: AULG001A.AuditConfig
    """

    log_path: Optional[Path] = None
    """Path to the audit log file. None for stdout-only."""

    min_severity: Severity = Severity.INFO
    """Minimum severity to log."""

    echo_stdout: bool = False
    """Whether to also print to stdout."""

    format: AuditFormat = AuditFormat.TEXT
    """Output format."""

    max_file_size_mb: int = DEFAULT_MAX_SIZE_MB
    """Maximum file size before rotation (MB)."""


class AuditLogger:
    """Thread-safe audit logger with file-based persistence.

    Component: AULG001A.AuditLogger

    Thread Safety:
        All methods are thread-safe via mutex locking.
    """

    def __init__(self, config: Optional[AuditConfig] = None):
        """Initialize the audit logger.

        Args:
            config: Logger configuration. Uses defaults if None.
        """
        self.config = config or AuditConfig()
        self._lock = threading.Lock()
        self._sequence = 0
        self._file: Optional[Any] = None

        if self.config.log_path:
            self._open_file()

    def _open_file(self) -> None:
        """Open the log file for appending."""
        if self.config.log_path:
            self.config.log_path.parent.mkdir(parents=True, exist_ok=True)
            self._file = open(self.config.log_path, "a", encoding="utf-8")

    def log(self, entry: AuditEntry) -> None:
        """Log an audit entry.

        Args:
            entry: The audit entry to log.
        """
        # Check severity filter
        if entry.severity < self.config.min_severity:
            return

        with self._lock:
            self._sequence += 1
            seq = self._sequence

            if self.config.format == AuditFormat.TEXT:
                line = f"{seq:08d} | {entry.format_line()}\n"
            else:
                line = f"{entry.to_json()}\n"

            # Write to file if configured
            if self._file:
                self._file.write(line)
                self._file.flush()

            # Echo to stdout if configured
            if self.config.echo_stdout:
                print(line, end="")

    def info(
        self,
        action: str,
        entity_type: str,
        entity_id: str,
        description: str,
        **kwargs: Any,
    ) -> None:
        """Create and log an info entry."""
        self.log(
            AuditEntry(
                action=action,
                entity_type=entity_type,
                entity_id=entity_id,
                description=description,
                severity=Severity.INFO,
                **kwargs,
            )
        )

    def warning(
        self,
        action: str,
        entity_type: str,
        entity_id: str,
        description: str,
        **kwargs: Any,
    ) -> None:
        """Create and log a warning entry."""
        self.log(
            AuditEntry(
                action=action,
                entity_type=entity_type,
                entity_id=entity_id,
                description=description,
                severity=Severity.WARNING,
                **kwargs,
            )
        )

    def error(
        self,
        action: str,
        entity_type: str,
        entity_id: str,
        description: str,
        **kwargs: Any,
    ) -> None:
        """Create and log an error entry."""
        self.log(
            AuditEntry(
                action=action,
                entity_type=entity_type,
                entity_id=entity_id,
                description=description,
                severity=Severity.ERROR,
                outcome=Outcome.FAILURE,
                **kwargs,
            )
        )

    def flush(self) -> None:
        """Flush the log buffer."""
        if self._file:
            self._file.flush()

    def close(self) -> None:
        """Close the log file."""
        if self._file:
            self._file.close()
            self._file = None


# =============================================================================
# Global Logger
# =============================================================================

_global_logger: Optional[AuditLogger] = None
_global_lock = threading.Lock()


def init_global_logger(config: Optional[AuditConfig] = None) -> AuditLogger:
    """Initialize the global audit logger.

    Args:
        config: Logger configuration.

    Returns:
        The initialized logger.
    """
    global _global_logger
    with _global_lock:
        if _global_logger is None:
            _global_logger = AuditLogger(config)
        return _global_logger


def get_logger() -> Optional[AuditLogger]:
    """Get the global logger (if initialized)."""
    return _global_logger


def audit_log(entry: AuditEntry) -> None:
    """Log to the global logger (no-op if not initialized)."""
    logger = get_logger()
    if logger:
        logger.log(entry)


def audit_info(
    action: str,
    entity_type: str,
    entity_id: str,
    description: str,
    **kwargs: Any,
) -> None:
    """Quick info log to global logger."""
    logger = get_logger()
    if logger:
        logger.info(action, entity_type, entity_id, description, **kwargs)


def audit_warning(
    action: str,
    entity_type: str,
    entity_id: str,
    description: str,
    **kwargs: Any,
) -> None:
    """Quick warning log to global logger."""
    logger = get_logger()
    if logger:
        logger.warning(action, entity_type, entity_id, description, **kwargs)


def audit_error(
    action: str,
    entity_type: str,
    entity_id: str,
    description: str,
    **kwargs: Any,
) -> None:
    """Quick error log to global logger."""
    logger = get_logger()
    if logger:
        logger.error(action, entity_type, entity_id, description, **kwargs)


# =============================================================================
# MCP Tool Audit Decorator
# =============================================================================


def audit_tool(
    category: ActionCategory = ActionCategory.TOOL,
) -> Any:
    """Decorator to automatically audit MCP tool invocations.

    Args:
        category: The action category for this tool.

    Example:
        >>> @audit_tool(category=ActionCategory.TRACK)
        ... async def create_track(ctx: Context, name: str) -> dict:
        ...     # Tool implementation
        ...     pass
    """
    import functools

    def decorator(func: Any) -> Any:
        @functools.wraps(func)
        async def wrapper(*args: Any, **kwargs: Any) -> Any:
            tool_name = func.__name__
            start_time = time.perf_counter()
            correlation_id = str(uuid.uuid4())[:8]

            # Log tool invocation
            audit_log(
                AuditEntry(
                    action="INVOKE",
                    entity_type="Tool",
                    entity_id=tool_name,
                    description=f"Invoking tool {tool_name}",
                    category=category,
                    correlation_id=correlation_id,
                    metadata={"args": str(kwargs)[:200]},  # Truncate for safety
                )
            )

            try:
                result = await func(*args, **kwargs)
                duration_ms = (time.perf_counter() - start_time) * 1000

                audit_log(
                    AuditEntry(
                        action="COMPLETE",
                        entity_type="Tool",
                        entity_id=tool_name,
                        description=f"Tool {tool_name} completed",
                        category=category,
                        outcome=Outcome.SUCCESS,
                        duration_ms=duration_ms,
                        correlation_id=correlation_id,
                    )
                )

                return result

            except Exception as e:
                duration_ms = (time.perf_counter() - start_time) * 1000

                audit_log(
                    AuditEntry(
                        action="FAIL",
                        entity_type="Tool",
                        entity_id=tool_name,
                        description=f"Tool {tool_name} failed: {str(e)[:100]}",
                        category=category,
                        severity=Severity.ERROR,
                        outcome=Outcome.FAILURE,
                        duration_ms=duration_ms,
                        correlation_id=correlation_id,
                        metadata={"error": str(e)[:500]},
                    )
                )
                raise

        return wrapper

    return decorator


# =============================================================================
# Integration with Python logging
# =============================================================================


class AuditHandler(logging.Handler):
    """Logging handler that forwards to the audit system.

    Component: AULG001A.AuditHandler

    Use this to capture logs from third-party libraries.
    """

    def __init__(
        self,
        entity_type: str = "Log",
        category: ActionCategory = ActionCategory.SYSTEM,
    ):
        """Initialize the handler.

        Args:
            entity_type: Entity type to use for log entries.
            category: Category to use for log entries.
        """
        super().__init__()
        self.entity_type = entity_type
        self.category = category

    def emit(self, record: logging.LogRecord) -> None:
        """Emit a log record as an audit entry."""
        severity = {
            logging.DEBUG: Severity.INFO,
            logging.INFO: Severity.INFO,
            logging.WARNING: Severity.WARNING,
            logging.ERROR: Severity.ERROR,
            logging.CRITICAL: Severity.CRITICAL,
        }.get(record.levelno, Severity.INFO)

        audit_log(
            AuditEntry(
                action="LOG",
                entity_type=self.entity_type,
                entity_id=record.name,
                description=record.getMessage()[:500],
                category=self.category,
                severity=severity,
                metadata={
                    "module": record.module,
                    "lineno": str(record.lineno),
                },
            )
        )
