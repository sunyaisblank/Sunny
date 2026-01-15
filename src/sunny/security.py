"""Security Module for Sunny MCP Server.

Component: SRSC001A
Domain: SR (Server) | Category: SC (Security)

Implements the MCP trust model with proper security boundaries:
- Local trust: Localhost connections are implicitly trusted
- Network trust: Requires explicit API key authentication
- Input validation: All tool parameters are validated
- Rate limiting: Protection against abuse

Trust Model:
    MCP servers follow a different trust model than web applications:
    1. The user explicitly configures which MCP servers to trust
    2. Local connections (stdio, localhost) are implicitly trusted
    3. Network access requires additional authentication

Design Principles:
- Secure by default: Network access disabled unless explicitly enabled
- Defense in depth: Multiple validation layers
- Fail closed: Reject on any validation failure
- Audit trail: All security events are logged
"""

from __future__ import annotations

import hashlib
import hmac
import ipaddress
import os
import re
import secrets
import threading
import time
from dataclasses import dataclass, field
from enum import Enum
from functools import wraps
from typing import Any, Callable, Optional, TypeVar, Union

from sunny.audit import (
    ActionCategory,
    AuditEntry,
    Outcome,
    Severity,
    audit_log,
)


# =============================================================================
# Trust Model
# =============================================================================


class TrustLevel(Enum):
    """Trust level for a connection."""

    UNTRUSTED = 0    # Rejected
    LOCAL = 1        # Localhost connection
    AUTHENTICATED = 2  # Valid API key
    ADMIN = 3        # Full admin access


class TrustModel(Enum):
    """Trust model configuration."""

    LOCAL_ONLY = "local"      # Only localhost connections (default)
    NETWORK = "network"       # Allow network with authentication
    PERMISSIVE = "permissive"  # Trust all (NOT recommended)


@dataclass(frozen=True)
class SecurityConfig:
    """Security configuration for the MCP server."""

    # Trust model
    trust_model: TrustModel = TrustModel.LOCAL_ONLY

    # API key for network access (required if trust_model == NETWORK)
    api_key: Optional[str] = None

    # Allowed network hosts (CIDR notation)
    allowed_hosts: tuple[str, ...] = ("127.0.0.1", "::1")

    # Rate limiting (requests per minute, 0 = unlimited)
    rate_limit: int = 0

    # Maximum request size in bytes
    max_request_size: int = 10 * 1024 * 1024  # 10MB

    # Input validation strictness
    strict_validation: bool = True


# =============================================================================
# Connection Security
# =============================================================================


def is_localhost(host: str) -> bool:
    """Check if a host is localhost.

    Args:
        host: IP address or hostname to check.

    Returns:
        True if the host is localhost.
    """
    if not host:
        return False

    # Check common localhost values
    localhost_values = {"localhost", "127.0.0.1", "::1", "0.0.0.0"}
    if host.lower() in localhost_values:
        return True

    # Check if it's a loopback IP
    try:
        ip = ipaddress.ip_address(host)
        return ip.is_loopback
    except ValueError:
        return False


def is_host_allowed(host: str, allowed_hosts: tuple[str, ...]) -> bool:
    """Check if a host is in the allowed list.

    Args:
        host: IP address to check.
        allowed_hosts: Tuple of allowed hosts/networks (CIDR notation).

    Returns:
        True if the host is allowed.
    """
    if not host or not allowed_hosts:
        return False

    try:
        ip = ipaddress.ip_address(host)
    except ValueError:
        return False

    for allowed in allowed_hosts:
        try:
            # Check if it's a network (CIDR)
            if "/" in allowed:
                network = ipaddress.ip_network(allowed, strict=False)
                if ip in network:
                    return True
            else:
                # Check exact match
                if ip == ipaddress.ip_address(allowed):
                    return True
        except ValueError:
            continue

    return False


def verify_api_key(provided_key: Optional[str], expected_key: Optional[str]) -> bool:
    """Verify an API key using constant-time comparison.

    Args:
        provided_key: The key provided by the client.
        expected_key: The expected/configured key.

    Returns:
        True if the keys match.
    """
    if not provided_key or not expected_key:
        return False

    return hmac.compare_digest(provided_key.encode(), expected_key.encode())


def get_trust_level(
    host: str,
    api_key: Optional[str],
    config: SecurityConfig,
) -> TrustLevel:
    """Determine the trust level for a connection.

    Args:
        host: The client's IP address.
        api_key: The API key provided (if any).
        config: Security configuration.

    Returns:
        The trust level for this connection.
    """
    # Permissive mode - trust everything (NOT recommended)
    if config.trust_model == TrustModel.PERMISSIVE:
        audit_log(
            AuditEntry(
                action="TRUST_CHECK",
                entity_type="Connection",
                entity_id=host,
                description="Permissive mode - all connections trusted",
                category=ActionCategory.SYSTEM,
                severity=Severity.WARNING,
            )
        )
        return TrustLevel.AUTHENTICATED

    # Check if localhost
    if is_localhost(host):
        return TrustLevel.LOCAL

    # For non-localhost, check trust model
    if config.trust_model == TrustModel.LOCAL_ONLY:
        audit_log(
            AuditEntry(
                action="TRUST_REJECT",
                entity_type="Connection",
                entity_id=host,
                description="Non-local connection rejected (local-only mode)",
                category=ActionCategory.SYSTEM,
                severity=Severity.WARNING,
                outcome=Outcome.FAILURE,
            )
        )
        return TrustLevel.UNTRUSTED

    # Network mode - require authentication
    if config.trust_model == TrustModel.NETWORK:
        # Check if host is in allowed list
        if not is_host_allowed(host, config.allowed_hosts):
            audit_log(
                AuditEntry(
                    action="TRUST_REJECT",
                    entity_type="Connection",
                    entity_id=host,
                    description="Host not in allowed list",
                    category=ActionCategory.SYSTEM,
                    severity=Severity.WARNING,
                    outcome=Outcome.FAILURE,
                )
            )
            return TrustLevel.UNTRUSTED

        # Check API key
        if verify_api_key(api_key, config.api_key):
            return TrustLevel.AUTHENTICATED
        else:
            audit_log(
                AuditEntry(
                    action="AUTH_FAIL",
                    entity_type="Connection",
                    entity_id=host,
                    description="Invalid or missing API key",
                    category=ActionCategory.SYSTEM,
                    severity=Severity.WARNING,
                    outcome=Outcome.FAILURE,
                )
            )
            return TrustLevel.UNTRUSTED

    return TrustLevel.UNTRUSTED


# =============================================================================
# Rate Limiting
# =============================================================================


@dataclass
class RateLimiter:
    """Token bucket rate limiter."""

    max_requests: int
    window_seconds: int = 60

    _buckets: dict[str, list[float]] = field(default_factory=dict)
    _lock: threading.Lock = field(default_factory=threading.Lock)

    def is_allowed(self, client_id: str) -> bool:
        """Check if a request is allowed.

        Args:
            client_id: Identifier for the client (IP address, API key, etc.)

        Returns:
            True if the request is allowed.
        """
        if self.max_requests <= 0:
            return True  # Rate limiting disabled

        now = time.time()
        window_start = now - self.window_seconds

        with self._lock:
            # Get or create bucket for this client
            if client_id not in self._buckets:
                self._buckets[client_id] = []

            bucket = self._buckets[client_id]

            # Remove old entries
            bucket[:] = [t for t in bucket if t > window_start]

            # Check if under limit
            if len(bucket) >= self.max_requests:
                return False

            # Record this request
            bucket.append(now)
            return True

    def get_remaining(self, client_id: str) -> int:
        """Get remaining requests for a client.

        Args:
            client_id: Identifier for the client.

        Returns:
            Number of remaining requests in the current window.
        """
        if self.max_requests <= 0:
            return -1  # Unlimited

        now = time.time()
        window_start = now - self.window_seconds

        with self._lock:
            bucket = self._buckets.get(client_id, [])
            recent = sum(1 for t in bucket if t > window_start)
            return max(0, self.max_requests - recent)


# =============================================================================
# Input Validation
# =============================================================================


class ValidationError(Exception):
    """Raised when input validation fails."""

    def __init__(self, field: str, message: str):
        self.field = field
        self.message = message
        super().__init__(f"{field}: {message}")


def validate_string(
    value: Any,
    field_name: str,
    *,
    min_length: int = 0,
    max_length: int = 10000,
    pattern: Optional[str] = None,
    allowed_chars: Optional[str] = None,
) -> str:
    """Validate a string parameter.

    Args:
        value: Value to validate.
        field_name: Name of the field (for error messages).
        min_length: Minimum length.
        max_length: Maximum length.
        pattern: Regex pattern to match.
        allowed_chars: Set of allowed characters.

    Returns:
        The validated string.

    Raises:
        ValidationError: If validation fails.
    """
    if value is None:
        raise ValidationError(field_name, "Required field is missing")

    if not isinstance(value, str):
        raise ValidationError(field_name, f"Expected string, got {type(value).__name__}")

    if len(value) < min_length:
        raise ValidationError(field_name, f"Minimum length is {min_length}")

    if len(value) > max_length:
        raise ValidationError(field_name, f"Maximum length is {max_length}")

    if pattern and not re.match(pattern, value):
        raise ValidationError(field_name, f"Does not match required pattern")

    if allowed_chars:
        invalid = set(value) - set(allowed_chars)
        if invalid:
            raise ValidationError(
                field_name, f"Contains invalid characters: {invalid}"
            )

    return value


def validate_int(
    value: Any,
    field_name: str,
    *,
    min_value: Optional[int] = None,
    max_value: Optional[int] = None,
) -> int:
    """Validate an integer parameter.

    Args:
        value: Value to validate.
        field_name: Name of the field (for error messages).
        min_value: Minimum allowed value.
        max_value: Maximum allowed value.

    Returns:
        The validated integer.

    Raises:
        ValidationError: If validation fails.
    """
    if value is None:
        raise ValidationError(field_name, "Required field is missing")

    if isinstance(value, bool):
        raise ValidationError(field_name, "Expected integer, got boolean")

    if not isinstance(value, int):
        try:
            value = int(value)
        except (ValueError, TypeError):
            raise ValidationError(
                field_name, f"Expected integer, got {type(value).__name__}"
            )

    if min_value is not None and value < min_value:
        raise ValidationError(field_name, f"Minimum value is {min_value}")

    if max_value is not None and value > max_value:
        raise ValidationError(field_name, f"Maximum value is {max_value}")

    return value


def validate_float(
    value: Any,
    field_name: str,
    *,
    min_value: Optional[float] = None,
    max_value: Optional[float] = None,
) -> float:
    """Validate a float parameter.

    Args:
        value: Value to validate.
        field_name: Name of the field (for error messages).
        min_value: Minimum allowed value.
        max_value: Maximum allowed value.

    Returns:
        The validated float.

    Raises:
        ValidationError: If validation fails.
    """
    if value is None:
        raise ValidationError(field_name, "Required field is missing")

    if not isinstance(value, (int, float)):
        try:
            value = float(value)
        except (ValueError, TypeError):
            raise ValidationError(
                field_name, f"Expected number, got {type(value).__name__}"
            )

    if min_value is not None and value < min_value:
        raise ValidationError(field_name, f"Minimum value is {min_value}")

    if max_value is not None and value > max_value:
        raise ValidationError(field_name, f"Maximum value is {max_value}")

    return float(value)


def validate_midi_note(value: Any, field_name: str) -> int:
    """Validate a MIDI note number (0-127).

    Args:
        value: Value to validate.
        field_name: Name of the field.

    Returns:
        The validated MIDI note.

    Raises:
        ValidationError: If validation fails.
    """
    return validate_int(value, field_name, min_value=0, max_value=127)


def validate_midi_velocity(value: Any, field_name: str) -> int:
    """Validate a MIDI velocity (0-127).

    Args:
        value: Value to validate.
        field_name: Name of the field.

    Returns:
        The validated velocity.

    Raises:
        ValidationError: If validation fails.
    """
    return validate_int(value, field_name, min_value=0, max_value=127)


def validate_tempo(value: Any, field_name: str) -> float:
    """Validate a tempo in BPM (20-999).

    Args:
        value: Value to validate.
        field_name: Name of the field.

    Returns:
        The validated tempo.

    Raises:
        ValidationError: If validation fails.
    """
    return validate_float(value, field_name, min_value=20.0, max_value=999.0)


def validate_track_index(value: Any, field_name: str) -> int:
    """Validate a track index (0-999).

    Args:
        value: Value to validate.
        field_name: Name of the field.

    Returns:
        The validated track index.

    Raises:
        ValidationError: If validation fails.
    """
    return validate_int(value, field_name, min_value=0, max_value=999)


def validate_clip_index(value: Any, field_name: str) -> int:
    """Validate a clip/scene index (0-999).

    Args:
        value: Value to validate.
        field_name: Name of the field.

    Returns:
        The validated clip index.

    Raises:
        ValidationError: If validation fails.
    """
    return validate_int(value, field_name, min_value=0, max_value=999)


def validate_note_name(value: Any, field_name: str) -> str:
    """Validate a note name (e.g., 'C', 'F#', 'Bb').

    Args:
        value: Value to validate.
        field_name: Name of the field.

    Returns:
        The validated note name.

    Raises:
        ValidationError: If validation fails.
    """
    value = validate_string(value, field_name, min_length=1, max_length=3)

    # Valid note names
    valid_notes = {
        "C", "C#", "Db", "D", "D#", "Eb", "E", "F", "F#", "Gb",
        "G", "G#", "Ab", "A", "A#", "Bb", "B"
    }

    if value not in valid_notes:
        raise ValidationError(field_name, f"Invalid note name: {value}")

    return value


def validate_scale_name(value: Any, field_name: str) -> str:
    """Validate a scale name.

    Args:
        value: Value to validate.
        field_name: Name of the field.

    Returns:
        The validated scale name.

    Raises:
        ValidationError: If validation fails.
    """
    value = validate_string(value, field_name, min_length=1, max_length=50)

    # Common valid scales
    valid_scales = {
        "major", "minor", "harmonic_minor", "melodic_minor",
        "dorian", "phrygian", "lydian", "mixolydian", "aeolian", "locrian",
        "pentatonic_major", "pentatonic_minor", "blues",
        "whole_tone", "chromatic", "diminished", "augmented",
    }

    # Convert to lowercase for comparison
    normalized = value.lower().replace(" ", "_").replace("-", "_")

    if normalized not in valid_scales:
        # Allow unknown scales but log a warning
        audit_log(
            AuditEntry(
                action="VALIDATE",
                entity_type="Parameter",
                entity_id=field_name,
                description=f"Unknown scale name: {value}",
                category=ActionCategory.SYSTEM,
                severity=Severity.WARNING,
            )
        )

    return value


# =============================================================================
# Security Decorator
# =============================================================================

T = TypeVar("T")


def require_trust(min_level: TrustLevel = TrustLevel.LOCAL) -> Callable:
    """Decorator to require a minimum trust level.

    Args:
        min_level: Minimum required trust level.

    Returns:
        Decorator function.
    """

    def decorator(func: Callable[..., T]) -> Callable[..., T]:
        @wraps(func)
        async def wrapper(*args: Any, **kwargs: Any) -> T:
            # Get trust level from context (implementation-specific)
            trust_level = kwargs.pop("_trust_level", TrustLevel.LOCAL)

            if trust_level.value < min_level.value:
                audit_log(
                    AuditEntry(
                        action="ACCESS_DENIED",
                        entity_type="Tool",
                        entity_id=func.__name__,
                        description=f"Insufficient trust level: {trust_level.name} < {min_level.name}",
                        category=ActionCategory.TOOL,
                        severity=Severity.WARNING,
                        outcome=Outcome.FAILURE,
                    )
                )
                raise PermissionError(
                    f"Insufficient trust level for {func.__name__}"
                )

            return await func(*args, **kwargs)

        return wrapper

    return decorator


# =============================================================================
# Utility Functions
# =============================================================================


def generate_api_key(length: int = 32) -> str:
    """Generate a secure random API key.

    Args:
        length: Length of the key in bytes (will be hex-encoded).

    Returns:
        A secure random API key.
    """
    return secrets.token_hex(length)


def hash_api_key(key: str) -> str:
    """Hash an API key for storage.

    Args:
        key: The API key to hash.

    Returns:
        The hashed key.
    """
    return hashlib.sha256(key.encode()).hexdigest()


# =============================================================================
# Default Configuration
# =============================================================================


def get_security_config_from_env() -> SecurityConfig:
    """Load security configuration from environment variables.

    Returns:
        SecurityConfig populated from environment.
    """
    trust_model_str = os.getenv("SUNNY_TRUST_MODEL", "local").lower()
    trust_model = {
        "local": TrustModel.LOCAL_ONLY,
        "network": TrustModel.NETWORK,
        "permissive": TrustModel.PERMISSIVE,
    }.get(trust_model_str, TrustModel.LOCAL_ONLY)

    allowed_hosts_str = os.getenv("SUNNY_ALLOWED_HOSTS", "127.0.0.1,::1")
    allowed_hosts = tuple(h.strip() for h in allowed_hosts_str.split(",") if h.strip())

    rate_limit = int(os.getenv("SUNNY_RATE_LIMIT", "0"))

    return SecurityConfig(
        trust_model=trust_model,
        api_key=os.getenv("SUNNY_API_KEY"),
        allowed_hosts=allowed_hosts,
        rate_limit=rate_limit,
    )
