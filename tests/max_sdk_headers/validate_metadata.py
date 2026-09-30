"""Validate authored Max package metadata against the registered wrapper methods.

Each external's reference page, help patcher and `class_addmethod` registrations must describe
the same public method set; a method documented but not registered (or the reverse) is a user-
visible defect that no compiler catches.
"""

from __future__ import annotations

import json
import re
import xml.etree.ElementTree as element_tree
from pathlib import Path

OBJECTS = {
    "sunny.lfo~": "sunny.lfo_tilde",
    "sunny.adsr~": "sunny.adsr_tilde",
    "sunny.hold~": "sunny.hold_tilde",
    "sunny.clock~": "sunny.clock_tilde",
    "sunny.events": "sunny.events",
}
SDK_TO_REFERENCE_TYPE = {"A_FLOAT": "float", "A_LONG": "int", "A_SYM": "symbol"}
HOST_SELECTORS = {"assist", "dsp64"}


def _message_selector(text: str) -> str:
    selector = text.strip().split()[0]
    try:
        float(selector)
    except ValueError:
        return selector
    return "float"


def _registered_schemas(
    object_name: str, source: str, documented: dict[str, tuple[str, ...]]
) -> dict[str, tuple[str, ...]]:
    registrations = re.findall(
        r"class_addmethod\(\s*klass,\s*reinterpret_cast<method>\(([^)]+)\),\s*"
        r'"([^"]+)",\s*([^;]+?)\);',
        source,
        flags=re.DOTALL,
    )
    schemas: dict[str, tuple[str, ...]] = {}
    for _, selector, sdk_arguments in registrations:
        if selector in HOST_SELECTORS:
            continue
        tokens = []
        for token in (part.strip() for part in sdk_arguments.split(",")):
            if token == "0":
                break
            tokens.append(token)
        if selector in schemas:
            raise ValueError(f"{object_name} registers {selector} more than once")
        if tokens == ["A_GIMME"]:
            # A_GIMME handlers validate their own atoms; the reference page is the schema.
            schemas[selector] = documented.get(selector, ("gimme",))
            continue
        unknown = [token for token in tokens if token not in SDK_TO_REFERENCE_TYPE]
        if unknown:
            raise ValueError(f"{object_name} {selector} uses unsupported SDK types {unknown}")
        schemas[selector] = tuple(SDK_TO_REFERENCE_TYPE[token] for token in tokens)
    return schemas


def main() -> None:
    """Reject divergence between package metadata and registered wrapper methods."""
    repository = Path(__file__).resolve().parents[2]
    package = repository / "max-package"
    manifest = json.loads((package / "package-info.json").read_text(encoding="utf-8"))
    if manifest["name"] != "Sunny" or manifest["version"] != "0.4.0":
        raise ValueError("Max manifest identity/version diverges from Sunny 0.4.0")

    package_cmake = (package / "CMakeLists.txt").read_text(encoding="utf-8")
    for object_name, source_directory in OBJECTS.items():
        if f"add_subdirectory(source/{source_directory})" not in package_cmake:
            raise ValueError(f"{object_name} is not attached to the staged Max package")

        root = element_tree.parse(package / "docs" / f"{object_name}.maxref.xml").getroot()
        if root.tag != "c74object" or root.get("name") != object_name:
            raise ValueError(f"invalid Max reference identity for {object_name}")
        documented = {
            method.get("name", ""): tuple(
                argument.get("type", "") for argument in method.findall("./arglist/arg")
            )
            for method in root.findall("./methodlist/method")
        }

        help_boxes = [
            box["box"]
            for box in json.loads(
                (package / "help" / f"{object_name}.maxhelp").read_text(encoding="utf-8")
            )["patcher"]["boxes"]
        ]
        instantiated = {
            box["text"].split()[0]
            for box in help_boxes
            if box.get("maxclass") == "newobj" and box.get("text")
        }
        if object_name not in instantiated:
            raise ValueError(f"{object_name} help patcher does not instantiate the object")
        help_selectors = {
            _message_selector(box["text"])
            for box in help_boxes
            if box.get("maxclass") == "message" and box.get("text")
        }
        if set(documented) - help_selectors:
            raise ValueError(
                f"{object_name} help patcher omits methods "
                f"{sorted(set(documented) - help_selectors)}"
            )

        source = (package / "source" / source_directory / f"{source_directory}.cpp").read_text(
            encoding="utf-8"
        )
        registered = _registered_schemas(object_name, source, documented)
        if documented != registered:
            raise ValueError(
                f"{object_name} method metadata mismatch: documented={documented}, "
                f"registered={registered}"
            )


if __name__ == "__main__":
    main()
