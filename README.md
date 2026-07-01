# Espresso

Examples, benchmarks, and assets for the [Peaberry](https://github.com/whiteghostwork-prog/peaberry) Vulkan renderer.

## Build

Clone peaberry as a sibling directory (or pass `-DPEABERRY_SOURCE_DIR=...`):

```bash
git clone git@github.com:whiteghostwork-prog/peaberry.git ../peaberry
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

## Run

```bash
./build/examples/peaberry_gltf assets/models/test_animation.gltf
./build/benchmarks/peaberry_bench clear --frames 60 --warmup 10
./build/benchmarks/peaberry_bench gltf_shadows --frames 100
./build/benchmarks/peaberry_bench gltf_stress --frames 100
./build/benchmarks/peaberry_bench gltf_shadows --window --frames 100
```

Add `--window` to present frames on screen during the run; the window title shows GPU-derived FPS (no compositor/vsync overhead). Timing stats print to the console when the run finishes. Headless (no flag) is the default for CI.

Stress scenes (`gltf_stress`, `gltf_stress_shadows`) load `assets/scenes/stress_grid.gltf` (64 draws, 4 materials). Regenerate with `python3 scripts/gen_stress_scene.py -o assets/scenes/stress_grid.gltf`.
