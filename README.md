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
```
