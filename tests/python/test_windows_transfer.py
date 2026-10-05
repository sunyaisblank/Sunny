"""Exercise owned offline-release transfer on actual NTFS and Windows binary pipes."""

from __future__ import annotations

import base64
import json
import os
import shutil
import subprocess
import tempfile
from pathlib import Path

import pytest


def test_windows_release_transfer_foundation():
    """Use local filesystem and actual child processes without any SSH endpoint access."""
    powershell = shutil.which("powershell.exe")
    if powershell is None:
        pytest.skip("Windows PowerShell interop is required for the NTFS transfer witness")
    root = Path(__file__).resolve().parents[2]
    driver = (root / "tests/windows/transfer_test.ps1").read_text()
    for placeholder, name in (
        ("__INSTALLER_SOURCE__", "Sunny.ps1"),
        ("__TRANSFER_SOURCE__", "SunnyTransfer.ps1"),
        ("__REMOTE_SOURCE__", "SunnyRemote.ps1"),
    ):
        driver = driver.replace(
            placeholder, base64.b64encode((root / "tools/windows" / name).read_bytes()).decode()
        )
    temporary = subprocess.check_output(
        [powershell, "-NoProfile", "-NonInteractive", "-Command", "[IO.Path]::GetTempPath()"],
        text=True,
        timeout=10,
    ).strip()
    converter = shutil.which("wslpath")
    if os.name != "nt" and converter is None:
        pytest.skip("Windows path conversion is unavailable")
    local = (
        temporary
        if os.name == "nt"
        else subprocess.check_output([converter, "-u", temporary], text=True).strip()
    )
    with tempfile.TemporaryDirectory(prefix="Sunny-transfer-driver-", dir=local) as directory:
        source = Path(directory) / "test.ps1"
        source.write_text(driver, encoding="utf-8-sig")
        selected = (
            str(source)
            if os.name == "nt"
            else subprocess.check_output([converter, "-w", str(source)], text=True).strip()
        )
        completed = subprocess.run(
            [
                powershell,
                "-NoProfile",
                "-NonInteractive",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                selected,
            ],
            capture_output=True,
            text=True,
            timeout=150,
            check=False,
        )
    assert completed.returncode == 0, completed.stdout + completed.stderr
    # Receiver fixtures acknowledge directly through Console.Out. The final line
    # independently reports which complete real filesystem transitions passed.
    report = json.loads(completed.stdout.strip().splitlines()[-1])
    assert report["ssh_executed"] is False and report["native_live_executed"] is False
    assert report["literal_disk_saturation_tested"] is False
    assert report["passed"] == [
        "full_binary_unicode_repeat_read_only_status",
        "truncated_chunk_resume_source_prefix",
        "changed_prefix_preserved_by_recovery_and_clean",
        "authenticated_partial_cleanup",
        "foreign_destination_and_stage_preserved",
        "post_move_recovery_and_owner_mismatch",
        "strict_control_data_and_corrupt_chunks_fail_before_payload_writes",
        "actual_ntfs_hardlinks_and_junctions_preserved",
        "actual_readonly_denial_and_injected_disk_write_rename_recovery",
        "post_proof_cleanup_drift_preserved",
        "expired_filesystem_phase_budgets_preserve_recoverable_states",
        "real_windows_binary_pipe_bootstrap_and_zero_payload_repeat",
        "actual_invalid_acknowledgments_and_output_bounds_reap_owned_peer",
        "actual_sender_receiver_deadlines_pid_absence_and_recovery",
    ]
