## Description

<!-- What does this PR do? Keep it short and focused on the "why". -->

## Related issues

Closes #

## Type of change

- [ ] Bugfix
- [ ] New feature (F-number from PROJECT_PLAN.md: ___)
- [ ] Refactor / YAML cleanup
- [ ] CI / tooling
- [ ] Documentation

## Checklist

- [ ] `esphome config fancontrol.yaml` passes
- [ ] `esphome compile fancontrol.yaml` succeeds (lambdas type-check)
- [ ] Tested on hardware (LOLIN D32 + DHT22 + Ruck EM 125L EC 02) if change affects runtime behaviour
- [ ] `CHANGELOG.md` updated under `[Unreleased]`
- [ ] No hard-coded credentials — all secrets referenced via `!secret ...`
- [ ] Safety behaviour preserved (sensor stall → fan 100 %, min-% floor, alarm thresholds)
