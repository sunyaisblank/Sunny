"""Audit Infrastructure.

Component: AUPK001A (Audit Package)
Domain: AU (Audit) | Category: PK (Package)

Structured audit logging following event sourcing principles.

Modules:
    - AULG001A: Core audit logging implementation
"""

from __future__ import annotations

from sunny.infrastructure.audit.AULG001A import (
    # Constants
    MAX_LINE_LENGTH,
    DEFAULT_MAX_SIZE_MB,
    # Core types
    Severity,
    Outcome,
    ActionCategory,
    # Audit entry
    AuditEntry,
    # Logger configuration
    AuditFormat,
    AuditConfig,
    AuditLogger,
    # Global logger functions
    init_global_logger,
    get_logger,
    audit_log,
    audit_info,
    audit_warning,
    audit_error,
    # Decorator
    audit_tool,
    # Python logging integration
    AuditHandler,
)

__all__ = [
    # Constants
    "MAX_LINE_LENGTH",
    "DEFAULT_MAX_SIZE_MB",
    # Core types
    "Severity",
    "Outcome",
    "ActionCategory",
    # Audit entry
    "AuditEntry",
    # Logger configuration
    "AuditFormat",
    "AuditConfig",
    "AuditLogger",
    # Global logger functions
    "init_global_logger",
    "get_logger",
    "audit_log",
    "audit_info",
    "audit_warning",
    "audit_error",
    # Decorator
    "audit_tool",
    # Python logging integration
    "AuditHandler",
]
