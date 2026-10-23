"""Opt-in released-image named-volume proof, without Live or SSH access.

The fixture owns every container and volume it creates. It proves stopped-writer
backup, literal authored/history preservation, and actual container death; native
mutation receipts and two-host musical qualification remain separate boundaries.
"""

from __future__ import annotations

import hashlib
import json
import os
import re
import subprocess
import tarfile
import uuid

import pytest
from test_live_end_to_end import _McpClient


def test_released_image_named_volume_backup_restore_and_writer_lifetime(tmp_path):
    """Restore all hidden history and modes, then read authored state through MCP."""
    if os.environ.get("SUNNY_RELEASE_VOLUME_TESTS") != "1":
        pytest.skip("Enable the local released-image named-volume witness explicitly")
    image = os.environ.get("SUNNY_RELEASE_VOLUME_IMAGE", "")
    assert re.fullmatch(r"sha256:[0-9a-f]{64}", image), "Supply an immutable image ID"
    owner = uuid.uuid4().hex
    label = "com.sunny.fixture.owner=" + owner
    containers, volumes, clients = [], [], []

    def docker(*arguments, **kwargs):
        return subprocess.run(
            ["docker", *arguments], capture_output=True, text=True, timeout=30, **kwargs
        )

    def successful(*arguments):
        result = docker(*arguments)
        assert result.returncode == 0, (arguments, result.stdout, result.stderr)
        return result.stdout

    context = json.loads(successful("context", "inspect"))[0]
    assert context["Endpoints"]["docker"]["Host"].startswith(("unix://", "npipe://"))
    assert not os.environ.get("DOCKER_HOST") or os.environ["DOCKER_HOST"].startswith(
        ("unix://", "npipe://")
    ), "Local Docker engine only"
    daemon = successful("info", "--format", "{{.ID}}").strip()
    assert daemon
    configuration = tmp_path / "offline configuration.json"
    configuration.write_text(
        json.dumps(
            {
                "configuration_schema_version": 1,
                "client": {
                    "transport": {"mode": "offline"},
                    "workspace": {"path": "/data/workspace.sunny.json", "recovery": "none"},
                },
            }
        )
    )
    configuration.chmod(0o644)

    def volume(kind):
        name = "sunny-volume-fixture-" + owner + "-" + kind
        successful("volume", "create", "--label", label, name)
        volumes.append(name)
        return name

    def command(data, name):
        containers.append(name)
        return [
            "docker",
            "run",
            "-i",
            "--rm",
            "--pull=never",
            "--network",
            "none",
            "--name",
            name,
            "--label",
            label,
            "--mount",
            f"type=volume,source={data},target=/data",
            "--mount",
            f"type=bind,source={configuration},target=/run/sunny/configuration.json,readonly",
            "--env",
            "SUNNY_CONFIG_PATH=/run/sunny/configuration.json",
            image,
        ]

    def start(data, suffix):
        name = "sunny-volume-fixture-" + owner + "-" + suffix
        client = _McpClient(None, 0, host=None, command=command(data, name))
        clients.append(client)
        return client, name

    def save(client):
        result = client.call("workspace_save", path="/data/workspace.sunny.json")
        assert result["success"] and result["committed"] and result["durability_confirmed"]
        assert result["native_history_available"], result

    def absence(name):
        assert not successful("ps", "-aq", "--filter", "name=^/" + name + "$").strip()

    def snapshot(data):
        # Independent filesystem evidence includes hidden state, numeric owners,
        # exact bytes and modes. This does not open a second writer.
        code = (
            "import hashlib,json,pathlib,stat; root=pathlib.Path('/data'); result={}; "
            "paths=sorted(root.rglob('*')); "
            "\nfor p in paths:\n"
            " s=p.lstat(); assert not p.is_symlink(); "
            "result[str(p.relative_to(root))]={'mode':stat.S_IMODE(s.st_mode),"
            "'uid':s.st_uid,'gid':s.st_gid,'directory':p.is_dir(),"
            "'sha256':hashlib.sha256(p.read_bytes()).hexdigest() if p.is_file() else None}\n"
            "print(json.dumps(result,sort_keys=True))"
        )
        return json.loads(
            successful(
                "run",
                "--rm",
                "--pull=never",
                "--network",
                "none",
                "--label",
                label,
                "--mount",
                f"type=volume,source={data},target=/data,readonly",
                "--entrypoint",
                "python3",
                image,
                "-c",
                code,
            )
        )

    try:
        original = volume("original")
        client, name = start(original, "owner")
        score = client.call(
            "score_create",
            title="Preserved literal phrase",
            total_bars=1,
            parts=[{"name": "Piano", "instrument_type": 47}],
        )["score_id"]
        save(client)
        save(client)  # Establish a real authored backup, not a synthetic file.
        before = snapshot(original)
        assert any(path.endswith("ledger.json") for path in before), before
        assert "workspace.sunny.json.bak" in before
        assert any(path.endswith(".sunny-writer.lock") for path in before)
        assert all(item["uid"] == 1001 for item in before.values()), before

        rejected_name = "sunny-volume-fixture-" + owner + "-" + "competitor"
        refused = subprocess.run(
            command(original, rejected_name),
            input=json.dumps({"jsonrpc": "2.0", "id": 1, "method": "initialize"}) + "\n",
            capture_output=True,
            text=True,
            timeout=30,
        )
        assert refused.returncode != 0 and refused.stdout == "", refused
        assert "Workspace writer admission" in refused.stderr
        assert snapshot(original) == before
        absence(rejected_name)

        # Kill the actual named container. Killing an attached Docker CLI alone
        # does not establish that kernel writer locks have been released.
        successful("kill", "--signal", "KILL", name)
        client._process.wait(timeout=10)
        absence(name)
        assert snapshot(original) == before
        replacement, replacement_name = start(original, "replacement")
        observed = replacement.call("score_get_json", score_id=score)
        assert observed["metadata"]["title"] == "Preserved literal phrase"
        replacement.close()
        absence(replacement_name)

        backup_dir = tmp_path / "whole volume backups"
        backup_dir.mkdir()
        backup_dir.chmod(0o777)
        archive = backup_dir / "stopped-writer.tar"
        successful(
            "run",
            "--rm",
            "--pull=never",
            "--network",
            "none",
            "--label",
            label,
            "--user",
            "0:0",
            "--entrypoint",
            "tar",
            "--mount",
            f"type=volume,source={original},target=/data,readonly",
            "--mount",
            f"type=bind,source={backup_dir},target=/backup",
            image,
            "--numeric-owner",
            "-C",
            "/data",
            "-cf",
            "/backup/stopped-writer.tar",
            ".",
        )
        trusted_archive_hash = hashlib.sha256(archive.read_bytes()).hexdigest()
        with tarfile.open(archive) as contents:
            assert {member.name.removeprefix("./") for member in contents if member.isfile()} == {
                path for path, item in before.items() if not item["directory"]
            }
            workspace = json.load(contents.extractfile("./workspace.sunny.json"))
        namespace = workspace["native_realization"]["workspace_namespace"]
        assert re.fullmatch(r"[0-9a-f]{32}", namespace)
        assert workspace["native_realization"]["history_base_directory"] == "/data"
        assert workspace["scores"][0]["document"]["metadata"]["title"] == "Preserved literal phrase"

        restored = volume("restored")
        assert hashlib.sha256(archive.read_bytes()).hexdigest() == trusted_archive_hash
        successful(
            "run",
            "--rm",
            "--pull=never",
            "--network",
            "none",
            "--label",
            label,
            "--user",
            "0:0",
            "--entrypoint",
            "tar",
            "--mount",
            f"type=volume,source={restored},target=/data,volume-nocopy",
            "--mount",
            f"type=bind,source={backup_dir},target=/backup,readonly",
            image,
            "--numeric-owner",
            "--same-owner",
            "--same-permissions",
            "-C",
            "/data",
            "-xf",
            "/backup/stopped-writer.tar",
        )
        assert snapshot(restored) == before
        assert snapshot(original) == before
        readback, readback_name = start(restored, "restored-readback")
        assert readback.call("score_get_json", score_id=score) == observed
        # Read the restored real ledger through its query-only surface first;
        # history is opened lazily after restart.
        history = readback.call("ordinary_clip_history")
        assert history["success"] is True and history["workspace_namespace"] == namespace
        assert history["attempts"] == [] and history["mutation_dispatched"] is False
        assert history["undo_stack_restored"] is False
        save(readback)  # UID 1001 can publish with the restored namespace open.
        readback.close()
        absence(readback_name)
        after = snapshot(restored)
        for path in before:
            if path.endswith("ledger.json"):
                assert after[path] == before[path], "Authored save changed native history"
        assert snapshot(original) == before, "Restore or readback touched the source volume"
    finally:
        assert successful("info", "--format", "{{.ID}}").strip() == daemon, (
            "Docker daemon changed; retain fixture resources rather than selecting another engine"
        )
        for name in containers:
            rows = docker("inspect", name)
            if rows.returncode == 0:
                info = json.loads(rows.stdout)[0]
                assert info["Config"]["Labels"].get("com.sunny.fixture.owner") == owner
                successful("rm", "-f", name)
        for client in clients:
            if client._process.poll() is None:
                client.close()
        for data in volumes:
            info = json.loads(successful("volume", "inspect", data))[0]
            assert info["Labels"].get("com.sunny.fixture.owner") == owner
            successful("volume", "rm", data)
