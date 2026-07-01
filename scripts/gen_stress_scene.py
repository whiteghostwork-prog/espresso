#!/usr/bin/env python3
# Copyright 2026 The Peaberry Authors
# SPDX-License-Identifier: Apache-2.0
"""Generate a glTF stress scene: N×M cube grid with multiple materials."""

from __future__ import annotations

import argparse
import json
import math
import os
import sys
from pathlib import Path
from typing import Any

# Shared geometry from test_cube.gltf (840 bytes).
CUBE_BUFFER_PATH = Path("assets/models/test_cube.bin")
CUBE_BUFFER_BYTE_LENGTH = 840

MATERIAL_PRESETS: list[dict[str, Any]] = [
    {
        "name": "stress_red",
        "pbrMetallicRoughness": {
            "baseColorFactor": [0.85, 0.2, 0.2, 1.0],
            "metallicFactor": 0.0,
            "roughnessFactor": 0.45,
        },
    },
    {
        "name": "stress_green",
        "pbrMetallicRoughness": {
            "baseColorFactor": [0.2, 0.75, 0.35, 1.0],
            "metallicFactor": 0.5,
            "roughnessFactor": 0.35,
        },
    },
    {
        "name": "stress_blue",
        "pbrMetallicRoughness": {
            "baseColorFactor": [0.25, 0.45, 0.9, 1.0],
            "metallicFactor": 0.8,
            "roughnessFactor": 0.25,
        },
    },
    {
        "name": "stress_gold",
        "pbrMetallicRoughness": {
            "baseColorFactor": [0.95, 0.8, 0.2, 1.0],
            "metallicFactor": 0.2,
            "roughnessFactor": 0.7,
        },
    },
]

ACCESSORS = [
    {
        "bufferView": 0,
        "componentType": 5126,
        "count": 24,
        "type": "VEC3",
        "max": [0.5, 0.5, 0.5],
        "min": [-0.5, -0.5, -0.5],
    },
    {"bufferView": 1, "componentType": 5126, "count": 24, "type": "VEC3"},
    {"bufferView": 2, "componentType": 5126, "count": 24, "type": "VEC2"},
    {"bufferView": 3, "componentType": 5123, "count": 36, "type": "SCALAR"},
]

BUFFER_VIEWS = [
    {"buffer": 0, "byteOffset": 0, "byteLength": 288, "target": 34962},
    {"buffer": 0, "byteOffset": 288, "byteLength": 288, "target": 34962},
    {"buffer": 0, "byteOffset": 576, "byteLength": 192, "target": 34962},
    {"buffer": 0, "byteOffset": 768, "byteLength": 72, "target": 34963},
]

PRIMITIVE = {
    "attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2},
    "indices": 3,
}


def build_scene(
    cols: int,
    rows: int,
    spacing: float,
    material_count: int,
    buffer_uri: str,
) -> dict[str, Any]:
    if cols < 1 or rows < 1:
        raise ValueError("cols and rows must be >= 1")
    if material_count < 1:
        raise ValueError("material_count must be >= 1")

    materials = MATERIAL_PRESETS[: min(material_count, len(MATERIAL_PRESETS))]
    if material_count > len(MATERIAL_PRESETS):
        for i in range(len(MATERIAL_PRESETS), material_count):
            hue = (i * 0.17) % 1.0
            materials.append(
                {
                    "name": f"stress_{i}",
                    "pbrMetallicRoughness": {
                        "baseColorFactor": [
                            0.5 + 0.5 * math.cos(hue * 6.28),
                            0.5 + 0.5 * math.cos(hue * 6.28 + 2.09),
                            0.5 + 0.5 * math.cos(hue * 6.28 + 4.18),
                            1.0,
                        ],
                        "metallicFactor": (i % 3) * 0.35,
                        "roughnessFactor": 0.25 + (i % 5) * 0.12,
                    },
                }
            )

    meshes = [
        {
            "name": f"cube_mat_{i}",
            "primitives": [{**PRIMITIVE, "material": i}],
        }
        for i in range(len(materials))
    ]

    nodes: list[dict[str, Any]] = []
    for z in range(rows):
        for x in range(cols):
            mesh_index = (x + z) % len(materials)
            tx = (float(x) - (float(cols) - 1.0) * 0.5) * spacing
            tz = (float(z) - (float(rows) - 1.0) * 0.5) * spacing
            nodes.append(
                {
                    "name": f"cube_{x}_{z}",
                    "mesh": mesh_index,
                    "translation": [tx, 0.0, tz],
                }
            )

    return {
        "asset": {
            "version": "2.0",
            "generator": "peaberry gen_stress_scene.py (Phase 7.8)",
        },
        "buffers": [{"byteLength": CUBE_BUFFER_BYTE_LENGTH, "uri": buffer_uri}],
        "bufferViews": BUFFER_VIEWS,
        "accessors": ACCESSORS,
        "materials": materials,
        "meshes": meshes,
        "nodes": nodes,
        "scenes": [{"nodes": list(range(len(nodes)))}],
        "scene": 0,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("-o", "--output", type=Path, required=True, help="Output .gltf path")
    parser.add_argument("--cols", type=int, default=8, help="Grid columns (default 8)")
    parser.add_argument("--rows", type=int, default=8, help="Grid rows (default 8)")
    parser.add_argument("--spacing", type=float, default=2.0, help="Cube spacing (default 2.0)")
    parser.add_argument(
        "--materials",
        type=int,
        default=4,
        help="Number of materials for draw-sort stress (default 4)",
    )
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[1]
    cube_path = (repo_root / CUBE_BUFFER_PATH).resolve()
    output_parent = args.output.resolve().parent
    try:
        buffer_uri = cube_path.relative_to(output_parent).as_posix()
    except ValueError:
        buffer_uri = os.path.relpath(cube_path, output_parent).replace(os.sep, "/")

    scene = build_scene(args.cols, args.rows, args.spacing, args.materials, buffer_uri)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8") as handle:
        json.dump(scene, handle, indent=2)
        handle.write("\n")

    draw_count = args.cols * args.rows
    print(f"wrote {args.output} ({draw_count} draws, {len(scene['materials'])} materials)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
