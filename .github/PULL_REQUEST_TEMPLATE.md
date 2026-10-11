<!-- One change per pull request. See CONTRIBUTING.md for the code and benchmark rules. -->

## What changed

<!-- A sentence or two. If it fixes a bug, say how you reproduced it before the fix. -->

## How it was verified

<!-- The command you ran and what it printed. For a UI or viewer change, a screenshot. -->

```
```

## Performance

<!-- Delete this section unless the change is meant to affect speed. -->

- Command:
- Machine, compiler, flags:
- Before:
- After:

<!-- Only A/B comparisons from alternating builds within one session count as evidence. -->

## Checklist

- [ ] `ctest --preset release` passes.
- [ ] The build is warning-free on the compiler I used.
- [ ] Comments are still under 2.5% of lines.
- [ ] `clang-format` was run, and the diff contains no unrelated reformatting.
- [ ] Docs were updated if a number or a claim changed.
