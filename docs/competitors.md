# competitor benchmarks

The `terrain` and `noise` subcommands of `batholith` are head-to-head harnesses against the
projects below. Everything here depends on gitignored checkouts under `third_party/`, so those
subcommands are compiled in only when the checkouts exist (see the `BATHOLITH_BUILD_COMPETITORS`
block in the root `CMakeLists.txt`).

The results and the fairness rules are in [`benchmark.md`](benchmark.md). This file records how
each competitor was obtained and built, so the numbers can be reproduced.

## Summary

| engine | commit | artifact | benchmarkable | why |
|---|---|---|---|---|
| cubiomes | `e61f90580cbdd883214a8054670dacae655e59c0` | `third_party/cubiomes/libcubiomes.a` | **yes**, noise layer | pure C, no runtime; same algorithm as batholith |
| FastNoise2 | `0fcf9fab8a828ad35af4e08abea5c70c372d1ce3` | `third_party/FastNoise2/build/Release/lib/libFastNoise.a` | **yes**, noise layer | C++ library, no runtime; different gradient field |
| Luanti | `5bb520ba544536c4c2914bc0a5ad386e758d7036` | `third_party/luanti/bin/luantiserver.exe` | **no**, see below | mapgen is reachable, but its timing cannot be isolated |
| Cuberite | `7fd3fa5c9345a3f1b949c0988c4849db00a68486` | `third_party/Cuberite/build/bin/cuberite_bench.exe` | **yes**, chunk generation | generator links standalone; needs no server, world or Lua VM |

## cubiomes

```
git clone --depth 1 https://github.com/Cubitect/cubiomes.git third_party/cubiomes
cd third_party/cubiomes && make          # produces libcubiomes.a
```

Used for two things:

1. **Differential verification.** `crosscheck_double_perlin_*` in `src/bridges/dpn_bridge.c` drives
   `xDoublePerlinInit`/`sampleDoublePerlin` from the identical derived RNG state that batholith
   derives. batholith and cubiomes agree to a few double ULP across all four climate parameters.
   This is the check that catches a wrong amplitude table or octave seed.
2. **Noise throughput.** `batholith noise` (`src/cmd_noise.cxx`).

The bridge is C, not C++, because cubiomes declares a plain `lerp()` that collides with
`std::lerp` when its headers share a translation unit with the C++ standard library.

`crosscheck_double_perlin_init` and `crosscheck_double_perlin_sample` are deliberately split.
`xDoublePerlinInit` shuffles 256-entry permutation tables per octave, so re-initialising per
sample measures table construction instead of noise evaluation. An early harness made exactly
that mistake and reported cubiomes at 14,254 ns/sample; the correct figure is ~1,100.

`mapApproxHeight` is **not** used as a height comparison. cubiomes does not implement an
overworld surface height (its own `finders.h` says so), and for MC ≥ 1.18 the function returns
`np[NP_DEPTH] / 76.0`, a biome depth parameter. It is reported only as a throughput figure for a
like-named operation.

## FastNoise2

```
git clone --depth 1 https://github.com/Auburn/FastNoise2.git third_party/FastNoise2
cmake -S third_party/FastNoise2 -B third_party/FastNoise2/build -G Ninja \
      -DCMAKE_BUILD_TYPE=Release -DFASTNOISE2_TOOLS=OFF -DFASTNOISE2_TESTS=OFF
cmake --build third_party/FastNoise2/build
```

Consumers need `-DFASTNOISE_STATIC_LIB -DFASTSIMD_STATIC_LIB` plus two include roots
(`include` and `build/_deps/fastsimd-src/include`); dropping either include root fails to
compile.

`libFastNoise.a` is a **C++** library built with MinGW GCC, so the `noise` subcommand is
restricted to GCC. llvm-mingw defaults to libc++ and MSVC to its own STL, and neither can
satisfy symbols from the GCC C++ runtime.

The tree built here is an explicit **additive** node graph: 18 Perlin nodes (both of
Minecraft's ladders, `octA` and `octB` at 337/331 × input, 9 octaves each), each scaled by
Minecraft's own per-octave weight and summed. It is deliberately **not** `FractalFBm`:
`FractalFBm` applies a constant geometric gain, so it cannot express MC's non-geometric
amplitude array `{1,1,2,2,2,1,1,1,1}`. Benchmarking `FractalFBm` would measure a different
function. A best-fit gain of 0.9659 was computed during an earlier attempt; its residual was
0.31, which is why the explicit tree replaced it.

## Luanti

Built successfully. Full recipe is in `third_party/luanti-build/build_luanti.sh` and
`deps_build.sh`; the essentials:

```
# Luanti hard-rejects MinGW GCC (src/main.cpp:63, broken TLS), so clang is mandatory here.
git clone --depth 1 https://github.com/luanti-org/luanti.git third_party/luanti
cmake -S third_party/luanti -B third_party/luanti/build-server -G Ninja \
      -DBUILD_CLIENT=FALSE -DBUILD_SERVER=TRUE -DBUILD_UNITTESTS=FALSE \
      -DCMAKE_BUILD_TYPE=Release -DENABLE_CURL=FALSE -DENABLE_POSTGRESQL=FALSE \
      -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
      -DCMAKE_PREFIX_PATH=third_party/luanti-deps/prefix
cmake --build third_party/luanti/build-server -j 12
```

SQLite3, zlib and zstd are absent from both toolchains on this machine and were built from
source into `third_party/luanti-deps/prefix`. The linkable archive must be created with
`llvm-ar`, not GNU `ar`: `-flto=thin` makes the objects LLVM bitcode and GNU `ar`'s symbol
index is unusable to `lld`.

**Worldgen is reachable and was verified to run.** `MapgenV7::makeChunk`
(`src/mapgen/mapgen_v7.cpp:299`) executes for real, producing terrain nodes
(`basenodes:dirt_with_grass` over `basenodes:stone`) at the expected surface. Note that the
entry point is `Mapgen::makeChunk`; `MapgenBasic::generateChunk` does not exist in Luanti, and
`calcBiomeNoise` is a `BiomeGen` method, not a `Mapgen` method.

**It is nevertheless not benchmarked, and this is a measurement limitation rather than a
build one.** `third_party/luanti-bench/run_luanti_bench.py` drives `core.emerge_area()` and
counts `EMERGE_GENERATED` blocks, but the emerge counters report *load state*, not generation
cost: the server pre-generates the spawn area before worldmods run, and a benchmark area far
from spawn still reports the overwhelming majority of blocks as `EMERGE_FROM_MEMORY`
(measured: 676 generated vs 64,860 from memory over a 2048² node area). Dividing elapsed time
by the generated count therefore does not measure mapgen. The per-block cost is not
recoverable from these counters, and the engine's profiler did not emit
`EmergeThread: Mapgen::makeChunk` with this configuration.

The control that settles it: running the identical area with `mg_flags = biomes` versus the
full `caves,dungeons,light,decorations,biomes,ores` pipeline produced **43.94 s and 44.15 s**,
with the same 676 generated blocks. Turning on caves, ores, dungeons, decorations and lighting
cost 0.5%. Mapgen is not what that time measures, so no Luanti throughput figure is reported.

An isolated measurement would need to call `makeChunk` directly against a hand-built
`BlockMakeData`. That is feasible, since the objects are in
`third_party/luanti/build-server/libluanti_engine.a` and the factory symbols resolve, but it
requires a `BiomeGen`, a `NodeDefManager` with the `mapgen_stone`/`mapgen_water_source`
aliases registered, and a workaround for `EmergeParams`' deleted public constructor. That is
a separate piece of work and is not claimed here.

Luanti's published figure (~1474 mapblocks/s on an i7-1165G7) is quoted in
`benchmark.md` as a cross-CPU reference point only.

## Cuberite

Built and benchmarked. `third_party/Cuberite/bench/cuberite_bench.cpp` is a purpose-built
harness: it builds an in-memory `cIniFile`, calls `cChunkGenerator::CreateFromIniFile()`, and
times `gen->Generate(cChunkDesc)` over a chunk grid. No server, world, Lua VM or network.

```
git clone --depth 1 https://github.com/cuberite/cuberite.git third_party/Cuberite
git -C third_party/Cuberite submodule update --init --depth 1 \
    lib/mbedtls lib/SQLiteCpp lib/libevent lib/jsoncpp lib/TCLAP lib/expat lib/lua \
    lib/luaexpat lib/luaproxy lib/sqlite lib/tolua++ lib/fmt lib/libdeflate
# Cuberite's CMake invokes `lua` from PATH to generate bindings and none is installed, so
# build the bundled copy:  gcc -O2 -w -o .tools/bin/lua.exe lib/lua/src/*.c
cmake -S third_party/Cuberite -B third_party/Cuberite/build -G Ninja \
      -DCMAKE_BUILD_TYPE=Release -DBUILD_BENCH=ON -DSELF_TEST=OFF \
      -DWHOLE_PROGRAM_OPTIMISATION=OFF          # MSVC 19.44 x64
cmake --build third_party/Cuberite/build --target cuberite_bench -- -j 12
```

Entry points, verified by execution:

- `cChunkGenerator::CreateFromIniFile(cIniFile &)`, `src/Generating/ChunkGenerator.h:47`
- `cChunkGenerator::Generate(cChunkDesc &)`, `src/Generating/ChunkGenerator.h:40`

The ini keys are **top-level**, not under `World/Generator/...`: `[Seed]/Seed`,
`[General]/Dimension`, `[Generator]/Generator` (`Composable` selects `cComposableGenerator`),
which then fills in `BiomeGen=Grown`, `ShapeGen=BiomalNoise3D`, `CompositionGen=Biomal`.

`cuberite_bench.exe` links `lua51.dll` from `build/Server`, so that DLL is copied next to the
binary. Invoke directly:

```
third_party/Cuberite/build/bin/cuberite_bench.exe <seed> <chunks> <runs> <useFinishers>
```

### Measured

1024 chunks, best of 5, seed 987654321, 1 thread, after a warm-up pass.

| configuration | chunks/s | blocks/s |
|---|---|---|
| terrain only (`useFinishers=0`) | 626-1469 | 41.0-96.3 M |

Five independent runs, 1 thread. The spread is ~2.3×: this machine is not quiet, and
Cuberite's generator is fast enough that scheduler noise dominates. Median 82 M.

### Limitation: finishers cannot run in this harness

`useFinishers=1` loads the Overworld finisher list (caves, ravines, ores, decorations, trees,
villages, lighting, ...) and then **crashes with exit 5**:

```
Block "rosebush" is invalid
Block "peony" is invalid
...
```

The finisher path requires a fully populated block registry, which a standalone harness does
not build. So the reported figure covers biome, shape and composition only, with **no caves,
ores, decorations or lighting**. This makes it a *narrower* pipeline than batholith's, which does run
caves, aquifers and surface rules, and it is why the two blocks/s figures are not directly
comparable despite being measured on the same machine.

`cChunkDef::NumBlocks` is 16 × 256 × 16 = 65,536 (`src/ChunkDef.h:124-125`), used as the
denominator.
