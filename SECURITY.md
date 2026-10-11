# security policy

## Supported versions

batholith is pre-1.0. Only the `main` branch is supported. Fixes land there, and there are no
maintenance branches.

## What the attack surface is

Worth stating plainly, because it is small and it shapes what counts as a vulnerability:

- The library opens no sockets and reads no files. It takes a seed and a chunk coordinate and
  returns a block buffer.
- The command-line tool parses its own arguments and nothing else. It does not read a config
  file, a world save, or any user-supplied data format.
- There is no serialization, no networking, and no plugin or script loading. Those are
  deliberate scope limits, listed in `docs/worldgen.md`.

So the realistic issues are memory-safety bugs in the generator or mesher reachable from a seed
or a chunk coordinate, such as an out-of-bounds write on an unusual terrain shape, and integer
overflow in the argument parsing.

Two things are outside that surface and are still worth reporting:

- `tools/gen_router.py` fetches the vanilla data pack from `misode/mcmeta` at a pinned tag. A
  supply-chain problem there would affect anyone regenerating the tables.
- The CI workflow runs on pull requests. A workflow injection would be a real finding.

## Reporting

Use GitHub's private vulnerability reporting on this repository, under the Security tab. Please
do not open a public issue for a memory-safety bug before there is a fix.

Include the seed, the chunk coordinate, the compiler and the flags, and the sanitizer output if
you have it. A crash reproduced under `-fsanitize=address,undefined` is the most useful report.

This is a spare-time project, so expect an initial response within about a week. There is no
bug bounty.
