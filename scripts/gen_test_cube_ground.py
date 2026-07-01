#!/usr/bin/env python3
# Copyright 2026 The Peaberry Authors
# SPDX-License-Identifier: Apache-2.0
"""Generate test_cube_ground.gltf — unit cube above a floor for cast-shadow demos."""

import json
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT_DIR = ROOT / "assets" / "models"
OUT_DIR.mkdir(parents=True, exist_ok=True)

s = 0.5
half_extent = 8.0

face_defs = [
    ((0, 0, 1), [(-s, -s, s), (s, -s, s), (s, s, s), (-s, s, s)]),
    ((0, 0, -1), [(s, -s, -s), (-s, -s, -s), (-s, s, -s), (s, s, -s)]),
    ((1, 0, 0), [(s, -s, s), (s, -s, -s), (s, s, -s), (s, s, s)]),
    ((-1, 0, 0), [(-s, -s, -s), (-s, -s, s), (-s, s, s), (-s, s, -s)]),
    ((0, 1, 0), [(-s, s, s), (s, s, s), (s, s, -s), (-s, s, -s)]),
    ((0, -1, 0), [(-s, -s, -s), (s, -s, -s), (s, -s, s), (-s, -s, s)]),
]

cube_positions = []
cube_normals = []
cube_uvs = []
cube_indices = []

for normal, corners in face_defs:
    base = len(cube_positions) // 3
    face_uvs = [0, 0, 1, 0, 1, 1, 0, 1]
    for i, corner in enumerate(corners):
        cube_positions.extend(corner)
        cube_normals.extend(normal)
        cube_uvs.extend(face_uvs[i * 2 : i * 2 + 2])
    cube_indices.extend([base, base + 1, base + 2, base + 2, base + 3, base])

ground_positions = [
    -half_extent,
    0.0,
    -half_extent,
    half_extent,
    0.0,
    -half_extent,
    half_extent,
    0.0,
    half_extent,
    -half_extent,
    0.0,
    half_extent,
]
ground_normals = [0.0, 1.0, 0.0] * 4
ground_uvs = [0.0, 0.0, 8.0, 0.0, 8.0, 8.0, 0.0, 8.0]
ground_indices = [0, 1, 2, 2, 3, 0]

cube_pos_bytes = struct.pack(f"{len(cube_positions)}f", *cube_positions)
cube_norm_bytes = struct.pack(f"{len(cube_normals)}f", *cube_normals)
cube_uv_bytes = struct.pack(f"{len(cube_uvs)}f", *cube_uvs)
cube_idx_bytes = struct.pack(f"{len(cube_indices)}H", *cube_indices)

ground_pos_bytes = struct.pack(f"{len(ground_positions)}f", *ground_positions)
ground_norm_bytes = struct.pack(f"{len(ground_normals)}f", *ground_normals)
ground_uv_bytes = struct.pack(f"{len(ground_uvs)}f", *ground_uvs)
ground_idx_bytes = struct.pack(f"{len(ground_indices)}H", *ground_indices)

bin_blob = (
    cube_pos_bytes
    + cube_norm_bytes
    + cube_uv_bytes
    + cube_idx_bytes
    + ground_pos_bytes
    + ground_norm_bytes
    + ground_uv_bytes
    + ground_idx_bytes
)

bin_path = OUT_DIR / "test_cube_ground.bin"
bin_path.write_bytes(bin_blob)

cube_pos_off = 0
cube_norm_off = len(cube_pos_bytes)
cube_uv_off = cube_norm_off + len(cube_norm_bytes)
cube_idx_off = cube_uv_off + len(cube_uv_bytes)
cube_vertex_count = len(cube_positions) // 3

ground_pos_off = cube_idx_off + len(cube_idx_bytes)
ground_norm_off = ground_pos_off + len(ground_pos_bytes)
ground_uv_off = ground_norm_off + len(ground_norm_bytes)
ground_idx_off = ground_uv_off + len(ground_uv_bytes)

gltf = {
    "asset": {
        "version": "2.0",
        "generator": "peaberry gen_test_cube_ground.py",
    },
    "buffers": [{"byteLength": len(bin_blob), "uri": "test_cube_ground.bin"}],
    "bufferViews": [
        {"buffer": 0, "byteOffset": cube_pos_off, "byteLength": len(cube_pos_bytes), "target": 34962},
        {"buffer": 0, "byteOffset": cube_norm_off, "byteLength": len(cube_norm_bytes), "target": 34962},
        {"buffer": 0, "byteOffset": cube_uv_off, "byteLength": len(cube_uv_bytes), "target": 34962},
        {"buffer": 0, "byteOffset": cube_idx_off, "byteLength": len(cube_idx_bytes), "target": 34963},
        {"buffer": 0, "byteOffset": ground_pos_off, "byteLength": len(ground_pos_bytes), "target": 34962},
        {"buffer": 0, "byteOffset": ground_norm_off, "byteLength": len(ground_norm_bytes), "target": 34962},
        {"buffer": 0, "byteOffset": ground_uv_off, "byteLength": len(ground_uv_bytes), "target": 34962},
        {"buffer": 0, "byteOffset": ground_idx_off, "byteLength": len(ground_idx_bytes), "target": 34963},
    ],
    "accessors": [
        {
            "bufferView": 0,
            "componentType": 5126,
            "count": cube_vertex_count,
            "type": "VEC3",
            "max": [s, s, s],
            "min": [-s, -s, -s],
        },
        {"bufferView": 1, "componentType": 5126, "count": cube_vertex_count, "type": "VEC3"},
        {"bufferView": 2, "componentType": 5126, "count": cube_vertex_count, "type": "VEC2"},
        {"bufferView": 3, "componentType": 5123, "count": len(cube_indices), "type": "SCALAR"},
        {
            "bufferView": 4,
            "componentType": 5126,
            "count": 4,
            "type": "VEC3",
            "max": [half_extent, 0.0, half_extent],
            "min": [-half_extent, 0.0, -half_extent],
        },
        {"bufferView": 5, "componentType": 5126, "count": 4, "type": "VEC3"},
        {"bufferView": 6, "componentType": 5126, "count": 4, "type": "VEC2"},
        {"bufferView": 7, "componentType": 5123, "count": len(ground_indices), "type": "SCALAR"},
    ],
    "materials": [
        {
            "name": "cube",
            "pbrMetallicRoughness": {
                "baseColorFactor": [0.85, 0.25, 0.2, 1.0],
                "metallicFactor": 0.1,
                "roughnessFactor": 0.55,
            },
        },
        {
            "name": "ground",
            "pbrMetallicRoughness": {
                "baseColorFactor": [0.55, 0.58, 0.62, 1.0],
                "metallicFactor": 0.0,
                "roughnessFactor": 0.9,
            },
        },
    ],
    "meshes": [
        {
            "name": "cube",
            "primitives": [
                {
                    "attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2},
                    "indices": 3,
                    "material": 0,
                }
            ],
        },
        {
            "name": "ground",
            "primitives": [
                {
                    "attributes": {"POSITION": 4, "NORMAL": 5, "TEXCOORD_0": 6},
                    "indices": 7,
                    "material": 1,
                }
            ],
        },
    ],
    "nodes": [
        {"name": "ground", "mesh": 1},
        {"name": "cube", "mesh": 0, "translation": [0.0, 0.5, 0.0]},
    ],
    "scenes": [{"nodes": [0, 1]}],
    "scene": 0,
}

gltf_path = OUT_DIR / "test_cube_ground.gltf"
gltf_path.write_text(json.dumps(gltf, indent=2) + "\n")
print(f"Wrote {gltf_path} and {bin_path} (2 draws)")
