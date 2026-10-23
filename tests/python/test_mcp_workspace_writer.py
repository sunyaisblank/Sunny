"""Workspace writer admission through independent real MCP processes."""

from __future__ import annotations

import json
import os
import shlex
import subprocess
from pathlib import Path

import pytest
from test_live_end_to_end import _McpClient, _sunny_mcp_binary


class Processes:
    """One shared mount, independent process lifetimes and literal file evidence."""

    def __init__(self, directory: Path, monkeypatch: pytest.MonkeyPatch) -> None:
        """Select native execution or the existing mounted container test command."""
        self.directory = directory
        self.monkeypatch = monkeypatch
        configured = os.environ.get("SUNNY_WORKSPACE_MCP_COMMAND")
        self.container = bool(configured)
        self.command = (
            [part.format(workspace_dir=str(directory)) for part in shlex.split(configured)]
            if configured
            else [str(_sunny_mcp_binary())]
        )
        self.clients: list[_McpClient] = []
        if self.container:
            directory.chmod(0o777)

    def path(self, name: str) -> str:
        """Use the same file under the native path or the mounted container path."""
        return f"/data/{name}" if self.container else str(self.directory / name)

    def environment(self, name: str | None) -> None:
        """Set only the next child's launch environment."""
        self.monkeypatch.delenv("SUNNY_ABLETON_HOST", raising=False)
        self.monkeypatch.delenv("SUNNY_WORKSPACE_RECOVERY", raising=False)
        if name is None:
            if self.container:
                self.monkeypatch.setenv("SUNNY_WORKSPACE_PATH", self.path("unsaved-client.json"))
            else:
                self.monkeypatch.delenv("SUNNY_WORKSPACE_PATH", raising=False)
        else:
            self.monkeypatch.setenv("SUNNY_WORKSPACE_PATH", self.path(name))

    def start(self, name: str | None = "workspace.json") -> _McpClient:
        """Initialize one actual executable; each client has its own stdin."""
        self.environment(name)
        client = _McpClient(None, 0, host=None, command=self.command)
        self.clients.append(client)
        return client

    def refused_start(self, name: str = "workspace.json") -> None:
        """A competing executable must exit before answering initialize."""
        self.environment(name)
        request = {
            "jsonrpc": "2.0",
            "id": 1,
            "method": "initialize",
            "params": {"protocolVersion": "2024-11-05"},
        }
        result = subprocess.run(
            self.command,
            input=json.dumps(request) + "\n",
            capture_output=True,
            text=True,
            timeout=30,
            check=False,
        )
        assert result.returncode != 0, result
        assert result.stdout == "", result
        assert "Workspace writer admission" in result.stderr, result

    def files(self) -> dict[str, bytes]:
        """Compare actual main, backup and namespace bytes after a refusal."""
        return {
            path.relative_to(self.directory).as_posix(): path.read_bytes()
            for path in self.directory.rglob("*")
            if path.is_file()
        }

    def close(self) -> None:
        """Release every child even if an assertion fails."""
        for client in self.clients:
            if client._process.poll() is None:
                client.close()


@pytest.fixture
def processes(tmp_path: Path, monkeypatch: pytest.MonkeyPatch):
    """Isolate durable files and process ownership for one finite case."""
    directory = tmp_path / "musical work with spaces"
    directory.mkdir()
    factory = Processes(directory, monkeypatch)
    try:
        yield factory
    finally:
        factory.close()


def author(client: _McpClient, title: str) -> int:
    """Create a distinct literal authored document without a Live connection."""
    result = client.call(
        "score_create", title=title, total_bars=1, parts=[{"name": "Piano", "instrument_type": 47}]
    )
    assert "error" not in result, result
    return result["score_id"]


def save(factory: Processes, client: _McpClient, name: str = "workspace.json") -> None:
    """A supported owning save must commit durably."""
    result = client.call("workspace_save", path=factory.path(name))
    assert result["success"] is True, result
    assert result["committed"] is True and result["durability_confirmed"] is True, result


def titles(factory: Processes, name: str = "workspace.json") -> list[str]:
    """Derive expected persistence from literal JSON, independently of the reader."""
    records = json.loads((factory.directory / name).read_text())["scores"]
    return [record["document"]["metadata"]["title"] for record in records]


def test_new_and_restored_owner_refuse_second_writer_without_changing_bytes(processes):
    """Admission precedes new/restored startup, not merely native mutation."""
    owner = processes.start()
    before = processes.files()
    processes.refused_start()
    assert processes.files() == before
    assert not (processes.directory / "workspace.json").exists()
    author(owner, "Original authored phrase")
    save(processes, owner)
    save(processes, owner)
    owner.close()

    restored = processes.start()
    author(restored, "Owning revision")
    save(processes, restored)
    before = processes.files()
    processes.refused_start()
    assert processes.files() == before
    assert titles(processes) == ["Original authored phrase", "Owning revision"]
    restored.close()
    replacement = processes.start()
    author(replacement, "Replacement after clean exit")
    save(processes, replacement)
    assert titles(processes)[-1] == "Replacement after clean exit"


def test_crashed_owner_releases_admission_without_removing_history(processes):
    """Kernel ownership releases on actual process death, without deleting locks."""
    if processes.container:
        pytest.skip(
            "CLI death does not prove container death; test named container kill separately"
        )
    owner = processes.start()
    author(owner, "Survives process death")
    save(processes, owner)
    before = processes.files()
    owner._process.kill()
    owner._process.wait(timeout=10)
    assert processes.files() == before
    replacement = processes.start()
    save(processes, replacement)
    assert titles(processes) == ["Survives process death"]


def test_independent_workspaces_and_reserved_backup_names(processes):
    """Independent files work concurrently; a writable backup aliases owned state."""
    first = processes.start("first.json")
    second = processes.start("second.json")
    author(first, "First independent workspace")
    author(second, "Second independent workspace")
    save(processes, first, "first.json")
    save(processes, first, "first.json")
    save(processes, second, "second.json")
    before = processes.files()
    processes.refused_start("first.json.bak")
    assert processes.files() == before
    assert titles(processes, "first.json") == ["First independent workspace"]
    assert titles(processes, "second.json") == ["Second independent workspace"]


def test_open_save_and_applied_recovery_cannot_bypass_configured_owner(processes):
    """An initially unconfigured client cannot acquire an already owned target."""
    owner = processes.start()
    author(owner, "Owner's preserved document")
    save(processes, owner)
    save(processes, owner)
    outsider = processes.start(None)
    sid = author(outsider, "Outsider's unsaved document")
    before = processes.files()
    for tool, arguments in (
        ("workspace_open", {}),
        ("workspace_save", {}),
        ("workspace_recover_backup", {"apply": True}),
    ):
        result = outsider.call(tool, path=processes.path("workspace.json"), **arguments)
        assert result["success"] is False, result
        assert "Workspace writer admission" in result["error"], result
        assert processes.files() == before
        score = outsider.call("score_get_json", score_id=sid)
        assert score["metadata"]["title"] == "Outsider's unsaved document"
    preview = outsider.call("workspace_recover_backup", path=processes.path("workspace.json"))
    assert preview["success"] is True and preview["preview"] is True, preview
    assert processes.files() == before
    owner.close()
    restored = outsider.call(
        "workspace_recover_backup", path=processes.path("workspace.json"), apply=True
    )
    assert restored["success"] is True, restored
    processes.refused_start()
    save(processes, outsider)


def test_open_keeps_prior_workspace_owned_until_process_exit(processes):
    """A session transition cannot leave an older in-memory writer unprotected."""
    first = processes.start("first.json")
    author(first, "First snapshot")
    save(processes, first, "first.json")
    second = processes.start("second.json")
    author(second, "Second snapshot")
    save(processes, second, "second.json")
    second.close()
    opened = first.call("workspace_open", path=processes.path("second.json"))
    assert opened["success"] is True, opened
    processes.refused_start("first.json")
    processes.refused_start("second.json")
    first.close()
    processes.start("first.json")
    processes.start("second.json")


def test_legacy_migration_remains_owned_before_and_after_save(processes):
    """Version-one restoration cannot defer exclusion until native history exists."""
    seed = processes.start()
    author(seed, "Migrated authored music")
    save(processes, seed)
    seed.close()
    path = processes.directory / "workspace.json"
    legacy = json.loads(path.read_text())
    legacy["version"] = 1
    del legacy["native_realization"]
    path.write_text(json.dumps(legacy))
    owner = processes.start()
    before = processes.files()
    processes.refused_start()
    assert processes.files() == before
    save(processes, owner)
    assert json.loads(path.read_text())["version"] == 2
    before = processes.files()
    processes.refused_start()
    assert processes.files() == before
    assert titles(processes) == ["Migrated authored music"]


def test_canonical_parent_alias_cannot_create_second_writer(processes):
    """A parent symlink resolves to the existing workspace admission."""
    if processes.container:
        pytest.skip("host parent alias is outside this container's declared mount")
    owner = processes.start()
    author(owner, "Canonical parent's music")
    save(processes, owner)
    alias = processes.directory.parent / "alias"
    alias.symlink_to(processes.directory, target_is_directory=True)
    before = processes.files()
    processes.monkeypatch.setenv("SUNNY_WORKSPACE_PATH", str(alias / "workspace.json"))
    result = subprocess.run(processes.command, input="", text=True, capture_output=True, timeout=30)
    assert result.returncode != 0 and "Workspace writer admission" in result.stderr
    assert processes.files() == before


def test_lock_replacement_revokes_publication_without_changing_saved_music(processes):
    """Atomic-file saves must detect a replaced lock inode and refuse stale ownership."""
    owner = processes.start()
    author(owner, "Before lock replacement")
    save(processes, owner)
    save(processes, owner)
    author(owner, "Must remain unsaved")
    lock = processes.directory / ".workspace.json.sunny-writer.lock"
    lock.rename(processes.directory / "retained-lock")
    lock.write_bytes(b"")
    before = processes.files()
    result = owner.call("workspace_save", path=processes.path("workspace.json"))
    assert result["success"] is False and "identity changed" in result["error"], result
    assert processes.files() == before
    assert titles(processes) == ["Before lock replacement"]


@pytest.mark.parametrize("alias_kind", ["symlink", "hardlink"])
def test_final_file_aliases_are_refused_before_publication(processes, alias_kind):
    """Atomic replacement cannot safely preserve two writable final-file aliases."""
    seed = processes.start()
    author(seed, "Alias preserves this music")
    save(processes, seed)
    seed.close()
    alias = processes.directory / "alias.json"
    target = processes.directory / "workspace.json"
    if alias_kind == "symlink":
        alias.symlink_to(target.name)
    else:
        alias.hardlink_to(target)
    before = target.read_bytes()
    processes.refused_start("alias.json")
    assert target.read_bytes() == before
