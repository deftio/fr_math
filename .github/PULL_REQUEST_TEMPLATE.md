## What and why

<!-- What does this change, and why was the old behavior wrong? Link the issue: Fixes #123 -->

## Checklist

- [ ] `make test` passes with no compiler warnings
- [ ] Behavior changes have a test that fails before this change and passes after
- [ ] Precision changes are measured and documented (`tests/test_tdd.cpp`, `docs/`)
- [ ] No new dependencies, no `long`, no float in the library (`src/`)
- [ ] `docs/` and `pages/guide/` mirrors updated together, if docs changed
- [ ] Entry added to the Unreleased section of `release_notes.md`

Security problems should be reported privately; see `SECURITY.md`.
