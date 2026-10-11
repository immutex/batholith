# worldgen spec

batholith reproduces **Minecraft Java 1.21.4** overworld terrain generation. This document
records where the numbers come from, so the implementation can be audited rather than trusted.

## Source of truth

All data tables are generated from `misode/mcmeta`, tag `1.21.4-data-json`, the data-generator
output of the official server jar.

```
python tools/gen_router.py     # regenerates include/batholith/router_data.hxx + src/router_data.cxx
```

The generated files are committed so a plain build needs no network.

| artefact | value |
|---|---|
| noises | 43 (42 normal + 1 blended) |
| density nodes | 651 |
| spline points | 430 |
| prepared router outputs | 9 |

## Shape of the terrain function

`final_density` reduces to:

```
min( squeeze(0.64 * interpolated(blend_density(...))), caves/noodle )
```

The `interpolated` node is what makes this tractable. Vanilla evaluates the density graph on a
coarse lattice. `size_horizontal = 1` and `size_vertical = 2` give a 4×8×4 block cell, so a
16×384×16 chunk becomes **5 × 49 × 5 = 1225 lattice points**, which are then trilinearly
interpolated to 98,304 blocks. Evaluating the graph per block costs 80× more.

Everything outside `interpolated` (`squeeze`, `mul`, `min`, and `caves/noodle`, which contains
its own `interpolated` nodes) is genuinely per-block, and is implemented that way.

## Fresh-world folds

Two node types are folded at generation time rather than implemented:

- `blend_density`: identical to its argument unless newly generated chunks border terrain from
  an older generator. `blend_alpha` and `blend_offset` are 0 on a fresh world, so
  `lerp(blend_alpha, first, second)` is just `first`.
- `beardifier`: driven by structure placement. batholith generates no structures, so it is 0.

Both are folded to their fresh-world values rather than stubbed out, so a generator that supports
blending or structures has to revisit them.

## Unused router outputs

Only the outputs the generator actually samples are prepared: `final_density`, the five climate
parameters, `depth`, and the two aquifer parameters. `vein_toggle`, `vein_ridged`, `vein_gap`,
`ore_vein`, `barrier`, `ridges` and `initial_density_without_jaggedness` are pruned. The
`density_graph` constructor takes the output list and derives its evaluation order by walking
backwards from those roots, so a pruned subtree's `interpolated` lattice is never built or filled,
which removes several full lattice evaluations per chunk.

## Verified primitives

Every low-level primitive was checked against the official 1.21.4 server jar, executed via
reflection, plus a decompiled source mirror. The noise stack is additionally cross-checked
bit-for-bit against `cubiomes` (an independent reimplementation); see `docs/benchmark.md` for the
differential test. The shipped test suite covers:

- `Xoroshiro128PlusPlus.nextLong` and the seed expansion (`mixStafford13`, silver/golden ratios)
- `nextDouble` / `nextFloat` / `nextInt(bound)` bit-for-bit, including the Lemire rejection path
- `RandomSupport.seedFromHashOf`: MD5 of the UTF-8 name, first 8 bytes big-endian as `lo`,
  last 8 as `hi`
- `Mth.getSeed`: `x * 3129871` is a **32-bit** multiply that then widens, and the naive 64-bit form
  diverges for |x| ≥ 687
- `PerlinNoise` octave construction, `wrap()` (modulus exactly 2²⁵), `edgeValue`, `maxBrokenValue`
- `ImprovedNoise`: 256-entry table (`& 0xFF` on every lookup, no doubled 512 table), the
  `SimplexNoise.GRADIENT` 16-row table, draw order 3 doubles + 256 bounded ints
- `NormalNoise`: `valueFactor = (1/6) / (0.1 * (1 + 1/(hi-lo+1)))` over the min/max **non-zero
  amplitude indices**; `INPUT_FACTOR = 337/331`. The result is often not a round number:
  continentalness yields `1.4999999999999998`, not `1.5`
- `BlendedNoise`: 40 sequential `ImprovedNoise` instances (16 min, 16 max, 8 main), the
  `(main/10 + 1)/2` blend factor, `clampedLerp(lo/512, hi/512, t)/128`
- `CubicSpline`: **float** arithmetic throughout, cubic Hermite inside the range, and linear
  extrapolation by the end derivative outside it, which is flat when that derivative is 0.
  Boundary values are not clamped
- `WeirdScaledSampler`: `scale * abs(noise(x/scale, y/scale, z/scale))` where `scale` is a
  quantized step function of the input. TYPE1 steps at −0.5/0/0.5 to 0.75/1.0/1.5/2.0;
  TYPE2 steps at −0.75/−0.5/0.5/0.75 to 0.5/0.75/1.0/2.0/3.0. Boundary values take the upper
  branch

## Node evaluation cost

Over a full chunk, `final_density` performs **12.7 node evaluations per block** and **zero noise
samples per block**, because every noise evaluation happens while filling the lattice. The
per-chunk split is about a third lattice build (`prepare`) and two thirds per-block evaluation,
so the lattice is where the wins are and per-block work is mostly cheap arithmetic over values
that already exist.

One block position is shared by every `interpolated` node visited for it, 3.8 visits per block on
average, so the trilinear cell and its fractions are computed once and cached in the workspace.
That reuse is worth +12.4% single-threaded.

## Chunk-local interpolation lattice

The `interpolated` and `flat_cache` tables are sized for one chunk and are filled in
**chunk-local** coordinates (0..16 horizontally). `density_workspace` therefore carries the chunk
origin, and both `interpolate()` and the `flat_cache` lookup subtract it. Sampling still takes
world coordinates, which is what the noise needs. Getting this wrong is silent: every chunk
beyond the first clamps to the same lattice corner and the terrain flattens to a plane.

## Known deviations

- **No structures, features, lighting, or serialization.** Output is a block palette only.
- **No biome registry.** Biomes are a coarse climate classifier used solely to pick surface
  material; they are not vanilla's biome IDs.
- **Aquifers are simplified.** Fluid placement follows the floodedness model but omits the
  cross-chunk "is there a nearby surface below sea level" probe.
- **Surface rules are a compiled-in approximation** of the overworld rule tree, not a
  general `material_rule` interpreter.

These are scope limits rather than unfinished work, and each one is a prerequisite for claiming
block-for-block parity with vanilla.
