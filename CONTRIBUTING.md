# contributing

Thanks for looking. This is a small project with a narrow scope, so the most useful
contributions are usually bug reports with a reproduction, and corrections to the documentation.

## What fits

- **Correctness fixes.** batholith aims to reproduce vanilla terrain. If a primitive disagrees
  with the 1.21.4 server, that is a real bug and the most valuable thing you can report.
- **Performance work.** Only with a measurement, following the rules below.
- **New competitor harnesses.** A new subcommand that measures another generator on the same
  machine is welcome. The fairness rules in `docs/benchmark.md` apply to any new row.
- **Documentation.** The docs state where numbers come from. If a number is stale or a claim is
  wrong, a correction is worth more than a new page.

## What does not fit

- Rendering, gameplay, entities, networking, lighting, or chunk serialization. These are
  deliberate scope limits, listed under "Known deviations" in `docs/worldgen.md`. A pull request
  adding one of them will be closed.
- A new dependency for the library itself. The generator has none and should keep none.
- An abstraction with one implementation, or a helper with one caller.

## Building

A C++23 compiler (GCC 13+, Clang 17+, MSVC 19.36+) and CMake 3.25 or newer. Nothing else.

```
cmake --preset release
cmake --build --preset release
ctest --preset release
```

The `release` preset uses the default compiler. To pin one explicitly, configure by hand:

```
cmake -S . -B build/gcc -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build build/gcc
```

The build is warning-free under `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion`
(MSVC `/W4`), and it is expected to stay that way. Warnings are errors in review.

## Running the tool

There is one executable with subcommands. `gen` and `bench` are always built; `view` is built on
Windows, and `terrain` and `noise` only when the competitor checkouts under `third_party/` exist.

```
batholith gen   --seed 0 --chunks 256 --threads 12 --dump
batholith bench --side 16 --threads 12 --repeats 5
batholith view  --seed 0 --walk 2
batholith --help
```

`batholith view --no-capture` renders headless and exits non-zero when the frame is a single flat
colour, which is a useful check that the mesher still works.

## Code style

The rules are in `.clang-format` and `.editorconfig`, and the short version is:

- `snake_case` for files, types, functions, variables and namespaces. `enum class`, never a
  plain `enum`.
- Declarations in a `.hxx`, definitions in a `.cxx`. One `namespace batholith {` per file, no
  nesting.
- `const` and `constexpr` by default. `[[nodiscard]]` on a pure query. No `new`, `delete` or
  `malloc`.
- Comments are capped at 2.5% of lines. Write one only when the code cannot state the fact
  itself, such as a constant traced to the spec or a deliberate deviation. Never restate a name
  or a call. If a change makes a comment stale, delete it.
- The C++ Core Guidelines apply. The rules that bite here are immutability (Con.1-5), RAII
  (R.1, R.3, R.11), constructors that establish invariants (C.41), and the Rule of Zero (C.20).

Run `clang-format` before you push. Unrelated reformatting in a pull request makes it hard to
review, so keep the diff to the change.

## Benchmark rules

A performance claim without a measurement is not accepted. The method matters more than the
number:

- Report the command you ran, the CPU, the compiler and the flags.
- Give a range over repeated runs, not the best single run.
- Only A/B comparisons taken by alternating builds within one session count as evidence of a
  speedup. Absolute numbers track machine state, and on a busy machine they move by a factor of
  two. This has already produced two wrong results in this repository, both documented in
  `docs/benchmark.md`.
- Say which pipeline stages a number covers. batholith's chunk generation includes terrain,
  caves, aquifers and surface material. It does not light, decorate or serialize, so a figure
  from a full server is not comparable.

## Pull requests

Keep a pull request to one change. In the description, say what changed, how you verified it, and
include the numbers if it is a performance change. If it fixes a bug, describe how you reproduced
it before the fix.

Add a test when behaviour is subtle or a boundary is easy to get wrong. Do not add a test that
only re-checks a wiring path or restates the implementation.

## Reporting a bug

Open an issue with the command you ran, the output you got, and the output you expected. For a
terrain mismatch, the seed and chunk coordinate are what make it reproducible. If you can, say
which primitive you believe is wrong and how you checked it against the server.
