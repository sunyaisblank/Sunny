"""Physical source selections through actual owning MCP tools and durable restart."""

from __future__ import annotations

from test_mcp_workspace_workflow import WorkspaceProcess, _call


def test_drift_plan_reads_authored_hz_and_ms_without_legacy_mapping(tmp_path, monkeypatch):
    """Literal Hz/ms intent survives edits and restart without native access."""
    process = WorkspaceProcess(tmp_path, monkeypatch)
    pairs = (
        ("source.filter.cutoff", "LP Freq", "drift.lp.frequency", 1200.0, "Hertz", 20.0, 20000.0),
        (
            "source.amplifier.stages[0].duration",
            "Env 1 Attack",
            "drift.env.1.attack",
            250.0,
            "Milliseconds",
            0.0,
            10000.0,
        ),
        (
            "source.amplifier.stages[1].duration",
            "Env 1 Decay",
            "drift.env.1.decay",
            500.0,
            "Milliseconds",
            0.0,
            10000.0,
        ),
        (
            "source.amplifier.stages[2].duration",
            "Env 1 Release",
            "drift.env.1.release",
            1000.0,
            "Milliseconds",
            0.0,
            10000.0,
        ),
    )
    selections = [
        {"source_path": path, "capability_id": capability, "tolerance": 0.1}
        for path, _, capability, _, _, _, _ in pairs
    ]
    with process.start() as client:
        _call(
            client,
            "score_create",
            title="Explicit Drift",
            total_bars=1,
            parts=[{"name": "Synth", "instrument_type": 47}],
        )
        _call(client, "create_project", score_id=1)
        _call(
            client,
            "set_sound_source",
            profile_id=1,
            source_type="subtractive",
            filter_cutoff=1200.0,
            attack=250.0,
            decay=500.0,
            sustain=0.4,
            release=1000.0,
        )
        for path, name, _, _, _, minimum, maximum in pairs:
            _call(
                client,
                "map_timbre_parameter",
                profile_id=1,
                ir_path=path,
                device_index=0,
                parameter_name=name,
                device_name="Drift",
                source_min=minimum,
                source_max=maximum,
                target_min=0.0,
                target_max=1.0,
                value_property="display_value",
            )
        authored = _call(client, "get_timbre_json", profile_id=1)
        project = _call(client, "get_project_json", score_id=1)
        revision = project["project"]["revision"]
        planned = _call(
            client,
            "project_realization_plan_timbre",
            score_id=1,
            part_id=1,
            expected_project_revision=revision,
            selections=selections,
        )
        assert planned["mutation_dispatched"] is False
        assert planned["native_observation_available"] is False
        assert planned["complete_project_realization"] is False
        plan = planned["plan"]
        assert (
            plan["selection_semantics"] == "ReadAuthoredPhysicalValueWithoutApplyingLegacyMapping"
        )
        assert [(item["target"], item["unit"]) for item in plan["intents"]] == [
            (1200.0, "Hertz"),
            (250.0, "Milliseconds"),
            (500.0, "Milliseconds"),
            (1000.0, "Milliseconds"),
        ]
        assert plan["device_browser_name"] == plan["device_class_name"] == "Drift"
        assert plan["dsp_equivalence_qualified"] is False and plan["host_qualified"] is False
        assert plan["residuals"]  # Sustain/shape/modulation/effects retain explicit coverage gaps.
        assert _call(client, "get_timbre_json", profile_id=1) == authored
        assert _call(client, "get_project_json", score_id=1) == project
        stale = client.call(
            "project_realization_plan_timbre",
            score_id=1,
            part_id=1,
            expected_project_revision=revision - 1,
            selections=selections,
        )
        assert stale["success"] is False and "revision changed" in stale["error"]
        _call(client, "workspace_save", path=process.path)
    with process.start() as restarted:
        restored_revision = _call(restarted, "get_project_json", score_id=1)["project"]["revision"]
        assert (
            _call(
                restarted,
                "project_realization_plan_timbre",
                score_id=1,
                part_id=1,
                expected_project_revision=restored_revision,
                selections=selections,
            )["plan"]
            == plan
        )
