"""Literal Docker grammar for the image-owned final developer qualification kit."""

from __future__ import annotations

import copy
import importlib.util
import socket
import subprocess
import sys
from pathlib import Path

import pytest

IMAGE = "sha256:" + "a" * 64
OTHER_IMAGE = "sha256:" + "b" * 64
KIT = Path(__file__).resolve().parents[2] / "tools/live_qualification/common.py"


@pytest.fixture
def common():
    """Load the real maintained helper without a child or daemon operation."""
    specification = importlib.util.spec_from_file_location("qualification_launch_test", KIT)
    module = importlib.util.module_from_spec(specification)
    specification.loader.exec_module(module)
    return module


def configuration():
    """Use literal production argv with one external JSON file and one data directory."""
    return {
        "release_directory": "/verified/offline/release",
        "image_local_immutable_id": IMAGE,
        "host_mount_directory": "/caller/qualification data",
        "server_mount_prefix": "/data",
        "mcp_command": [
            "docker",
            "run",
            "-i",
            "--rm",
            "--pull=never",
            "--mount",
            "type=bind,source=/caller/production.json,target=/run/sunny/configuration.json,readonly",
            "--mount",
            "type=bind,source=/caller/qualification data,target=/data",
            "--env",
            "SUNNY_CONFIG_PATH=/run/sunny/configuration.json",
            IMAGE,
        ],
    }


@pytest.mark.parametrize("spelling", ["literal", "equals", "interactive", "windows_cli"])
def test_correct_final_profile_identifies_actual_image(common, spelling):
    """Useful canonical spellings select the exact immutable image, never a trailing command."""
    config = configuration()
    command = config["mcp_command"]
    if spelling == "equals":
        for index in (9, 7, 5):
            command[index : index + 2] = [command[index] + "=" + command[index + 1]]
    elif spelling == "interactive":
        command[2] = "--interactive"
        command[4:5] = ["--pull", "never"]
    elif spelling == "windows_cli":
        command[0] = "/mnt/c/Program Files/Docker/Docker/resources/bin/docker.exe"
    assert common.qualification_command(config, IMAGE) is command


def test_bounded_optional_identity_and_bridge_route_options(common):
    """The route does not require host networking, executable overlays or extra environment."""
    config = configuration()
    config["mcp_command"][2:2] = [
        "--name",
        "sunny-final-1",
        "--label",
        "qualification.run=literal-1",
        "--platform=linux/amd64",
        "--network=bridge",
        "--add-host=host.docker.internal:host-gateway",
    ]
    assert common.qualification_command(config, IMAGE) == config["mcp_command"]


@pytest.mark.parametrize("selector", [None, "", False, []])
def test_missing_release_cannot_fall_through_to_an_unverified_child(
    common, monkeypatch, tmp_path, selector
):
    """Reconnect/readback/reconcile launches obey the same complete-release boundary."""
    config = {"mcp_command": ["python3", "-c", "pass"]}
    if selector is not None:
        config["release_directory"] = selector
    monkeypatch.setattr(
        subprocess, "Popen", lambda *args, **kwargs: pytest.fail("Unverified child launched")
    )
    with pytest.raises(ValueError, match="complete verified release"):
        common.Mcp(config, tmp_path / "observations.jsonl")


@pytest.mark.parametrize(
    "fault",
    [
        "different_image",
        "wrong_image_trailing_verified_id",
        "trailing_command",
        "entrypoint",
        "binary_overlay",
        "operator_overlay",
        "root_overlay",
        "data_parent_overlay",
        "legacy_env",
        "env_file",
        "unselected_config",
        "missing_config_readonly",
        "readonly_false",
        "readonly_data",
        "different_data",
        "duplicate_data",
        "duplicate_config",
        "duplicate_mount_field",
        "mount_alias",
        "unknown_mount_field",
        "volume_shorthand",
        "missing_mount_value",
        "host_network",
        "custom_network",
        "user",
        "workdir",
        "platform",
        "add_host",
        "tty",
        "detach",
        "pull_latest",
        "missing_pull",
        "missing_rm",
        "missing_interactive",
        "duplicate_env",
        "duplicate_option",
        "duplicate_label",
        "reserved_label",
        "missing_image",
        "boolean_argument",
    ],
)
def test_unpaired_or_ambiguous_profile_declines_before_any_child_or_socket(
    common, monkeypatch, tmp_path, fault
):
    """The real Mcp boundary fails closed before process, daemon or native endpoint access."""
    config = configuration()
    command = config["mcp_command"]
    if fault == "different_image":
        command[-1] = OTHER_IMAGE
    elif fault == "wrong_image_trailing_verified_id":
        command[-1:] = [OTHER_IMAGE, IMAGE]
    elif fault == "trailing_command":
        command.append("another-server")
    elif fault == "entrypoint":
        command[2:2] = ["--entrypoint=/tmp/other-server"]
    elif fault.endswith("overlay"):
        target = {
            "binary_overlay": "/usr/local/bin/sunny-mcp",
            "operator_overlay": "/opt/sunny/operator",
            "root_overlay": "/",
            "data_parent_overlay": "/run",
        }[fault]
        command[2:2] = ["--mount", "type=bind,source=/caller/older,target=" + target]
    elif fault == "legacy_env":
        command[2:2] = ["--env", "SUNNY_ABLETON_HOST=other-host"]
    elif fault == "env_file":
        command[2:2] = ["--env-file=/caller/other.env"]
    elif fault == "unselected_config":
        command[10] = "SUNNY_CONFIG_PATH=/caller/other.json"
    elif fault == "missing_config_readonly":
        command[6] = command[6].removesuffix(",readonly")
    elif fault == "readonly_false":
        command[6] += "=false"
    elif fault == "readonly_data":
        command[8] += ",readonly"
    elif fault == "different_data":
        command[8] = command[8].replace("/caller/qualification data", "/caller/other")
    elif fault == "duplicate_data":
        command[2:2] = command[7:9]
    elif fault == "duplicate_config":
        command[2:2] = command[5:7]
    elif fault == "duplicate_mount_field":
        command[8] += ",target=/data"
    elif fault == "mount_alias":
        command[8] = command[8].replace("source=", "src=")
    elif fault == "unknown_mount_field":
        command[8] += ",bind-propagation=rshared"
    elif fault == "volume_shorthand":
        command[2:2] = ["-v", "/caller/older:/usr/local/bin"]
    elif fault == "missing_mount_value":
        command[-1:] = ["--mount"]
    elif fault == "host_network":
        command[2:2] = ["--network=host"]
    elif fault == "custom_network":
        command[2:2] = ["--network=other-network"]
    elif fault == "user":
        command[2:2] = ["--user=0"]
    elif fault == "workdir":
        command[2:2] = ["--workdir=/caller"]
    elif fault == "platform":
        command[2:2] = ["--platform=linux/arm64"]
    elif fault == "add_host":
        command[2:2] = ["--add-host=host.docker.internal:192.0.2.1"]
    elif fault == "tty":
        command[2:2] = ["-t"]
    elif fault == "detach":
        command[2:2] = ["--detach"]
    elif fault == "pull_latest":
        command[4] = "--pull=always"
    elif fault == "missing_pull":
        del command[4]
    elif fault == "missing_rm":
        del command[3]
    elif fault == "missing_interactive":
        del command[2]
    elif fault == "duplicate_env":
        command[2:2] = ["-e", "SUNNY_CONFIG_PATH=/run/sunny/configuration.json"]
    elif fault == "duplicate_option":
        command[2:2] = ["--platform=linux/amd64", "--platform=linux/amd64"]
    elif fault == "duplicate_label":
        command[2:2] = ["--label=qualification.run=1", "--label=qualification.run=2"]
    elif fault == "reserved_label":
        command[2:2] = ["--label=com.sunny.doctor.owner=caller"]
    elif fault == "missing_image":
        command.pop()
    elif fault == "boolean_argument":
        command[3] = True
    monkeypatch.setattr(
        common, "verify_selected_release", lambda ignored: {"image": {"local_immutable_id": IMAGE}}
    )
    monkeypatch.setattr(
        subprocess, "Popen", lambda *args, **kwargs: pytest.fail("Unpaired launch started a child")
    )
    monkeypatch.setattr(
        socket, "socket", lambda *args, **kwargs: pytest.fail("Unpaired launch reached a socket")
    )
    with pytest.raises(ValueError):
        common.Mcp(copy.deepcopy(config), tmp_path / "no-effect.jsonl")
    assert not (tmp_path / "no-effect.jsonl").exists()


@pytest.mark.parametrize(
    "host,port",
    [
        ("0.0.0.0", 9003),
        ("192.0.2.1", 9003),
        ("localhost", 9003),
        ("::1", 9003),
        ("127.0.0.1", 0),
        ("127.0.0.1", 65536),
    ],
)
def test_fault_proxy_refuses_plain_tcp_exposure_before_configuration_or_listener(
    tmp_path, host, port
):
    """The finite effect-forwarding instrument shares the supported loopback boundary."""
    log = tmp_path / "no-effects.jsonl"
    result = subprocess.run(
        [
            sys.executable,
            str(KIT.with_name("fault_proxy.py")),
            "--config",
            str(tmp_path / "configuration-does-not-exist.json"),
            "--listen-host",
            host,
            "--listen-port",
            str(port),
            "--drop-method",
            "sunny_managed_create_clip",
            "--log",
            str(log),
        ],
        capture_output=True,
        text=True,
        timeout=5,
    )
    assert result.returncode == 2, result.stderr
    assert "requires a numeric IPv4 loopback and port 1..65535" in result.stderr
    assert "FileNotFoundError" not in result.stderr
    assert not log.exists()
