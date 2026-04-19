## Description

<!-- What does this PR do? Keep it short and focused on the "why". -->

## Related issues

Closes #

## Type of change

- [ ] Bugfix (T-number: ___)
- [ ] New feature (F-number from PROJECT_PLAN.md: ___)
- [ ] Refactor
- [ ] CI / tooling

## Checklist

- [ ] `pio run` compiles cleanly (no warnings)
- [ ] `pio test -e native` passes
- [ ] Tested on hardware (LOLIN D32 + DHT22 + Ruck EM 125L EC 02)
- [ ] `CHANGELOG.md` updated under `[Unreleased]`
- [ ] No blocking calls >10ms in `loop()`
- [ ] All NVS writes check their return value and log the result
