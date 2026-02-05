"""Infrastructure Layer.

Component: IFPK001A (Infrastructure Package)
Domain: IF (Infrastructure) | Category: PK (Package)

Contains cross-cutting concerns:
- audit: Structured audit logging (AUPK001A)
- security: Trust model and validation (SCPK001A)

Note: Transport layer moved to sunny.host.transport (HSTP001A)
as it integrates with the Ableton Remote Script.
"""

from __future__ import annotations

# Audit
from sunny.infrastructure.audit import (
    Severity,
    Outcome,
    ActionCategory,
    AuditEntry,
    AuditFormat,
    AuditConfig,
    AuditLogger,
    init_global_logger,
    get_logger,
    audit_log,
    audit_info,
    audit_warning,
    audit_error,
    audit_tool,
    AuditHandler,
)

# Security
from sunny.infrastructure.security import (
    TrustLevel,
    TrustModel,
    SecurityConfig,
    RateLimiter,
    ValidationError,
    is_localhost,
    is_host_allowed,
    verify_api_key,
    get_trust_level,
    validate_string,
    validate_int,
    validate_float,
    validate_tempo,
    validate_track_index,
    validate_clip_index,
    validate_midi_note,
    validate_midi_velocity,
    validate_note_name,
    validate_scale_name,
    require_trust,
    generate_api_key,
    hash_api_key,
    get_security_config_from_env,
)

__all__ = [
    # Audit
    "Severity",
    "Outcome",
    "ActionCategory",
    "AuditEntry",
    "AuditFormat",
    "AuditConfig",
    "AuditLogger",
    "init_global_logger",
    "get_logger",
    "audit_log",
    "audit_info",
    "audit_warning",
    "audit_error",
    "audit_tool",
    "AuditHandler",
    # Security
    "TrustLevel",
    "TrustModel",
    "SecurityConfig",
    "RateLimiter",
    "ValidationError",
    "is_localhost",
    "is_host_allowed",
    "verify_api_key",
    "get_trust_level",
    "validate_string",
    "validate_int",
    "validate_float",
    "validate_tempo",
    "validate_track_index",
    "validate_clip_index",
    "validate_midi_note",
    "validate_midi_velocity",
    "validate_note_name",
    "validate_scale_name",
    "require_trust",
    "generate_api_key",
    "hash_api_key",
    "get_security_config_from_env",
]
