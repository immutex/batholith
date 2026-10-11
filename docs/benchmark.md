# benchmark methodology

Performance claims are worthless without the method, so this file states what is measured, on
what, and what is not comparable.

## How to reproduce

```
cmake --preset release && cmake --build --preset release
./build/release/batholith bench --side 16 --threads 1 --repeats 5 --csv
```

Best-of-N wall clock over a square region of chunks starting at (0, 0), seed 0. Raw data is
committed at [`docs/bench/batholith_chunk.csv`](bench/batholith_chunk.csv).

## Metrics

`chunks/s` is meaningless across projects, because chunk sizes differ:

| project | chunk | blocks |
|---|---|---|
| Minecraft / batholith | 16 × 384 × 16 | 98,304 |
| Cuberite | 16 × 256 × 16 | 65,536 |
| Luanti | 16 × 16 × 16 | 4,096 |

Cuberite's chunk height is 256, read from `cChunkDef::Height` in
`third_party/Cuberite/src/ChunkDef.h:125` rather than assumed.

So the primary metric is blocks generated per second, normalised to blocks per second per core.

## What each engine's number includes

This is the part that makes naive comparisons dishonest, so it is stated per row.

| engine | terrain | caves | aquifers | surface | lighting | features | serialization |
|---|---|---|---|---|---|---|---|
| batholith | yes | yes | partial | yes | no | no | no |
| Cuberite, as measured here | yes | no | n/a | partial | no | no | no |
| Cuberite, full server | yes | yes | n/a | yes | yes | yes | yes |
| Vanilla + Chunky | yes | yes | yes | yes | yes | yes | yes |
| Luanti, full server | yes | yes | n/a | yes | yes | yes | yes |

Two distortions have to be held in mind, and they point in opposite directions:

- batholith runs a narrower pipeline than a full server. It does not light, decorate or
  serialize chunks, all of which are a large share of vanilla's cost.
- The Cuberite figure runs a narrower pipeline than batholith. Finishers are disabled, so it has
  no caves, ores, decorations or lighting, work that batholith does do.

So the head-to-head is not a clean win for either side, and no row here should be quoted without
its stage annotation.

## Results

Machine: AMD Ryzen 5 5500X3D (6 cores / 12 threads), GCC 16.1.0, `-O3 -march=native`, seed 0,
side 16 (256 chunks = 25,165,824 blocks), best of 5, five independent repeats per row.

| threads | blocks/s | blocks/s/core | chunks/s |
|---|---|---|---|
| 1 | 4.6-4.7 M | 4.6-4.7 M | 46-48 |
| 6 | 22.2-23.7 M | 3.7-4.0 M | 226-241 |
| 12 | 34.1-35.7 M | 2.8-3.0 M | 347-364 |

Medians 4.66 M / 22.72 M / 34.43 M.

Measurement variance is real and is reported rather than hidden. An identical configuration
measured 20 minutes earlier on a busier machine gave a 3.20 M median with a 2.8-3.7 M spread, and
samples taken before the thread-accounting fix below spanned 6.6 to 12.2 M. Absolute figures are
therefore only meaningful alongside the machine state. Only A/B comparisons taken by alternating
builds within one session are treated as evidence of a speedup.

Scaling is 4.9× at 6 threads and 7.4× at 12. It is sublinear beyond 6 because the 5500X3D has 6
physical cores, so the 12 logical threads contend for the same FP units.

### Correction: every earlier figure was ~2× too high

`thread_pool(n)` used to spawn n worker threads and have the calling thread of `parallel_for`
execute items as well. The pool therefore delivered n+1 threads of parallelism while `batholith
bench` divided throughput by n, so `--threads 1` was really running two threads.

Confirmed by A/B inside a single process: `generate_region` ran at exactly 2.00× the rate of the
identical work driven from one thread, reproducibly across repeats. The pool now provides exactly
n threads (`parallelism - 1` workers, the caller being the last), and `tests/test_thread_pool.cxx`
measures peak concurrency so a regression fails the suite.

This invalidates the previously published 8.4-12.2 M blocks/s/core range. The corrected
single-thread figure is 4.6-4.7 M.

## Head-to-head: equal-math noise benchmark

This is the only genuinely like-for-like comparison in the repository, because every engine
evaluates the identical function: 9-octave Perlin, `firstOctave = -9`, amplitudes
`{1,1,2,2,2,1,1,1,1}` (Minecraft's continentalness climate noise), including both of vanilla's
ladders (`octA` at the input, `octB` at 337/331 × input).

```
./build/release/batholith noise --samples 2000000 --reps 5
```

| engine | ns/sample | Msample/s | notes |
|---|---|---|---|
| batholith | **357-466** | 2.15-2.80 | scalar `normal_noise::sample`, both ladders |
| cubiomes | 1118-1207 | 0.83-0.89 | scalar `sampleDoublePerlin`, both ladders |
| FastNoise2 | 1531-1761 | 0.57-0.65 | scalar `GenSingle3D`, 18-node explicit additive tree |
| FastNoise2 | **89-91** | 11.0-11.2 | `GenPositionArray3D`, AVX2 batch |

Ranges are three independent runs of 2,000,000 samples, best of 5 each. The ranking is stable
across runs; the absolute values are not, which is why a range is given.

### Fairness rules

- Equal math, not equal API. FastNoise2 is built as an explicit additive node tree of 18
  independent Perlin nodes (2 ladders × 9 octaves), each scaled by Minecraft's own per-octave
  weight. It is not `FractalFBm`, which applies a constant geometric gain and so cannot express
  Minecraft's non-geometric amplitude array `{1,1,2,2,2,1,1,1,1}`. Benchmarking `FractalFBm` would
  measure a different function.
- Same position set. A 16×16 grid, spread over 64 blocks so the window spans several
  dominant-octave periods. A narrower window would make the field statistically degenerate.
- Initialisation is excluded. cubiomes' `xDoublePerlinInit` shuffles 256-entry permutation tables
  per octave, so re-initialising per sample measures table construction rather than noise
  evaluation. An early version of this harness made exactly that mistake and reported cubiomes at
  14,254 ns/sample instead of ~1,100. Both the bridge and the harness now initialise once.
- Best of 5, at least 500,000 samples per run.

### Agreement, verified not assumed

Phase 1 of the harness compares outputs before phase 2 compares speed.

| comparison | max abs error | verdict |
|---|---|---|
| batholith vs cubiomes | `1.4e-15` | a few double ULP from floating-point association order |
| batholith vs FastNoise2 | `1.34` | different gradient tables, so not comparable |

batholith and cubiomes implement the same algorithm from the same derived RNG state and agree to
a few double ULP (measured 1.1e-16 to 1.4e-15, varying with position). A wrong amplitude table or
octave seed would show up as an error of order 0.1, twelve orders of magnitude larger, which is
the failure this check exists to catch.

FastNoise2 draws its gradient tables from its own RNG, so it produces a different field rather
than a worse estimate of the same one. A point-wise error or correlation against vanilla's values
would be meaningless and is not reported as a quality signal. What is verified is that the tree is
equal-cost and statistically non-degenerate:

| engine | mean | stddev | range |
|---|---|---|---|
| batholith | -0.026668 | 0.269891 | [-0.664, 0.694] |
| cubiomes | -0.026668 | 0.269891 | [-0.664, 0.694] |
| FastNoise2 | 0.101108 | 0.349409 | [-0.613, 0.894] |

## Full chunk generation

```
./build/release/batholith terrain --side 16 --reps 5 --threads 1
```

The subcommand times the same generator `bench` does, with two additions: it computes a surface
height per column, and it calls cubiomes' height function over the same region. Its timed loop
therefore includes work `bench` omits, but run-to-run variance on this machine is larger than
that difference, so the two agree within noise. The committed CSV above is the number to quote
for the generator.

cubiomes does not provide an overworld surface height. Its own `finders.h` says so, and for
MC ≥ 1.18 `mapApproxHeight()` returns `np[NP_DEPTH]/76.0`, a biome depth parameter rather than a
block height. Measured on this machine that output spans ~46-101 while batholith's real surface
spans ~57-136. The two are not compared, and a ratio between them would be meaningless. The height
map is reported only as an independently reproducible throughput figure for a like-named
operation.

| measurement | result |
|---|---|
| batholith surface (real block height) | 57-136 |
| cubiomes `mapApproxHeight` (depth proxy) | 46-101 |
| cubiomes height map, 1 thread | ~0.15 Mdepth-samples/s |

### Cuberite

Cuberite's generator is linkable standalone and was benchmarked.
`third_party/Cuberite/bench/cuberite_bench.cpp` builds an in-memory `cIniFile`, calls
`cChunkGenerator::CreateFromIniFile()`, and times `gen->Generate(cChunkDesc)` over a 1024-chunk
grid, best of 5, seed 987654321, after a warm-up pass.

| configuration | chunks/s | blocks/s |
|---|---|---|
| terrain (BiomeGen + ShapeGen + CompositionGen) | 626-1469 | 41.0-96.3 M |

Measured 1 thread, five independent runs. Resolved config: `BiomeGen=Grown`,
`ShapeGen=BiomalNoise3D`, `CompositionGen=Biomal`. Raw data:
[`docs/bench/cuberite_chunk.csv`](bench/cuberite_chunk.csv).

The spread is ~2.3×, wider than batholith's own. This machine is not quiet, and Cuberite's
generator is bursty: it is fast enough that scheduler noise and boost state dominate the
measurement. The median (82 M) is quoted alongside the range rather than either extreme.

Two limits on this number, both material:

1. Finishers are off. `Generator/Finishers` is empty, so caves, ravines, ores, decorations,
   lighting and structures do not run. Enabling them crashes the harness with exit 5
   (`Block "rosebush" is invalid`) because the finisher path needs a fully populated block
   registry that a standalone harness does not have. This is a terrain-only figure, not
   comparable to batholith's, which additionally runs caves, aquifers and surface rules.
2. It is faster than batholith by roughly 18×. Cuberite's median is 82 M blocks/s against
   batholith's 4.66 M, so the gap is 82 / 4.66 ≈ 17.6. There is no honest way to spin that:
   Cuberite's shape generator is a coarse 3D-noise heightfield, and batholith evaluates a
   651-node vanilla density graph on a 1,225-point lattice plus per-block caves and aquifer
   tests. They are not the same amount of work, and the gap measures that difference rather than
   a defect in batholith.

### Luanti

Not benchmarked. Luanti's mapgen was verified to run, and `MapgenV7::makeChunk` produces real
terrain, but its per-chunk cost cannot be isolated:

the `core.emerge_area()` counters report load state rather than generation cost, and the spawn
area is pre-generated before worldmods run. A 2048² node area still reported 676 generated blocks
against 64,860 loaded from memory, so elapsed-time-over-generated-count does not measure mapgen.
The control run settles it: `mg_flags = biomes` took 43.94 s and the full
`caves,dungeons,light,decorations,biomes,ores` pipeline took 44.15 s, so enabling caves, ores,
dungeons, decorations and lighting changed the time by 0.5%. Mapgen is not what that number
measures. An isolated measurement needs a hand-built `BlockMakeData`, which is feasible but is
separate work.

Luanti's published figure appears below as a cross-CPU reference point only. Build recipes and
the exact blockers are in [`docs/competitors.md`](competitors.md).

## Reference points from published sources

Different CPUs, different pipelines, different chunk definitions. Included only to show the order
of magnitude, not to claim a like-for-like win.

| engine | CPU | threads | reported | blocks/s |
|---|---|---|---|---|
| Cuberite (measured here, terrain only) | Ryzen 5 5500X3D | 1 | 626-1469 chunks/s | 41.0-96.3 M |
| Vanilla + Chunky 1.20.4 | Ryzen 9 7900X3D | 4 | 183 chunks/s | 18.0 M |
| Vanilla + Chunky 1.20.4 | Ryzen 7 5800X3D | 4 | 141 chunks/s | 13.9 M |
| Vanilla + Chunky 1.20.4 | Ryzen 5 5600 | 4 | 132 chunks/s | 13.0 M |
| Cuberite | i5 (2015) | n/a | 160-180 chunks/s | 5.2-5.9 M |
| Luanti `/emergeblocks` | i7-1165G7 | n/a | 1474 mapblocks/s | 6.0 M |

Sources: `nemez.net` Minecraft CPU benchmarks; `forum.cuberite.org` chunk generator survey;
`forum.luanti.org` emergeblocks benchmark.

## What survived measurement

- Hoisting the trilinear decomposition (+12.4% single-threaded). Every `interpolated` node visited
  for a given block recomputed the same cell index and fractions from the same coordinates, 3.8
  visits per block, each doing floor/divide/clamp work. Caching the decomposition in the workspace
  removes that. A/B inside one process, alternating builds: OLD median 3.62 M blocks/s
  (3.48-3.90) against NEW median 4.07 M (4.01-4.15), non-overlapping ranges and NEW faster in all
  three rounds.
- The lattice evaluation is arithmetic rather than measured: per-block evaluation would touch
  98,304 points per chunk instead of 1,225, an 80× difference that follows directly from
  `size_horizontal`/`size_vertical` in the vanilla data pack.
- Measured stage split, single-threaded, per chunk: `prepare` (lattice build) 9.4 ms and per-block
  evaluation 17.6 ms, so the fixed per-chunk cost is about a third. A node-visit count over a full
  chunk shows 12.7 node evaluations per block and zero noise samples per block, so every noise
  evaluation happens inside the lattice. That is what makes the 80× lattice win real rather than
  aspirational.

## What did not survive measurement

- Per-thread scratch reuse, ~25 allocations removed per chunk: measured 8.08-8.48 M blocks/s with
  it and 7.45-8.02 M without. Overlapping ranges, so the effect is inside noise. Kept because it is
  strictly less work, not because it is measurably faster. These numbers were taken before the
  thread-accounting fix and are ~2× inflated, but the conclusion is unaffected by a uniform scale
  factor.
- Node-level reachability pruning, previously documented as cutting "690 nodes to 234". False.
  `prepared_node_count()` now reports it directly: 651 of 651 nodes are reachable from the nine
  prepared outputs, so no node-level pruning occurs. What pruning does exist is at the output
  level, where unused router outputs (`vein_*`, `ore_vein`, `barrier`, `ridges`,
  `initial_density_without_jaggedness`) are not prepared at all, so their lattices are never
  filled. That saves whole lattice evaluations rather than individual nodes, and the old figure
  conflated the two.
- cubiomes at 14,254 ns/sample: a harness bug, not a result. The bridge re-initialised the
  permutation tables on every sample. Fixed to ~1,100 ns/sample.
- "batholith and cubiomes agree bit-for-bit": measured rather than assumed. The harness reports a
  residue of 1.1e-16 to 1.4e-15, a few double ULP from floating-point association order.
  "Bit-identical" was an overclaim and has been replaced with the measured figure.

## Honest read

- Cuberite is roughly 18× faster than batholith per core on chunk generation, 82 M against 4.66 M
  blocks/s, on a narrower pipeline. This is the headline result and it does not favour batholith.
  Cuberite's shape generator is a coarse 3D-noise heightfield; batholith evaluates the full vanilla
  density graph, 651 nodes on a 1,225-point lattice, plus per-block caves and aquifer tests. The
  honest reading is that batholith's per-block fidelity costs more than an order of magnitude.
- batholith's noise layer is 2.4-2.8× cubiomes on the identical function, and its scalar path beats
  FastNoise2's scalar path by 3.4-4.3×.
- FastNoise2's batch API is 6× faster than any scalar path here, batholith's included. A point-wise
  scalar comparison understates what a SIMD-first library can do, and the real lesson is that
  batholith's own SIMD opportunity is unexploited.
- batholith does not light, decorate or serialize chunks. Those stages are a large share of
  vanilla's cost, and closing that gap is prerequisite to any like-for-like claim against a server.
- Cross-CPU comparison at this granularity is indicative rather than rigorous. The defensible
  head-to-heads are the ones in this file, where every engine ran on this machine: the noise layer
  (all four), and Cuberite's chunk generation.
- No engine here reproduces vanilla terrain, so "faster" is never "faster at the same job" except
  in the noise benchmark, where the function is provably identical.
