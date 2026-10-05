"""Exercise managed transitions on a real Windows filesystem, away from Live.

Fixture releases model checksum-valid content. They do not qualify Live, SSH,
an OCI archive, disk capacity, or sudden power loss. Faults interrupt the real
copy/rename/state boundaries, and recovery runs from the persisted JSON.
"""

from __future__ import annotations

import base64
import json
import os
import shutil
import subprocess
import tempfile
from pathlib import Path

import pytest

WINDOWS_DRIVER = (Path(__file__).resolve().parents[1] / "windows/installer_test.ps1").read_text()


def test_windows_managed_installer_lifecycle():
    """Run transfer, activation, recovery and preservation on a Windows volume."""
    powershell = shutil.which("powershell.exe")
    if powershell is None:
        pytest.skip("Windows PowerShell is required for real Windows filesystem evidence")
    source = Path(__file__).resolve().parents[2] / "tools/windows/Sunny.ps1"
    driver = WINDOWS_DRIVER.replace("__SOURCE__", base64.b64encode(source.read_bytes()).decode())
    host_source = source.with_name("SunnyHost.ps1")
    driver = driver.replace("__HOST_SOURCE__", base64.b64encode(host_source.read_bytes()).decode())
    remote_source = source.with_name("SunnyRemote.ps1")
    driver = driver.replace(
        "__REMOTE_SOURCE__", base64.b64encode(remote_source.read_bytes()).decode()
    )
    temporary_query = subprocess.run(
        [powershell, "-NoProfile", "-NonInteractive", "-Command", "[IO.Path]::GetTempPath()"],
        capture_output=True,
        text=True,
        check=True,
        timeout=10,
    )
    windows_temp = temporary_query.stdout.strip()
    if os.name == "nt":
        local_temp = windows_temp
    else:
        wslpath = shutil.which("wslpath")
        if wslpath is None:
            pytest.skip("Windows interop path conversion is unavailable")
        local_temp = subprocess.check_output([wslpath, "-u", windows_temp], text=True).strip()
    with tempfile.TemporaryDirectory(prefix="Sunny-installer-driver-", dir=local_temp) as directory:
        script = Path(directory) / "test.ps1"
        script.write_text(driver, encoding="utf-8-sig")
        windows_script = (
            str(script)
            if os.name == "nt"
            else subprocess.check_output([wslpath, "-w", str(script)], text=True).strip()
        )
        completed = subprocess.run(
            [
                powershell,
                "-NoProfile",
                "-NonInteractive",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                windows_script,
            ],
            capture_output=True,
            text=True,
            timeout=90,
        )
    assert completed.returncode == 0, completed.stdout + completed.stderr
    result = json.loads(completed.stdout.strip())
    assert result["native_live_executed"] is False
    assert result["passed"] == [
        "verified_plan_fresh_repeat",
        "interrupted_transfer_checksum",
        "partial_copy_recovery",
        "identical_bridge_pre_activation_recovery",
        "rename_gap_recovery",
        "activated_uncommitted_recovery",
        "committed_update_recovery",
        "backup_retirement_requires_fresh_paired_health",
        "partial_prepared_cleanup_resumes",
        "partial_committed_retirement_resumes",
        "rollback_and_failed_rollback",
        "unmanaged_content_and_concurrent_operation_refused",
        "uninstall_retains_backup_and_user_data",
        "reinstall_preserves_backup_and_refuses_unrecorded_stage",
        "owner_mismatch_refused",
        "unregistered_initial_content_preserved",
        "independent_bounded_read_only_host_log",
        "strict_remote_plan_and_literal_local_payload",
    ]
