"""Exercise the no-Python Windows doctor through actual owned processes and pipes."""

from __future__ import annotations

import base64
import json
import os
import shutil
import subprocess
import tempfile
from pathlib import Path

import pytest


def test_windows_owned_readonly_doctor(record_testsuite_property):
    """Check literal peers, real jobs, NTFS receipts and an available local Docker fixture."""
    powershell = shutil.which("powershell.exe")
    if powershell is None:
        pytest.skip("Windows PowerShell interop is required for owned Windows job witnesses")
    root = Path(__file__).resolve().parents[2]
    driver = (root / "tests/windows/doctor_test.ps1").read_text()
    for placeholder, name in (
        ("__INSTALLER_SOURCE__", "Sunny.ps1"),
        ("__REMOTE_SOURCE__", "SunnyRemote.ps1"),
        ("__CLIENT_SOURCE__", "SunnyClient.ps1"),
        ("__DOCTOR_SOURCE__", "SunnyDoctor.ps1"),
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
    with tempfile.TemporaryDirectory(prefix="Sunny-doctor-driver-", dir=local) as directory:
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
    report = json.loads(completed.stdout.strip().splitlines()[-1])
    assert report["ssh_executed"] is False and report["native_live_executed"] is False
    assert report["physical_kernel_stalls_tested"] is False
    assert report["incremental_polling_fixtures_passed"] is True
    assert type(report["actual_docker_witness"]) is bool
    record_testsuite_property(
        "actual_windows_docker_orphan_cleanup", report["actual_docker_witness"]
    )
    assert report["passed"] == [
        "fresh_closed_readonly_protocol_and_exact_owned_cleanup",
        "typed_pairing_session_freshness_and_correlated_closed_protocol_refusals",
        "honest_offline_evidence_and_semantic_structured_content",
        "normal_exit_and_trailing_output_are_part_of_readiness",
        "deadline_and_stdout_stderr_bounds_reap_actual_owned_process",
        "actual_descendant_cannot_escape_suspended_owned_job",
        "durable_exact_child_identity_precedes_any_resumed_launcher_code",
        "exclusive_standard_handle_list_preserves_unrelated_caller_pipes",
        "unproved_ownership_and_uncertain_cleanup_preserve_durable_evidence",
        "actual_readonly_checkpoint_denial_cannot_resume_or_leak_owned_child",
        "optin_exclusive_bounded_valid_json_export_and_value_redaction",
        "escaped_duplicate_keys_and_actual_ntfs_hardlinks_refused",
        "exact_verified_release_sources_and_failclosed_recovery_admission",
        "fixed_base64_data_utf8_launcher_holds_verified_bytes_and_propagates_exit",
        "finite_empty_backoff_contiguous_pagination_and_messages_absent_default",
        "reset_rollover_ahead_cursor_and_gap_facts_stay_incomplete_after_progress",
        "strict_log_page_types_fields_timestamps_unicode_and_loss_algebra",
        "poll_deadline_no_retry_retention_caps_and_deliberate_redacted_export",
        "deterministic_continuous_producer_cannot_escape_the_finite_request_cap",
        (
            "actual_local_docker_orphan_exact_cleanup_and_foreign_preservation"
            if report["actual_docker_witness"]
            else "docker_orphan_fixture_not_run_without_preloaded_busybox"
        ),
    ]
