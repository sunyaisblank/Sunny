"""Authored bypass does not turn new selected physical values into observed values."""

from __future__ import annotations

from decimal import Decimal

from test_mcp_native_device_workflow import _source, _timbre
from test_mcp_native_effect_workflow import _effect_selections, _effects, _parameters
from test_mcp_native_effect_workflow import effect_workflow as effect_workflow
from test_mcp_realization_workflow import _author, _ok, _realize


def test_changed_selected_bypassed_eq_authors_new_physical_values_before_final_bypass(request):
    """Saved733/-5/1.75 cannot prove newly selected1200/+3/2.75 by observing only Off."""
    workflow = request.getfixturevalue("effect_workflow")
    with workflow.process() as client:
        _author(client, workflow)
        source = _source(client)
        effects, _, eq_id = _effect_selections(client, enabled=False)
        assert _realize(client, "project_realization_create")["success"]
        assert _timbre(client, source)["success"]
        authored = _effects(client, effects)
        assert authored["success"] and authored["selected_effect_phases_completed"], authored
        native = workflow.live.song.tracks[1]._devices[3]
        parameters = _parameters(native)
        assert native.is_active is False
        old = tuple(
            parameters[name].value for name in ("1 Frequency A", "1 Gain A", "1 Resonance A")
        )
        assert old != (0.5, 0.625, 0.75)
        _ok(
            client,
            "replace_mix_effect",
            graph_id=1,
            effect_id=eq_id,
            configuration={
                "effect_type": "eq",
                "enabled": False,
                "bands": [{"frequency": 1200.0, "gain": 3.0, "q": 2.75, "type": 0}],
            },
        )
        revised = _effects(client, effects)
        actual = tuple(
            parameters[name].value for name in ("1 Frequency A", "1 Gain A", "1 Resonance A")
        )
        displays = tuple(
            Decimal(parameters[name].oracle(parameters[name].value).split()[0])
            for name in ("1 Frequency A", "1 Gain A", "1 Resonance A")
        )
        diagnostic = {
            "success": revised.get("success"),
            "state": revised.get("state"),
            "selected_effect_phases_completed": revised.get("selected_effect_phases_completed"),
            "phase_attempt_ids": revised.get("phase_attempt_ids"),
            "old_internal_values": old,
            "actual_internal_values": actual,
            "expected_internal_values": (0.5, 0.625, 0.75),
            "actual_display_values": tuple(str(value) for value in displays),
            "expected_display_values": ("1200", "3", "2.75"),
            "actual_bypass": native.is_active is False,
        }
        assert revised["success"] and revised["selected_effect_phases_completed"], diagnostic
        assert all(
            abs(actual - Decimal(target)) <= Decimal("0.01")
            for actual, target in zip(displays, ("1200", "3", "2.75"))
        ), diagnostic
        assert native.is_active is False
