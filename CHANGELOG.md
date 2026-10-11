# Changelog

Notable changes to batholith. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and the project aims to follow [Semantic Versioning](https://semver.org/spec/v2.0.0.html) once it
reaches 1.0. Before that, minor versions may change behaviour.

## [Unreleased]

## [0.1.0] - 2026-10-11

First public release.

### Added

- Overworld terrain generation for Minecraft Java 1.21.4: the 651-node density graph, 43 noises,
  430 spline points, caves, aquifers and surface rules, producing a block palette per chunk.
- Router tables generated from the official data pack by `tools/gen_router.py`, committed so a
  plain build needs no network.
- A single `batholith` executable with `gen`, `bench`, `view`, `terrain` and `noise` subcommands.
  `view` builds on Windows, and `terrain` and `noise` only when their competitor checkouts exist.
- An OpenGL viewer that generates and meshes chunks on a background thread, with a headless
  `--no-capture` mode that fails when a frame is a single flat colour.
- Head-to-head harnesses against cubiomes and FastNoise2 on the identical climate noise, and
  against Cuberite on chunk generation.
- 234 tests, warning-free under `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion` on GCC,
  Clang and MSVC.

### Known limitations

- No lighting, features, structures, biome registry or Anvil serialization. See "Known deviations"
  in `docs/worldgen.md`.
- Luanti's mapgen runs but its per-chunk cost cannot be isolated, so it is not benchmarked.

[Unreleased]: https://github.com/immutex/batholith/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/immutex/batholith/releases/tag/v0.1.0
