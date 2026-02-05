"""Security Infrastructure.

Component: SCPK001A (Security Package)
Domain: SC (Security) | Category: PK (Package)

Trust model, validation, and rate limiting.

Modules:
    - SCRT001A: Core security implementation
"""

from __future__ import annotations

from sunny.infrastructure.security.SCRT001A import (
    # Constants
    RATE_LIMIT_WINDOW,
    MAX_REQUEST_SIZE,
    API_KEY_LENGTH,
    # Trust model
    TrustLevel,
    TrustModel,
    SecurityConfig,
    # Rate limiting
    RateLimiter,
    # Errors
    ValidationError,
    # Connection security
    is_localhost,
    is_host_allowed,
    verify_api_key,
    get_trust_level,
    # Validation functions
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
    # Decorators
    require_trust,
    # Utilities
    generate_api_key,
    hash_api_key,
    get_security_config_from_env,
)

__all__ = [
    # Constants
    "RATE_LIMIT_WINDOW",
    "MAX_REQUEST_SIZE",
    "API_KEY_LENGTH",
    # Trust model
    "TrustLevel",
    "TrustModel",
    "SecurityConfig",
    # Rate limiting
    "RateLimiter",
    # Errors
    "ValidationError",
    # Connection security
    "is_localhost",
    "is_host_allowed",
    "verify_api_key",
    "get_trust_level",
    # Validation functions
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
    # Decorators
    "require_trust",
    # Utilities
    "generate_api_key",
    "hash_api_key",
    "get_security_config_from_env",
]
