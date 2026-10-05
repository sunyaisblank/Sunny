"""Bounded production configuration and explicit legacy environment migration.

This module is standalone stdlib code: its migration/validation CLI does not
import Sunny, Live, create logging handlers or start a socket.
"""

from __future__ import annotations

import argparse
import ipaddress
import json
import os
import re
from pathlib import Path

# CMake derives the compiled client's contract constants from this source.
CONFIGURATION_SCHEMA_VERSION = 1
CONFIGURATION_CONTRACT = "versioned_json"
MAX_CONFIGURATION_BYTES = 65536
MAX_CONFIGURATION_DEPTH = 8
MAX_CONFIGURATION_PATH_BYTES = 4096
LEGACY_KEYS = (
    "SUNNY_ABLETON_HOST",
    "SUNNY_TCP_PORT",
    "SUNNY_WORKSPACE_PATH",
    "SUNNY_WORKSPACE_RECOVERY",
    "SUNNY_BIND_HOST",
)
DEFAULT_BIND_HOST = "127.0.0.1"
DEFAULT_PORT = 9001


def _fields(value, required, optional=(), label="configuration"):
    if (
        not isinstance(value, dict)
        or set(value) - set(required) - set(optional)
        or set(required) - set(value)
    ):
        raise ValueError(label + " has missing or unknown fields")


def _text(value, limit, label):
    if not isinstance(value, str) or not value:
        raise ValueError(label + " must be a nonempty string")
    try:
        valid = len(value.encode("utf-8")) <= limit
    except UnicodeError:
        valid = False
    if not valid or any(ord(character) < 32 or ord(character) == 127 for character in value):
        raise ValueError(label + " contains invalid characters or exceeds its byte limit")
    return value


def _path(value, label):
    value = _text(value, MAX_CONFIGURATION_PATH_BYTES, label)
    if not (
        value.startswith("/") or value.startswith("\\\\") or re.match(r"^[A-Za-z]:[\\/]", value)
    ):
        raise ValueError(label + " must be absolute on its consumer platform")
    return value


def _host(value, label, native=False):
    value = _text(value, 253, label)
    try:
        address = ipaddress.ip_address(value)
    except ValueError:
        address = None
    if native:
        if not isinstance(address, ipaddress.IPv4Address) or not address.is_loopback:
            raise ValueError(label + " must be an explicit IPv4 loopback address")
    elif address is None:
        labels = (value[:-1] if value.endswith(".") else value).split(".")
        if all(character in "0123456789." for character in value) or any(
            not re.fullmatch(r"[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?", part)
            for part in labels
        ):
            raise ValueError(label + " must be an ASCII DNS name or IPv4/IPv6 address")
    elif isinstance(address, ipaddress.IPv6Address) and "%" in value:
        raise ValueError(label + " scoped IPv6 addresses are unsupported")
    return value


def _port(value, label):
    if type(value) is not int or not 1 <= value <= 65535:
        raise ValueError(label + " must be an integer from 1 to 65535")
    return value


def legacy_port(environment):
    raw = environment.get("SUNNY_TCP_PORT", str(DEFAULT_PORT))
    if (
        not isinstance(raw, str)
        or not re.fullmatch(r"[1-9][0-9]{0,4}", raw)
        or not 1 <= int(raw) <= 65535
    ):
        raise ValueError(
            "SUNNY_TCP_PORT must be an ASCII decimal port from 1 to 65535 "
            "without signs, whitespace, or leading zeros"
        )
    return int(raw)


def validate_document(document):
    """Validate every present role, including roles not consumed on this machine."""
    _fields(document, ("configuration_schema_version",), ("client", "native"))
    if (
        type(document["configuration_schema_version"]) is not int
        or document["configuration_schema_version"] != CONFIGURATION_SCHEMA_VERSION
    ):
        raise ValueError(
            "Unsupported configuration_schema_version; preserve the source and use a supported migrator"
        )
    if not {"client", "native"} & set(document):
        raise ValueError("Configuration requires a client or native role")
    if "client" in document:
        client = document["client"]
        _fields(client, ("transport", "workspace"), label="client")
        transport = client["transport"]
        if isinstance(transport, dict) and transport.get("mode") == "offline":
            _fields(transport, ("mode",), label="client.transport")
        else:
            _fields(transport, ("mode", "host", "port"), label="client.transport")
            if transport["mode"] != "tcp":
                raise ValueError("client.transport.mode must be 'offline' or 'tcp'")
            _host(transport["host"], "client.transport.host")
            _port(transport["port"], "client.transport.port")
        workspace = client["workspace"]
        if workspace is not None:
            _fields(workspace, ("path", "recovery"), label="client.workspace")
            _path(workspace["path"], "client.workspace.path")
            if workspace["recovery"] not in ("none", "backup"):
                raise ValueError("client.workspace.recovery must be 'none' or 'backup'")
    if "native" in document:
        _fields(document["native"], ("bridge",), label="native")
        bridge = document["native"]["bridge"]
        _fields(bridge, ("bind_host", "port"), label="native.bridge")
        _host(bridge["bind_host"], "native.bridge.bind_host", native=True)
        _port(bridge["port"], "native.bridge.port")
    return document


def parse_document(raw):
    """Reject oversized, deeply nested, duplicate-key and non-UTF8 JSON before effects."""
    if len(raw) > MAX_CONFIGURATION_BYTES:
        raise ValueError("Configuration exceeds 64 KiB")
    text = raw.decode("utf-8-sig")
    depth, quoted, escaped = 0, False, False
    for character in text:
        if quoted:
            if escaped:
                escaped = False
            elif character == "\\":
                escaped = True
            elif character == '"':
                quoted = False
        elif character == '"':
            quoted = True
        elif character in "[{":
            depth += 1
            if depth > MAX_CONFIGURATION_DEPTH:
                raise ValueError("Configuration exceeds depth 8")
        elif character in "]}":
            depth -= 1

    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError("Configuration contains duplicate JSON fields")
            result[key] = value
        return result

    def invalid_constant(_value):
        raise ValueError("Configuration requires finite JSON numbers")

    return validate_document(
        json.loads(text, object_pairs_hook=unique, parse_constant=invalid_constant)
    )


def load_native_configuration(environment=None):
    """Return (host, port, contract) without creating Live, log or socket state."""
    environment = os.environ if environment is None else environment
    if "SUNNY_CONFIG_PATH" in environment:
        if any(key in environment for key in LEGACY_KEYS):
            raise ValueError(
                "SUNNY_CONFIG_PATH cannot be combined with legacy Sunny runtime variables"
            )
        selected = _text(
            environment["SUNNY_CONFIG_PATH"], MAX_CONFIGURATION_PATH_BYTES, "SUNNY_CONFIG_PATH"
        )
        if not Path(selected).is_file():
            raise ValueError("Selected configuration must be a regular file")
        with Path(selected).open("rb") as source:
            document = parse_document(source.read(MAX_CONFIGURATION_BYTES + 1))
        if "native" not in document:
            raise ValueError("Configuration requires the native role for the Remote Script")
        bridge = document["native"]["bridge"]
        return bridge["bind_host"], bridge["port"], CONFIGURATION_CONTRACT
    host = environment.get("SUNNY_BIND_HOST", DEFAULT_BIND_HOST)
    _text(host, 253, "SUNNY_BIND_HOST")
    return host, legacy_port(environment), "legacy_environment"


def migrate_native_environment(output, environment=None):
    """Write a new validated native-role document; preserve all prior settings and files."""
    environment = os.environ if environment is None else environment
    if "SUNNY_CONFIG_PATH" in environment:
        raise ValueError("Legacy migration requires SUNNY_CONFIG_PATH to be unset")
    host, port, _ = load_native_configuration(environment)
    document = validate_document(
        {
            "configuration_schema_version": CONFIGURATION_SCHEMA_VERSION,
            "native": {"bridge": {"bind_host": host, "port": port}},
        }
    )
    body = (json.dumps(document, indent=2, ensure_ascii=True) + "\n").encode("utf-8")
    descriptor = os.open(output, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(descriptor, "wb") as target:
        target.write(body)
        target.flush()
        os.fsync(target.fileno())
    return document


def main(argv=None):
    """Provide deliberate offline validation or exclusive legacy migration."""
    parser = argparse.ArgumentParser(description=__doc__)
    operation = parser.add_mutually_exclusive_group(required=True)
    operation.add_argument("--migrate-legacy", type=Path, metavar="OUTPUT")
    operation.add_argument("--validate", action="store_true")
    args = parser.parse_args(argv)
    try:
        if args.migrate_legacy is not None:
            migrate_native_environment(args.migrate_legacy)
            contract = CONFIGURATION_CONTRACT
        else:
            _, _, contract = load_native_configuration()
    except (OSError, ValueError, UnicodeError) as error:
        parser.error(str(error))
    print(
        json.dumps(
            {
                "valid": True,
                "role": "native",
                "configuration_contract": contract,
                "configuration_schema_version": CONFIGURATION_SCHEMA_VERSION
                if contract == CONFIGURATION_CONTRACT
                else None,
            }
        )
    )


if __name__ == "__main__":
    main()
